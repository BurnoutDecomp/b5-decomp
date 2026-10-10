#include "SDKs/Realmc/RealmcIfaceMessageSaveDone.h"

// ===========================================================================
// RealmcIface::MessageSaveDone -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcIfaceMessageSaveDone.h for the dispatch layout (target vtable slot
// +0x34) and the class layout, and for the note that this TU has no ctor.
//
// Bodied here:
//   MessageSaveDone::Apply @0x82B56088
//   MessageSaveDone::`vector deleting destructor` @0x82B560A8 (compiler-generated
//     from the virtual dtor + the class operator delete in the header)
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageSaveDone::Apply (Message vtable +8)
//
// Swap the two arguments so the processor becomes `this`, load the processor's
// vtable slot +0x34 and tail-call it with the message.
// ---------------------------------------------------------------------------
void MessageSaveDone::Apply(RealmcCore::IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

} // namespace RealmcIface
