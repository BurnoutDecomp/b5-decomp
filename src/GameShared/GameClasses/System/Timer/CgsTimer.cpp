#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTimer.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsSystem::Timer::Prepare @ 0x828D72E0
//   CgsSystem::Timer::Reset   @ 0x828D7320  (Update @ 0x828D7320 / Reset @ 0x828D73A0)
//   CgsSystem::Timer::Update  @ 0x828D7320
//
// A fractional tick accumulator: Update advances an accumulator by rate*scale each
// frame, bumping the integer tick count and carrying the fraction once it reaches 1.
// The class is declared in CgsTimer.h (the canonical Timer home); the bodies live here.

namespace CgsSystem
{
    bool Timer::Prepare(float lfRate)
    {
        mfAccumulator  = 0.0f;
        mfRate         = lfRate;
        miTicks        = 0;
        miAccumTicks   = 0;
        mbRunning      = 0;
        mfScaleCurrent = 1.0f;
        mfScaleTarget  = 1.0f;
        return true;
    }

    Timer* Timer::Reset()
    {
        mfAccumulator = 0.0f;
        miTicks       = 0;
        miAccumTicks  = 0;
        return this;
    }

    Timer* Timer::Update()
    {
        if (mbRunning)
        {
            mfScaleCurrent = mfScaleTarget;

            float lfNext = mfRate * mfScaleTarget + mfAccumulator;
            mfAccumulator = lfNext;
            ++miTicks;

            if (lfNext >= 1.0f)
            {
                int liWhole = static_cast<int>(lfNext);
                miAccumTicks += liWhole;
                mfAccumulator = lfNext - static_cast<float>(liWhole);
            }
        }
        return this;
    }
}
