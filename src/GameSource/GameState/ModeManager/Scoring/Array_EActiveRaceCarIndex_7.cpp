#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/BurnoutConstants.h"  // EActiveRaceCarIndex element home (global enum, unqualified per DWARF)

// Explicit instantiation of the generic Array<T,N> container method (inline in CgsArray.h)
// for the Array<EActiveRaceCarIndex,7> leaf instantiation -- the committed
// Array_/EventQueue_ explicit-instantiation pattern.
//   X360 0x8231B0D0 = Array<EActiveRaceCarIndex,7u>::FindFirstInstanceOf
//     (DWARF spells the return uint32_t with KU_INVALID==0xffffffff; the X360 build returns the
//      signed -1 literal -- same bit pattern -- so the committed s32 return is faithful.)
// Element type is the GLOBAL EActiveRaceCarIndex enum (BurnoutConstants.h); the DWARF key is the
// unqualified Array<EActiveRaceCarIndex,7u>, and X360 is authoritative over the PS3 DWARF's
// BrnGameState-scoped same-valued duplicate.
// Sole caller in the boot trace: BrnGameState::ScoringSystem::UpdateRacePositions.
template s32 Array<EActiveRaceCarIndex, 7>::FindFirstInstanceOf(const EActiveRaceCarIndex&) const;
