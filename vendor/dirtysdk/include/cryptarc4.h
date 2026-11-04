#pragma once

#include "types.hpp"

struct CryptArc4T
{
    u8 state[256];
    u8 walk;
    u8 swap;
};

extern "C"
{
    void CryptArc4Init(CryptArc4T* pState, const u8* pKeyBuf, s32 iKeyLen, s32 iIter);
    void CryptArc4Apply(CryptArc4T* pState, u8* pBuffer, s32 iLength);
    void CryptArc4Advance(CryptArc4T* pState, s32 iLength);
}
