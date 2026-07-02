#include "GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.h"

#include "GameShared/GameClasses/System/Timer/CgsTimer.h"   // CgsSystem::Timer

// CgsSystem::TimerRequestInterface -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.cpp):
//   TimerRequestInterface::ApplyToTimers @0x828D7468

namespace CgsSystem
{

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
