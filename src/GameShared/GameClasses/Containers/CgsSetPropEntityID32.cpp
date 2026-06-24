// Per-instantiation .cpp for Set<BrnWorld::PropEntityID, 32>. The generic Set<T,N> body
// (Insert / Erase / Find / Contains / IsFull + siblings) is fully inline in CgsSet.h, so this
// TU is just the explicit class instantiation (the X360 emits one out-of-line copy of each
// member per using-TU). The five methods the X360 emitted out-of-line for this instance:
//   Set<PropEntityID,32>::Contains @ 0x822E52C8  (PropEntityModule::ProcessContacts, ::Insert)
//   Set<PropEntityID,32>::Erase    @ 0x822E5220  (PropCellManager::DeactivateCell)
//   Set<PropEntityID,32>::Find     @ 0x822CACE0  (Erase / Contains / PropCellManager::DeactivateCell)
//   Set<PropEntityID,32>::Insert   @ 0x822F21E8  (PropEntityModule::ProcessContacts)
//   Set<PropEntityID,32>::IsFull   @ 0x822CA328  (PropEntityModule::ProcessContacts)
//
// LAYOUT (X360 element addressing + count word, authoritative):
//   maElements[32]   +0x00  (32 * 4 = 128 bytes; PropEntityID is one 32-bit EntityId word)
//   muLength         +0x80  (the count word read/written at *(this+0x80), `lwz 0x80(r31)`)
// matching the slwi-by-2 element addressing (4-byte stride) and the single-dword element copy:
//   Insert  `lwz r10,0(other); stwx r10,r11,this`  -- one-word PropEntityID copy at maElements[muLength]
//   Erase   `lwz r11,-4(... muLength*4 ...); stwx r11,idx*4,this` -- maElements[idx] = maElements[muLength-1]
//   IsFull  `addi r11,-0x20; cntlzw; extrwi 1,26`   -- muLength == 32
//   Find    linear scan comparing the full 32-bit word; per-element + per-key owner tripwire
//           ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278) is the
//           PropEntityID owner-byte assert; the comparison itself is muValue == muValue.
//
// The X360 members carry the CgsSet.h used-before-Construct / out-of-space / present asserts
// (lines 332/203/206/305/179/183/227) -- supplied by the committed generic CGS_ASSERT body.
// The "Set used before Construct/Clear was called" / "Set container out of space" /
// "luIndex != KU_INVALID" dynamic messages are the static strings in the generic body.
//
// Spelled unqualified to match the committed Set<T,N> / Array<T,N> container convention
// (CgsSet.h); the DecFIGS DWARF spells the type CgsContainers::Set<BrnWorld::PropEntityID,32u>.
// operator== on PropEntityID (additive grow in BrnPropEntityID.h) satisfies the equality-based
// generic Find/Contains the explicit instantiation forces; the owner-byte tripwire the X360
// runs inside the comparison is benign assert machinery and does not change the comparison.
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"

template class Set<BrnWorld::PropEntityID, 32>;
