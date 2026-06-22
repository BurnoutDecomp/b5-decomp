#pragma once

// ===========================================================================
// RealmcIface::TitleInfo -- a Realmc save/load "title info" record used by the
// X360 memory-card / save-load interface (the RealmcIface family in
// BURNOUT_X360_ARTIST.XEX; sibling to RealmcIface::CardData in RealmcCardData.h,
// RealmcIface::LoadEntryInfo in RealmcLoadEntryInfo.h, and the RealmcCore
// primitives in RealmcCore.h).
//
// This header is the canonical OWNING home for the TitleInfo struct and its two
// reconstructed member functions:
//
//     RealmcIface::TitleInfo::TitleInfo  @ 0x82B51B78   (copy ctor)
//     RealmcIface::TitleInfo::Empty      @ 0x82B51B88   (singleton accessor)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, TitleInfo) are preserved
// verbatim per the naming convention.
//
// LAYOUT (from asm):
//
//   +0x00  muTitle -- a single 32-bit word. The copy ctor copies exactly this one
//                     word (lwz r11,0(src) ; stw r11,0(dst)); Empty() zero-stores
//                     exactly this one word into the shared singleton. No other
//                     field is touched by either function, so the object is
//                     modelled as a single opaque 32-bit title/state word rather
//                     than fabricating additional named members.
//
//   Total modelled size = 0x04 (4) bytes.
//
// PLATFORM/VENDOR EXTERNS (flagged):
//   * Empty() returns a shared, lazily-initialised singleton TitleInfo living in
//     Realmc static storage (X360: dword_832500B4 = the object's word,
//     dword_832500B8 = a one-shot "initialised" flag word, bit 0). Modelled here
//     with a function-local one-shot guard + static instance; the X360 used a raw
//     global flag word, but the observable behaviour (zero-init once, then return
//     the same object) is identical -- mirrors the sibling CardData::Empty.
// ===========================================================================

#include <cstdint>

namespace RealmcIface
{

class TitleInfo
{
public:
    // @ 0x82B51B78 -- copy ctor: copy the single title word from rOther.
    TitleInfo(const TitleInfo& rOther);

    // The X360 binary only ever copy-constructs TitleInfo from an existing
    // instance (the three call sites all pass a source object). A default ctor is
    // provided so the lazily-initialised Empty() singleton can be constructed and
    // zeroed; it is not a distinct X360 thunk.
    TitleInfo() : muTitle(0) {}

    // @ 0x82B51B88 -- return the shared singleton TitleInfo, zero-initialised once
    //                 on first use.
    static TitleInfo& Empty();

    std::uint32_t muTitle;   // +0x00  opaque title/state word (the copied word)
};
// sizeof(TitleInfo) == 0x04 (4) bytes.

} // namespace RealmcIface
