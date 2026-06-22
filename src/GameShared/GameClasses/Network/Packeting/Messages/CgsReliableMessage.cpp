#include "types.hpp"

#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828821A8
//   (CgsNetwork::ReliableMessage::PackOrUnpack)
//
// Behaviour-faithful to the X360 pseudocode:
//     v3 = *(this + 28);                       // read the 16-bit reliable id
//     result = sub_82881250(this, &v3, 0, 65534);
//     *(this + 28) = v3;                        // write it back
//     return result;
//
// +0x1C is the inherited Message::mu16Frame field, which a reliable message reuses to
// carry its wrapped reliable id. sub_82881250 is the shared 16-bit field serialiser
// (now CgsNetwork::PackOrUnpackU16, declared in CgsMessage.h): it packs or unpacks a
// u16 in the inclusive range [0, 65534], reading/writing through the pointer. It is
// its own not-yet-reconstructed TU; the real body lands when that TU is worked.
//
// (Previously this file forked a local opaque `struct ReliableMessage` -- mPad[28] +
// mu16ReliableId @ 0x1C -- which is now resolved against the canonical hierarchy
// header CgsReliableMessage.h; the reliable id == base mu16Frame at the same +0x1C.)

namespace CgsNetwork
{
    PackOrUnpackResult ReliableMessage::PackOrUnpack()
    {
        u16 lu16ReliableId = mu16Frame;
        const PackOrUnpackResult lxResult = PackOrUnpackU16(this, &lu16ReliableId, 0, 65534);
        mu16Frame = lu16ReliableId;
        return lxResult;
    }
}
