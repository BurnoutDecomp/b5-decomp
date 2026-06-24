#include "GameShared/GameClasses/Network/Time/CgsReadyMessage.h"

// ===================================================================================
// CgsNetwork::ReadyMessage -- definition home for the two ledger funcs of this TU.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Both are bare identical-code-folded
// tail-call forwarders in the image:
//
//   GetPackedMessageSize @ 0x827DE0F8:
//       0x827DE0F8  b  CgsNetwork__TestConnectionMessage__GetPackedMessageSize
//     A ReadyMessage is a bare ReliableMessage subclass with no extra payload, exactly
//     like TestConnectionMessage; the X360 linker folded the two GetPackedMessageSize
//     bodies onto the one address. Since both leaves add no fields, the folded body is
//     just the bare-ReliableMessage packed size -- expressed here as the in-hierarchy
//     base delegate ReliableMessage::GetPackedMessageSize (same value, links to the
//     committed base rather than to a sibling needing an instance).
//
//   PackOrUnpack @ 0x827DE100:
//       0x827DE100  b  CgsNetwork__ReliableMessage__PackOrUnpack
//     The leaf adds no fields, so it forwards straight to the base which (de)serialises
//     the wrapped reliable id.
// ===================================================================================

namespace CgsNetwork
{
    s32 ReadyMessage::GetPackedMessageSize()
    {
        // @0x827DE0F8: ICF-folded onto TestConnectionMessage::GetPackedMessageSize;
        // both are bare ReliableMessage subclasses, so the value is the base size.
        return ReliableMessage::GetPackedMessageSize();
    }

    PackOrUnpackResult ReadyMessage::PackOrUnpack()
    {
        // @0x827DE100: b CgsNetwork__ReliableMessage__PackOrUnpack.
        return ReliableMessage::PackOrUnpack();
    }
}
