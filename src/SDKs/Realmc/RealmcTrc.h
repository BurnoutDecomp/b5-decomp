#pragma once

// ===========================================================================
// RealmcCore::Trc -- a Realmc TCR (Technical Certification Requirements) message
// record: a localized message plus up to four selectable, individually-localized
// options. Sibling to the committed RealmcCore core (SDKs/Realmc/RealmcCore.h);
// embedded by RealmcCore::MessageTrc and torn down by the RealmcIface bootup /
// save / load tasks (BootupCheckTask / SaveTask / LoadTask ::DisplayTcrMessages
// and ::Execute).
//
// This header is the canonical OWNING home for the three Trc member functions the
// X360 binary defines:
//
//     RealmcCore::Trc::Trc            (copy ctor) @ 0x82C465C0
//     RealmcCore::Trc::_SetMsgOptions             @ 0x82C466E0
//     RealmcCore::Trc::~Trc                        @ 0x82B555D8
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 pseudocode + asm (BURNOUT_X360_ARTIST.XEX).
// `Realmc` is a vendor library boundary, so its identifiers (RealmcCore, Trc,
// _SetMsgOptions) are preserved verbatim per the naming convention.
//
// LAYOUT (from the copy-ctor / dtor / _SetMsgOptions store offsets; X360 dword
// index n == byte +0x4*n, over 4-byte X360 pointers -- reproduced BY NAME on the
// 64-bit host, so the byte spans widen but the member set is identical):
//   +0x00  miMessageId  (a1[0])   -- the message id, copied verbatim (*a1 = *a2)
//   +0x04  mMessage     (a1[1..]) -- the main localized message (String16, 16 B)
//   +0x14  miNumOptions (a1[5])   -- number of active options (0..4)
//   +0x18  maOptions[4] (a1[6..]) -- 4 x { int miCode; String16 mText } (20 B each)
//                                    option k: code @ +0x18+20k, string @ +0x1C+20k
// Total X360 size 0x68 (104 bytes: last option value @0x54 .. +20).
//
// Each String16 (mMessage and every option's mText) is the shared
// eastl::basic_string<char16_t, RealmcCore::allocator> homed in RealmcContainers.h;
// the copy ctor / dtor de-inline that string's copy-construct / assign / free the
// same way the X360 folded them inline.
// ===========================================================================

#include "types.hpp"                        // u16
#include "SDKs/Realmc/RealmcContainers.h"   // RealmcCore::String16

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// One selectable option of a Trc message: a numeric code plus its localized text.
// The copy ctor default-constructs each element (miCode 0, mText the empty string)
// and then copies -- exactly the X360 init loop that zeroes each value word and
// seats each string on the empty singleton before the per-option copy.
// ---------------------------------------------------------------------------
struct TrcMessageOption
{
    TrcMessageOption() : miCode(0) {}      // mText default-constructs to the empty string

    int      miCode;   // +0x00  the option code byte (0..255) from the packed argument
    String16 mText;    // +0x04  the option's localized text (String16, 16 B on X360)
};

// ---------------------------------------------------------------------------
// RealmcCore::Trc
// ---------------------------------------------------------------------------
class Trc
{
public:
    // @ 0x82C465C0 -- copy construct: copy miMessageId and miNumOptions verbatim,
    //                 RangeInitialize mMessage from the source range, and copy each
    //                 of the four options (code + a self-guarded range-assign of the
    //                 option string). The X360 unrolls the four options and keeps the
    //                 inlined operator= self-assignment guard (&src.str != &dst.str).
    Trc(const Trc& rOther);

    // @ 0x82B555D8 -- destroy: free every option string (options 3..0) then the main
    //                 message string, in reverse member order -- reproduced by the
    //                 member-wise teardown of an empty user destructor.
    ~Trc();

    // @ 0x82C466E0 -- rebuild the option list from a packed code word: reset the
    //                 count, then for each non-zero byte (low byte first, arithmetic
    //                 >> 8 each step) append an option whose code is that byte and
    //                 whose text is Locale::GetString(code). The X360 leaves the last
    //                 helper's result in r3; there is no meaningful return (a setter).
    void _SetMsgOptions(int nPackedCodes);

private:
    int              miMessageId;    // +0x00
    String16         mMessage;       // +0x04  main localized message
    int              miNumOptions;   // +0x14  active option count (0..4)
    TrcMessageOption maOptions[4];   // +0x18  4 x 20 bytes (X360)
};

} // namespace RealmcCore
