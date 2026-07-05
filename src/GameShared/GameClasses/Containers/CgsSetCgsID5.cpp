// Per-instantiation .cpp for Set<CgsID, 5>. The generic Set<T,N> body (Insert / Erase /
// Find / Contains / GetLength + siblings) is fully inline in CgsSet.h, so this TU is just the
// explicit class instantiation (the X360 emits one out-of-line copy of each member per
// using-TU). The four members the X360 emitted out-of-line for this instance:
//   Set<CgsID,5>::Contains  @ 0x823677B8  (Profile::IsDriveThruDiscoverd, ::Insert)
//   Set<CgsID,5>::Find      @ 0x8235EAC8  (Contains)
//   Set<CgsID,5>::GetLength @ 0x8235BC58  (Profile::AreAllDriveThrusCompleted /
//                                          GetNumDriveThrusDiscovered / AddDriveThru,
//                                          BrnGameModule::TranslateGameActionsToGuiEvents)
//   Set<CgsID,5>::Insert    @ 0x823734F8  (Profile::AddDriveThru)
//
// LAYOUT (X360 element addressing + count word, authoritative):
//   maElements[5]   +0x00  (5 * 8 = 40 bytes; CgsID is one 8-byte identifier -- 8-byte
//                           stride: Find `ld`/`cmpld`/`addi r11,8`, Insert `slwi 3`/`stdx`)
//   muLength        +0x28  (the count word read/written at *(this+0x28), `lwz 0x28(r31)`;
//                           +0x28 == 40, right after the packed 5-element buffer)
// Capacity N == 5 (Insert out-of-space gate `cmplwi muLength,5`).
//
// The X360 members carry the CgsSet.h used-before-Construct (lines 332/305/227/179) and
// out-of-space (line 183) asserts -- supplied by the committed generic CGS_ASSERT body. The
// "Set used before Construct/Clear was called" / "Set container out of space" dynamic
// messages are the static strings in the generic body (no trailing newline in rodata).
//
// Spelled unqualified to match the committed Set<T,N> / Array<T,N> container convention
// (CgsSet.h); the DecFIGS DWARF spells the type CgsContainers::Set<CgsID,5u>. CgsID is
// a typedef for u64 (BrnCommonTypes.h) -- IDA renders it __int64 -- whose built-in operator==
// satisfies the equality-based generic Find/Contains the explicit instantiation forces.
// BrnProfile embeds two of these by value: mJunkYardsDriveThruSet and mPaintShopsDriveThruSet.
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "BrnCommonTypes.h"   // CgsID (typedef u64)

template class Set<CgsID, 5>;
