// Per-instantiation .cpp for the u16/extent-160 fixed-capacity containers the BrnWorld::CrashModule +
// BrnTraffic::TrafficEntityModule crash-traffic bookkeeping drives. The IDA-truncated ledger key
// "short,160>" is the single using-TU that emits out-of-line copies of the Array<u16,160> storage
// members AND the Set<u16,160> uniqueness-layer members it uses, both element=unsigned short(u16), N=160.
//
// Two distinct generics resolve here (distinguished by the assert string + source line each member
// carries in the X360 rodata) -- exactly mirroring the committed CgsArrayShort400.cpp sibling
// (Array<u16,400> + Stack<u16,400> conflated under the truncated key "short,400>"):
//
//   Array<u16,160>  (CgsArray.h inline body -- the storage layer):
//     Erase               @ 0x8270D010                                        CgsArray.h:380/381
//     EraseInstancesOf    @ 0x8271AFE8                                        CgsArray.h:426
//     FindFirstInstanceOf @ 0x8270D0C0                                        CgsArray.h:480
//     Contains            @ 0x8271B090                                        CgsArray.h:506
//     GetItem             @ 0x8270D158                                        CgsArray.h:556/557
//     Append              @ 0x8270B300                                        CgsArray.h:225/226
//   Set<u16,160>  (CgsSet.h inline body -- the uniqueness layer):
//     Find                @ 0x827BA7E0                                        CgsSet.h:305
//     Insert              @ 0x827C7B40                                        CgsSet.h:179/183
//
// LAYOUT (X360 element addressing + count word, authoritative -- identical for both generics):
//   maElements[160]  +0x00   (160 * 2 = 320 bytes; u16 element -- 2-byte stride, lhz/sthx/`2*idx+base`)
//   miCount/muLength +0x140  (=320; the count word read/written at *(this+0x140), `lwz 0x140`)
// Capacity N == 160 (Append/Insert out-of-space gate `cmplwi count,0xA0`; 0xA0 = 160).
//
// The ARRAY-string members (GetItem etc.) carry CgsArray.h asserts and address elements as the bare
// storage buffer, so they are the Array<u16,160> generic -- forced by `template class Array<u16,160>;`.
// The SET-string members (Insert/Find) carry CgsSet.h asserts + the not-already-present dedupe, so
// they are the Set<u16,160> uniqueness layer -- forced as explicit member instantiations so the Set
// generic's own GetItem ("Set index out of bounds") is NOT emitted; the Array<u16,160>::GetItem the
// X360 rodata actually attests is the one supplied here.
//
// Both bodies are header-inline (CgsArray.h / CgsSet.h); this TU only forces their emission. Spelled
// UNQUALIFIED (global scope) to match the committed CgsArray.h / CgsSet.h -- both `Array<T,N>` and
// `Set<T,N>` live at GLOBAL scope in this tree (there is NO `namespace CgsContainers` around them,
// unlike CgsStack.h). The DecFIGS DWARF spells them CgsContainers::Array<uint16_t,160u> /
// CgsContainers::Set<uint16_t,160u>, but the committed containers are global -- the CgsSetUnsignedShort15.cpp
// sibling instantiates `Set<u16,15>` unqualified. u16 is a plain primitive whose built-in operator==
// satisfies the equality-based generic Find/Contains.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"

// Array<u16,160> -- the storage layer. `template class` forces the full set of Array-string members
// the X360 emitted out-of-line for this instance (Append / Erase / EraseInstancesOf /
// FindFirstInstanceOf / Contains / GetItem), all inline in CgsArray.h.
template class Array<u16, 160>;

// Set<u16,160> -- the uniqueness layer. Only Insert + Find are emitted out-of-line for this instance
// (the Set-string members); force just those rather than the whole class, so the Set generic's own
// GetItem ("Set index out of bounds") is NOT emitted -- the Array<u16,160>::GetItem above is the one
// the X360 rodata attests. Spelled GLOBAL scope (::Set), matching committed CgsSet.h.
template void Set<u16, 160>::Insert(const u16&);
template u32  Set<u16, 160>::Find(const u16&) const;
