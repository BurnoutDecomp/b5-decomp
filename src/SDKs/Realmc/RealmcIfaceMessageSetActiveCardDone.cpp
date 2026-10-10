#include "SDKs/Realmc/RealmcIfaceMessageSetActiveCardDone.h"

// ===========================================================================
// RealmcIface::MessageSetActiveCardDone -- reconstructed from the console image. See RealmcIfaceMessageSetActiveCardDone.h.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageSetActiveCardDone::Apply (Message vtable +8)
//
// Swap the two arguments so the processor becomes `this`, load the processor's
// vtable slot +0x04 and tail-call it with the message.
// ---------------------------------------------------------------------------
void MessageSetActiveCardDone::Apply(RealmcCore::IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

} // namespace RealmcIface
