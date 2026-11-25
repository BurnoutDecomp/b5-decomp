#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameShared/GameClasses/System/Timer/CgsTimer.h"   // CgsSystem::Timer (StoreTimers' source)

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

// TimerStatusInterface::operator= and TimerStatus::operator= are header-inline (DWARF h:98 / h:77,
// no CgsTimerStatusInterface.cpp definition): see CgsTimerStatusInterface.h.

// CgsSystem::TimerStatusInterface::StoreTimers  @ 0x828D7518
// Snapshot a live Timer pair into the two TimerStatus blocks. The console body is one
// straight run of loads/stores per timer (game block at this+0, sim block at this+0x18),
// with CgsSystem::Time::SetFraction called out of line for the fraction lane:
//
//   *(this+0)    = *(timer+0)     miFrameCount         <- Timer::miTicks
//   *(this+4)    = *(timer+0xC)   mfBaseTimeStep       <- Timer::mfRate
//   *(this+8)    = *(timer+0x10)  mfTimeStepMultiplier <- Timer::mfScaleCurrent
//   *(this+0xC)  = *(timer+0x18)  mbRunning            <- Timer::mbRunning
//   Time::SetFraction(this+0x10, *(timer+8))           <- Timer::mfAccumulator
//   *(this+0x10) = *(timer+4)     mTime.miSeconds      <- Timer::miAccumTicks
//
// (the seconds store lands AFTER SetFraction because Time's two lanes share the +0x10
// slot -- SetFraction writes +0x14 and the raw stw writes +0x10.)
//
// This is the function that turns the ticking Timer pair into the published frame
// timestep every behaviour reads: TimerStatus::GetCurrentTimeStep() ==
// mfBaseTimeStep * mfTimeStepMultiplier == Timer::mfRate * Timer::mfScaleCurrent.
void
CgsSystem::TimerStatusInterface::StoreTimers(Timer* lpGameTimer, Timer* lpSimTimer)
{
    mGameTimerStatus.miFrameCount         = lpGameTimer->GetTicks();
    mGameTimerStatus.mfBaseTimeStep       = lpGameTimer->GetRate();
    mGameTimerStatus.mfTimeStepMultiplier = lpGameTimer->GetScaleCurrent();
    mGameTimerStatus.mbRunning            = lpGameTimer->IsRunning();
    mGameTimerStatus.mTime.SetFraction(lpGameTimer->GetAccumulator());
    mGameTimerStatus.mTime.SetSeconds(lpGameTimer->GetAccumTicks());

    mSimTimerStatus.miFrameCount         = lpSimTimer->GetTicks();
    mSimTimerStatus.mfBaseTimeStep       = lpSimTimer->GetRate();
    mSimTimerStatus.mfTimeStepMultiplier = lpSimTimer->GetScaleCurrent();
    mSimTimerStatus.mbRunning            = lpSimTimer->IsRunning();
    mSimTimerStatus.mTime.SetFraction(lpSimTimer->GetAccumulator());
    mSimTimerStatus.mTime.SetSeconds(lpSimTimer->GetAccumTicks());
}
