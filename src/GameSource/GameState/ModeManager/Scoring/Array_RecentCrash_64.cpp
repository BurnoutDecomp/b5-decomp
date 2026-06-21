#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the CrashModeScoring::RecentCrash,64 leaf instantiation -- the committed
// Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::Append(
    const BrnGameState::CrashModeScoring::RecentCrash&);
template void Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::Erase(u32);
template bool Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::IsFull() const;
// X360 0x8231AEB8 (class:BrnGameState catch-all TU) = the bounds-checked indexed accessor of this
// same instantiation (the truncated ledger name "BrnGameState::Cras..."). The X360 CrashModeScoring::
// GetRecentCra() walks the live set comparing GetRecentCra's u16 traffic-car index against each
// element via this operator[] (stride 8) -- the non-const overload (it returns a mutable element).
template BrnGameState::CrashModeScoring::RecentCrash& Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::operator[](u32);
