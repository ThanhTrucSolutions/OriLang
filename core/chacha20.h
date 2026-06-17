// ChaCha20 (RFC 8439, 20 rounds) — matches src/OriLang/ChaCha20.cs byte-for-byte.
#ifndef ORI_CHACHA20_H
#define ORI_CHACHA20_H
#include <stdint.h>
#include <string.h>

static uint32_t cc_rotl(uint32_t v,int n){ return (v<<n)|(v>>(32-n)); }
static uint32_t cc_rdu32(const uint8_t* b){ return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24); }
static void cc_wru32(uint8_t* b,uint32_t v){ b[0]=(uint8_t)v;b[1]=(uint8_t)(v>>8);b[2]=(uint8_t)(v>>16);b[3]=(uint8_t)(v>>24); }

static void cc_qr(uint32_t* x,int a,int b,int c,int d){
    x[a]+=x[b]; x[d]=cc_rotl(x[d]^x[a],16);
    x[c]+=x[d]; x[b]=cc_rotl(x[b]^x[c],12);
    x[a]+=x[b]; x[d]=cc_rotl(x[d]^x[a],8);
    x[c]+=x[d]; x[b]=cc_rotl(x[b]^x[c],7);
}

static void cc_block(const uint32_t in[16], uint8_t out[64]){
    uint32_t x[16]; for(int i=0;i<16;i++) x[i]=in[i];
    for(int r=0;r<20;r+=2){
        cc_qr(x,0,4,8,12); cc_qr(x,1,5,9,13); cc_qr(x,2,6,10,14); cc_qr(x,3,7,11,15);
        cc_qr(x,0,5,10,15); cc_qr(x,1,6,11,12); cc_qr(x,2,7,8,13); cc_qr(x,3,4,9,14);
    }
    for(int i=0;i<16;i++) cc_wru32(out+i*4, x[i]+in[i]);
}

// XOR-crypt data in place using key (32) + nonce (12), starting at block `counter`.
static void chacha20_crypt(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter,
                           uint8_t* data, size_t len){
    uint32_t st[16];
    st[0]=0x61707865;st[1]=0x3320646e;st[2]=0x79622d32;st[3]=0x6b206574;
    for(int i=0;i<8;i++) st[4+i]=cc_rdu32(key+i*4);
    st[12]=counter;
    st[13]=cc_rdu32(nonce+0); st[14]=cc_rdu32(nonce+4); st[15]=cc_rdu32(nonce+8);
    uint8_t ks[64]; size_t off=0;
    while(off<len){
        cc_block(st,ks); st[12]++;
        size_t chunk=len-off; if(chunk>64) chunk=64;
        for(size_t i=0;i<chunk;i++) data[off+i]^=ks[i];
        off+=chunk;
    }
}
#endif
