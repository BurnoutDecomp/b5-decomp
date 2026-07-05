#include "GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/System/Timer/CgsTimer.h"   // CgsSystem::Timer

// CgsSystem::TimerRequests / TimerRequestInterface -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (DWARF primary file
// GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.cpp):
//   TimerRequests::SetTimestepMultiplier @0x821F4588
//   TimerRequests::GetMultiplier         @0x828D73C0
//   TimerRequests::Append                @0x823A5FB0
//   TimerRequestInterface::ApplyToTimers @0x828D7468

namespace CgsSystem
{

// @ 0x821F4588 -- record a slowmo multiplier for this frame and raise its request
// bit. The guard asserts we haven't already queued a change this frame (bit2 set).
void TimerRequests::SetTimestepMultiplier(f32 lfMultiplier)
{
    CGS_ASSERT(!IsMultiplierRequested(),
               "Attempt to change slowmo multiple times - we arn't allowing this cos it's dodgy\n");

    mfMultiplier = lfMultiplier;
    muFlags |= KU_FLAG_MULTIPLIER;
}

// @ 0x828D73C0 -- read back the queued multiplier; asserts one was actually set.
f32 TimerRequests::GetMultiplier() const
{
    CGS_ASSERT(IsMultiplierRequested(), "Attempt to get multiplier when it hasn't been set\n");

    return mfMultiplier;
}

// @ 0x823A5FB0 -- fold another frame's request into this one. Two guards reject
// conflicting combinations (start/stop from both, or a second slowmo); the
// OR-merge means a pending flag on either side survives, and other's multiplier
// overwrites ours (the multiplier store precedes the flag OR, matching the asm).
void TimerRequests::Append(const TimerRequests& lrOther)
{
    CGS_ASSERT(
        !((IsStartRequested() || IsStopRequested()) &&
          (lrOther.IsStartRequested() || lrOther.IsStopRequested())),
        "Attempting to combine conflicting interfaces - only 1 start/stop request can be made per frame\n");

    CGS_ASSERT(
        !(IsMultiplierRequested() && lrOther.IsMultiplierRequested()),
        "Attempting to combine conflicting interfaces - only 1 slowmo request can be made per frame\n");

    mfMultiplier = lrOther.mfMultiplier;
    muFlags |= lrOther.muFlags;
}

// @ 0x828D7468 -- start beats stop only by write order (a request with both bits
// set leaves the timer STOPPED, exactly as the X360's sequential stores do); the
// multiplier lands on the timer's chase target.
void TimerRequestInterface::ApplyToTimers(Timer* lpGameTimer, Timer* lpSimTimer) const
{
    if (mGameTimer.IsStartRequested())
        lpGameTimer->SetRunning(true);
    if (mGameTimer.IsStopRequested())
        lpGameTimer->SetRunning(false);
    if (mGameTimer.IsMultiplierRequested())
        lpGameTimer->SetScaleTarget(mGameTimer.GetMultiplier());

    if (mSimTimer.IsStartRequested())
        lpSimTimer->SetRunning(true);
    if (mSimTimer.IsStopRequested())
        lpSimTimer->SetRunning(false);
    if (mSimTimer.IsMultiplierRequested())
        lpSimTimer->SetScaleTarget(mSimTimer.GetMultiplier());
}

}
