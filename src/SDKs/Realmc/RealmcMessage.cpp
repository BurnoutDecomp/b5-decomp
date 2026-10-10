#include "SDKs/Realmc/RealmcMessage.h"

// ===========================================================================
// RealmcIface::MessageShowAutosaveIcon -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcMessage.h for the dispatch layout (target vtable slot +0x40). The body
// is the same one-line virtual dispatch idiom as RealmcCore::Message::Apply
// (@0x82C44C08, RealmcCore.cpp), differing only in the dispatched slot.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageShowAutosaveIcon::Apply (Message vtable +8)
//
// Swap the two arguments so the processor becomes `this`, load the processor's
// vtable slot +0x40 and tail-call it with the message.
// ---------------------------------------------------------------------------
void MessageShowAutosaveIcon::Apply(RealmcCore::IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

} // namespace RealmcIface
