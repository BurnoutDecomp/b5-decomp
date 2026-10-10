#include "SDKs/Realmc/RealmcIfaceMessages.h"

// ===========================================================================
// RealmcIface::MessageBootupDone -- reconstructed from the console image. See RealmcIfaceMessages.h.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageBootupDone::Apply (Message vtable +8)
//
// Swap the two arguments so the processor becomes `this`, load the processor's
// vtable slot +0x3C and tail-call it with the message.
// ---------------------------------------------------------------------------
void MessageBootupDone::Apply(RealmcCore::IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

} // namespace RealmcIface
