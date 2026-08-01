// ============================================================================
// GameShared/GameClasses/Containers/CgsDictionary.cpp
//
// CgsContainers::DictionaryBase -- the untyped relocation pass every
// DictionaryResourceType<T>::FixUp / FixDown runs before (resp. after) the
// per-entry T::FixUp / T::FixDown loop.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   DictionaryBase::FixUp   @ 0x828157F8
//   DictionaryBase::FixDown @ 0x82815848
//
// ⚠️ FixUp is ABSENT FROM THE .ida-exports JSON SET -- it sits in the gap between
// BaseLinkedList::InternalRemoveNode (0x82815708) and FixDown (0x82815848), which the
// project's export set does not cover. Its address is named by the FixUp wrapper's own
// xref list (DictionaryResourceType<ICE::ICETakeData>::FixUp @0x82665FF8 branches to
// 0x828157F8) and the body below was recovered from the ARTIST IDA database with
// headless IDA 9.3, instruction for instruction:
//
//   0x828157F8  lwz  r10, 8(r3)      ; mpaIndex
//   0x828157FC  li   r11, 0
//   0x82815800  lwz  r9, 0(r3)       ; miNumEntries
//   0x82815804  add  r10, r3, r10    ; mpaIndex += this
//   0x82815808  cmpwi cr6, r9, 0
//   0x8281580C  stw  r10, 8(r3)
//   0x82815810  blelr cr6            ; no entries -> done (index already rebased)
//   0x82815814  li   r10, 0
//   0x82815818  lwz  r9, 8(r3)       ; (re-read each iteration, as the asm does)
//   0x8281581C  addi r11, r11, 1
//   0x82815820  add  r9, r10, r9
//   0x82815824  addi r10, r10, 0x10  ; += sizeof(DictEntry)
//   0x82815828  lwz  r8, 8(r9)       ; entry->mpData
//   0x8281582C  add  r8, r8, r3      ; += this
//   0x82815830  stw  r8, 8(r9)
//   0x82815834  lwz  r9, 0(r3)
//   0x82815838  cmpw cr6, r11, r9
//   0x8281583C  blt  cr6, loc_82815818
//
// Note the asymmetry with FixDown, which is real and preserved: FixUp rebases the index
// FIRST and has NO per-entry assert; FixDown asserts `(int32_t)lpEntry->mpData >
// (int32_t)this` on every entry and un-rebases the index LAST.
//
// [x64] Both stored slots are RESOURCE-RELATIVE OFFSETS on disk, not addresses, on both
// platforms -- so this is an offset<->pointer conversion, not a pointer widening. The
// console's `0x10` entry stride is expressed as array indexing (sizeof(DictEntry) is 24
// on the host); nothing here transcribes a console byte size. Verified against the
// shipped PC resource that tools/assets/bundles/ice_transcode.py emits for
// CAMERAS.BUNDLE's ICETakeDictionary: miNumEntries 549, mpaIndex 0x10 (== the host
// sizeof(DictionaryBase)), entry[0].mpData 0x3388, all relative to the block base.
// ============================================================================

#include "GameShared/GameClasses/Containers/CgsDictionary.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsContainers
{

// @0x828157F8 -- relocate-on-load: turn the stored offsets into real pointers.
void DictionaryBase::FixUp()
{
    u8* const lpBase = reinterpret_cast<u8*>(this);

    mpaIndex = reinterpret_cast<DictEntry*>(
        lpBase + reinterpret_cast<uintptr_t>(mpaIndex));

    for ( s32 liEntry = 0; liEntry < miNumEntries; ++liEntry )
    {
        DictEntry& lrEntry = mpaIndex[ liEntry ];
        lrEntry.mpData = reinterpret_cast<char*>(
            lpBase + reinterpret_cast<uintptr_t>(lrEntry.mpData));
    }
}

// @0x82815848 -- relocate-for-save: the exact inverse, entries first then the index.
// The per-entry guard is the console's own (CgsDictionary.cpp:91): an entry's payload
// must live ABOVE the dictionary head, otherwise the subtraction underflows.
void DictionaryBase::FixDown()
{
    u8* const lpBase = reinterpret_cast<u8*>(this);

    for ( s32 liEntry = 0; liEntry < miNumEntries; ++liEntry )
    {
        DictEntry& lrEntry = mpaIndex[ liEntry ];
        CGS_ASSERT(reinterpret_cast<u8*>(lrEntry.mpData) > lpBase,
                   "(int32_t)lpEntry->mpData > (int32_t)this");
        lrEntry.mpData = reinterpret_cast<char*>(
            static_cast<uintptr_t>(reinterpret_cast<u8*>(lrEntry.mpData) - lpBase));
    }

    mpaIndex = reinterpret_cast<DictEntry*>(
        static_cast<uintptr_t>(reinterpret_cast<u8*>(mpaIndex) - lpBase));
}

} // namespace CgsContainers
