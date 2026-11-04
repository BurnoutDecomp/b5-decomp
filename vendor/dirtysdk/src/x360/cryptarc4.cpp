#include "cryptarc4.h"
#include <cstddef>

CGS_STATIC_ASSERT(sizeof(CryptArc4T) == 0x102, "ARTIST ARC4 state extent");
CGS_STATIC_ASSERT(offsetof(CryptArc4T, walk) == 0x100, "ARTIST ARC4 walk index");
CGS_STATIC_ASSERT(offsetof(CryptArc4T, swap) == 0x101, "ARTIST ARC4 swap index");

// ARTIST 0x82B3B200
extern "C" void CryptArc4Init(CryptArc4T* pState, const u8* pKeyBuf, s32 iKeyLen, s32 iIter)
{
    pState->walk = 0;
    pState->swap = 0;
    for (u32 i = 0; i < 256; ++i)
        pState->state[i] = static_cast<u8>(i);

    if (iKeyLen <= 0)
        return;

    if (iIter < 1)
        iIter = 1;

    u8 swap = 0;
    for (; iIter > 0; --iIter)
    {
        for (u32 i = 0; i < 256; ++i)
        {
            const u8 old = pState->state[i];
            swap = static_cast<u8>(swap + old + pKeyBuf[i % static_cast<u32>(iKeyLen)]);
            pState->state[i] = pState->state[swap];
            pState->state[swap] = old;
        }
    }
}

static inline u8 CryptArc4Step(u8* pTable, u8& walk, u8& swap)
{
    walk = static_cast<u8>(walk + 1);
    const u8 oldWalk = pTable[walk];
    swap = static_cast<u8>(swap + oldWalk);
    const u8 oldSwap = pTable[swap];
    pTable[walk] = oldSwap;
    pTable[swap] = oldWalk;
    return static_cast<u8>(oldWalk + oldSwap);
}

// ARTIST 0x82B3B298
extern "C" void CryptArc4Apply(CryptArc4T* pState, u8* pBuffer, s32 iLength)
{
    u8 walk = pState->walk;
    u8 swap = pState->swap;
    for (; iLength > 0; --iLength, ++pBuffer)
    {
        const u8 index = CryptArc4Step(pState->state, walk, swap);
        *pBuffer ^= pState->state[index];
    }
    pState->walk = walk;
    pState->swap = swap;
}

// ARTIST 0x82B3B300
extern "C" void CryptArc4Advance(CryptArc4T* pState, s32 iLength)
{
    u8 walk = pState->walk;
    u8 swap = pState->swap;
    for (; iLength > 0; --iLength)
        CryptArc4Step(pState->state, walk, swap);
    pState->walk = walk;
    pState->swap = swap;
}
