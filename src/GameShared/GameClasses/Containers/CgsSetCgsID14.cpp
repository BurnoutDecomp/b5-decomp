// Per-instantiation .cpp for Set<CgsID, 14>. The generic Set<T,N> body (Insert / Find /
// Contains / GetLength + siblings) is fully inline in CgsSet.h, so this TU is just the
// explicit class instantiation (the X360 emits one out-of-line copy of each member per
// using-TU). The four members the X360 emitted out-of-line for this instance:
//   Set<CgsID,14>::GetLength @ 0x8235BD08  (BrnProgression::Profile::AreAllDriveThrusCompleted /
//                                           GetNumDriveThrusDiscovered / AddDriveThru,
//                                           BrnGameModule::TranslateGameActionsToGuiEvents)
//   Set<CgsID,14>::Find      @ 0x8235EBF8  (called by Contains)
//   Set<CgsID,14>::Contains  @ 0x823678B8  (BrnProgression::Profile::IsDriveThruDiscoverd, Insert)
//   Set<CgsID,14>::Insert    @ 0x82373648  (BrnProgression::Profile::AddDriveThru)
// -- i.e. this instance is BrnProgression::Profile's discovered-drive-thru set (CgsID keys).
//
// LAYOUT (X360 element addressing + count word, authoritative):
//   maElements[14]   +0x00  (14 * 8 = 112 bytes; CgsID == u64, BrnCommonTypes.h -- 8-byte stride)
//   muLength         +0x70  (count word read/written at *(this+0x70), `lwz 0x70(r31)`; 0x70 = 112)
// Capacity N == 14 (Insert out-of-space gate `cmplwi muLength,0xE`).
//
// Attesting the 8-byte CgsID element (all four bodies):
//   Find     `ld r9,0(other); ld r8,0(elem); cmpld` -- full 8-byte doubleword equality; `addi r11,8` stride
//   Insert   `lwz muLength; ld r10,0(other); slwi r11,3; stdx r10,r11,this` -- 8-byte CgsID copy at maElements[muLength]
//   Contains `Find(x) != -1` via `li -1; subf; cntlzw; extrwi 1,26; xori 1` == (Find != KU_INVALID)
//   GetLength returns `lwz r3,0x70(r31)` == muLength
//
// The X360 members carry the CgsSet.h used-before-Construct / out-of-space asserts (lines
// 179/183/227/305/332) -- supplied by the committed generic CGS_ASSERT body. The
// "Set used before Construct/Clear was called" / "Set container out of space" dynamic
// messages are the static strings in the generic body.
//
// Spelled unqualified to match the committed Set<T,N> / Array<T,N> container convention
// (CgsSet.h); the DecFIGS DWARF spells the type CgsContainers::Set<CgsID,14u>. CgsID is a
// plain u64 whose built-in operator== satisfies the equality-based generic Find/Contains.
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "BrnCommonTypes.h"   // typedef u64 CgsID (8-byte element)

template class Set<CgsID, 14>;
