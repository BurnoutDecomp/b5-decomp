#include "SDKs/Realmc/RealmcCoreMessageClear.h"

// ===========================================================================
// RealmcCore::MessageClear -- reconstructed from the console image. See RealmcCoreMessageClear.h.
// ===========================================================================

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// MessageClear::Apply (Message vtable +8)
//
// Swap the two arguments so the processor becomes `this`, load the processor's
// vtable slot +0x44 and tail-call it with the message.
// ---------------------------------------------------------------------------
void MessageClear::Apply(RealmcCore::IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

} // namespace RealmcCore
