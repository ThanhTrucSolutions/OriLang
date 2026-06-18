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
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#include <process.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <dirent.h>
#include <fnmatch.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#if defined(__linux__) && !defined(__ANDROID__)
#  include <sys/random.h>
#elif defined(__ANDROID__)
#  include <fcntl.h>
#endif
#ifndef __ANDROID__
#include <glob.h>
#endif
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#endif
#include "sha256.h"
#include "chacha20.h"

// ---------------------------------------------------------------------------
//  Portable TCP socket helpers (used by http_serve)
// ---------------------------------------------------------------------------
#ifdef _WIN32
typedef SOCKET ori_sock_t;
#define ORI_SOCK_INVALID INVALID_SOCKET
#define ori_sock_close(s) closesocket(s)
static void ori_net_init(void){ static int done=0; if(!done){ WSADATA w; WSAStartup(MAKEWORD(2,2),&w); done=1; } }
#else
typedef int ori_sock_t;
#define ORI_SOCK_INVALID (-1)
#define ori_sock_close(s) close(s)
static void ori_net_init(void){}
#endif

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
    uint32_t s=0;
#if defined(_WIN32)
    BCryptGenRandom(NULL,(PUCHAR)&s,sizeof(s),BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#elif defined(__linux__) && !defined(__ANDROID__)
    { int _gr=getrandom(&s,sizeof(s),0); (void)_gr; }
#elif defined(__ANDROID__) || defined(__APPLE__)
    { int _fd=open("/dev/urandom",O_RDONLY); if(_fd>=0){ read(_fd,&s,sizeof(s)); close(_fd); } }
#endif
    if(s==0){
        s=(uint32_t)time(NULL);
        s ^= (uint32_t)clock()*2654435761u;
        s ^= (uint32_t)(uintptr_t)&s;
        s ^= s<<13; s^=s>>17; s^=s<<5;
        if(s==0) s=0x9E3779B9u;
    }
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
    if(vm->gcount>=65535) rt_error("global variable limit exceeded (>65535)");
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

static void check_finite(double r){ if(isnan(r)||isinf(r)) rt_error("arithmetic result is NaN or Inf"); }
static Value bin_add(Value a, Value b){
    if(a.t==V_STR||b.t==V_STR){ Sb s; sb_init(&s); val_display(&s,a); val_display(&s,b); return vstr_n(s.d,s.len); }
    if(a.t==V_NUM&&b.t==V_NUM){ double r=a.u.num+b.u.num; check_finite(r); return vnum(r); }
    rt_error("cannot add these types"); return vnil();
}
static double need_num(Value v){ if(v.t!=V_NUM) rt_error("arithmetic requires numbers"); return v.u.num; }

// Run until vm->fp drops to min_fp (0 for normal execution, saved_fp for nested calls).
static Value run_until(VM* vm, int min_fp){
    uint64_t steps=0;
    while(vm->fp>min_fp){
        Frame* fr=&vm->frames[vm->fp-1];
        Func* fn=fr->fn;
        for(;;){
            if(fr->ip>=fn->codeCount){ free(fr->locals); vm->fp--; break; }
            if(++steps > 100000000ULL) rt_error("execution limit exceeded (100M instructions)");
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
                case OP_SUB: { double b=need_num(pop(vm)),a=need_num(pop(vm)); double rs=a-b; check_finite(rs); push(vm,vnum(rs)); break; }
                case OP_MUL: { double b=need_num(pop(vm)),a=need_num(pop(vm)); double rm=a*b; check_finite(rm); push(vm,vnum(rm)); break; }
                case OP_DIV: { double b=need_num(pop(vm)),a=need_num(pop(vm)); if(b==0) rt_error("division by zero"); push(vm,vnum(a/b)); break; }
                case OP_MOD: { double b=need_num(pop(vm)),a=need_num(pop(vm)); if(b==0) rt_error("modulo by zero"); push(vm,vnum(fmod(a,b))); break; }
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
                    if(vm->fp<=min_fp) return res;
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
static Value run(VM* vm){ return run_until(vm, 0); }

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
    if(a[0].t==V_STR){ char* e; double d=strtod(a[0].u.s->d,&e); if(e!=a[0].u.s->d && isfinite(d)) return vnum(d); }
    return vnum(0);
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
static Value h_abs(VM* vm, Value* a, int argc){ double r=fabs(argnum(a,argc,0)); check_finite(r); return vnum(r); }
static Value h_floor(VM* vm, Value* a, int argc){ double r=floor(argnum(a,argc,0)); check_finite(r); return vnum(r); }
static Value h_sqrt(VM* vm, Value* a, int argc){ double r=sqrt(argnum(a,argc,0)); check_finite(r); return vnum(r); }
static Value h_max(VM* vm, Value* a, int argc){ if(argc==0)return vnil(); double m=argnum(a,argc,0); for(int i=1;i<argc;i++){double x=argnum(a,argc,i); if(x>m)m=x;} return vnum(m); }
static Value h_min(VM* vm, Value* a, int argc){ if(argc==0)return vnil(); double m=argnum(a,argc,0); for(int i=1;i<argc;i++){double x=argnum(a,argc,i); if(x<m)m=x;} return vnum(m); }
static Value h_upper(VM* vm, Value* a, int argc){ Str* s=argstr(a,argc,0); Value v=vstr_n(s->d,s->len); for(int i=0;i<v.u.s->len;i++){char c=v.u.s->d[i]; if(c>='a'&&c<='z')v.u.s->d[i]=c-32;} return v; }
static Value h_lower(VM* vm, Value* a, int argc){ Str* s=argstr(a,argc,0); Value v=vstr_n(s->d,s->len); for(int i=0;i<v.u.s->len;i++){char c=v.u.s->d[i]; if(c>='A'&&c<='Z')v.u.s->d[i]=c+32;} return v; }

/* Returns 1 if path contains a ".." component — catches /../ ..\ leading ../ etc. */
static int has_dotdot(const char* path){
    const char* p=path;
    while(*p){
        if(p[0]=='.'&&p[1]=='.'){
            int at_start=(p==path)||(p[-1]=='/'||p[-1]=='\\');
            char after=p[2];
            int at_end=(after=='/'||after=='\\'||after=='\0');
            if(at_start&&at_end) return 1;
        }
        p++;
    }
    return 0;
}

// ---- tooling builtins: file IO + program args (so Ori can be a compiler) ----
static Value h_read_file(VM* vm, Value* a, int argc){
    Str* path=argstr(a,argc,0);
    if(has_dotdot(path->d)){ fprintf(stderr,"[Ori] read_file: path traversal rejected\n"); rt_error("read_file: path traversal rejected"); }
    FILE* f=fopen(path->d,"rb"); if(!f){ rt_error("read_file: cannot open file"); }
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if(n<0||n>64*1024*1024){ fclose(f); rt_error("read_file: file unseekable or too large"); }
    char* buf=xmalloc((size_t)n+1); size_t got=fread(buf,1,(size_t)n,f); buf[got]=0; fclose(f);
    Value v=vstr_n(buf,(int)got); free(buf); return v;
}
static Value h_write_bytes(VM* vm, Value* a, int argc){
    Str* path=argstr(a,argc,0);
    if(has_dotdot(path->d)){ fprintf(stderr,"[Ori] write_bytes: path traversal rejected\n"); rt_error("write_bytes: path traversal rejected"); }
    if(argc<2||a[1].t!=V_ARR) rt_error("write_bytes(path, array)");
    Arr* arr=a[1].u.a;
    FILE* f=fopen(path->d,"wb"); if(!f) rt_error("write_bytes: cannot open file");
    for(int i=0;i<arr->len;i++){ unsigned char c=(unsigned char)(arr->it[i].t==V_NUM?safe_int(arr->it[i].u.num)&0xFF:0); fputc(c,f); }
    fclose(f); return vnum(arr->len);
}
static Value h_write_file(VM* vm, Value* a, int argc){
    Str* path=argstr(a,argc,0); Str* s=argstr(a,argc,1);
    if(has_dotdot(path->d)){ fprintf(stderr,"[Ori] write_file: path traversal rejected\n"); rt_error("write_file: path traversal rejected"); }
    FILE* f=fopen(path->d,"wb"); if(!f) rt_error("write_file: cannot open file");
    fwrite(s->d,1,s->len,f); fclose(f); return vnum(s->len);
}
static Value h_argc(VM* vm, Value* a, int argc){ return vnum(vm->pargc); }
static Value h_argv(VM* vm, Value* a, int argc){ int i=(int)argnum(a,argc,0); if(i<0||i>=vm->pargc) return vstr(""); return vstr(vm->pargv[i]); }

// ---- OS / build host functions (so the toolchain can be written in Ori) ----
static Value h_env(VM* vm, Value* a, int argc){ if(argc<1||a[0].t!=V_STR) return vstr(""); char* e=getenv(a[0].u.s->d); return vstr(e?e:""); }
static Value h_exists(VM* vm, Value* a, int argc){ if(argc<1||a[0].t!=V_STR) return vnum(0); if(has_dotdot(a[0].u.s->d)) return vnum(0); struct stat st; return vnum(stat(a[0].u.s->d,&st)==0?1:0); }
/* DANGER: bare system() — compile with -DORI_NO_SHELL to disable in networked contexts */
static Value h_sh(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vnum(-1);
    if(a[0].u.s->len > 65535) rt_error("sh: command too long (>64KB)");
#if defined(ORI_NO_SHELL) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
    rt_error("sh: disabled in this build (ORI_NO_SHELL)");
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
    /* guard total argv size against ARG_MAX (~2MB on Linux) */
    size_t total_arg_bytes=strlen(a[0].u.s->d)+1;
    for(int i=0;i<ar->len;i++) total_arg_bytes+=(ar->it[i].t==V_STR?ar->it[i].u.s->len:0)+1;
    if(total_arg_bytes>1048576) rt_error("run: total argument size exceeds 1MB");
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
static void ori_mkdirs(const char* path){
    char tmp[1024]; strncpy(tmp,path,sizeof tmp-1); tmp[sizeof tmp-1]=0;
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
}
static Value h_mkdirs(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vnum(0);
    if(has_dotdot(a[0].u.s->d)){ fprintf(stderr,"[Ori] mkdirs: path traversal rejected\n"); return vnum(0); }
    ori_mkdirs(a[0].u.s->d);
    return vnum(1);
}
static Value h_copy(VM* vm, Value* a, int argc){
    if(argc<2||a[0].t!=V_STR||a[1].t!=V_STR) return vnum(0);
    if(has_dotdot(a[0].u.s->d)||has_dotdot(a[1].u.s->d)){ fprintf(stderr,"[Ori] copy: path traversal rejected\n"); return vnum(0); }
    FILE* s=fopen(a[0].u.s->d,"rb"); if(!s) return vnum(0);
    FILE* d=fopen(a[1].u.s->d,"wb"); if(!d){ fclose(s); return vnum(0); }
    char buf[8192]; size_t r;
    while((r=fread(buf,1,sizeof buf,s))>0) fwrite(buf,1,r,d);
    fclose(s); fclose(d); return vnum(1);
}
static Value h_glob(VM* vm, Value* a, int argc){
    Value arr=varr_new();
    if(argc<1||a[0].t!=V_STR) return arr;
    if(has_dotdot(a[0].u.s->d)){ fprintf(stderr,"[Ori] glob: path traversal rejected\n"); return arr; }
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
static Value h_http_post(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vstr("");
    const char* url=a[0].u.s->d;
    if(!url_shell_safe(url)){ fprintf(stderr,"[Ori] http_post: URL contains unsafe characters\n"); return vstr(""); }
    if(strlen(url)>1800){ fprintf(stderr,"[Ori] http_post: URL too long\n"); return vstr(""); }
    const char* body=argc>=2&&a[1].t==V_STR?a[1].u.s->d:"{}";
    /* write body to temp file to avoid shell-injection */
    char tmp[512];
    static int post_seq=0;
#ifdef _WIN32
    char td[MAX_PATH]; GetTempPathA(sizeof td,td);
    snprintf(tmp,sizeof tmp,"%sori_post_%lu_%d.tmp",td,(unsigned long)GetCurrentProcessId(),++post_seq);
#else
    snprintf(tmp,sizeof tmp,"/tmp/ori_post_%d_%d.tmp",(int)getpid(),++post_seq);
#endif
    FILE* tf=fopen(tmp,"wb"); if(!tf) return vstr("");
    fwrite(body,1,strlen(body),tf); fclose(tf);
    char cmd[2600];
#ifdef _WIN32
    snprintf(cmd,sizeof cmd,"curl -sL --max-time 8 --connect-timeout 5 -X POST -H \"Content-Type: application/json\" --data-binary @\"%s\" \"%s\" 2>nul",tmp,url);
    FILE* p=_popen(cmd,"r");
#else
    snprintf(cmd,sizeof cmd,"curl -sL --max-time 8 --connect-timeout 5 -X POST -H 'Content-Type: application/json' --data-binary @'%s' '%s' 2>/dev/null",tmp,url);
    FILE* p=popen(cmd,"r");
#endif
    char buf[65536]; size_t total=0,r;
    if(p){
        while((r=fread(buf+total,1,sizeof buf-total-1,p))>0){ total+=r; if(total>=sizeof buf-1) break; }
        buf[total]=0;
#ifdef _WIN32
        _pclose(p);
#else
        pclose(p);
#endif
    } else { buf[0]=0; }
    remove(tmp);
    return vstr_n(buf,(int)total);
}

// Simple single-threaded blocking HTTP server.
// Calls handler(route, body) for each request where route = "METHOD /path?qs".
static Value h_http_serve(VM* vm, Value* a, int argc){
    if(argc<2||a[0].t!=V_NUM||a[1].t!=V_STR) rt_error("http_serve(port, handler_name)");
    int port=safe_int(a[0].u.num);
    if(port<1||port>65535) rt_error("http_serve: port out of range (1-65535)");
    char hname[256]; int hn=a[1].u.s->len;
    if(hn<1||hn>=255) rt_error("http_serve: handler name too long");
    memcpy(hname,a[1].u.s->d,hn); hname[hn]=0;
    Value hfn; if(!g_get(vm,hname,&hfn)||hfn.t!=V_FUNC) rt_error("http_serve: handler function not found");
    int fn_idx=hfn.u.i;
    Func* fn=&vm->prog->funcs[fn_idx];
    if(fn->arity!=2){ char m[128]; snprintf(m,sizeof m,"http_serve: handler '%s' must take 2 args (route, body)",hname); rt_error(m); }

    ori_net_init();
    ori_sock_t srv=socket(AF_INET,SOCK_STREAM,0);
    if(srv==ORI_SOCK_INVALID) rt_error("http_serve: socket() failed");
    int yes=1; setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(yes));
    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET; addr.sin_port=htons((uint16_t)port); addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(srv,(struct sockaddr*)&addr,sizeof(addr))!=0){ ori_sock_close(srv); rt_error("http_serve: bind() failed — port may be in use"); }
    listen(srv,16);
    fprintf(stderr,"[Ori] http_serve listening on http://localhost:%d\n",port); fflush(stderr);

    static char req_buf[1048576]; /* 1 MB — static to avoid large stack frame */
    for(;;){
        ori_sock_t conn=accept(srv,NULL,NULL);
        if(conn==ORI_SOCK_INVALID) continue;
        /* 10 s receive/send timeout */
#ifdef _WIN32
        DWORD tv_ms=10000;
        setsockopt(conn,SOL_SOCKET,SO_RCVTIMEO,(char*)&tv_ms,sizeof(tv_ms));
        setsockopt(conn,SOL_SOCKET,SO_SNDTIMEO,(char*)&tv_ms,sizeof(tv_ms));
#else
        struct timeval tv; tv.tv_sec=10; tv.tv_usec=0;
        setsockopt(conn,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        setsockopt(conn,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
#endif
        /* read headers (until \r\n\r\n) */
        int total=0;
        while(total<(int)sizeof(req_buf)-1){
            int r=(int)recv(conn,req_buf+total,(int)(sizeof(req_buf)-1-total),0);
            if(r<=0) break; total+=r; req_buf[total]=0;
            if(strstr(req_buf,"\r\n\r\n")||strstr(req_buf,"\n\n")) break;
        }
        req_buf[total]=0;
        /* parse method and path */
        char method[16]="GET", path[2048]="/";
        const char* rp=req_buf;
        int mi=0; while(*rp&&*rp!=' '&&mi<15) method[mi++]=*rp++; method[mi]=0;
        if(*rp==' ') rp++;
        int pi=0; while(*rp&&*rp!=' '&&*rp!='\r'&&*rp!='\n'&&pi<2047) path[pi++]=*rp++; path[pi]=0;
        /* find header/body boundary */
        char* sep=strstr(req_buf,"\r\n\r\n");
        int body_offset=sep?(int)(sep-req_buf)+4:total;
        if(!sep){ char* sep2=strstr(req_buf,"\n\n"); if(sep2){ body_offset=(int)(sep2-req_buf)+2; sep=sep2; } }
        /* parse Content-Length (case-insensitive header scan) */
        int clen=0;
        {
            const char* h=req_buf;
            while(h<req_buf+body_offset-16){
                if((*h=='c'||*h=='C') &&
                   (h[1]=='o'||h[1]=='O') && (h[2]=='n'||h[2]=='N') &&
                   (h[3]=='t'||h[3]=='T') && (h[4]=='e'||h[4]=='E') &&
                   (h[5]=='n'||h[5]=='N') && (h[6]=='t'||h[6]=='T') &&
                   h[7]=='-' &&
                   (h[8]=='l'||h[8]=='L') && (h[9]=='e'||h[9]=='E') &&
                   (h[10]=='n'||h[10]=='N') && (h[11]=='g'||h[11]=='G') &&
                   (h[12]=='t'||h[12]=='T') && (h[13]=='h'||h[13]=='H') &&
                   h[14]==':'){
                    clen=(int)strtol(h+15,NULL,10); break;
                }
                h++;
            }
        }
        if(clen<0) clen=0;
        if(clen>(int)sizeof(req_buf)-body_offset-1) clen=(int)sizeof(req_buf)-body_offset-1;
        /* read remaining body bytes if not already buffered */
        while(clen>0 && total-body_offset<clen && total<(int)sizeof(req_buf)-1){
            int r=(int)recv(conn,req_buf+total,(int)(sizeof(req_buf)-1-total),0);
            if(r<=0) break; total+=r; req_buf[total]=0;
        }
        /* body pointer */
        const char* body=sep?req_buf+body_offset:"";
        /* build route string */
        char route[2080]; snprintf(route,sizeof route,"%s %s",method,path);
        fprintf(stderr,"[Ori] %s %s\n",method,path); fflush(stderr);
        /* call Ori handler: handler(route, body) — isolated via run_until */
        /* Note: route/body Str* are intentionally not freed here — the Ori handler
           may have captured them into a global (no ref-counting in this VM). The leak
           is bounded: ~path+body bytes per request (~4 KB typical). */
        int saved_fp=vm->fp;
        Value args[2]; args[0]=vstr(route); args[1]=vstr(body);
        push_frame(vm,fn_idx,args,2);
        Value resp=run_until(vm,saved_fp);
        char* resp_s=val_cstr(resp); size_t resp_len=strlen(resp_s);
        /* detect content type */
        const char* ct="application/json";
        if(resp_len>0&&resp_s[0]=='<') ct="text/html; charset=utf-8";
        else if(resp_len==0||(resp_s[0]!='{'&&resp_s[0]!='['&&resp_s[0]!='"'&&resp_s[0]!='n'&&resp_s[0]!='t'&&resp_s[0]!='f'&&resp_s[0]!='0')) ct="text/plain; charset=utf-8";
        /* send response */
        char hdr[512]; int hlen=snprintf(hdr,sizeof hdr,
            "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
            "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Content-Type\r\n"
            "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            "Connection: close\r\n\r\n",ct,resp_len);
        send(conn,hdr,hlen,0);
        if(resp_len>0) send(conn,resp_s,(int)resp_len,0);
        free(resp_s); ori_sock_close(conn);
    }
    ori_sock_close(srv); return vnil();
}

static Value h_is_dir(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vnum(0);
    if(has_dotdot(a[0].u.s->d)) return vnum(0);
    struct stat st; if(stat(a[0].u.s->d,&st)!=0) return vnum(0);
    return vnum((st.st_mode & S_IFDIR)?1:0);
}
static Value h_mtime(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vnum(0);
    if(has_dotdot(a[0].u.s->d)) return vnum(0);
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
    if(has_dotdot(a[0].u.s->d)){ fprintf(stderr,"[Ori] read_bytes_b64: path traversal rejected\n"); return vstr(""); }
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

// json_get_str(json, key) — extract string value of a JSON key
static Value h_json_get_str(VM* vm, Value* a, int argc){
    if(argc<2||a[0].t!=V_STR||a[1].t!=V_STR) return vstr("");
    const char* js=a[0].u.s->d; const char* key=a[1].u.s->d;
    char needle[512]; snprintf(needle,sizeof needle,"\"%s\":",key);
    const char* p=strstr(js,needle); if(!p) return vstr("");
    p+=strlen(needle);
    while(*p==' '||*p=='\t') p++;
    if(*p!='"') return vstr("");
    p++;
    const char* start=p;
    char buf[65536]; int bi=0;
    while(*p&&*p!='"'&&bi<(int)sizeof(buf)-1){
        if(*p=='\\'&&*(p+1)){
            p++;
            switch(*p){
                case 'n': buf[bi++]='\n'; break;
                case 't': buf[bi++]='\t'; break;
                case 'r': buf[bi++]='\r'; break;
                case '"': buf[bi++]='"'; break;
                case '\\': buf[bi++]='\\'; break;
                case '/': buf[bi++]='/'; break;
                case 'b': buf[bi++]='\b'; break;
                case 'f': buf[bi++]='\f'; break;
                case 'u': {
                    /* decode \uXXXX → UTF-8 */
                    unsigned int cp=0; int ok=1;
                    for(int hi=0;hi<4;hi++){
                        char h=*(p+1+hi);
                        if(h>='0'&&h<='9') cp=(cp<<4)|(unsigned)(h-'0');
                        else if(h>='a'&&h<='f') cp=(cp<<4)|(unsigned)(h-'a'+10);
                        else if(h>='A'&&h<='F') cp=(cp<<4)|(unsigned)(h-'A'+10);
                        else { ok=0; break; }
                    }
                    if(ok){ p+=4;
                        if(cp<0x80){ buf[bi++]=(char)cp; }
                        else if(cp<0x800){ buf[bi++]=(char)(0xC0|(cp>>6)); buf[bi++]=(char)(0x80|(cp&0x3F)); }
                        else { buf[bi++]=(char)(0xE0|(cp>>12)); buf[bi++]=(char)(0x80|((cp>>6)&0x3F)); buf[bi++]=(char)(0x80|(cp&0x3F)); }
                    } else { buf[bi++]='\\'; buf[bi++]='u'; }
                    break;
                }
                default: buf[bi++]='\\'; if(bi<(int)sizeof(buf)-1) buf[bi++]=*p; break;
            }
        } else { buf[bi++]=*p; }
        p++;
    }
    buf[bi]=0; (void)start;
    return vstr_n(buf,bi);
}

// json_get_num(json, key) — extract numeric value of a JSON key
static Value h_json_get_num(VM* vm, Value* a, int argc){
    if(argc<2||a[0].t!=V_STR||a[1].t!=V_STR) return vnum(0);
    const char* js=a[0].u.s->d; const char* key=a[1].u.s->d;
    char needle[512]; snprintf(needle,sizeof needle,"\"%s\":",key);
    const char* p=strstr(js,needle); if(!p) return vnum(0);
    p+=strlen(needle);
    while(*p==' '||*p=='\t') p++;
    char* end; double d=strtod(p,&end);
    if(end==p) return vnum(0);
    return vnum(isfinite(d)?d:0);
}

// json_escape(str) — escape a string for inclusion in JSON
static Value h_json_escape(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vstr("");
    Str* s=a[0].u.s;
    char* buf=(char*)xmalloc(s->len*6+3);
    int bi=0;
    for(int i=0;i<s->len;i++){
        unsigned char c=(unsigned char)s->d[i];
        if(c=='"'){ buf[bi++]='\\'; buf[bi++]='"'; }
        else if(c=='\\'){ buf[bi++]='\\'; buf[bi++]='\\'; }
        else if(c=='\n'){ buf[bi++]='\\'; buf[bi++]='n'; }
        else if(c=='\r'){ buf[bi++]='\\'; buf[bi++]='r'; }
        else if(c=='\t'){ buf[bi++]='\\'; buf[bi++]='t'; }
        else if(c<0x20){ /* RFC 8259: escape all control chars as \u00XX */
            buf[bi++]='\\'; buf[bi++]='u'; buf[bi++]='0'; buf[bi++]='0';
            buf[bi++]="0123456789abcdef"[c>>4];
            buf[bi++]="0123456789abcdef"[c&0xF];
        }
        else { buf[bi++]=(char)c; }
    }
    buf[bi]=0;
    Value v=vstr_n(buf,bi); free(buf); return v;
}

#define JSON_ARR_MAX 10000
static Value h_json_parse_arr(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return varr_new();
    const char* s=a[0].u.s->d;
    int n=(int)a[0].u.s->len;
    Value arr=varr_new();
    int depth=0, obj_start=-1, count=0;
    for(int i=0;i<n;i++){
        char c=s[i];
        if(c=='"'){ i++; while(i<n&&s[i]!='"'){ if(s[i]=='\\') i++; i++; } continue; }
        if(c=='{'){
            if(depth==0) obj_start=i;
            depth++;
        } else if(c=='}'){
            if(depth>0) depth--;
            if(depth==0&&obj_start>=0){
                if(count<JSON_ARR_MAX) arr_push(arr.u.a, vstr_n(s+obj_start, i-obj_start+1));
                count++; obj_start=-1;
                if(count>=JSON_ARR_MAX) break;
            }
        }
    }
    return arr;
}

static Value h_http_put(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vstr("");
    const char* url=a[0].u.s->d;
    if(!url_shell_safe(url)){ fprintf(stderr,"[Ori] http_put: URL unsafe\n"); return vstr(""); }
    if(strlen(url)>1800){ fprintf(stderr,"[Ori] http_put: URL too long\n"); return vstr(""); }
    const char* body=argc>=2&&a[1].t==V_STR?a[1].u.s->d:"{}";
    char tmp[512]; static int put_seq=0;
#ifdef _WIN32
    char td[MAX_PATH]; GetTempPathA(sizeof td,td);
    snprintf(tmp,sizeof tmp,"%sori_put_%lu_%d.tmp",td,(unsigned long)GetCurrentProcessId(),++put_seq);
#else
    snprintf(tmp,sizeof tmp,"/tmp/ori_put_%d_%d.tmp",(int)getpid(),++put_seq);
#endif
    FILE* tf=fopen(tmp,"wb"); if(!tf) return vstr("");
    fwrite(body,1,strlen(body),tf); fclose(tf);
    char cmd[2600];
#ifdef _WIN32
    snprintf(cmd,sizeof cmd,"curl -sL --max-time 8 -X PUT -H \"Content-Type: application/json\" --data-binary @\"%s\" \"%s\" 2>nul",tmp,url);
    FILE* p=_popen(cmd,"r");
#else
    snprintf(cmd,sizeof cmd,"curl -sL --max-time 8 -X PUT -H 'Content-Type: application/json' --data-binary @'%s' '%s' 2>/dev/null",tmp,url);
    FILE* p=popen(cmd,"r");
#endif
    char buf[65536]; size_t total=0,r;
    if(p){ while((r=fread(buf+total,1,sizeof buf-total-1,p))>0){ total+=r; if(total>=sizeof buf-1) break; }
        buf[total]=0;
#ifdef _WIN32
        _pclose(p);
#else
        pclose(p);
#endif
    } else buf[0]=0;
    remove(tmp); return vstr_n(buf,(int)total);
}

static Value h_http_delete(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vstr("");
    const char* url=a[0].u.s->d;
    if(!url_shell_safe(url)){ fprintf(stderr,"[Ori] http_delete: URL unsafe\n"); return vstr(""); }
    if(strlen(url)>1800){ fprintf(stderr,"[Ori] http_delete: URL too long\n"); return vstr(""); }
    char cmd[2048];
#ifdef _WIN32
    snprintf(cmd,sizeof cmd,"curl -sL --max-time 8 -X DELETE \"%s\" 2>nul",url);
    FILE* p=_popen(cmd,"r");
#else
    snprintf(cmd,sizeof cmd,"curl -sL --max-time 8 -X DELETE '%s' 2>/dev/null",url);
    FILE* p=popen(cmd,"r");
#endif
    char buf[65536]; size_t total=0,r;
    if(p){ while((r=fread(buf+total,1,sizeof buf-total-1,p))>0){ total+=r; if(total>=sizeof buf-1) break; }
        buf[total]=0;
#ifdef _WIN32
        _pclose(p);
#else
        pclose(p);
#endif
    } else buf[0]=0;
    return vstr_n(buf,(int)total);
}

/* max dir path length for store functions — keeps path[1024] buffer safe */
#define STORE_DIR_MAX 900

/* sanitize a store id: only allow [0-9A-Za-z_-], max 63 chars.
   Returns 0 if id is safe, -1 if it contains path-traversal chars. */
static int store_id_safe(Value* v, char* out, int outsz){
    char tmp[64];
    if(v->t==V_NUM){ snprintf(tmp,sizeof tmp,"%d",safe_int(v->u.num)); }
    else if(v->t==V_STR){ snprintf(tmp,sizeof tmp,"%s",v->u.s->d); }
    else { return -1; }
    for(int i=0;tmp[i];i++){
        char c=tmp[i];
        if(!((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'||c=='-'))
            return -1;
    }
    snprintf(out,outsz,"%s",tmp);
    return 0;
}

// store_next_id(dir) — returns next auto-increment ID for a table directory
#define STORE_SEQ_MAX 2000000000
static Value h_store_next_id(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return vnum(1);
    if(has_dotdot(a[0].u.s->d)){ fprintf(stderr,"[Ori] store_next_id: path traversal rejected\n"); return vnum(1); }
    ori_mkdirs(a[0].u.s->d);
    char path[1024]; snprintf(path,sizeof path,"%s/_seq",a[0].u.s->d);
    int id=1;
    FILE* f=fopen(path,"r"); if(f){ fscanf(f,"%d",&id); fclose(f); if(id<1||id>STORE_SEQ_MAX) id=1; }
    FILE* w=fopen(path,"w"); if(w){ fprintf(w,"%d",id<STORE_SEQ_MAX?id+1:id); fclose(w); }
    return vnum((double)id);
}

// store_set(dir, id, json) — write a record; creates dir if needed
static Value h_store_set(VM* vm, Value* a, int argc){
    if(argc<3||a[0].t!=V_STR||a[2].t!=V_STR) return vnum(0);
    if(a[0].u.s->len>STORE_DIR_MAX) return vnum(0);
    if(has_dotdot(a[0].u.s->d)){ fprintf(stderr,"[Ori] store_set: path traversal rejected\n"); return vnum(0); }
    const char* dir=a[0].u.s->d;
    char id_s[64]; if(store_id_safe(&a[1],id_s,sizeof id_s)<0) return vnum(0);
    ori_mkdirs(dir);
    char path[1024]; snprintf(path,sizeof path,"%s/%s.json",dir,id_s);
    FILE* f=fopen(path,"wb"); if(!f) return vnum(0);
    fwrite(a[2].u.s->d,1,a[2].u.s->len,f); fclose(f); return vnum(1);
}

// store_get(dir, id) — read a record; returns "" if not found
static Value h_store_get(VM* vm, Value* a, int argc){
    if(argc<2||a[0].t!=V_STR) return vstr("");
    if(a[0].u.s->len>STORE_DIR_MAX) return vstr("");
    if(has_dotdot(a[0].u.s->d)){ fprintf(stderr,"[Ori] store_get: path traversal rejected\n"); return vstr(""); }
    char id_s[64]; if(store_id_safe(&a[1],id_s,sizeof id_s)<0) return vstr("");
    char path[1024]; snprintf(path,sizeof path,"%s/%s.json",a[0].u.s->d,id_s);
    FILE* f=fopen(path,"rb"); if(!f) return vstr("");
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if(n<=0||n>1048576){ fclose(f); return vstr(""); }
    char* buf=(char*)xmalloc(n+1); size_t got=fread(buf,1,n,f); fclose(f); buf[got]=0;
    Value v=vstr_n(buf,(int)got); free(buf); return v;
}

// store_delete(dir, id) — delete a record; returns 1 if deleted
static Value h_store_delete(VM* vm, Value* a, int argc){
    if(argc<2||a[0].t!=V_STR) return vnum(0);
    if(a[0].u.s->len>STORE_DIR_MAX) return vnum(0);
    if(has_dotdot(a[0].u.s->d)){ fprintf(stderr,"[Ori] store_delete: path traversal rejected\n"); return vnum(0); }
    char id_s[64]; if(store_id_safe(&a[1],id_s,sizeof id_s)<0) return vnum(0);
    char path[1024]; snprintf(path,sizeof path,"%s/%s.json",a[0].u.s->d,id_s);
    return vnum(remove(path)==0?1:0);
}

// store_list(dir) — returns array of all JSON records (max 10,000 records)
#define STORE_LIST_MAX 10000
static Value h_store_list(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_STR) return varr_new();
    if(a[0].u.s->len>STORE_DIR_MAX) return varr_new();
    if(has_dotdot(a[0].u.s->d)){ fprintf(stderr,"[Ori] store_list: path traversal rejected\n"); return varr_new(); }
    const char* dir=a[0].u.s->d;
    Value arr=varr_new();
    int count=0;
#ifdef _WIN32
    char pat[1024]; snprintf(pat,sizeof pat,"%s\\*.json",dir);
    WIN32_FIND_DATAA fd; HANDLE h=FindFirstFileA(pat,&fd);
    if(h==INVALID_HANDLE_VALUE) return arr;
    do {
        if(count>=STORE_LIST_MAX) break;
        if(strcmp(fd.cFileName,"_seq.json")==0) continue;
        char path[1024]; snprintf(path,sizeof path,"%s\\%s",dir,fd.cFileName);
        FILE* f=fopen(path,"rb"); if(!f) continue;
        fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
        if(n>0&&n<=1048576){
            char* buf=(char*)xmalloc(n+1); size_t got=fread(buf,1,n,f);
            buf[got]=0; arr_push(arr.u.a,vstr_n(buf,(int)got)); free(buf); count++;
        }
        fclose(f);
    } while(FindNextFileA(h,&fd));
    FindClose(h);
#else
    DIR* d=opendir(dir); if(!d) return arr;
    struct dirent* en;
    while(count<STORE_LIST_MAX&&(en=readdir(d))!=NULL){
        const char* nm=en->d_name;
        int nl=strlen(nm);
        if(nl<6||strcmp(nm+nl-5,".json")!=0) continue;
        if(strcmp(nm,"_seq.json")==0) continue;
        char path[1024]; snprintf(path,sizeof path,"%s/%s",dir,nm);
        FILE* f=fopen(path,"rb"); if(!f) continue;
        fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
        if(n>0&&n<=1048576){
            char* buf=(char*)xmalloc(n+1); size_t got=fread(buf,1,n,f);
            buf[got]=0; arr_push(arr.u.a,vstr_n(buf,(int)got)); free(buf); count++;
        }
        fclose(f);
    }
    closedir(d);
#endif
    return arr;
}

#ifdef ORI_MYSQL
/* MySQL must be installed: https://dev.mysql.com/downloads/connector/c/
   Build: cl /DORI_MYSQL /I"path\to\mysql\include" orivm.c mysqlclient.lib */
#include <mysql.h>

#define ORI_DB_MAX 8
static MYSQL* g_db[ORI_DB_MAX];

static Value h_db_connect(VM* vm, Value* a, int argc){
    if(argc<5) return vnum(0);
    const char* host=a[0].t==V_STR?a[0].u.s->d:"localhost";
    int port=a[1].t==V_NUM?(int)a[1].u.num:3306;
    const char* user=a[2].t==V_STR?a[2].u.s->d:"root";
    const char* pass=a[3].t==V_STR?a[3].u.s->d:"";
    const char* db=a[4].t==V_STR?a[4].u.s->d:"";
    int slot=-1;
    for(int i=0;i<ORI_DB_MAX;i++) if(!g_db[i]){slot=i;break;}
    if(slot<0){fprintf(stderr,"[Ori] db_connect: connection pool full\n");return vnum(0);}
    MYSQL* m=mysql_init(NULL);
    if(!m) return vnum(0);
    if(!mysql_real_connect(m,host,user,pass,db,(unsigned int)port,NULL,0)){
        fprintf(stderr,"[Ori] db_connect error: %s\n",mysql_error(m));
        mysql_close(m); return vnum(0);
    }
    mysql_set_character_set(m,"utf8mb4");
    g_db[slot]=m; return vnum((double)(slot+1));
}

static Value h_db_exec(VM* vm, Value* a, int argc){
    if(argc<2||a[0].t!=V_NUM||a[1].t!=V_STR) return vnum(-1);
    int slot=(int)a[0].u.num-1;
    if(slot<0||slot>=ORI_DB_MAX||!g_db[slot]) return vnum(-1);
    if(mysql_query(g_db[slot],a[1].u.s->d)){
        fprintf(stderr,"[Ori] db_exec error: %s\n",mysql_error(g_db[slot])); return vnum(-1);
    }
    return vnum((double)mysql_affected_rows(g_db[slot]));
}

static Value h_db_query(VM* vm, Value* a, int argc){
    Value arr=varr_new();
    if(argc<2||a[0].t!=V_NUM||a[1].t!=V_STR) return arr;
    int slot=(int)a[0].u.num-1;
    if(slot<0||slot>=ORI_DB_MAX||!g_db[slot]) return arr;
    if(mysql_query(g_db[slot],a[1].u.s->d)){
        fprintf(stderr,"[Ori] db_query error: %s\n",mysql_error(g_db[slot])); return arr;
    }
    MYSQL_RES* res=mysql_store_result(g_db[slot]); if(!res) return arr;
    unsigned int nc=mysql_num_fields(res);
    MYSQL_FIELD* fields=mysql_fetch_fields(res);
    MYSQL_ROW row;
    Sb sb;
    while((row=mysql_fetch_row(res))!=NULL){
        sb_init(&sb); sb_putc(&sb,'{');
        unsigned long* lens=mysql_fetch_lengths(res);
        for(unsigned int i=0;i<nc;i++){
            if(i) sb_putc(&sb,',');
            sb_putc(&sb,'"'); sb_puts(&sb,fields[i].name); sb_puts(&sb,"\":");
            if(!row[i]){ sb_puts(&sb,"null"); }
            else if(IS_NUM_FIELD(fields[i].flags)){
                sb_put(&sb,row[i],(int)lens[i]);
            } else {
                sb_putc(&sb,'"');
                for(unsigned long j=0;j<lens[i];j++){
                    char c=row[i][j];
                    if(c=='"'){ sb_putc(&sb,'\\'); sb_putc(&sb,'"'); }
                    else if(c=='\\'){ sb_putc(&sb,'\\'); sb_putc(&sb,'\\'); }
                    else if(c=='\n'){ sb_putc(&sb,'\\'); sb_putc(&sb,'n'); }
                    else { sb_putc(&sb,c); }
                }
                sb_putc(&sb,'"');
            }
        }
        sb_putc(&sb,'}'); arr_push(arr.u.a,vstr_n(sb.d,sb.len)); free(sb.d);
    }
    mysql_free_result(res); return arr;
}

static Value h_db_escape(VM* vm, Value* a, int argc){
    if(argc<2||a[0].t!=V_NUM||a[1].t!=V_STR) return vstr("");
    int slot=(int)a[0].u.num-1;
    if(slot<0||slot>=ORI_DB_MAX||!g_db[slot]) return vstr("");
    Str* s=a[1].u.s;
    char* buf=(char*)xmalloc(s->len*2+1);
    mysql_real_escape_string(g_db[slot],buf,s->d,(unsigned long)s->len);
    Value v=vstr(buf); free(buf); return v;
}

static Value h_db_last_id(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_NUM) return vnum(0);
    int slot=(int)a[0].u.num-1;
    if(slot<0||slot>=ORI_DB_MAX||!g_db[slot]) return vnum(0);
    return vnum((double)mysql_insert_id(g_db[slot]));
}

static Value h_db_error(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_NUM) return vstr("");
    int slot=(int)a[0].u.num-1;
    if(slot<0||slot>=ORI_DB_MAX||!g_db[slot]) return vstr("");
    return vstr(mysql_error(g_db[slot]));
}

static Value h_db_close(VM* vm, Value* a, int argc){
    if(argc<1||a[0].t!=V_NUM) return vnil();
    int slot=(int)a[0].u.num-1;
    if(slot<0||slot>=ORI_DB_MAX||!g_db[slot]) return vnil();
    mysql_close(g_db[slot]); g_db[slot]=NULL; return vnil();
}
#else
static Value h_db_connect(VM* vm, Value* a, int argc){ rt_error("db_connect: MySQL support not compiled in (rebuild with -DORI_MYSQL)"); return vnil(); }
static Value h_db_exec(VM* vm, Value* a, int argc){ rt_error("db_exec: MySQL support not compiled in"); return vnil(); }
static Value h_db_query(VM* vm, Value* a, int argc){ rt_error("db_query: MySQL support not compiled in"); return vnil(); }
static Value h_db_escape(VM* vm, Value* a, int argc){ rt_error("db_escape: MySQL support not compiled in"); return vnil(); }
static Value h_db_last_id(VM* vm, Value* a, int argc){ rt_error("db_last_id: MySQL support not compiled in"); return vnil(); }
static Value h_db_error(VM* vm, Value* a, int argc){ rt_error("db_error: MySQL support not compiled in"); return vnil(); }
static Value h_db_close(VM* vm, Value* a, int argc){ rt_error("db_close: MySQL support not compiled in"); return vnil(); }
#endif /* ORI_MYSQL */

static void register_hosts(VM* vm){
    static const char* names[] = {
        "say","print","str","num","len","push","pop","char_at","ord","chr","substr","type",
        "abs","floor","sqrt","max","min","upper","lower",
        "read_file","write_bytes","write_file","argc","argv",
        "env","exists","sh","run","mkdirs","copy","glob","abspath",
        "is_dir","mtime","sleep_ms","read_bytes_b64","http_get","http_post","http_serve","json_get_str","json_get_num","json_escape","json_parse_arr","http_put","http_delete","store_next_id","store_set","store_get","store_delete","store_list","db_connect","db_exec","db_query","db_escape","db_last_id","db_error","db_close" };
    static HostFn fns[] = {
        h_say,h_say,h_str,h_num,h_len,h_push,h_pop,h_char_at,h_ord,h_chr,h_substr,h_type,
        h_abs,h_floor,h_sqrt,h_max,h_min,h_upper,h_lower,
        h_read_file,h_write_bytes,h_write_file,h_argc,h_argv,
        h_env,h_exists,h_sh,h_run,h_mkdirs,h_copy,h_glob,h_abspath,
        h_is_dir,h_mtime,h_sleep_ms,h_read_bytes_b64,h_http_get,h_http_post,h_http_serve,h_json_get_str,h_json_get_num,h_json_escape,h_json_parse_arr,h_http_put,h_http_delete,h_store_next_id,h_store_set,h_store_get,h_store_delete,h_store_list,h_db_connect,h_db_exec,h_db_query,h_db_escape,h_db_last_id,h_db_error,h_db_close };
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
    if(n>256L*1024*1024){ fprintf(stderr,"image too large (>256MB): %s\n",path); exit(2); }
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
