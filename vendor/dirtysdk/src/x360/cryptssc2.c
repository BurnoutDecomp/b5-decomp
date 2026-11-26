// DirtySDK 5.5.3 CryptSSC2. ARTIST 82B32170/82B322B0/82B32340.
// XDK size optimization matches ARTIST at 0x82B322B0.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "types.hpp"
#include <stddef.h>
#include <string.h>

typedef struct CryptSSC2T CryptSSC2T;   // DirtySDK is C
struct CryptSSC2T { u8 sbox[256]; u8 idx; u32 crc; };
typedef char SSC2SizeCheck[(sizeof(CryptSSC2T) == 264) ? 1 : -1];
typedef char SSC2CrcOffsetCheck[(offsetof(CryptSSC2T, crc) == 260) ? 1 : -1];
// ARTIST 0x82146AB0, 256 PPC big-endian words (SHA256 of source bytes:
// da5c898b03a4b8559186e7ce3b034e14aab00e7672966ce3032e2b2a2fd4574e).
static const u32 _Cipher_Crc32[256] = {
    0x00000000u, 0x77073096u, 0xee0e612cu, 0x990951bau, 0x076dc419u, 0x706af48fu, 0xe963a535u, 0x9e6495a3u,
    0x0edb8832u, 0x79dcb8a4u, 0xe0d5e91eu, 0x97d2d988u, 0x09b64c2bu, 0x7eb17cbdu, 0xe7b82d07u, 0x90bf1d91u,
    0x1db71064u, 0x6ab020f2u, 0xf3b97148u, 0x84be41deu, 0x1adad47du, 0x6ddde4ebu, 0xf4d4b551u, 0x83d385c7u,
    0x136c9856u, 0x646ba8c0u, 0xfd62f97au, 0x8a65c9ecu, 0x14015c4fu, 0x63066cd9u, 0xfa0f3d63u, 0x8d080df5u,
    0x3b6e20c8u, 0x4c69105eu, 0xd56041e4u, 0xa2677172u, 0x3c03e4d1u, 0x4b04d447u, 0xd20d85fdu, 0xa50ab56bu,
    0x35b5a8fau, 0x42b2986cu, 0xdbbbc9d6u, 0xacbcf940u, 0x32d86ce3u, 0x45df5c75u, 0xdcd60dcfu, 0xabd13d59u,
    0x26d930acu, 0x51de003au, 0xc8d75180u, 0xbfd06116u, 0x21b4f4b5u, 0x56b3c423u, 0xcfba9599u, 0xb8bda50fu,
    0x2802b89eu, 0x5f058808u, 0xc60cd9b2u, 0xb10be924u, 0x2f6f7c87u, 0x58684c11u, 0xc1611dabu, 0xb6662d3du,
    0x76dc4190u, 0x01db7106u, 0x98d220bcu, 0xefd5102au, 0x71b18589u, 0x06b6b51fu, 0x9fbfe4a5u, 0xe8b8d433u,
    0x7807c9a2u, 0x0f00f934u, 0x9609a88eu, 0xe10e9818u, 0x7f6a0dbbu, 0x086d3d2du, 0x91646c97u, 0xe6635c01u,
    0x6b6b51f4u, 0x1c6c6162u, 0x856530d8u, 0xf262004eu, 0x6c0695edu, 0x1b01a57bu, 0x8208f4c1u, 0xf50fc457u,
    0x65b0d9c6u, 0x12b7e950u, 0x8bbeb8eau, 0xfcb9887cu, 0x62dd1ddfu, 0x15da2d49u, 0x8cd37cf3u, 0xfbd44c65u,
    0x4db26158u, 0x3ab551ceu, 0xa3bc0074u, 0xd4bb30e2u, 0x4adfa541u, 0x3dd895d7u, 0xa4d1c46du, 0xd3d6f4fbu,
    0x4369e96au, 0x346ed9fcu, 0xad678846u, 0xda60b8d0u, 0x44042d73u, 0x33031de5u, 0xaa0a4c5fu, 0xdd0d7cc9u,
    0x5005713cu, 0x270241aau, 0xbe0b1010u, 0xc90c2086u, 0x5768b525u, 0x206f85b3u, 0xb966d409u, 0xce61e49fu,
    0x5edef90eu, 0x29d9c998u, 0xb0d09822u, 0xc7d7a8b4u, 0x59b33d17u, 0x2eb40d81u, 0xb7bd5c3bu, 0xc0ba6cadu,
    0xedb88320u, 0x9abfb3b6u, 0x03b6e20cu, 0x74b1d29au, 0xead54739u, 0x9dd277afu, 0x04db2615u, 0x73dc1683u,
    0xe3630b12u, 0x94643b84u, 0x0d6d6a3eu, 0x7a6a5aa8u, 0xe40ecf0bu, 0x9309ff9du, 0x0a00ae27u, 0x7d079eb1u,
    0xf00f9344u, 0x8708a3d2u, 0x1e01f268u, 0x6906c2feu, 0xf762575du, 0x806567cbu, 0x196c3671u, 0x6e6b06e7u,
    0xfed41b76u, 0x89d32be0u, 0x10da7a5au, 0x67dd4accu, 0xf9b9df6fu, 0x8ebeeff9u, 0x17b7be43u, 0x60b08ed5u,
    0xd6d6a3e8u, 0xa1d1937eu, 0x38d8c2c4u, 0x4fdff252u, 0xd1bb67f1u, 0xa6bc5767u, 0x3fb506ddu, 0x48b2364bu,
    0xd80d2bdau, 0xaf0a1b4cu, 0x36034af6u, 0x41047a60u, 0xdf60efc3u, 0xa867df55u, 0x316e8eefu, 0x4669be79u,
    0xcb61b38cu, 0xbc66831au, 0x256fd2a0u, 0x5268e236u, 0xcc0c7795u, 0xbb0b4703u, 0x220216b9u, 0x5505262fu,
    0xc5ba3bbeu, 0xb2bd0b28u, 0x2bb45a92u, 0x5cb36a04u, 0xc2d7ffa7u, 0xb5d0cf31u, 0x2cd99e8bu, 0x5bdeae1du,
    0x9b64c2b0u, 0xec63f226u, 0x756aa39cu, 0x026d930au, 0x9c0906a9u, 0xeb0e363fu, 0x72076785u, 0x05005713u,
    0x95bf4a82u, 0xe2b87a14u, 0x7bb12baeu, 0x0cb61b38u, 0x92d28e9bu, 0xe5d5be0du, 0x7cdcefb7u, 0x0bdbdf21u,
    0x86d3d2d4u, 0xf1d4e242u, 0x68ddb3f8u, 0x1fda836eu, 0x81be16cdu, 0xf6b9265bu, 0x6fb077e1u, 0x18b74777u,
    0x88085ae6u, 0xff0f6a70u, 0x66063bcau, 0x11010b5cu, 0x8f659effu, 0xf862ae69u, 0x616bffd3u, 0x166ccf45u,
    0xa00ae278u, 0xd70dd2eeu, 0x4e048354u, 0x3903b3c2u, 0xa7672661u, 0xd06016f7u, 0x4969474du, 0x3e6e77dbu,
    0xaed16a4au, 0xd9d65adcu, 0x40df0b66u, 0x37d83bf0u, 0xa9bcae53u, 0xdebb9ec5u, 0x47b2cf7fu, 0x30b5ffe9u,
    0xbdbdf21cu, 0xcabac28au, 0x53b39330u, 0x24b4a3a6u, 0xbad03605u, 0xcdd70693u, 0x54de5729u, 0x23d967bfu,
    0xb3667a2eu, 0xc4614ab8u, 0x5d681b02u, 0x2a6f2b94u, 0xb40bbe37u, 0xc30c8ea1u, 0x5a05df1bu, 0x2d02ef8du,
};
// The module statics, named as the PS3 DecFIGS TOC names them (_SSC2_Random_ptr, _SSC2_Init_ptr).
// ARTIST addresses the init flag at _SSC2_Random+0x108 (CryptSSC2Init 0x82B32194). With a plain
// tentative definition the XDK C compiler places the flag first; the explicit zero initialiser
// gives ARTIST's layout.
static CryptSSC2T _SSC2_Random;
static s32 _SSC2_Init = 0;
void CryptSSC2Init(CryptSSC2T* pState, const u8* pKey, s32 iKeyLen, s32 iIterations)
{
    // Locals from DWARF (cryptssc2.c:128-130); statement order from the PS3 -O0 body (0xF0FD90).
    s32 iIdx;
    u32 uCrc;
    u8 uSwap0, uSwap1;
    if (!_SSC2_Init)
    {
        _SSC2_Init = 1;
        CryptSSC2Init(&_SSC2_Random, (const u8*)("hello world"), -1, 10);
    }
    if (iIterations < 0)
    {
        iIterations = -iIterations;
    }
    else
    {
        for (iIdx = 0; iIdx < 256; ++iIdx) pState->sbox[iIdx] = (u8)iIdx;
        pState->idx = 0;
        pState->crc = 0;
    }
    if (iKeyLen < 0)
    {
        for (iKeyLen = 0; pKey[iKeyLen] != 0; ++iKeyLen)
            ;
    }
    if (iKeyLen > 0)
    {
        uCrc = 0;
        for (iIdx = 0; iIdx < (iIterations << 8); ++iIdx)
        {
            uCrc = (uCrc >> 8) ^ _Cipher_Crc32[(uCrc & 255) ^ pKey[iIdx % iKeyLen]];
            uCrc = (uCrc >> 8) ^ _Cipher_Crc32[(uCrc & 255) ^ pState->sbox[iIdx & 255]];
            uSwap0 = pState->sbox[iIdx & 255];
            uSwap1 = pState->sbox[uCrc & 255];
            pState->sbox[iIdx & 255] = uSwap1;
            pState->sbox[uCrc & 255] = uSwap0;
        }
    }
}
void CryptSSC2Apply(CryptSSC2T* pState, u8* pData, s32 iDataLen)
{
    u8 uIndex = pState->idx;
    u32 uCrc = pState->crc;
    while (iDataLen-- > 0)
    {
        u8 a;
        u8 b;
        uCrc = _Cipher_Crc32[pState->sbox[uIndex] ^ (u8)uCrc] ^ (uCrc >> 8);
        ++uIndex;
        a = pState->sbox[uIndex];
        b = pState->sbox[(u8)uCrc];
        pState->sbox[uIndex] = b;
        pState->sbox[(u8)uCrc] = a;
        *pData++ ^= pState->sbox[(u8)(a - b)];
    }
    pState->idx = uIndex;
    pState->crc = uCrc;
}
s32 CryptSSC2StringEncrypt(char* pDst, s32 iLen, const char* pSrc, const u8* pKey, s32 iKeyLen, s32 iIterations)
{
    // Locals from DWARF (cryptssc2.c:259-262); statement order from the PS3 -O0 body (0xF10248).
    s32 iSum;
    CryptSSC2T ref;
    u8 uDat;
    u8 uEnc = 0;
    CryptSSC2Init(&ref, pKey, iKeyLen, iIterations);
    CryptSSC2Init(&_SSC2_Random, pKey, iKeyLen, -iIterations);
    CryptSSC2Init(&_SSC2_Random, (const u8*)("ru paranoid?"), -1, -1);
    for (; iLen > 1; --iLen)
    {
        if (pSrc == 0)
        {
            CryptSSC2Apply(&_SSC2_Random, &uDat, 1);
            uDat = (uDat & 63) + 32;
        }
        else if ((uDat = (u8)(*pSrc++)) == 0)
        {
            pSrc = 0;
        }
        if (uDat < 32 || uDat >= 127) uDat = 127;   // ARTIST cmplwi 0x7F; blt (0x82B3241C)
        CryptSSC2Apply(&ref, &uEnc, 1);
        iSum = (uEnc % 96) + uDat + 64;
        *pDst++ = (char)((iSum % 96) + 32);
    }
    if (iLen > 0) *pDst = 0;
    return 0;
}
