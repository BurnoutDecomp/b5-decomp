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
#include "SDKs/Realmc/RealmcCore.h"         // RealmcCore::Message (base of MessageTrc)

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

// ===========================================================================
// RealmcCore::MessageTrc -- a Realmc message that carries a Trc (TCR message)
// payload. It is a RealmcCore::Message (same base vtable off_821BA2CC installed
// first in the ctor, restored last in the dtor, and the same interrupt-masked
// lwarx/stwcx. lock word at +4) that embeds a Trc by value and one extra id word,
// then dispatches itself onto a target via the double-dispatch Apply thunk below.
//
// This header is the canonical OWNING home for the three MessageTrc member
// functions the X360 binary defines (BURNOUT_X360_ARTIST.XEX; no Feb-2007 leak
// source, no DWARF -- SHAPE and BODIES from the X360 pseudocode + asm):
//
//     RealmcCore::MessageTrc::Apply        @ 0x82C44CF8
//     RealmcCore::MessageTrc::MessageTrc   @ 0x82C467D0  (value ctor)
//     RealmcCore::MessageTrc::~MessageTrc  @ 0x82C46428
//
// LAYOUT (from the ctor/dtor store offsets; Message base == vtable + lock = 8 B):
//   +0x00  vtable pointer (Message base off_821BA2CC then final off_821BA37C)
//   +0x04  muLock   -- inherited from Message; atomically zeroed in the ctor
//   +0x08  mTrc     -- the embedded Trc payload (copy-constructed from the ctor's
//                      Trc argument at a1+2 == this+8; ends at +0x70 on X360)
//   +0x70  miValue  -- the ctor's third argument, stored verbatim (a1[28] = a3).
//                      FLAG: role unrecovered -- a bare id/value word read by no
//                      homed function; modelled as a plain int.
// (Message widens to 16 B of members on the 64-bit host, so the byte spans widen
//  but the member set is identical -- semantic parity by NAME, not byte offset.)
// ===========================================================================

class MessageTrc;

// ---------------------------------------------------------------------------
// IRealmcTrcTarget -- the object MessageTrc::Apply() dispatches into. Its vtable
// slot +0x48 (18) accepts the MessageTrc and handles the TCR message. The padding
// virtuals below pin the dispatched method to byte offset 0x48 (slot 18 for 4-byte
// X360 pointers: slot 0 == dtor at +0x00, so the 17 reserved slots fill +0x04 ..
// +0x44 and DisplayTrcMessage lands at +0x48). Mirrors the RealmcCore::Message /
// RealmcIface::MessageShowAutosaveIcon dispatch-target idiom, only the slot differs.
// ---------------------------------------------------------------------------
class IRealmcTrcTarget
{
public:
    virtual ~IRealmcTrcTarget() {}                   // +0x00
    virtual void Reserved01() = 0;  virtual void Reserved02() = 0;  // +0x04 +0x08
    virtual void Reserved03() = 0;  virtual void Reserved04() = 0;  // +0x0C +0x10
    virtual void Reserved05() = 0;  virtual void Reserved06() = 0;  // +0x14 +0x18
    virtual void Reserved07() = 0;  virtual void Reserved08() = 0;  // +0x1C +0x20
    virtual void Reserved09() = 0;  virtual void Reserved10() = 0;  // +0x24 +0x28
    virtual void Reserved11() = 0;  virtual void Reserved12() = 0;  // +0x2C +0x30
    virtual void Reserved13() = 0;  virtual void Reserved14() = 0;  // +0x34 +0x38
    virtual void Reserved15() = 0;  virtual void Reserved16() = 0;  // +0x3C +0x40
    virtual void Reserved17() = 0;                                  // +0x44
    virtual int  DisplayTrcMessage(MessageTrc* pMessage) = 0;       // +0x48
};

// ---------------------------------------------------------------------------
// RealmcCore::MessageTrc
// ---------------------------------------------------------------------------
class MessageTrc : public Message
{
public:
    // @ 0x82C467D0 -- construct: run the Message base ctor (install the base
    //                 vtable, atomically zero the lock word), copy-construct the
    //                 embedded Trc from rTrc (X360 Trc::Trc copy ctor on this+8),
    //                 install the final MessageTrc vtable, then store miValue.
    //
    // IDA reports a trailing fourth register argument (a4) that the asm never
    // reads; it is omitted here. a2 (the Trc source) and a3 (miValue) are the two
    // arguments the body actually consumes.
    MessageTrc(const Trc& rTrc, int nValue);

    // @ 0x82C46428 -- destroy: tear down the embedded Trc (X360 Trc::~Trc on
    //                 this+8), then the Message base dtor restores the base vtable.
    ~MessageTrc() override;

    // @ 0x82C44CF8 -- dispatch this message onto pTarget via pTarget's vtable slot
    //                 +0x48 (18): pTarget->DisplayTrcMessage(pThis).
    //
    // IDA lists (a1 = pThis, a2 = pTarget); the asm swaps r3/r4 so the target is
    // `this` for the dispatch (mirrors RealmcCore::Message::Apply @ 0x82C44C08).
    static int Apply(MessageTrc* pThis, IRealmcTrcTarget* pTarget);

private:
    Trc mTrc;      // +0x08  embedded TCR-message payload (copy-constructed)
    int miValue;   // +0x70  ctor arg 3, stored verbatim (FLAG: role unrecovered)
};

} // namespace RealmcCore
