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
    class SamplerState
    {
    public:
        void* GetResourceDescriptor(void* pOut);
        void* Initialize(void** ppState, const void* pParams);
    };

    void* SamplerState::GetResourceDescriptor(void* pOut)
    {
        u32* lpOut = reinterpret_cast<u32*>(pOut);
        for (int liEntry = 0; liEntry < 5; ++liEntry)
        {
            lpOut[0] = 0;
            lpOut[1] = 1;
            lpOut += 2;
        }
        u32* lpBase = reinterpret_cast<u32*>(pOut);
        lpBase[0] = 0x20;   // 64-bit store {high=0x20, low=4}
        lpBase[1] = 4;
        return pOut;
    }

    void* SamplerState::Initialize(void** ppState, const void* pParams)
    {
        u32* lpState = reinterpret_cast<u32*>(*ppState);
        uintptr_t lParams = reinterpret_cast<uintptr_t>(pParams);

        auto Word = [lParams](int liOff) -> u32 { return *reinterpret_cast<const u32*>(lParams + liOff); };
        auto Bool = [lParams](int liOff) -> u32 { return *reinterpret_cast<const u8*>(lParams + liOff) != 0 ? 1u : 0u; };
        auto BoolZ = [lParams](int liOff) -> u32 { return *reinterpret_cast<const u8*>(lParams + liOff) == 0 ? 1u : 0u; };

        lpState[8]  = Word(0);
        lpState[9]  = Word(4);
        lpState[10] = Word(8);
        lpState[11] = BoolZ(67);    // (cntlzw(x) & 0x20) != 0  -> x == 0
        lpState[12] = Word(20);
        lpState[13] = Word(52);
        lpState[14] = Word(56);
        lpState[15] = Bool(64);     // (cntlzw(x) & 0x20) == 0  -> x != 0
        lpState[16] = Bool(66);
        lpState[17] = Word(60);
        lpState[20] = Word(12);
        lpState[18] = Word(16);
        lpState[21] = Word(24);
        lpState[19] = Word(28);
        lpState[25] = Word(32);
        lpState[24] = Word(36);
        lpState[23] = Word(40);
        lpState[0]  = Word(44);
        lpState[4]  = Word(48);
        lpState[22] = Bool(65);
        lpState[26] = Bool(68);
        lpState[28] = 1;
        return lpState;
    }
}
