#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_SHOT_SELECTOR_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_SHOT_SELECTOR_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<s32,50> (the three shot lists)

// ============================================================================
// GameSource/Director/Utils/BrnShotSelector.h
//
// BrnDirector::ShotSelector -- the director helper that picks camera "shots". It owns three
// fixed-capacity (50-slot) int lists -- one per shot category/priority bucket -- and a back
// pointer to the director that owns it. HOME for the ShotSelector class slice this TU bodies
// (Construct @0x8221B410). The rest of the selector's behaviour (the shot-picking queries)
// lands with its own TUs; this header models only what Construct touches, BY NAME.
//
// ----------------------------------------------------------------------------
// LAYOUT (pinned from BrnDirector::ShotSelector::Construct @0x8221B410):
//   Construct stores the owner pointer (a2) at +0x268, then runs an outer loop THREE times
//   over an Array<int,50> stride of 0xCC (204 == 50*4 elements + the +0xC8 count word):
//     iteration k: array base = this + k*0xCC
//        stw 50, 0xC8(base)        ; the array's live-element count word -> full (50)
//        for i in 0..49:  GetItem(base, i) ; *result = 0   ; zero every slot
//   So the three Array<int,50> live at +0x000 / +0x0CC / +0x198 and the owner pointer at
//   +0x268. (Array<int,50> layout: s32 maElements[50] @+0x00, s32 miCount @+0xC8 -- the
//   committed CgsArray.h shape, matching the *(base + 0xC8) = 50 count store.)
//
//   +0x000  Array<s32,50>  maShotLists[0]
//   +0x0CC  Array<s32,50>  maShotLists[1]
//   +0x198  Array<s32,50>  maShotLists[2]
//   +0x264  (4-byte gap / alignment; not written by Construct)
//   +0x268  void*          mpOwner       (the owning director, a2)
// ----------------------------------------------------------------------------

namespace BrnDirector
{

class ShotSelector
{
public:

    // Number of shot lists (the outer loop runs three times). Each list holds up to 50 ints.
    static const u32 KU_NUM_SHOT_LISTS = 3;
    static const u32 KU_SHOT_LIST_SIZE = 50;

    // Initialise the selector: record the owning director, then bring all three shot lists to
    // their full (50-element) state with every slot zeroed. @0x8221B410.
    //
    // FLAG: the owner argument is modelled as an opaque void* -- Construct only stores the
    //   pointer (stw r4, 0x268(this)); the owner type (the director/MainDirector) lands with
    //   its own TU. Replace the void* with the real owner type when that TU is reconstructed;
    //   the stored offset (+0x268) is pinned from asm.
    void Construct(void* lpOwner);

private:

    // FLAG: the three lists' role names (priority/category buckets) are not attested by this
    //   TU -- Construct treats them identically (full + zeroed). Modelled as an indexable array
    //   of three Array<int,50>; the per-list semantics land with the shot-query TUs.
    Array<s32, KU_SHOT_LIST_SIZE> maShotLists[KU_NUM_SHOT_LISTS];   // +0x000 / +0x0CC / +0x198
    u8                            maReserved264[0x268 - 0x264];     // +0x264  alignment gap (not written)
    void*                         mpOwner;                          // +0x268  owning director (a2)
};

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_SHOT_SELECTOR_H
