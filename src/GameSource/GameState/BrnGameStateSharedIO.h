#pragma once

#include "BrnCommonTypes.h"

namespace BrnGameState
{
    namespace GameStateModuleIO
    {
        // Recovered from BrnGameStateSharedIO.h / CgsBitArray.h (DecFIGS DWARF).
        // FastBitArray<2000> stores 2000 bits in 63 32-bit words. The X360 queue
        // aligns its inline buffer to 16, so the element is 16-byte aligned.
        struct alignas(16) CompletedFburnChallengesData
        {
            s32 mNetworkPlayerID;
            u32 mCompletedFreeburnChallenges[63]; // FastBitArray<2000>
        };
    }
}
