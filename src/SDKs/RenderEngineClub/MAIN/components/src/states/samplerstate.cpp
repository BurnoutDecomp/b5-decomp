#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::SamplerState::GetResourceDescriptor @ 0x82B63588
//   renderengine::SamplerState::Initialize            @ 0x82B62630
//
// GetResourceDescriptor builds the standard five-entry rw resource descriptor: the
// loop fills five {size=0, align=1} pairs, then the first pair is overwritten by a
// 64-bit store with {size=0x20, align=4}. Initialize unpacks a packed sampler-params
// block (a2) into the 29-word runtime sampler state (*a1), converting the packed
// boolean bytes (via the count-leading-zeros idiom: non-zero -> 1) into word flags.

namespace renderengine
{
    struct ResourceDescriptorEntry
    {
        u32 muSize;
        u32 muAlignment;
    };

    struct SamplerStateData
    {
        u32 maState[29];
    };

    struct SamplerStateParameters
    {
        u32 muState8;
        u32 muState9;
        u32 muState10;
        u32 muState20;
        u32 muState18;
        u32 muState12;
        u32 muState21;
        u32 muState19;
        u32 muState25;
        u32 muState24;
        u32 muState23;
        u32 muState0;
        u32 muState4;
        u32 muState13;
        u32 muState14;
        u32 muState17;
        u8  mbState15;
        u8  mbState22;
        u8  mbState16;
        u8  mbState11Zero;
        u8  mbState26;
    };

    class SamplerState
    {
    public:
        void* GetResourceDescriptor(void* pOut);
        void* Initialize(SamplerStateData** ppState, const SamplerStateParameters* pParams);
    };

    void* SamplerState::GetResourceDescriptor(void* pOut)
    {
        ResourceDescriptorEntry* lpOut = static_cast<ResourceDescriptorEntry*>(pOut);
        for (ResourceDescriptorEntry* lpEntry = lpOut; lpEntry != lpOut + 5; ++lpEntry)
        {
            lpEntry->muSize = 0;
            lpEntry->muAlignment = 1;
        }
        lpOut[0].muSize = 0x20;
        lpOut[0].muAlignment = 4;
        return pOut;
    }

    void* SamplerState::Initialize(SamplerStateData** ppState, const SamplerStateParameters* pParams)
    {
        SamplerStateData* lpState = *ppState;

        lpState->maState[8]  = pParams->muState8;
        lpState->maState[9]  = pParams->muState9;
        lpState->maState[10] = pParams->muState10;
        lpState->maState[11] = pParams->mbState11Zero == 0 ? 1u : 0u;
        lpState->maState[12] = pParams->muState12;
        lpState->maState[13] = pParams->muState13;
        lpState->maState[14] = pParams->muState14;
        lpState->maState[15] = pParams->mbState15 != 0 ? 1u : 0u;
        lpState->maState[16] = pParams->mbState16 != 0 ? 1u : 0u;
        lpState->maState[17] = pParams->muState17;
        lpState->maState[20] = pParams->muState20;
        lpState->maState[18] = pParams->muState18;
        lpState->maState[21] = pParams->muState21;
        lpState->maState[19] = pParams->muState19;
        lpState->maState[25] = pParams->muState25;
        lpState->maState[24] = pParams->muState24;
        lpState->maState[23] = pParams->muState23;
        lpState->maState[0]  = pParams->muState0;
        lpState->maState[4]  = pParams->muState4;
        lpState->maState[22] = pParams->mbState22 != 0 ? 1u : 0u;
        lpState->maState[26] = pParams->mbState26 != 0 ? 1u : 0u;
        lpState->maState[28] = 1;
        return lpState;
    }
}
