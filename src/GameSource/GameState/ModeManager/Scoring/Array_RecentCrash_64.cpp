#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the CrashModeScoring::RecentCrash,64 leaf instantiation -- the committed
// Array_/EventQueue_ explicit-instantiation pattern.
template void CgsContainers::Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::Append(
    const BrnGameState::CrashModeScoring::RecentCrash&);
template void CgsContainers::Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::Erase(u32);
template bool CgsContainers::Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::IsFull() const;
// The const lookup at ARTIST 0x8231AEB8 uses GetItem's assertions at CgsArray.h
// lines 538 and 539. GetRecentCrash calls it at 0x8232BF54 and 0x8232BF84.
template const BrnGameState::CrashModeScoring::RecentCrash&
CgsContainers::Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::GetItem(u32) const;
template BrnGameState::CrashModeScoring::RecentCrash& CgsContainers::Array<BrnGameState::CrashModeScoring::RecentCrash, 64>::operator[](u32);
