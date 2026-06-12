#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828821A8
//   (CgsNetwork::ReliableMessage::PackOrUnpack)
//
// Behaviour-faithful to the X360 pseudocode:
//     v3 = *(this + 28);                       // read the 16-bit reliable id
//     result = sub_82881250(this, &v3, 0, 65534);
//     *(this + 28) = v3;                        // write it back
//     return result;
//
// sub_82881250 is the shared 16-bit field (de)serialiser used by the whole
// Message hierarchy (25 callers): it packs or unpacks a u16 in the inclusive
// range [0, 65534], reading/writing the value through the pointer. It is not yet
// reconstructed, so it is forward-declared here (a stub-style external reference);
// the real body lands when its own TU is reconstructed.

namespace CgsNetwork
{
    struct ReliableMessage;

    // Shared serialiser helper, unnamed in the export @ 0x82881250.
    int PackOrUnpackU16(ReliableMessage* lpThis, u16* lpu16Value, int liMin, int liMax);

    struct ReliableMessage
    {
        u8   mPad[28];          // [0x00] opaque
        u16  mu16ReliableId;    // [0x1C] the reliable-message id (range [0,65534])

        int PackOrUnpack();
    };

    int ReliableMessage::PackOrUnpack()
    {
        u16 lu16Value = mu16ReliableId;
        const int liResult = PackOrUnpackU16(this, &lu16Value, 0, 65534);
        mu16ReliableId = lu16Value;
        return liResult;
    }
}
