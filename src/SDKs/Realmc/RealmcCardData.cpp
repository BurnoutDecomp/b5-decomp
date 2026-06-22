#include "SDKs/Realmc/RealmcCardData.h"

#include <cstring>  // std::memset / std::memcpy -- the X360 bodies are literally
                    // calls to memset/memcpy on the two 80-byte blocks.

// ===========================================================================
// RealmcIface::CardData -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcCardData.h for the layout and the flagged Empty() singleton extern.
//
// Sizes are pinned to the on-object block size (sizeof(CardDataBlock) == 0x50)
// rather than a bare literal, so the host build stays self-consistent while
// reproducing the X360's 0x50/0xA0 memset/memcpy lengths exactly.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// CardData::CardData @ 0x82B53188
//
//   li   r5, 0x50 ; li r4, 0 ; mr r31, r3 ; bl memset      -> memset(this,0,80)
//   li   r30, 0 ; addi r3, r31, 0x50 ; li r5, 0x50 ; li r4, 0
//   stw  r30, 0(r31)                                        -> this[0] = 0
//   bl   memset                                             -> memset(this+80,0,80)
//   stw  r30, 0x50(r31)                                     -> this[80] = 0
//
// Two adjacent 80-byte blocks zeroed; each block's leading state word is then
// explicitly re-stored as 0 (the asm store order: zero current word BEFORE the
// second memset, zero previous word AFTER it).
// ---------------------------------------------------------------------------
CardData::CardData()
{
    std::memset(&maCurrent, 0, sizeof(CardDataBlock));   // memset(this, 0, 80)
    maCurrent.muState = 0;                               // this[0] = 0
    std::memset(&maPrevious, 0, sizeof(CardDataBlock));  // memset(this+80, 0, 80)
    maPrevious.muState = 0;                              // this[80] = 0
}

// ---------------------------------------------------------------------------
// CardData::operator= @ 0x82B531E8
//
//   lwz r11, 0(r31) ; lwz r10, 0(r30) ; cmplw cr6, r11, r10
//   beq cr6, skip
//     addi r3, r31, 0x50 ; li r5, 0x50 ; mr r4, r31 ; bl memcpy
//                                              -> memcpy(this+80, this, 80)
//   skip:
//   li r5, 0x50 ; mr r4, r30 ; mr r3, r31 ; bl memcpy
//                                              -> memcpy(this, rOther, 80)
//   return this
//
// If the incoming state word differs, snapshot the current block into the
// previous block FIRST; then copy the source's current block over ours.
// ---------------------------------------------------------------------------
CardData& CardData::operator=(const CardData& rOther)
{
    if (maCurrent.muState != rOther.maCurrent.muState)
    {
        std::memcpy(&maPrevious, &maCurrent, sizeof(CardDataBlock)); // this+80 <- this
    }
    std::memcpy(&maCurrent, &rOther.maCurrent, sizeof(CardDataBlock)); // this <- rOther
    return *this;
}

// ---------------------------------------------------------------------------
// CardData::operator== @ 0x82B53318
//
//   lwz r11, 0(r3) ; lwz r10, 0(r4) ; subf r11, r11, r10
//   cntlzw r11, r11 ; extrwi r3, r11, 1,26          -> (lhs == rhs) ? 1 : 0
//
// Compares ONLY the leading state word of each object.
// ---------------------------------------------------------------------------
bool CardData::operator==(const CardData& rOther) const
{
    return rOther.maCurrent.muState == maCurrent.muState;
}

// ---------------------------------------------------------------------------
// CardData::operator!= @ 0x82B53330
//
//   lwz r11, 0(r3) ; lwz r10, 0(r4) ; subf r11, r11, r10
//   cntlzw r11, r11 ; extrwi r11, r11, 1,26 ; xori r3, r11, 1
//                                                  -> (lhs != rhs) ? 1 : 0
// ---------------------------------------------------------------------------
bool CardData::operator!=(const CardData& rOther) const
{
    return rOther.maCurrent.muState != maCurrent.muState;
}

// ---------------------------------------------------------------------------
// CardData::PreviousState @ 0x82B532C8
//
//   li r5, 0xA0 ; mr r31, r3 ; mr r30, r4 ; bl memcpy
//                                              -> memcpy(this, rSource, 160)
//   addi r4, r30, 0x50 ; li r5, 0x50 ; mr r3, r31 ; bl memcpy
//                                              -> memcpy(this, rSource+80, 80)
//   return this
//
// Copy the whole source object (both blocks), then overwrite our current block
// with the source's PREVIOUS block -- i.e. roll *this back to the source's
// previous state.
// ---------------------------------------------------------------------------
CardData& CardData::PreviousState(const CardData& rSource)
{
    std::memcpy(this, &rSource, sizeof(CardData));                   // this <- rSource (160)
    std::memcpy(&maCurrent, &rSource.maPrevious, sizeof(CardDataBlock)); // this <- rSource+80 (80)
    return *this;
}

// ---------------------------------------------------------------------------
// CardData::Empty @ 0x82B53250
//
//   lwz r11, dword_83250190 ; clrlwi. r9, r11, 31 ; bne done   -> test flag bit0
//     ori r11, r11, 1 ; stw r11, dword_83250190                -> set flag bit0
//     memset(&dword_832500F0, 0, 80) ; dword_832500F0 = 0       -> zero current
//     memset(&dword_83250140, 0, 80) ; dword_83250140 = 0       -> zero previous
//   done:
//   return &dword_832500F0
//
// Lazily zero a shared singleton CardData exactly once, then return it. The
// X360 used a raw global flag word (bit 0) + raw global storage; modelled here
// with an equivalent function-local one-shot guard + static instance (the
// default ctor already zeroes both blocks, matching the X360's memset/store
// sequence). Behaviour is identical: zero-init once, return the same object.
// ---------------------------------------------------------------------------
CardData& CardData::Empty()
{
    static CardData sEmpty;  // default-constructed once -> both blocks zeroed
    return sEmpty;
}

} // namespace RealmcIface
