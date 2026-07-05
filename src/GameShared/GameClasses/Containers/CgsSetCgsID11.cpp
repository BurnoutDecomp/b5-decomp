// Per-instantiation .cpp for Set<CgsID, 11>. The generic Set<T,N> body (Insert /
// Erase / Find / Contains / GetLength + siblings) is fully inline in CgsSet.h, so
// this TU is just the explicit class instantiation (the X360 emits one out-of-line
// copy of each used member per using-TU). The four members the X360 emitted
// out-of-line for this instance:
//   Set<CgsID,11>::Contains  @ 0x82367838  (Profile::IsDriveThruDiscoverd, ::Insert,
//                                           ProgressionManager::UnlockToProgressionRank)
//   Set<CgsID,11>::Find      @ 0x8235EB60  (Contains)
//   Set<CgsID,11>::GetLength @ 0x8235BCB0  (Profile::AreAllDriveThrusCompleted /
//                                           GetNumDriveThrusDiscovered / AddDriveThru,
//                                           ProgressionManager::UnlockToProgressionRank,
//                                           BrnGameModule::TranslateGameActionsToGuiEvents)
//   Set<CgsID,11>::Insert    @ 0x823735A0  (Profile::AddDriveThru,
//                                           ProgressionManager::UnlockToProgressionRank)
//
// LAYOUT (X360 element addressing + count word, authoritative):
//   maElements[11]   +0x00  (11 * 8 = 88 bytes; CgsID == u64 -- 8-byte stride: ld/std,
//                            `slwi r11,muLength,3` element addressing, `stdx`/`ld`)
//   muLength         +0x58  (count word read/written at *(this+0x58) == 88 == 11*8)
// Capacity N == 11 (Insert out-of-space gate `cmplwi muLength,0xB`).
//
// The X360 members carry the CgsSet.h used-before-Construct (lines 332/305/227/179) and
// out-of-space (line 183) asserts -- supplied by the committed generic CGS_ASSERT bodies.
// The 'Set used before Construct/Clear was called' / 'Set container out of space' dynamic
// messages are the static strings in the generic body.
//
// Spelled unqualified to match the committed Set<T,N> / Array<T,N> container convention
// (CgsSet.h); the DecFIGS DWARF spells the type CgsContainers::Set<CgsID,11u>. BrnProfile
// embeds two of these by value (mBodyShopsDriveThruSet, mCarParksDriveThruSet). CgsID is
// `typedef u64 CgsID` (BrnCommonTypes.h) whose built-in operator== satisfies the
// equality-based generic Find/Contains the explicit instantiation forces.
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "GameShared/GameClasses/Core/CgsID.h"   // typedef u64 CgsID (8-byte element)

template class Set<CgsID, 11>;
