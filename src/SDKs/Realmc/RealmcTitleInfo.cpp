#include "SDKs/Realmc/RealmcTitleInfo.h"

// ===========================================================================
// RealmcIface::TitleInfo -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcTitleInfo.h for the layout and the flagged Empty() singleton extern.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// TitleInfo::TitleInfo @ 0x82B51B78  (copy ctor)
//
//   lwz r11, 0(r4)     -> load rOther.muTitle
//   stw r11, 0(r3)     -> this->muTitle = rOther.muTitle
//   blr
//
// Copy the single title word from the source object. Nothing else is touched.
// ---------------------------------------------------------------------------
TitleInfo::TitleInfo(const TitleInfo& rOther)
{
    muTitle = rOther.muTitle;
}

// ---------------------------------------------------------------------------
// TitleInfo::Empty @ 0x82B51B88
//
//   lis  r10, dword_832500B8@ha
//   lwz  r11, dword_832500B8@l(r10)   -> load the one-shot flag word
//   clrlwi. r9, r11, 31               -> test bit 0 (the "initialised" flag)
//   lis  r9, dword_832500B4@ha
//   addi r3, r9, dword_832500B4@l     -> result = &dword_832500B4 (the singleton)
//   bnelr                             -> if already initialised, return now
//   ori  r11, r11, 1                  -> set flag bit 0
//   stw  r11, dword_832500B8@l(r10)
//   li   r11, 0
//   stw  r11, dword_832500B4(r3)      -> dword_832500B4 = 0  (zero the word)
//   blr
//
// Lazily zero a shared singleton TitleInfo exactly once, then return it. The
// X360 used a raw global flag word (bit 0) + raw global storage; modelled here
// with an equivalent function-local one-shot guard + static instance (the
// default ctor already zeroes muTitle, matching the X360's single zero-store).
// Behaviour is identical: zero-init once, return the same object. Mirrors the
// sibling RealmcIface::CardData::Empty.
// ---------------------------------------------------------------------------
TitleInfo& TitleInfo::Empty()
{
    static TitleInfo sEmpty;  // default-constructed once -> muTitle zeroed
    return sEmpty;
}

} // namespace RealmcIface
