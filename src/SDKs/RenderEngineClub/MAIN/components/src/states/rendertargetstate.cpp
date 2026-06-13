#include "types.hpp"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::RenderTargetState::GetResourceDescriptor @ 0x82B63678
//   renderengine::RenderTargetState::Initialize            @ 0x82B62748
//
// GetResourceDescriptor builds the standard five-entry rw descriptor, then overwrites
// the first pair with {size=0x18, align=4}. Initialize copies the four target words
// from the params block into state[1..4], stores the format word (params[4]) at
// state[0], and sets the trailing flag state[5]=1. (The X360 zeroes the six-word
// state first; every word is then overwritten.)

namespace renderengine
{
    class RenderTargetState
    {
    public:
        void* GetResourceDescriptor(void* pOut);
        void* Initialize(void** ppState, const u32* pParams);
    };

    void* RenderTargetState::GetResourceDescriptor(void* pOut)
    {
        u32* lpOut = reinterpret_cast<u32*>(pOut);
        for (int liEntry = 0; liEntry < 5; ++liEntry)
        {
            lpOut[0] = 0;
            lpOut[1] = 1;
            lpOut += 2;
        }
        u32* lpBase = reinterpret_cast<u32*>(pOut);
        lpBase[0] = 0x18;   // 64-bit store {high=0x18, low=4}
        lpBase[1] = 4;
        return pOut;
    }

    void* RenderTargetState::Initialize(void** ppState, const u32* pParams)
    {
        u32* lpState = reinterpret_cast<u32*>(*ppState);

        memset(lpState, 0, 24);
        for (int liWord = 0; liWord < 4; ++liWord)
            lpState[1 + liWord] = pParams[liWord];

        lpState[0] = pParams[4];
        lpState[5] = 1;
        return lpState;
    }
}
