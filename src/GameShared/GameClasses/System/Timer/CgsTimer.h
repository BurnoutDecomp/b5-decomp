#ifndef GAMESHARED_GAMECLASSES_SYSTEM_TIMER_CGSTIMER_H
#define GAMESHARED_GAMECLASSES_SYSTEM_TIMER_CGSTIMER_H

#include "types.hpp"

// CgsSystem::Timer -- a fractional tick accumulator: Update advances an
// accumulator by rate*scale each frame, bumping the integer tick count and
// carrying the fraction once it reaches 1. Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   CgsSystem::Timer::Prepare @ 0x828D72E0
//   CgsSystem::Timer::Reset   @ 0x828D73A0
//   CgsSystem::Timer::Update  @ 0x828D7320
//
// Extracted to this header (the canonical Timer home) so that callers holding a
// CgsSystem::Timer by value -- e.g. the GameTalk protocol's inactivity timer --
// can name the type. The bodies remain in CgsTimer.cpp.
namespace CgsSystem
{
    class Timer
    {
        int    miTicks;        // [+0]
        int    miAccumTicks;   // [+4]
        float  mfAccumulator;  // [+8]
        float  mfRate;         // [+12]
        float  mfScaleCurrent; // [+16]
        float  mfScaleTarget;  // [+20]
        int    mbRunning;      // [+24]

    public:
        bool Prepare(float lfRate);
        Timer* Reset();
        Timer* Update();
        void SetRunning(bool lbRunning) { mbRunning = lbRunning ? 1 : 0; } // mbRunning @+0x18
    };
}

#endif // GAMESHARED_GAMECLASSES_SYSTEM_TIMER_CGSTIMER_H
