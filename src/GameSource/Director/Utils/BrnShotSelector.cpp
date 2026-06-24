// ============================================================================
// GameSource/Director/Utils/BrnShotSelector.cpp
//
// Compilation home for BrnDirector::ShotSelector. Bodies Construct @0x8221B410.
//
// Construct seeds the selector's three Array<int,50> shot lists to a full (50-element)
// state with every slot zeroed, and records the owning director. Reconstructed for
// semantic parity; members are accessed BY NAME (layout in BrnShotSelector.h).
// ============================================================================

#include "GameSource/Director/Utils/BrnShotSelector.h"

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// BrnDirector::ShotSelector::Construct @0x8221B410
//
//   stw   r4, 0x268(this)        ; mpOwner = lpOwner
//   li    r28, 3                 ; outer count
//   li    r27, 50                ; the count value
//   li    r29, 0                 ; the zero value
//   r30 = this
// loop_outer:                    ; three times, r30 += 0xCC each pass
//   r31 = 0
//   stw   r27, 0xC8(r30)         ; maShotLists[k].miCount = 50  (full)
// loop_inner:                    ; i = 0..49
//   r3 = r30; r4 = r31
//   bl    Array<int,50>::GetItem ; &maShotLists[k].maElements[i]
//   r31 += 1
//   stw   r29, 0(r3)             ; *element = 0
//   if r31 < 50 goto loop_inner
//   r28 -= 1; r30 += 0xCC
//   if r28 != 0 goto loop_outer
//
// The count word is set to 50 BEFORE the element writes (so the checked GetItem sees a
// fully-constructed, full-length list); each of the 50 ints is then zeroed in turn.
// ----------------------------------------------------------------------------
void
ShotSelector::Construct(void* lpOwner)
{
    mpOwner = lpOwner;                               // stw r4, 0x268(this)

    for (u32 luList = 0; luList < KU_NUM_SHOT_LISTS; ++luList)
    {
        Array<s32, KU_SHOT_LIST_SIZE>& lrList = maShotLists[luList];
        lrList.SetFullCount();                       // stw 50, 0xC8(base)  -> miCount = 50
        for (u32 luSlot = 0; luSlot < KU_SHOT_LIST_SIZE; ++luSlot)
        {
            lrList.GetItem(luSlot) = 0;              // *(&maElements[slot]) = 0
        }
    }
}

} // namespace BrnDirector
