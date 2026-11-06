// DirtySDK 5.5.3 CryptMD5, ARTIST 82B408B8/82B414B8/82B414F8/82B415C8.
#include <cstdint>
typedef uint8_t u8; typedef uint32_t u32; typedef int32_t s32;
#include <string.h>
struct CryptMD5T { u8 strData[72]; u32 uCount; u32 uRegs[4]; };
typedef char CryptMD5SizeCheck[sizeof(CryptMD5T) == 92 ? 1 : -1];
static u32 Rol(u32 x, u32 n) { return (x << n) | (x >> (32 - n)); }
static void CryptMD5Transform(CryptMD5T* ctx, const u8* data)
{
    static const u32 k[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    static const u8 shift[64] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
    };
    u32 m[16];
    for (u32 i=0;i<16;++i)
        m[i] = u32(data[i*4]) | (u32(data[i*4+1])<<8) | (u32(data[i*4+2])<<16) | (u32(data[i*4+3])<<24);
    u32 a=ctx->uRegs[0], b=ctx->uRegs[1], c=ctx->uRegs[2], d=ctx->uRegs[3];
    for (u32 i=0;i<64;++i)
    {
        u32 f,g;
        if (i<16) { f=(b&c)|(~b&d); g=i; }
        else if (i<32) { f=(d&b)|(~d&c); g=(5*i+1)%16; }
        else if (i<48) { f=b^c^d; g=(3*i+5)%16; }
        else { f=c^(b|~d); g=(7*i)%16; }
        const u32 oldD=d; d=c; c=b; b=b+Rol(a+f+k[i]+m[g],shift[i]); a=oldD;
    }
    ctx->uRegs[0]+=a; ctx->uRegs[1]+=b; ctx->uRegs[2]+=c; ctx->uRegs[3]+=d;
}
extern "C" void CryptMD5Init(CryptMD5T* ctx)
{
    ctx->uCount=0; ctx->uRegs[0]=0x67452301; ctx->uRegs[1]=0xefcdab89;
    ctx->uRegs[2]=0x98badcfe; ctx->uRegs[3]=0x10325476;
}
extern "C" void CryptMD5Update(CryptMD5T* ctx, const void* buffer, s32 len)
{
    const u8* p=static_cast<const u8*>(buffer);
    if (len<0) len=static_cast<s32>(strlen(reinterpret_cast<const char*>(p)));
    const u32 used=ctx->uCount&63; ctx->uCount+=len;
    if (used)
    {
        const s32 remain=64-used;
        if (len<remain) { memcpy(ctx->strData+used,p,len); return; }
        memcpy(ctx->strData+used,p,remain); p+=remain; len-=remain;
        CryptMD5Transform(ctx,ctx->strData);
    }
    while (len>=64) { CryptMD5Transform(ctx,p); p+=64; len-=64; }
    if (len>0) memcpy(ctx->strData,p,len);
}
extern "C" void CryptMD5Final(CryptMD5T* ctx, void* buffer, s32 len)
{
    u32 used=ctx->uCount&63; ctx->strData[used++]=0x80;
    if (used>56) { memset(ctx->strData+used,0,8); CryptMD5Transform(ctx,ctx->strData); used=0; }
    memset(ctx->strData+used,0,64-used);
    const u32 count=ctx->uCount;
    ctx->strData[56]=u8(count<<3); ctx->strData[57]=u8(count>>5);
    ctx->strData[58]=u8(count>>13); ctx->strData[59]=u8(count>>21); ctx->strData[60]=u8(count>>29);
    CryptMD5Transform(ctx,ctx->strData);
    static const char hex[]="0123456789abcdef";
    u8* out=static_cast<u8*>(buffer);
    for (s32 i=0;i<16;++i)
    {
        const u8 value=u8(ctx->uRegs[i/4]>>(8*(i&3)));
        if (len<33) { if (i<len) *out++=value; }
        else { *out++=hex[value>>4]; *out++=hex[value&15]; }
    }
    if (len>=33) *out=0;
}

#include <stdio.h>
int main(){ for(int n=0;n<=130;n++){ CryptMD5T c; unsigned char b[130]; for(int i=0;i<n;i++)b[i]=(unsigned char)i; char out[33]; CryptMD5Init(&c); CryptMD5Update(&c,b,n/2); CryptMD5Update(&c,b+n/2,n-n/2); CryptMD5Final(&c,out,33); printf("%d %s\n",n,out); } }
