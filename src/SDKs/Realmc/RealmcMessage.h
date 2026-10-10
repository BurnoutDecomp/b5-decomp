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
// MessageShowAutosaveIcon is a RealmcCore::Message: its Apply (Message vtable
// +8) hands the message to the shared RealmcCore::IMessageProcessor's +0x40
// slot, the ShowAutosaveIcon handler. The thunk swaps its two arguments so the
// processor becomes `this` and tail-calls that slot, exactly like
// RealmcCore::Message::Apply (+0x54).
// ===========================================================================

#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::Message / IMessageProcessor

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// RealmcIface::MessageShowAutosaveIcon -- a Realmc "show autosave icon" message.
// Only its Apply thunk is in this TU; Apply reads none of the message's own
// fields, so none are declared here.
// ---------------------------------------------------------------------------
class MessageShowAutosaveIcon : public RealmcCore::Message
{
public:
    // Message vtable +8 -- pProcessor->ProcessMessage(this), the processor's
    //                       +0x40 slot.
    void Apply(RealmcCore::IMessageProcessor* pProcessor) override;
};

} // namespace RealmcIface
