// ============================================================================
//  orivm.c — the Ori VM core, in C.
//
//  Loads and runs Ori program images:
//    *.orx : the encrypted format (ChaCha20 + HMAC + opcode permutation +
//            operand whitening) — byte-compatible with src/OriLang/Container.cs
//    *.orb : a plain bytecode format (no crypto) — what the Ori-written compiler
//            emits, so "Ori compiles Ori" and C runs it.
//
//  Build (Windows, from a VS dev shell):  cl /O2 /D_CRT_SECURE_NO_WARNINGS orivm.c
//  Build (clang/gcc):                      cc -O2 -o orivm orivm.c -lm
// ============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <dirent.h>
#include <fnmatch.h>
#ifndef __ANDROID__
#include <glob.h>
#endif
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#endif
#include "sha256.h"
#include "chacha20.h"

static uint8_t* read_all(const char* path, size_t* outN); // fwd

// ---------------------------------------------------------------------------
//  Opcodes (must match src/OriLang/OpCode.cs)
// ---------------------------------------------------------------------------
enum {
    OP_HALT=0, OP_PUSHCONST=1, OP_PUSHNIL=2, OP_PUSHTRUE=3, OP_PUSHFALSE=4, OP_POP=5,
    OP_LOADGLOBAL=6, OP_STOREGLOBAL=7, OP_LOADLOCAL=8, OP_STORELOCAL=9,
    OP_ADD=10, OP_SUB=11, OP_MUL=12, OP_DIV=13, OP_MOD=14, OP_NEG=15,
    OP_EQ=16, OP_NEQ=17, OP_LT=18, OP_GT=19, OP_LE=20, OP_GE=21, OP_NOT=22,
    OP_JMP=23, OP_JMPIFFALSE=24, OP_JMPIFTRUE=25, OP_CALL=26, OP_RET=27,
    OP_MAKEARRAY=28, OP_INDEX=29, OP_STOREINDEX=30,
    OP_PUSHINT=31  // arg = an integer literal pushed directly (used by the Ori-written compiler)
};

static void die(const char* msg){ fprintf(stderr, "%s\n", msg); exit(3); }
static void* xmalloc(size_t n){ void* p=malloc(n); if(!p) die("out of memory"); return p; }
static void* xrealloc(void* p, size_t n){ void* q=realloc(p,n); if(!q) die("out of memory"); return q; }
static char* dupstr(const char* s){ size_t n=strlen(s)+1; char* d=xmalloc(n); memcpy(d,s,n); return d; }
static void rt_error(const char* msg); /* fwd — defined after VM struct */

// ---------------------------------------------------------------------------
//  Values
// ---------------------------------------------------------------------------
typedef enum { V_NIL, V_NUM, V_BOOL, V_STR, V_FUNC, V_HOST, V_ARR } VType;
typedef struct { int len; char* d; } Str;
struct Value;
typedef struct { int len, cap; struct Value* it; } Arr;
typedef struct Value { VType t; union { double num; Str* s; Arr* a; int i; } u; } Value;

static Value vnil(){ Value v; v.t=V_NIL; v.u.num=0; return v; }
static Value vnum(double n){ Value v; v.t=V_NUM; v.u.num=n; return v; }
static Value vbool(int b){ Value v; v.t=V_BOOL; v.u.num=b?1:0; return v; }
static Value vfunc(int i){ Value v; v.t=V_FUNC; v.u.i=i; return v; }
static Value vhost(int i){ Value v; v.t=V_HOST; v.u.i=i; return v; }

static Value vstr_n(const char* s, int len){
    Str* st=xmalloc(sizeof(Str)); st->len=len; st->d=xmalloc(len+1);
    memcpy(st->d,s,len); st->d[len]=0;
    Value v; v.t=V_STR; v.u.s=st; return v;
}
static Value vstr(const char* s){ return vstr_n(s,(int)strlen(s)); }
static Value varr_new(){
    Arr* a=xmalloc(sizeof(Arr)); a->len=0; a->cap=4; a->it=xmalloc(sizeof(Value)*a->cap);
    Value v; v.t=V_ARR; v.u.a=a; return v;
}
static void arr_push(Arr* a, Value v){
    if(a->len>=0x400000) rt_error("array too large (>4M elements; 64MB limit)");
    if(a->len>=a->cap){ a->cap*=2; a->it=xrealloc(a->it,sizeof(Value)*a->cap); }
    a->it[a->len++]=v;
}

static int is_truthy(Value v){
    switch(v.t){
        case V_NIL: return 0;
        case V_BOOL: return v.u.num!=0;
        case V_NUM: return v.u.num!=0;
        case V_STR: return v.u.s->len!=0;
        default: return 1;
    }
}
static int val_eq(Value a, Value b){
    if(a.t!=b.t) return 0;
    switch(a.t){
        case V_NIL: return 1;
        case V_STR: return a.u.s->len==b.u.s->len && memcmp(a.u.s->d,b.u.s->d,a.u.s->len)==0;
        case V_ARR: return a.u.a==b.u.a;
        default: return a.u.num==b.u.num;
    }
}

// ---- string builder ----
typedef struct { char* d; int len, cap; } Sb;
static void sb_init(Sb* b){ b->cap=64; b->len=0; b->d=xmalloc(b->cap); b->d[0]=0; }
static void sb_putc(Sb* b, char c){ if(b->len>=0x3FFFFF0) rt_error("string too large (>64MB limit)"); if(b->len+1>=b->cap){ b->cap*=2; b->d=xrealloc(b->d,b->cap);} b->d[b->len++]=c; b->d[b->len]=0; }
static void sb_put(Sb* b, const char* s, int n){ for(int i=0;i<n;i++) sb_putc(b,s[i]); }
static void sb_puts(Sb* b, const char* s){ sb_put(b,s,(int)strlen(s)); }

static void val_display_d(Sb* b, Value v, int depth){
    char tmp[64];
    switch(v.t){
        case V_NIL: sb_puts(b,"nil"); break;
        case V_BOOL: sb_puts(b, v.u.num!=0?"true":"false"); break;
        case V_STR: sb_put(b, v.u.s->d, v.u.s->len); break;
        case V_FUNC: snprintf(tmp,sizeof tmp,"<fn#%d>",v.u.i); sb_puts(b,tmp); break;
        case V_HOST: snprintf(tmp,sizeof tmp,"<host#%d>",v.u.i); sb_puts(b,tmp); break;
        case V_NUM: {
            double n=v.u.num;
            if(isfinite(n) && n==floor(n) && fabs(n)<9.2e18){ snprintf(tmp,sizeof tmp,"%lld",(long long)n); }
            else { snprintf(tmp,sizeof tmp,"%.15g",n); }
            sb_puts(b,tmp); break;
        }
        case V_ARR: {
            if(depth>32){ sb_puts(b,"[...]"); break; }
            sb_putc(b,'[');
            for(int i=0;i<v.u.a->len;i++){
                if(i) sb_puts(b,", ");
                Value e=v.u.a->it[i];
                if(e.t==V_STR){ sb_putc(b,'"'); sb_put(b,e.u.s->d,e.u.s->len); sb_putc(b,'"'); }
                else val_display_d(b,e,depth+1);
            }
            sb_putc(b,']'); break;
        }
    }
}
static void val_display(Sb* b, Value v){ val_display_d(b,v,0); }
static char* val_cstr(Value v){ Sb b; sb_init(&b); val_display(&b,v); return b.d; }

// ---------------------------------------------------------------------------
//  Program model
// ---------------------------------------------------------------------------
typedef struct { uint8_t op; int32_t arg; } Instr;
typedef struct { char* name; int arity; int localCount; int codeCount; Instr* code; } Func;
typedef struct { int constCount; Value* consts; int mainIndex; int funcCount; Func* funcs; } Program;

// ---------------------------------------------------------------------------
//  Container: master key + permutation (must match Container.cs)
// ---------------------------------------------------------------------------
static const uint8_t KA[32]={
0x9E,0x37,0x79,0xB9,0x7F,0x4A,0x7C,0x15,0xF3,0x9C,0xC0,0x60,0x5C,0xED,0xC8,0x34,
0x1B,0x2D,0x6A,0xE5,0x49,0x0C,0x3F,0x71,0x88,0x14,0x57,0xA2,0xD3,0x6E,0xBC,0x05};
static const uint8_t KB[32]={
0x21,0x66,0x4C,0x9A,0x7E,0xD5,0x10,0x88,0x44,0x37,0x70,0x1B,0xE9,0x12,0x0F,0x6F,
0xA5,0xC3,0x91,0x2E,0x77,0xBA,0xD0,0x49,0x5B,0xE2,0x08,0x3C,0x6D,0x9F,0x14,0xF0};
#define PERM_SEED 0xC0FFEE17u
#define WHITE_SEED 0x1B873593u

static uint8_t PERM[256], INVPERM[256];
static void build_perm(){
    for(int i=0;i<256;i++) PERM[i]=(uint8_t)i;
    uint32_t st=PERM_SEED;
    for(int i=255;i>0;i--){
        st^=st<<13; st^=st>>17; st^=st<<5;
        int j=(int)(st % (uint32_t)(i+1));
        uint8_t t=PERM[i]; PERM[i]=PERM[j]; PERM[j]=t;
    }
    for(int i=0;i<256;i++) INVPERM[PERM[i]]=(uint8_t)i;
}
static void master_key(uint8_t out[32]){
    uint8_t seed[56];
    for(int i=0;i<32;i++) seed[i]=KA[i]^KB[i];
    for(int i=0;i<16;i++) seed[32+i]=PERM[(i*17+3)&0xFF];
    uint32_t ps=PERM_SEED;     seed[48]=(uint8_t)ps;seed[49]=(uint8_t)(ps>>8);seed[50]=(uint8_t)(ps>>16);seed[51]=(uint8_t)(ps>>24);
    uint32_t x=0xA5C391E2u;     seed[52]=(uint8_t)x;seed[53]=(uint8_t)(x>>8);seed[54]=(uint8_t)(x>>16);seed[55]=(uint8_t)(x>>24);
    sha256(seed,56,out);
}
static void derive(const uint8_t master[32], const uint8_t* salt, int saltLen, const char* label, uint8_t out[32]){
    int ll=(int)strlen(label);
    uint8_t* buf=xmalloc(32+saltLen+ll);
    memcpy(buf,master,32); memcpy(buf+32,salt,saltLen); memcpy(buf+32+saltLen,label,ll);
    sha256(buf,32+saltLen+ll,out); free(buf);
}

// ---- byte cursor — all reads bounds-checked to prevent buffer over-read ----
typedef struct { const uint8_t* p; size_t n, pos; } Cur;
static void cur_need(Cur* c, size_t need){
    if(c->pos + need > c->n) die("malformed bytecode image (truncated read)");
}
static int32_t rd_i32(Cur* c){ cur_need(c,4); const uint8_t* b=c->p+c->pos; c->pos+=4;
    return (int32_t)((uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24)); }
static uint8_t rd_u8(Cur* c){ cur_need(c,1); return c->p[c->pos++]; }
static double rd_double(Cur* c){ cur_need(c,8); uint64_t u=0; for(int i=0;i<8;i++) u|=((uint64_t)c->p[c->pos++])<<(8*i); double d; memcpy(&d,&u,8); return d; }
static char* rd_str(Cur* c, int* outLen){ int len=rd_i32(c);
    if(len<0||(size_t)len>c->n-c->pos) die("malformed bytecode image (bad string length)");
    char* s=xmalloc((size_t)len+1); memcpy(s,c->p+c->pos,len); s[len]=0; c->pos+=len; if(outLen)*outLen=len; return s; }

static void build_perm_seeded(uint32_t seed, uint8_t perm[256], uint8_t inv[256]){
    for(int i=0;i<256;i++) perm[i]=(uint8_t)i;
    uint32_t st=seed;
    for(int i=255;i>0;i--){ st^=st<<13; st^=st>>17; st^=st<<5; int j=(int)(st%(uint32_t)(i+1)); uint8_t t=perm[i];perm[i]=perm[j];perm[j]=t; }
    for(int i=0;i<256;i++) inv[perm[i]]=(uint8_t)i;
}

// Deserialize the plaintext payload into a Program.
// invPerm!=NULL: opcodes go through it. whitened: operands XORed by xorshift(whitenSeed).
static Program* deserialize(const uint8_t* data, size_t n, const uint8_t* invPerm, int whitened, uint32_t whitenSeed){
    Cur c={data,n,0};
    Program* pr=xmalloc(sizeof(Program));
    pr->constCount=rd_i32(&c);
    if(pr->constCount<0||pr->constCount>65535) die("malformed bytecode: constCount out of range");
    pr->consts=xmalloc(sizeof(Value)*(pr->constCount>0?pr->constCount:1));
    for(int i=0;i<pr->constCount;i++){
        uint8_t t=rd_u8(&c);
        switch(t){
            case V_NUM: pr->consts[i]=vnum(rd_double(&c)); break;
            case V_STR: { int len; char* s=rd_str(&c,&len); pr->consts[i]=vstr_n(s,len); free(s); break; }
            case V_BOOL: pr->consts[i]=vbool(rd_u8(&c)!=0); break;
            case V_NIL: pr->consts[i]=vnil(); break;
            default: die("bad constant tag in image");
        }
    }
    pr->mainIndex=rd_i32(&c);
    pr->funcCount=rd_i32(&c);
    if(pr->funcCount<0||pr->funcCount>16383) die("malformed bytecode: funcCount out of range");
    pr->funcs=xmalloc(sizeof(Func)*(pr->funcCount>0?pr->funcCount:1));
    uint32_t white=whitenSeed;
    for(int i=0;i<pr->funcCount;i++){
        Func* f=&pr->funcs[i];
        f->name=rd_str(&c,NULL);
        f->arity=rd_i32(&c);
        f->localCount=rd_i32(&c);
        if(f->localCount<0||f->localCount>4095) die("malformed bytecode: localCount out of range");
        f->codeCount=rd_i32(&c);
        if(f->codeCount<0||f->codeCount>1048575) die("malformed bytecode: codeCount out of range");
        f->code=xmalloc(sizeof(Instr)*(f->codeCount>0?f->codeCount:1));
        for(int j=0;j<f->codeCount;j++){
            uint8_t raw=rd_u8(&c);
            int32_t arg=rd_i32(&c);
            f->code[j].op = invPerm ? invPerm[raw] : raw;
            if(whitened){ white^=white<<13; white^=white>>17; white^=white<<5; arg ^= (int32_t)white; }
            f->code[j].arg=arg;
        }
    }
    return pr;
}

// .orx v2 header: "ORIX" ver=2 flags salt[16] nonce[12] cipherLen[4] | cipher | mac[32]
// Decrypted payload: permSeed[4] whitenSeed[4] | serialized program (opcodes permuted
// by a PER-BUILD random seed, operands whitened). So the opcode mapping differs every build.
static Program* load_orx(const uint8_t* img, size_t n){
    if(n<38+32) die("image too small");
    if(memcmp(img,"ORIX",4)!=0) die("bad magic (not .orx)");
    if(img[4]!=2) die("unsupported .orx version");
    const uint8_t* salt=img+6;
    const uint8_t* nonce=img+22;
    int32_t cipherLen=(int32_t)((uint32_t)img[34]|((uint32_t)img[35]<<8)|((uint32_t)img[36]<<16)|((uint32_t)img[37]<<24));
    size_t bodyLen=38+(size_t)cipherLen;
    if(cipherLen<8 || bodyLen+32>n) die("corrupt image (length mismatch)");
    uint8_t master[32]; master_key(master);
    uint8_t macKey[32]; derive(master,salt,16,"ORI-MAC-v2",macKey);
    uint8_t mac[32]; hmac_sha256(macKey,32,img,bodyLen,mac);
    if(memcmp(mac,img+bodyLen,32)!=0) die("integrity check failed (tampered or wrong key)");
    uint8_t encKey[32]; derive(master,salt,16,"ORI-ENC-v2",encKey);
    uint8_t* plain=xmalloc(cipherLen);
    memcpy(plain,img+38,cipherLen);
    chacha20_crypt(encKey,nonce,1,plain,cipherLen);
    uint32_t permSeed = (uint32_t)plain[0]|((uint32_t)plain[1]<<8)|((uint32_t)plain[2]<<16)|((uint32_t)plain[3]<<24);
    uint32_t whitenSeed=(uint32_t)plain[4]|((uint32_t)plain[5]<<8)|((uint32_t)plain[6]<<16)|((uint32_t)plain[7]<<24);
    uint8_t perm[256], inv[256]; build_perm_seeded(permSeed,perm,inv);
    return deserialize(plain+8,cipherLen-8,inv,1,whitenSeed);
}

static Program* load_orb(const uint8_t* img, size_t n){
    if(n<4 || memcmp(img,"ORB1",4)!=0) die("bad magic (not .orb)");
    return deserialize(img+4,n-4,NULL,0,0);
}

// ---- serialize a Program back to a plaintext payload (opcodes permuted, operands whitened) ----
typedef struct { uint8_t* d; size_t len, cap; } Buf;
static void buf_init(Buf* b){ b->cap=256; b->len=0; b->d=xmalloc(b->cap); }
static void buf_u8(Buf* b, uint8_t v){ if(b->len+1>b->cap){ b->cap*=2; b->d=xrealloc(b->d,b->cap);} b->d[b->len++]=v; }
static void buf_i32(Buf* b, int32_t v){ uint32_t u=(uint32_t)v; buf_u8(b,(uint8_t)u);buf_u8(b,(uint8_t)(u>>8));buf_u8(b,(uint8_t)(u>>16));buf_u8(b,(uint8_t)(u>>24)); }
static void buf_double(Buf* b, double d){ uint64_t u; memcpy(&u,&d,8); for(int i=0;i<8;i++) buf_u8(b,(uint8_t)(u>>(8*i))); }
static void buf_str(Buf* b, const char* s, int len){ buf_i32(b,len); for(int i=0;i<len;i++) buf_u8(b,(uint8_t)s[i]); }

static uint8_t* serialize_payload(Program* pr, const uint8_t perm[256], uint32_t whitenSeed, size_t* outLen){
    Buf b; buf_init(&b);
    buf_i32(&b, pr->constCount);
    for(int i=0;i<pr->constCount;i++){
        Value v=pr->consts[i];
        buf_u8(&b,(uint8_t)v.t);
        if(v.t==V_NUM) buf_double(&b,v.u.num);
        else if(v.t==V_STR) buf_str(&b,v.u.s->d,v.u.s->len);
        else if(v.t==V_BOOL) buf_u8(&b, v.u.num!=0?1:0);
        // V_NIL: nothing
    }
    buf_i32(&b, pr->mainIndex);
    buf_i32(&b, pr->funcCount);
    uint32_t white=whitenSeed;
    for(int f=0;f<pr->funcCount;f++){
        Func* fn=&pr->funcs[f];
        buf_str(&b, fn->name,(int)strlen(fn->name));
        buf_i32(&b, fn->arity);
        buf_i32(&b, fn->localCount);
        buf_i32(&b, fn->codeCount);
        for(int j=0;j<fn->codeCount;j++){
            buf_u8(&b, perm[(uint8_t)fn->code[j].op]);
            white^=white<<13; white^=white>>17; white^=white<<5;
            buf_i32(&b, fn->code[j].arg ^ (int32_t)white);
        }
    }
    *outLen=b.len; return b.d;
}

static uint32_t mix_seed(){
    uint32_t s=(uint32_t)time(NULL);
    s ^= (uint32_t)clock()*2654435761u;
    s ^= (uint32_t)(uintptr_t)&s;
    s ^= s<<13; s^=s>>17; s^=s<<5;
    if(s==0) s=0x9E3779B9u;
    return s;
}
static void fill_rand(uint8_t* p, int n, uint32_t* st){
    for(int i=0;i<n;i++){ *st^=*st<<13; *st^=*st>>17; *st^=*st<<5; p[i]=(uint8_t)(*st>>((i&3)*8)); }
}

// pack a plain .orb into an encrypted, per-build-randomized .orx (v2)
static int do_pack(const char* inPath, const char* outPath){
    size_t n; uint8_t* img=read_all(inPath,&n);
    Program* pr;
    if(n>=4 && memcmp(img,"ORB1",4)==0) pr=load_orb(img,n);
    else if(n>=4 && memcmp(img,"ORIX",4)==0) pr=load_orx(img,n);
    else { fprintf(stderr,"pack: input must be .orb or .orx\n"); return 2; }

    uint32_t rng=mix_seed();
    uint32_t permSeed; fill_rand((uint8_t*)&permSeed,4,&rng); if(permSeed==0) permSeed=1;
    uint32_t whitenSeed; fill_rand((uint8_t*)&whitenSeed,4,&rng); if(whitenSeed==0) whitenSeed=1;
    uint8_t salt[16], nonce[12]; fill_rand(salt,16,&rng); fill_rand(nonce,12,&rng);

    uint8_t perm[256], inv[256]; build_perm_seeded(permSeed,perm,inv);
    size_t plLen; uint8_t* body=serialize_payload(pr,perm,whitenSeed,&plLen);
    // prepend the two seeds
    size_t payLen=8+plLen; uint8_t* payload=xmalloc(payLen);
    payload[0]=(uint8_t)permSeed;payload[1]=(uint8_t)(permSeed>>8);payload[2]=(uint8_t)(permSeed>>16);payload[3]=(uint8_t)(permSeed>>24);
    payload[4]=(uint8_t)whitenSeed;payload[5]=(uint8_t)(whitenSeed>>8);payload[6]=(uint8_t)(whitenSeed>>16);payload[7]=(uint8_t)(whitenSeed>>24);
    memcpy(payload+8,body,plLen);

    uint8_t master[32]; master_key(master);
    uint8_t encKey[32]; derive(master,salt,16,"ORI-ENC-v2",encKey);
    chacha20_crypt(encKey,nonce,1,payload,payLen);

    // assemble header+cipher, then HMAC
    size_t headLen=38; size_t bodyLen=headLen+payLen;
    uint8_t* out=xmalloc(bodyLen+32);
    memcpy(out,"ORIX",4); out[4]=2; out[5]=0;
    memcpy(out+6,salt,16); memcpy(out+22,nonce,12);
    out[34]=(uint8_t)payLen;out[35]=(uint8_t)(payLen>>8);out[36]=(uint8_t)(payLen>>16);out[37]=(uint8_t)(payLen>>24);
    memcpy(out+38,payload,payLen);
    uint8_t macKey[32]; derive(master,salt,16,"ORI-MAC-v2",macKey);
    hmac_sha256(macKey,32,out,bodyLen,out+bodyLen);

    FILE* f=fopen(outPath,"wb"); if(!f){ fprintf(stderr,"pack: cannot write %s\n",outPath); return 2; }
    fwrite(out,1,bodyLen+32,f); fclose(f);
    printf("packed %s -> %s (%zu bytes, encrypted, per-build opcode map)\n", inPath, outPath, bodyLen+32);
    return 0;
}

// ---------------------------------------------------------------------------
//  VM
// ---------------------------------------------------------------------------
typedef struct VM VM;
typedef Value (*HostFn)(VM* vm, Value* args, int argc);

typedef struct { Func* fn; int ip; Value* locals; int localsCount; } Frame;
typedef struct { char* name; Value v; } GVar;

struct VM {
    Program* prog;
    Value* stack; int sp, stackCap;
    Frame* frames; int fp, frameCap;
    GVar* globals; int gcount, gcap;
    const char** hostNames; HostFn* hostFns; int hostCount;
    int pargc; char** pargv;   // program arguments
};

static VM gvm;   // the persistent VM instance (kept alive for web event callbacks)

static void g_set(VM* vm, const char* name, Value v){
    for(int i=0;i<vm->gcount;i++) if(strcmp(vm->globals[i].name,name)==0){ vm->globals[i].v=v; return; }
    if(vm->gcount>=vm->gcap){ vm->gcap=vm->gcap?vm->gcap*2:32; vm->globals=xrealloc(vm->globals,sizeof(GVar)*vm->gcap); }
    vm->globals[vm->gcount].name=dupstr(name); vm->globals[vm->gcount].v=v; vm->gcount++;
}
static int g_get(VM* vm, const char* name, Value* out){
    for(int i=0;i<vm->gcount;i++) if(strcmp(vm->globals[i].name,name)==0){ *out=vm->globals[i].v; return 1; }
    return 0;
}
static void push(VM* vm, Value v){
    if(vm->sp>=0x100000) rt_error("value stack overflow (>1M entries; 16MB limit)");
    if(vm->sp>=vm->stackCap){ vm->stackCap*=2; vm->stack=xrealloc(vm->stack,sizeof(Value)*vm->stackCap); }
    vm->stack[vm->sp++]=v;
}
static Value pop(VM* vm){ if(vm->sp<=0) rt_error("stack underflow"); return vm->stack[--vm->sp]; }

static void rt_error(const char* msg){ fprintf(stderr,"[Ori runtime error] %s\n",msg); exit(4); }

static int safe_int(double d){
    if(d>=2147483647.0) return 2147483647;
    if(d<=-2147483648.0) return -2147483648;
    return (int)d;
}
static int check_index(Value idx, int count){
    if(idx.t!=V_NUM) rt_error("index must be a number");
    int i=safe_int(idx.u.num);
    if(i<0||i>=count){ char m[64]; snprintf(m,sizeof m,"index %d out of range (0..%d)",i,count-1); rt_error(m); }
    return i;
}

static void push_frame(VM* vm, int fnIndex, Value* args, int argc){
    if(fnIndex<0||fnIndex>=vm->prog->funcCount) rt_error("invalid function index");
    Func* fn=&vm->prog->funcs[fnIndex];
    int n = fn->localCount>argc?fn->localCount:argc;
    Value* locals=xmalloc(sizeof(Value)*(n>0?n:1));
    for(int i=0;i<n;i++) locals[i]= i<argc?args[i]:vnil();
    if(vm->fp>=vm->frameCap){ vm->frameCap*=2; vm->frames=xrealloc(vm->frames,sizeof(Frame)*vm->frameCap); }
    vm->frames[vm->fp].fn=fn; vm->frames[vm->fp].ip=0; vm->frames[vm->fp].locals=locals; vm->frames[vm->fp].localsCount=n; vm->fp++;
}

static void do_call(VM* vm, int argc){
    if(vm->fp >= 2000) rt_error("stack overflow (call depth exceeded 2000)");
    if(argc<0||argc>65535) rt_error("bad call: argc out of range");
    Value* args=xmalloc(sizeof(Value)*(argc>0?argc:1));
    for(int i=argc-1;i>=0;i--) args[i]=pop(vm);
    Value callee=pop(vm);
    if(callee.t==V_FUNC){
        Func* fn=&vm->prog->funcs[callee.u.i];
        if(argc!=fn->arity){ char m[96]; snprintf(m,sizeof m,"%s() expects %d arg(s) but got %d",fn->name,fn->arity,argc); rt_error(m); }
        push_frame(vm,callee.u.i,args,argc);
    } else if(callee.t==V_HOST){
        if(callee.u.i<0||callee.u.i>=vm->hostCount) rt_error("invalid host function index");
        push(vm, vm->hostFns[callee.u.i](vm,args,argc));
    } else rt_error("value is not callable");
    free(args);
}

static Value bin_add(Value a, Value b){
    if(a.t==V_STR||b.t==V_STR){ Sb s; sb_init(&s); val_display(&s,a); val_display(&s,b); return vstr_n(s.d,s.len); }
    if(a.t==V_NUM&&b.t==V_NUM) return vnum(a.u.num+b.u.num);
    rt_error("cannot add these types"); return vnil();
}
static double need_num(Value v){ if(v.t!=V_NUM) rt_error("arithmetic requires numbers"); return v.u.num; }

static Value run(VM* vm){
    while(vm->fp>0){
        Frame* fr=&vm->frames[vm->fp-1];
        Func* fn=fr->fn;
        for(;;){
            if(fr->ip>=fn->codeCount){ free(fr->locals); vm->fp--; break; }
            Instr ins=fn->code[fr->ip++];
            switch(ins.op){
                case OP_HALT: return vm->sp>0?pop(vm):vnil();
                case OP_PUSHCONST:
                    if(ins.arg<0||ins.arg>=vm->prog->constCount) rt_error("const index out of range");
                    push(vm, vm->prog->consts[ins.arg]); break;
                case OP_PUSHINT: push(vm, vnum((double)ins.arg)); break;
                case OP_PUSHNIL: push(vm, vnil()); break;
                case OP_PUSHTRUE: push(vm, vbool(1)); break;
                case OP_PUSHFALSE: push(vm, vbool(0)); break;
                case OP_POP: if(vm->sp<=0) rt_error("stack underflow"); vm->sp--; break;
                case OP_LOADGLOBAL: {
                    if(ins.arg<0||ins.arg>=vm->prog->constCount||vm->prog->consts[ins.arg].t!=V_STR) rt_error("bad LOADGLOBAL operand");
                    const char* nm=vm->prog->consts[ins.arg].u.s->d; Value v;
                    if(!g_get(vm,nm,&v)){ char m[128]; snprintf(m,sizeof m,"undefined variable '%s'",nm); rt_error(m); }
                    push(vm,v); break;
                }
                case OP_STOREGLOBAL:
                    if(ins.arg<0||ins.arg>=vm->prog->constCount||vm->prog->consts[ins.arg].t!=V_STR) rt_error("bad STOREGLOBAL operand");
                    g_set(vm, vm->prog->consts[ins.arg].u.s->d, pop(vm)); break;
                case OP_LOADLOCAL:
                    if(ins.arg<0||ins.arg>=fr->localsCount) rt_error("local slot out of range");
                    push(vm, fr->locals[ins.arg]); break;
                case OP_STORELOCAL:
                    if(ins.arg<0||ins.arg>=fr->localsCount) rt_error("local slot out of range");
                    fr->locals[ins.arg]=pop(vm); break;
                case OP_ADD: { Value b=pop(vm),a=pop(vm); push(vm,bin_add(a,b)); break; }
                case OP_SUB: { double b=need_num(pop(vm)),a=need_num(pop(vm)); push(vm,vnum(a-b)); break; }
                case OP_MUL: { double b=need_num(pop(vm)),a=need_num(pop(vm)); push(vm,vnum(a*b)); break; }
                case OP_DIV: { double b=need_num(pop(vm)),a=need_num(pop(vm)); if(b==0 && a!=0) rt_error("division by zero"); push(vm,vnum(a/b)); break; }
                case OP_MOD: { double b=need_num(pop(vm)),a=need_num(pop(vm)); push(vm,vnum(fmod(a,b))); break; }
                case OP_NEG: { double a=need_num(pop(vm)); push(vm,vnum(-a)); break; }
                case OP_EQ: { Value b=pop(vm),a=pop(vm); push(vm,vbool(val_eq(a,b))); break; }
                case OP_NEQ:{ Value b=pop(vm),a=pop(vm); push(vm,vbool(!val_eq(a,b))); break; }
                case OP_LT: { double b=need_num(pop(vm)),a=need_num(pop(vm)); push(vm,vbool(a<b)); break; }
                case OP_GT: { double b=need_num(pop(vm)),a=need_num(pop(vm)); push(vm,vbool(a>b)); break; }
                case OP_LE: { double b=need_num(pop(vm)),a=need_num(pop(vm)); push(vm,vbool(a<=b)); break; }
                case OP_GE: { double b=need_num(pop(vm)),a=need_num(pop(vm)); push(vm,vbool(a>=b)); break; }
                case OP_NOT:{ Value a=pop(vm); push(vm,vbool(!is_truthy(a))); break; }
                case OP_JMP:
                    if(ins.arg<0||ins.arg>fn->codeCount) rt_error("jump target out of range");
                    fr->ip=ins.arg; break;
                case OP_JMPIFFALSE: { Value v=pop(vm); if(!is_truthy(v)){ if(ins.arg<0||ins.arg>fn->codeCount) rt_error("jump target out of range"); fr->ip=ins.arg; } break; }
                case OP_JMPIFTRUE:  { Value v=pop(vm); if(is_truthy(v)) { if(ins.arg<0||ins.arg>fn->codeCount) rt_error("jump target out of range"); fr->ip=ins.arg; } break; }
                case OP_CALL: do_call(vm,ins.arg); goto refetch;
                case OP_RET: {
                    Value res=vm->sp>0?pop(vm):vnil();
                    vm->fp--;
                    free(vm->frames[vm->fp].locals);
                    if(vm->fp==0) return res;
                    push(vm,res);
                    goto refetch;
                }
                case OP_MAKEARRAY: {
                    int k=ins.arg;
                    if(k<0||k>65535) rt_error("MAKEARRAY arg out of range");
                    Value arr=varr_new();
                    Value* tmp=xmalloc(sizeof(Value)*(k>0?k:1));
                    for(int i=k-1;i>=0;i--) tmp[i]=pop(vm);
                    for(int i=0;i<k;i++) arr_push(arr.u.a,tmp[i]);
                    free(tmp); push(vm,arr); break;
                }
                case OP_INDEX: {
                    Value idx=pop(vm), tgt=pop(vm);
                    if(tgt.t==V_ARR){ push(vm, tgt.u.a->it[check_index(idx,tgt.u.a->len)]); }
                    else if(tgt.t==V_STR){ int i=check_index(idx,tgt.u.s->len); push(vm, vstr_n(tgt.u.s->d+i,1)); }
                    else rt_error("cannot index into this value");
                    break;
                }
                case OP_STOREINDEX: {
                    Value val=pop(vm), idx=pop(vm), tgt=pop(vm);
                    if(tgt.t!=V_ARR) rt_error("cannot index-assign into non-array");
                    tgt.u.a->it[check_index(idx,tgt.u.a->len)]=val;
                    push(vm,val); break;
                }
                default: rt_error("bad opcode");
            }
            continue;
            refetch:
            fr=&vm->frames[vm->fp-1]; fn=fr->fn;
        }
    }
    return vnil();
}

// ---------------------------------------------------------------------------
//  Host built-ins
// ---------------------------------------------------------------------------
static double argnum(Value* a, int argc, int i){ if(i>=argc||a[i].t!=V_NUM) rt_error("expected a number"); return a[i].u.num; }
static Str* argstr(Value* a, int argc, int i){ if(i>=argc||a[i].t!=V_STR) rt_error("expected a string"); return a[i].u.s; }

void (*ori_say_hook)(const char* line) = NULL;   // set by embedders (e.g. Android JNI)
static Value h_say(VM* vm, Value* a, int argc){
    Sb b; sb_init(&b);
    for(int i=0;i<argc;i++){ if(i) sb_putc(&b,' '); val_display(&b,a[i]); }
    if(ori_say_hook){ ori_say_hook(b.d); }
    else { fwrite(b.d,1,b.len,stdout); fputc('\n',stdout); }
    free(b.d); return vnil();
}
static Value h_str(VM* vm, Value* a, int argc){ if(argc==0) return vstr(""); char* s=val_cstr(a[0]); Value v=vstr(s); free(s); return v; }
static Value h_num(VM* vm, Value* a, int argc){
    if(argc==0) return vnum(0);
    if(a[0].t==V_NUM) return a[0];
    if(a[0].t==V_STR){ char* e; double d=strtod(a[0].u.s->d,&e); if(e!=a[0].u.s->d) return vnum(d); }
    return vnil();
}
static Value h_len(VM* vm, Value* a, int argc){
    if(argc==0) rt_error("len() expects an argument");
    if(a[0].t==V_STR) return vnum(a[0].u.s->len);
    if(a[0].t==V_ARR) return vnum(a[0].u.a->len);
    rt_error("len() expects a string or array"); return vnil();
}
static Value h_push(VM* vm, Value* a, int argc){ if(argc<2||a[0].t!=V_ARR) rt_error("push(array,value)"); arr_push(a[0].u.a,a[1]); return vnum(a[0].u.a->len); }
static Value h_pop(VM* vm, Value* a, int argc){ if(argc<1||a[0].t!=V_ARR||a[0].u.a->len==0) rt_error("pop(array)"); return a[0].u.a->it[--a[0].u.a->len]; }
static Value h_char_at(VM* vm, Value* a, int argc){ Str* s=argstr(a,argc,0); int i=safe_int(argnum(a,argc,1)); if(i<0||i>=s->len) return vstr(""); return vstr_n(s->d+i,1); }
static Value h_ord(VM* vm, Value* a, int argc){ Str* s=argstr(a,argc,0); return vnum(s->len==0?-1:(unsigned char)s->d[0]); }
static Value h_chr(VM* vm, Value* a, int argc){ char c=(char)(safe_int(argnum(a,argc,0))&0xFF); return vstr_n(&c,1); }
static Value h_substr(VM* vm, Value* a, int argc){
    Str* s=argstr(a,argc,0); int start=safe_int(argnum(a,argc,1));
    int count=argc>2?safe_int(argnum(a,argc,2)):s->len-start;
    if(start<0)start=0; if(start>s->len)start=s->len; if(count<0)count=0; if(count>s->len-start)count=s->len-start;
    return vstr_n(s->d+start,count);
}
static Value h_type(VM* vm, Value* a, int argc){
    if(argc==0) return vstr("nil");
    switch(a[0].t){ case V_NIL:return vstr("nil"); case V_NUM:return vstr("number"); case V_BOOL:return vstr("bool");
        case V_STR:return vstr("string"); case V_ARR:return vstr("array"); default:return vstr("function"); }
}
static Value h_abs(VM* vm, Value* a, int argc){ return vnum(fabs(argnum(a,argc,0))); }
static Value h_floor(VM* vm, Value* a, int argc){ return vnum(floor(argnum(a,argc,0))); }
static Value h_sqrt(VM* vm, Value* a, int argc){ return vnum(sqrt(argnum(a,argc,0))); }
static Value h_max(VM* vm, Value* a, int argc){ if(argc==0)return vnil(); double m=argnum(a,argc,0); for(int i=1;i<argc;i++){double x=argnum(a,argc,i); if(x>m)m=x;} return vnum(m); }
static Value h_min(VM* vm, Value* a, int argc){ if(argc==0)return vnil(); double m=argnum(a,argc,0); for(int i=1;i<argc;i++){double x=argnum(a,argc,i); if(x<m)m=x;} return vnum(m); }
static Value h_upper(VM* vm, Value* a, int argc){ Str* s=argstr(a,argc,0); Value v=vstr_n(s->d,s->len); for(int i=0;i<v.u.s->len;i++){char c=v.u.s->d[i]; if(c>='a'&&c<='z')v.u.s->d[i]=c-32;} return v; }
static Value h_lower(VM* vm, Value* a, int argc){ Str* s=argstr(a,argc,0); Value v=vstr_n(s->d,s->len); for(int i=0;i<v.u.s->len;i++){char c=v.u.s->d[i]; if(c>='A'&&c<='Z')v.u.s->d[i]=c+32;} return v; }

// ---- tooling builtins: file IO + program args (so Ori can be a compiler) ----
static Value h_read_file(VM* vm, Value* a, int argc){
    Str* path=argstr(a,argc,0);
    FILE* f=fopen(path->d,"rb"); if(!f){ rt_error("read_file: cannot open file"); }
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if(n<0||n>64*1024*1024){ fclose(f); rt_error("read_file: file unseekable or too large"); }
    char* buf=xmalloc((size_t)n+1); size_t got=fread(buf,1,(size_t)n,f); (void)got; buf[n]=0; fclose(f);
    Value v=vstr_n(buf,(int)n); free(buf); return v;
}
static Value h_write_bytes(VM* vm, Value* a, int argc){
    Str* path=argstr(a,argc,0);
    if(argc<2||a[1].t!=V_ARR) rt_error("write_bytes(path, array)");
    Arr* arr=a[1].u.a;
    FILE* f=fopen(path->d,"wb"); if(!f) rt_error("write_bytes: cannot open file");
    for(int i=0;i<arr->len;i++){ unsigned char c=(unsigned char)(int)arr->it[i].u.num; fputc(c,f); }
    fclose(f); return vnum(arr->len);
}
static Value h_write_file(VM* vm, Value* a, int argc){
    Str* path=argstr(a,argc,0); Str* s=argstr(a,argc,1);
    FILE* f=fopen(path->d,"wb"); if(!f) rt_error("write_file: cannot open file");
    fwrite(s->d,1,s->len,f); fclose(f); return vnum(s->len);
}
static Value h_argc(VM* vm, Value* a, int argc){ return vnum(vm->pargc); }
static Value h_argv(VM* vm, Value* a, int argc){ int i=(int)argnum(a,argc,0); if(i<0||i>=vm->pargc) return vstr(""); return vstr(vm->pargv[i]); }

// ---- OS / build host functions (so the toolchain can be written in Ori) ----
static Value h_env(VM* vm, Value* a, int argc){ if(argc<1||a[0].t!=V_STR) return vstr(""); char* e=getenv(a[0].u.s->d); return vstr(e?e:""); }
static Value h_exists(VM* vm, Value* a, int argc){ if(argc<1||a[0].t!=V_STR) return vnum(0); struct stat st; return vnum(stat(a[0].u.s->d,&st)==0?1:0); }
static Value h_sh(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vnum(-1);
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    return vnum(-1);
#else
    fflush(stdout); return vnum((double)system(a[0].u.s->d));
#endif
}
static Value h_run(VM* vm, Value* a, int argc){
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    return vnum(-1);
#else
    if(argc<2||a[0].t!=V_STR||a[1].t!=V_ARR) return vnum(-1);
    Arr* ar=a[1].u.a;
    if(ar->len<0||ar->len>65535) rt_error("run: too many arguments (>65535)");
    char** av=xmalloc(sizeof(char*)*(ar->len+2));
    av[0]=a[0].u.s->d;
    for(int i=0;i<ar->len;i++) av[i+1]=(ar->it[i].t==V_STR)?ar->it[i].u.s->d:"";
    av[ar->len+1]=NULL;
    fflush(stdout);
#ifdef _WIN32
    intptr_t rc=_spawnvp(_P_WAIT, a[0].u.s->d, av);
    free(av);
    return vnum((double)rc);
#else
    pid_t pid=fork();
    if(pid==0){ execvp(a[0].u.s->d,av); _exit(127); }
    free(av);
    if(pid<0) return vnum(127);
    int st=0; while(waitpid(pid,&st,0)<0){ if(errno!=EINTR) return vnum(127); }
    if(WIFEXITED(st)) return vnum(WEXITSTATUS(st));
    if(WIFSIGNALED(st)) return vnum(128+WTERMSIG(st));
    return vnum(st);
#endif
#endif
}
static Value h_mkdirs(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vnum(0);
    char tmp[1024]; strncpy(tmp,a[0].u.s->d,sizeof tmp-1); tmp[sizeof tmp-1]=0;
    for(char* p=tmp+1; *p; p++){
        if(*p=='/'||*p=='\\'){ char c=*p; *p=0;
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp,0755);
#endif
            *p=c;
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp,0755);
#endif
    return vnum(1);
}
static Value h_copy(VM* vm, Value* a, int argc){
    if(argc<2||a[0].t!=V_STR||a[1].t!=V_STR) return vnum(0);
    FILE* s=fopen(a[0].u.s->d,"rb"); if(!s) return vnum(0);
    FILE* d=fopen(a[1].u.s->d,"wb"); if(!d){ fclose(s); return vnum(0); }
    char buf[8192]; size_t r;
    while((r=fread(buf,1,sizeof buf,s))>0) fwrite(buf,1,r,d);
    fclose(s); fclose(d); return vnum(1);
}
static Value h_glob(VM* vm, Value* a, int argc){
    Value arr=varr_new();
    if(argc<1||a[0].t!=V_STR) return arr;
#ifdef _WIN32
    const char* pat=a[0].u.s->d;
    char dir[1024]=""; strncpy(dir,pat,sizeof dir-1);
    char* sl1=strrchr(dir,'\\'), *sl2=strrchr(dir,'/');
    char* sl=sl1>sl2?sl1:sl2;
    if(sl) sl[1]=0; else dir[0]=0;
    struct _finddata_t fd; intptr_t h=_findfirst(pat,&fd);
    if(h!=-1){ do { if(strcmp(fd.name,".")&&strcmp(fd.name,"..")) {
        char full[1024]; snprintf(full,sizeof full,"%s%s",dir,fd.name);
        arr_push(arr.u.a, vstr(full));
    } } while(_findnext(h,&fd)==0); _findclose(h); }
#elif defined(__ANDROID__)
    // Android Bionic < API 28 has no glob(3) — use opendir + fnmatch instead
    const char* pat=a[0].u.s->d;
    char dir[1024]=".";
    const char* name_pat;
    const char* slash=strrchr(pat,'/');
    if(slash){ int dl=(int)(slash-pat)+1; if(dl>1022)dl=1022; memcpy(dir,pat,dl); dir[dl]=0; name_pat=slash+1; }
    else { name_pat=pat; }
    DIR* dp=opendir(dir);
    if(dp){ struct dirent* de;
        while((de=readdir(dp))!=NULL){
            if(strcmp(de->d_name,".")==0||strcmp(de->d_name,"..")==0) continue;
            if(fnmatch(name_pat,de->d_name,0)==0){
                char full[1024];
                if(strcmp(dir,".")!=0) snprintf(full,sizeof full,"%s/%s",dir,de->d_name);
                else strncpy(full,de->d_name,sizeof full-1);
                arr_push(arr.u.a,vstr(full));
            }
        }
        closedir(dp);
    }
#else
    glob_t g;
    if(glob(a[0].u.s->d,0,NULL,&g)==0){
        for(size_t i=0;i<g.gl_pathc;i++) arr_push(arr.u.a,vstr(g.gl_pathv[i]));
        globfree(&g);
    }
#endif
    return arr;
}
static Value h_abspath(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vstr("");
#ifdef _WIN32
    char out[1024]; if(_fullpath(out,a[0].u.s->d,sizeof out)) return vstr(out);
#else
    char out[PATH_MAX]; if(realpath(a[0].u.s->d,out)) return vstr(out);
#endif
    return a[0];
}
// Reject URLs containing shell metacharacters to prevent injection via popen.
// On Windows, cmd.exe expands %VAR% inside double-quoted arguments; if the env-var
// value contains '"' it can break out of the curl command.  Allow %XX (valid URL
// percent-encoding) but reject bare '%' not followed by exactly two hex digits.
static int url_shell_safe(const char* url){
    for(const char* p=url;*p;p++){
        char c=*p;
        if(c=='\''||c=='"'||c=='`'||c=='$'||c=='\\'||c=='\n'||c=='\r'||
           c==';'||c=='|'||c=='('||c==')'||c=='{'||c=='}'||c=='<'||c=='>')
            return 0;
#ifdef _WIN32
        if(c=='%'){
            char h1=p[1], h2=p[2];
            int ok1=(h1>='0'&&h1<='9')||(h1>='A'&&h1<='F')||(h1>='a'&&h1<='f');
            int ok2=(h2>='0'&&h2<='9')||(h2>='A'&&h2<='F')||(h2>='a'&&h2<='f');
            if(!ok1||!ok2) return 0;
        }
#endif
    }
    return 1;
}
static Value h_http_get(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vstr("");
    const char* url=a[0].u.s->d;
    if(!url_shell_safe(url)){ fprintf(stderr,"[Ori] http_get: URL contains unsafe characters\n"); return vstr(""); }
    if(strlen(url)>1800){ fprintf(stderr,"[Ori] http_get: URL too long\n"); return vstr(""); }
    char cmd[2048];
#ifdef _WIN32
    snprintf(cmd,sizeof cmd,"curl -sL --max-time 8 --connect-timeout 5 \"%s\" 2>nul",url);
    FILE* p=_popen(cmd,"r");
#else
    snprintf(cmd,sizeof cmd,"curl -sL --max-time 8 --connect-timeout 5 '%s' 2>/dev/null",url);
    FILE* p=popen(cmd,"r");
#endif
    if(!p) return vstr("");
    char buf[65536]; size_t total=0;
    size_t r;
    while((r=fread(buf+total,1,sizeof buf-total-1,p))>0){ total+=r; if(total>=sizeof buf-1) break; }
    buf[total]=0;
#ifdef _WIN32
    _pclose(p);
#else
    pclose(p);
#endif
    return vstr_n(buf,(int)total);
}
static Value h_is_dir(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vnum(0);
    struct stat st; if(stat(a[0].u.s->d,&st)!=0) return vnum(0);
    return vnum((st.st_mode & S_IFDIR)?1:0);
}
static Value h_mtime(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vnum(0);
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA d;
    if(!GetFileAttributesExA(a[0].u.s->d,GetFileExInfoStandard,&d)) return vnum(0);
    long long t=((long long)d.ftLastWriteTime.dwHighDateTime<<32)|d.ftLastWriteTime.dwLowDateTime;
    return vnum((double)t);
#else
    struct stat st; if(stat(a[0].u.s->d,&st)!=0) return vnum(0);
    return vnum((double)st.st_mtime);
#endif
}
static Value h_sleep_ms(VM* vm, Value* a, int argc){
    int ms=argc>0&&a[0].t==V_NUM?safe_int(a[0].u.num):0;
#ifdef _WIN32
    Sleep((DWORD)(ms<0?0:ms));
#else
    if(ms>0){ struct timespec ts; ts.tv_sec=ms/1000; ts.tv_nsec=(long)(ms%1000)*1000000L; nanosleep(&ts,NULL); }
#endif
    return vnil();
}
static Value h_read_bytes_b64(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vstr("");
    FILE* f=fopen(a[0].u.s->d,"rb"); if(!f) return vstr("");
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if(n<0||n>64*1024*1024){ fclose(f); return vstr(""); }
    if(n==0){ fclose(f); return vstr(""); }
    unsigned char* data=(unsigned char*)xmalloc((size_t)n);
    size_t got=fread(data,1,(size_t)n,f); fclose(f); (void)got;
    static const char T[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t outLen=((n+2)/3)*4;
    char* out=(char*)xmalloc(outLen+1);
    size_t i=0,j=0;
    while(i<(size_t)n){
        unsigned aa=i<(size_t)n?data[i++]:0;
        unsigned bb=i<(size_t)n?data[i++]:0;
        unsigned cc=i<(size_t)n?data[i++]:0;
        unsigned triple=(aa<<16)|(bb<<8)|cc;
        out[j++]=T[(triple>>18)&63]; out[j++]=T[(triple>>12)&63];
        out[j++]=T[(triple>>6)&63]; out[j++]=T[triple&63];
    }
    if(n%3==1){out[outLen-2]='=';out[outLen-1]='=';}
    else if(n%3==2){out[outLen-1]='=';}
    out[outLen]=0;
    free(data);
    Value v=vstr_n(out,(int)outLen); free(out); return v;
}

static void register_hosts(VM* vm){
    static const char* names[] = {
        "say","print","str","num","len","push","pop","char_at","ord","chr","substr","type",
        "abs","floor","sqrt","max","min","upper","lower",
        "read_file","write_bytes","write_file","argc","argv",
        "env","exists","sh","run","mkdirs","copy","glob","abspath",
        "is_dir","mtime","sleep_ms","read_bytes_b64","http_get" };
    static HostFn fns[] = {
        h_say,h_say,h_str,h_num,h_len,h_push,h_pop,h_char_at,h_ord,h_chr,h_substr,h_type,
        h_abs,h_floor,h_sqrt,h_max,h_min,h_upper,h_lower,
        h_read_file,h_write_bytes,h_write_file,h_argc,h_argv,
        h_env,h_exists,h_sh,h_run,h_mkdirs,h_copy,h_glob,h_abspath,
        h_is_dir,h_mtime,h_sleep_ms,h_read_bytes_b64,h_http_get };
    int n=(int)(sizeof(names)/sizeof(names[0]));
    vm->hostNames=names; vm->hostFns=fns; vm->hostCount=n;
    for(int i=0;i<n;i++) g_set(vm,names[i],vhost(i));
}

// ---------------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------------
static uint8_t* read_all(const char* path, size_t* outN){
    FILE* f=fopen(path,"rb"); if(!f){ fprintf(stderr,"cannot open %s\n",path); exit(2); }
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if(n<0){ fprintf(stderr,"cannot seek %s\n",path); exit(2); }
    uint8_t* b=xmalloc(n>0?(size_t)n:1); size_t got=fread(b,1,(size_t)n,f); (void)got; fclose(f); *outN=(size_t)n; return b;
}

static const char* OPNAME[]={
"HALT","PUSHCONST","PUSHNIL","PUSHTRUE","PUSHFALSE","POP","LOADGLOBAL","STOREGLOBAL",
"LOADLOCAL","STORELOCAL","ADD","SUB","MUL","DIV","MOD","NEG","EQ","NEQ","LT","GT","LE","GE",
"NOT","JMP","JMPIFFALSE","JMPIFTRUE","CALL","RET","MAKEARRAY","INDEX","STOREINDEX","PUSHINT"};

static void disassemble(Program* pr){
    printf("consts=%d  funcs=%d  main=%d\n", pr->constCount, pr->funcCount, pr->mainIndex);
    for(int f=0; f<pr->funcCount; f++){
        Func* fn=&pr->funcs[f];
        printf("fn#%d %s/%d (locals=%d, code=%d)\n", f, fn->name, fn->arity, fn->localCount, fn->codeCount);
        for(int i=0;i<fn->codeCount;i++){
            int op=fn->code[i].op;
            const char* nm = (op>=0&&op<=31)?OPNAME[op]:"?";
            printf("  %4d: %-12s %d\n", i, nm, fn->code[i].arg);
        }
    }
}

static Program* ori_load(const uint8_t* img, size_t n){
    if(n>=4 && memcmp(img,"ORIX",4)==0) return load_orx(img,n);
    if(n>=4 && memcmp(img,"ORB1",4)==0) return load_orb(img,n);
    die("unknown image format (expected .orx or .orb)"); return NULL;
}
static int ori_setup_run(Program* prog, int pargc, char** pargv){
    VM* vm=&gvm; memset(vm,0,sizeof *vm);
    vm->prog=prog;
    vm->stackCap=256; vm->stack=xmalloc(sizeof(Value)*vm->stackCap); vm->sp=0;
    vm->frameCap=64; vm->frames=xmalloc(sizeof(Frame)*vm->frameCap); vm->fp=0;
    vm->pargc=pargc; vm->pargv=pargv;
    register_hosts(vm);
    for(int i=0;i<prog->funcCount;i++){ if(strcmp(prog->funcs[i].name,"__main__")!=0) g_set(vm,prog->funcs[i].name,vfunc(i)); }
    push_frame(vm,prog->mainIndex,NULL,0);
    run(vm);
    return 0;
}
// Load an image into the persistent VM (gvm) and run its __main__ once.
// After this, ori_call_str can drive the program from GUI / JNI / JS events.
int ori_boot(const char* path, int pargc, char** pargv){
    build_perm();
    size_t n; uint8_t* img=read_all(path,&n);
    return ori_setup_run(ori_load(img,n), pargc, pargv);
}
// Same, but from an in-memory image (for Android JNI / embedding).
int ori_boot_mem(const uint8_t* img, size_t n){
    build_perm();
    return ori_setup_run(ori_load(img,n), 0, NULL);
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define ORI_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define ORI_EXPORT
#endif

// Call an Ori global function (taking one string arg); return its string result.
// Drives a persistent Ori "model" from GUI (Win32 / Android JNI) or web (JS) events.
ORI_EXPORT
char* ori_call_str(const char* fname, const char* arg){
    Value f;
    if(!g_get(&gvm, fname, &f) || f.t!=V_FUNC) return dupstr("");
    if(gvm.fp >= 2000) return dupstr("");
    Value a = vstr(arg ? arg : "");
    push_frame(&gvm, f.u.i, &a, 1);
    Value r = run(&gvm);
    if(r.t==V_STR) return dupstr(r.u.s->d);
    return val_cstr(r);
}

// Call an Ori function with two string args (e.g. dispatch(event, arg)).
ORI_EXPORT
char* ori_call2(const char* fname, const char* a1, const char* a2){
    Value f;
    if(!g_get(&gvm, fname, &f) || f.t!=V_FUNC) return dupstr("");
    if(gvm.fp >= 2000) return dupstr("");
    Value args[2]; args[0]=vstr(a1?a1:""); args[1]=vstr(a2?a2:"");
    push_frame(&gvm, f.u.i, args, 2);
    Value r = run(&gvm);
    if(r.t==V_STR) return dupstr(r.u.s->d);
    return val_cstr(r);
}

#ifndef ORI_AS_LIB
int main(int argc, char** argv){
    if(argc<2){ fprintf(stderr,"usage: orivm <file.orx|file.orb> [args...]\n       orivm dis <file>\n"); return 1; }
    build_perm();
    if(strcmp(argv[1],"pack")==0){
        if(argc<4){ fprintf(stderr,"usage: orivm pack <in.orb> <out.orx>\n"); return 1; }
        return do_pack(argv[2],argv[3]);
    }
    if(strcmp(argv[1],"dis")==0){
        size_t n; uint8_t* img=read_all(argv[2],&n);
        Program* prog;
        if(n>=4 && memcmp(img,"ORIX",4)==0) prog=load_orx(img,n);
        else if(n>=4 && memcmp(img,"ORB1",4)==0) prog=load_orb(img,n);
        else die("unknown image format");
        disassemble(prog); return 0;
    }
    return ori_boot(argv[1], argc-2, argv+2);
}
#endif
