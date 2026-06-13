#ifndef BRN_SOUND_DEBUG_STATISTICS_H
#define BRN_SOUND_DEBUG_STATISTICS_H

#include "types.hpp"

namespace BrnSound
{
namespace Debug
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665xxx.
// A block of sound debug counters/histograms; the constructor clears them all.
class Statistics
{
public:
    Statistics();

private:
    struct Histogram     // 20 words
    {
        u32   mData[16];
        float mF[4];
    };

    struct HistogramP    // 21 words (one padding word per the guest stride)
    {
        float mData[16];
        float mF[4];
        u32   mPad;
    };

    u8         mPad0[16];      // guest 0..15
    u32        mHead[14];      // guest 16..68
    u32        mBigArray[128]; // guest 72..584
    float      mF584[4];       // guest 584..600
    Histogram  mBlocks[4];     // guest 600..920
    HistogramP mBlocks2[5];    // guest 920..
};
}
}

#endif
