#pragma once

// ===========================================================================
// RealmcIface::MessageShowAutosaveIcon -- a Realmc save/load interface message
// that asks its target to show (or hide) the autosave icon. Part of the
// RealmcIface message family in BURNOUT_X360_ARTIST.XEX; the structural sibling
// of the RealmcCore::Message dispatch thunks (RealmcCore.h / RealmcCore.cpp).
//
// This header is the canonical OWNING home for the one reconstructed member
// function:
//
//     RealmcIface::MessageShowAutosaveIcon::Apply  @ 0x82B552A0
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, MessageShowAutosaveIcon,
// Apply) are preserved verbatim per the naming convention.
//
// Apply() does NOT touch a message object's own vtable -- exactly like
// RealmcCore::Message::Apply (@0x82C44C08), it swaps the two argument registers
// so the *target* becomes `this`, then tail-calls into the target's vtable. The
// only structural difference from the RealmcCore::Message thunk is the dispatched
// slot: here it is byte offset +0x40 (slot 16) rather than +0x54.
//
// Apply asm @ 0x82B552A0:
//     mr   r11, r4            ; r11 = pTarget   (a2)
//     mr   r4,  r3            ; r4  = pThis     (a1, the message)
//     mr   r3,  r11           ; r3  = pTarget   (now `this`)
//     lwz  r10, 0(r11)        ; r10 = pTarget->vtable
//     lwz  r11, 0x40(r10)     ; r11 = vtable[+0x40]  (slot 16)
//     mtctr r11 ; bctr        ; tail-call pTarget->[+0x40](pTarget, pThis)
//
// i.e. pTarget->ShowAutosaveIcon(pThis). The pseudocode `(*(*a2 + 64))(a2, a1)`
// is the same construct (64 == 0x40); the target is any object exposing a
// "show the autosave icon for this message" virtual at slot +0x40, modelled as
// the IRealmcAutosaveTarget interface below.
// ===========================================================================

namespace RealmcIface
{

class MessageShowAutosaveIcon;

// ---------------------------------------------------------------------------
// IRealmcAutosaveTarget -- the object MessageShowAutosaveIcon::Apply() dispatches
// into. Its vtable slot +0x40 (16) accepts the message and shows the autosave
// icon. The padding virtuals below pin the dispatched method to byte offset
// 0x40 (slot 16 for 4-byte X360 pointers: slot 0 == dtor at +0x00, so the 15
// reserved slots fill +0x04 .. +0x3C and ShowAutosaveIcon lands at +0x40).
// ---------------------------------------------------------------------------
class IRealmcAutosaveTarget
{
public:
    virtual ~IRealmcAutosaveTarget() {}              // +0x00
    virtual void Reserved01() = 0;  virtual void Reserved02() = 0;  // +0x04 +0x08
    virtual void Reserved03() = 0;  virtual void Reserved04() = 0;  // +0x0C +0x10
    virtual void Reserved05() = 0;  virtual void Reserved06() = 0;  // +0x14 +0x18
    virtual void Reserved07() = 0;  virtual void Reserved08() = 0;  // +0x1C +0x20
    virtual void Reserved09() = 0;  virtual void Reserved10() = 0;  // +0x24 +0x28
    virtual void Reserved11() = 0;  virtual void Reserved12() = 0;  // +0x2C +0x30
    virtual void Reserved13() = 0;  virtual void Reserved14() = 0;  // +0x34 +0x38
    virtual void Reserved15() = 0;                                  // +0x3C
    virtual int  ShowAutosaveIcon(MessageShowAutosaveIcon* pMessage) = 0; // +0x40
};

// ---------------------------------------------------------------------------
// RealmcIface::MessageShowAutosaveIcon -- a Realmc "show autosave icon" message.
//
// The TU exposes only the Apply dispatch thunk; the message object's own layout
// (a Realmc message base) is not touched by Apply, so no fields are reconstructed
// here. Apply is a static thunk (the X360 takes the message in r3 and the target
// in r4, then swaps them), matching the RealmcCore::Message::Apply signature.
// ---------------------------------------------------------------------------
class MessageShowAutosaveIcon
{
public:
    // @ 0x82B552A0 -- dispatch this message onto pTarget via its vtable slot
    //                 +0x40 (16): pTarget->ShowAutosaveIcon(pThis).
    //
    // IDA lists (a1 = pThis, a2 = pTarget); the asm swaps r3/r4 so the target is
    // `this` for the dispatch (mirrors RealmcCore::Message::Apply @ 0x82C44C08).
    static int Apply(MessageShowAutosaveIcon* pThis, IRealmcAutosaveTarget* pTarget);
};

} // namespace RealmcIface
