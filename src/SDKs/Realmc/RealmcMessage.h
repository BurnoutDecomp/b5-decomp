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
// RealmcIface::MessageShowAutosaveIcon -- the +8 byte carries the visibility
// value stored by BootupCheckTask::Execute; Apply dispatches the message.
// ---------------------------------------------------------------------------
class MessageShowAutosaveIcon : public RealmcCore::Message
{
public:
    explicit MessageShowAutosaveIcon(bool bVisible)
        : RealmcCore::Message(), mbVisible(bVisible) {}

    ~MessageShowAutosaveIcon() {}

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

    // Message vtable +8 -- pProcessor->ProcessMessage(this), the processor's
    //                       +0x40 slot.
    void Apply(RealmcCore::IMessageProcessor* pProcessor) ;

    bool IsVisible() const { return mbVisible; }

private:
    bool mbVisible; // +0x08; BootupCheckTask stores this byte with stb.
};

} // namespace RealmcIface

CGS_STATIC_ASSERT((sizeof(RealmcIface::MessageShowAutosaveIcon) == 12), "ARTIST MessageShowAutosaveIcon size");
