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

// ============================================================================================
// TimerRequestInterface::GetGameTimerRequests / GetSimTimerRequests -- DWARF h:100-111
// ============================================================================================
// [stuntrace waveB CLOSURE round, 2026-08-26] Bodied. GetSimTimerRequests was DECLARE-ONLY and
// was one of the five unresolved externals the cross-seam audit found on the wave's mount path
// (caller: BrnModeManager_Finish.cpp:460, FinishCurrentMode's mode-2 SetTimestepMultiplier leg).
//
// X360: header-inlined at every call site, so there is no standalone export -- but the inlining
// states the answer outright. ModeManager::FinishCurrentMode @0x8234B978, jumptable cases 2,16:
//
//   0x8234BB74  mr   r3, r25                                   ; the OutputBuffer
//   0x8234BB78  bl   GameStateModuleIO::OutputBuffer::GetTimerRequest...   ; -> TimerRequestInterface*
//   0x8234BB7C  lis  r11, flt_82001C98@ha                       ; image.bin @0x1C98 == 1.0f
//   0x8234BB80  addi r3, r3, 8                                  ; <- GetSimTimerRequests()
//   0x8234BB84  lfs  f1, flt_82001C98@l(r11)
//   0x8234BB88  bl   CgsSystem::TimerRequests::SetTimestepMultiplier
//
// `addi r3, r3, 8` IS the whole body: mSimTimer sits at +0x08 (DWARF h:121, and the two
// TimerRequests are 8 bytes each -- muFlags +0x00, mfMultiplier +0x04), so the accessor is
// &mSimTimer and nothing else. The game-timer pair is its +0x00 twin (DWARF h:120); both are
// bodied together rather than one of the four, because a half-bodied symmetric accessor set is
// precisely the asymmetry that hid GameModeParams::AddStartLocation's link hole this same wave.
// Reached by NAME here -- no offset arithmetic survives into the source.
// ============================================================================================
const TimerRequests* TimerRequestInterface::GetGameTimerRequests() const
{
    return &mGameTimer;
}

TimerRequests* TimerRequestInterface::GetGameTimerRequests()
{
    return &mGameTimer;
}

const TimerRequests* TimerRequestInterface::GetSimTimerRequests() const
{
    return &mSimTimer;
}

TimerRequests* TimerRequestInterface::GetSimTimerRequests()
{
    return &mSimTimer;
}

}
