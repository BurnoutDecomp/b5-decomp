#include "GameShared/GameClasses/Containers/CgsArray.h"
// MultiplierData is now homed as a nested type of the online stunt scorer (its owning TU).
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoringOnline.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the MultiplierData leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::StuntModeScoringOnline::MultiplierData, 3>::Erase(u32);
// X360 0x82319A50 (class:BrnGameState catch-all TU, ledger "BrnGameState::S...") = the reserve-a-slot
// accessor of this same instantiation. StuntModeScoringOnline::BankMultiplier reserves the next free
// MultiplierData slot then fills it in place: the X360 body asserts Construct/Clear'd + capacity-3
// ("Array container out of space", CgsArray.h:289/290) then returns &maElements[count] and
// post-increments the count (stride 40) -- the generic Array<T,N>::AddNew body.
template BrnGameState::StuntModeScoringOnline::MultiplierData* Array<BrnGameState::StuntModeScoringOnline::MultiplierData, 3>::AddNew();
