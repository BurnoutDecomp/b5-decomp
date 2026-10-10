#include "SDKs/Realmc/RealmcIfaceMessageCardRemoved.h"

// ===========================================================================
// RealmcIface::MessageCardRemoved -- reconstructed from the console image. See RealmcIfaceMessageCardRemoved.h.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageCardRemoved::Apply (Message vtable +8)
//
// Swap the two arguments so the processor becomes `this`, load the processor's
// vtable slot +0x0C and tail-call it with the message.
// ---------------------------------------------------------------------------
void MessageCardRemoved::Apply(RealmcCore::IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

} // namespace RealmcIface
