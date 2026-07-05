// Per-instantiation .cpp for Set<CgsID, 512>. The generic Set<T,N> body (Insert / Find /
// Contains / GetLength + siblings) is fully inline in CgsSet.h, so this TU is just the
// explicit class instantiation (the X360 emits one out-of-line copy of each member per
// using-TU). The four members the X360 emitted out-of-line for this instance:
//   Set<CgsID,512>::GetLength @ 0x823181C8  (BrnGameState::StuntModeScoring::DealWithStunt)
//   Set<CgsID,512>::Find      @ 0x82318220  (StuntModeScoring/StuntManager/Profile stunt-element de-dupe)
//   Set<CgsID,512>::Contains  @ 0x8231A8E0  (called by Insert)
//   Set<CgsID,512>::Insert    @ 0x82325040  (StuntModeScoring::DealWithStunt, Profile::AddStuntElement)
//
// LAYOUT (X360 element addressing + count word, authoritative):
//   maElements[512]  +0x000  (512 * 8 = 4096 bytes; CgsID is one 64-bit word -- 8-byte stride,
//                             ld/stdx; Insert `ld r10,0(other); slwi r11,r11,3; stdx r10,r11,this`)
//   muLength         +0x1000 (count word read/written at *(this+0x1000), `lwz 0x1000(r31)`;
//                             KU_INVALID==0xffffffff==-1 is the used-before-Construct sentinel)
// Capacity N == 512 (Insert out-of-space gate `cmplwi muLength,0x200`); Find/Insert compare/copy
// the full 64-bit CgsID (`ld`/`cmpld`/`stdx`).
//
// Spelled unqualified to match the committed Set<T,N> / Array<T,N> container convention
// (CgsSet.h); the DecFIGS DWARF spells the type CgsContainers::Set<CgsID,512u>. CgsID is a
// typedef for u64 (BrnCommonTypes.h), a plain primitive whose built-in operator== satisfies
// the equality-based generic Find/Contains. StuntModeScoring embeds one by value as
// StuntElementSet mRecentStuntElementSet.
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "BrnCommonTypes.h"   // typedef u64 CgsID (8-byte element)

template class Set<CgsID, 512>;
