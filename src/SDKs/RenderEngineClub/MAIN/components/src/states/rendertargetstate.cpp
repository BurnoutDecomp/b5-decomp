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
    struct ResourceDescriptorEntry
    {
        u32 muSize;
        u32 muAlignment;
    };

    struct RenderTargetStateData
    {
        u32 muFormat;
        u32 maTarget[4];
        u32 muInitialized;
    };

    class RenderTargetState
    {
    public:
        static ResourceDescriptorEntry* GetResourceDescriptor(ResourceDescriptorEntry* lpDescriptor);
        static RenderTargetStateData* Initialize(RenderTargetStateData** lppState, const u32* lpaParams);
    };

    ResourceDescriptorEntry* RenderTargetState::GetResourceDescriptor(ResourceDescriptorEntry* lpDescriptor)
    {
        for (ResourceDescriptorEntry* lpEntry = lpDescriptor; lpEntry != lpDescriptor + 5; ++lpEntry)
        {
            lpEntry->muSize = 0;
            lpEntry->muAlignment = 1;
        }

        lpDescriptor[0].muSize = 0x18;
        lpDescriptor[0].muAlignment = 4;
        return lpDescriptor;
    }

    RenderTargetStateData* RenderTargetState::Initialize(RenderTargetStateData** lppState, const u32* lpaParams)
    {
        RenderTargetStateData* lpState = *lppState;

        memset(lpState, 0, 24);
        for (int liWord = 0; liWord < 4; ++liWord)
            lpState->maTarget[liWord] = lpaParams[liWord];

        lpState->muFormat = lpaParams[4];
        lpState->muInitialized = 1;
        return lpState;
    }
}
