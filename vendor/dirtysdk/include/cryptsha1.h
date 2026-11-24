#pragma once

#include "types.hpp"

typedef struct CryptSha1T CryptSha1T;   // DirtySDK is C
struct CryptSha1T
{
    u32 uCount;
    u32 uPartialCount;
    u32 H[5];
    u8 strData[64];
};

#ifdef __cplusplus
extern "C"
{
#endif
    void CryptSha1Init(CryptSha1T* pSha1);
    void CryptSha1Update(CryptSha1T* pSha1, const u8* pInput, u32 uInputLen);
    void CryptSha1Final(CryptSha1T* pSha1, void* pBuffer, u32 uLength);
#ifdef __cplusplus
}
#endif
