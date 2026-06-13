#include "BrnSoundDebugStatistics.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSound::Debug::Statistics::Statistics
//
// Zero-initialises every counter block. The compiler unrolled the leading
// scalar clears and strength-reduced the per-block clears into pointer loops;
// the loops are re-rolled here.

namespace BrnSound
{
namespace Debug
{
Statistics::Statistics()
{
    for (int i = 0; i < 14; ++i)
    {
        mHead[i] = 0;
    }

    for (int i = 0; i < 128; ++i)
    {
        mBigArray[i] = 0;
    }
    for (int i = 0; i < 4; ++i)
    {
        mF584[i] = 0.0f;
    }

    for (int b = 0; b < 4; ++b)
    {
        for (int i = 0; i < 16; ++i)
        {
            mBlocks[b].mData[i] = 0;
        }
        for (int i = 0; i < 4; ++i)
        {
            mBlocks[b].mF[i] = 0.0f;
        }
    }

    for (int b = 0; b < 5; ++b)
    {
        for (int i = 0; i < 16; ++i)
        {
            mBlocks2[b].mData[i] = 0.0f;
        }
        for (int i = 0; i < 4; ++i)
        {
            mBlocks2[b].mF[i] = 0.0f;
        }
    }
}
}
}
