#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"

// CgsSystem::TimerStatusInterface::Clear  @ 0x821F2220
// X360 inlines TimerStatus::Clear twice: it writes a 24-byte TimerStatus block at
// +0 (game) and another at +24 (sim). Each block: miFrameCount=0, mfBaseTimeStep=0,
// mfTimeStepMultiplier=1, mbRunning=0, mTime={SetFraction(0), miSeconds=0}.
// (The Hex-Rays "return result" is the register artifact of the inlined Time::SetFraction
// returning its dest pointer; the function is logically void.)
void
CgsSystem::TimerStatusInterface::Clear()
{
    mGameTimerStatus.Clear();
    mSimTimerStatus.Clear();
}

// CgsSystem::TimerStatusInterface::IsSimTimerFrequency50Hz  @ 0x8230E990
// X360: return (*(a1+32) * *(a1+28)) == 0.02;
//   *(a1+28) = mSimTimerStatus.mfBaseTimeStep        (sim block +24, member +4)
//   *(a1+32) = mSimTimerStatus.mfTimeStepMultiplier  (sim block +24, member +8)
// product = sim current time step; 0.02s == 1/50 => 50Hz.
bool
CgsSystem::TimerStatusInterface::IsSimTimerFrequency50Hz() const
{
    return mSimTimerStatus.GetCurrentTimeStep() == 0.02f;
}
