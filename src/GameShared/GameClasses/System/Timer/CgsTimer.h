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
        u8     mbRunning;      // [+24] (byte on X360: stb/lbz in Prepare/Update @0x828D72E0/0x828D7320 and ApplyToTimers @0x828D7468)

    public:
        bool Prepare(float lfRate);
        Timer* Reset();
        Timer* Update();
        void SetRunning(bool lbRunning) { mbRunning = lbRunning ? 1 : 0; } // mbRunning @+0x18

        // ADDITIVE GROW (TimerRequestInterface::ApplyToTimers @0x828D7468, which
        // inlines the store to +0x14): retarget the tick-scale the accumulator
        // chases.
        void SetScaleTarget(f32 lfScaleTarget) { mfScaleTarget = lfScaleTarget; }

        // ADDITIVE GROW (CgsSystem::TimerStatusInterface::StoreTimers @0x828D7518,
        // which reads all five of these by raw offset off the Timer it snapshots:
        // +0 miTicks, +4 miAccumTicks, +8 mfAccumulator, +0xC mfRate, +0x10
        // mfScaleCurrent, +0x18 mbRunning). Named accessors so the snapshot reaches
        // them BY NAME instead of reproducing the console's offset arithmetic.
        int   GetTicks() const        { return miTicks; }
        int   GetAccumTicks() const   { return miAccumTicks; }
        float GetAccumulator() const  { return mfAccumulator; }
        float GetRate() const         { return mfRate; }
        float GetScaleCurrent() const { return mfScaleCurrent; }
        bool  IsRunning() const       { return mbRunning != 0; }
    };
}

#endif // GAMESHARED_GAMECLASSES_SYSTEM_TIMER_CGSTIMER_H
