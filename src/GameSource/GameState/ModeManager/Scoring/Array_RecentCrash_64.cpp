#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the CrashModeScoring::RecentCrash,64 leaf instantiation -- the committed
// Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::Append(
    const BrnGameState::CrashModeScoring::RecentCrash&);
template void Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::Erase(u32);
template bool Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::IsFull() const;
