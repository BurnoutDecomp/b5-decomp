#pragma once

#include "types.hpp"

struct CryptSha1T
{
    u32 uCount;
    u32 uPartialCount;
    u32 H[5];
    u8 strData[64];
};

extern "C"
{
    void CryptSha1Init(CryptSha1T* pSha1);
    void CryptSha1Update(CryptSha1T* pSha1, const u8* pInput, u32 uInputLen);
    void CryptSha1Final(CryptSha1T* pSha1, void* pBuffer, u32 uLength);
}
