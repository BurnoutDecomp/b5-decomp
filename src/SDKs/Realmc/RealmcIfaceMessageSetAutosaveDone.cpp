#include "SDKs/Realmc/RealmcIfaceMessageSetAutosaveDone.h"

// ===========================================================================
// RealmcIface::MessageSetAutosaveDone -- reconstructed from the console image. See RealmcIfaceMessageSetAutosaveDone.h.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageSetAutosaveDone::Apply (Message vtable +8)
//
// Swap the two arguments so the processor becomes `this`, load the processor's
// vtable slot +0x18 and tail-call it with the message.
// ---------------------------------------------------------------------------
void MessageSetAutosaveDone::Apply(RealmcCore::IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

} // namespace RealmcIface
