#include "SDKs/Realmc/RealmcIfaceMessageCheckLoadedData.h"

// ===========================================================================
// RealmcIface::MessageCheckLoadedData -- reconstructed from the console image. See RealmcIfaceMessageCheckLoadedData.h.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageCheckLoadedData::Apply (Message vtable +8)
//
// Swap the two arguments so the processor becomes `this`, load the processor's
// vtable slot +0x10 and tail-call it with the message.
// ---------------------------------------------------------------------------
void MessageCheckLoadedData::Apply(RealmcCore::IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

} // namespace RealmcIface
