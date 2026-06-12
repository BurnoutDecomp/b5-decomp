#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828687C8
//   (CgsMemory::Relocator::Construct)
//
// Behaviour-faithful to the X360 pseudocode (result is a _BYTE*):
//     result[1] = 0;
//     result[0] = 0;
//     return result;
//
// Clears the two leading bytes of the relocator (a small head/tail or count pair).

namespace CgsMemory
{
    struct Relocator
    {
        u8 mu8Field0;       // [0x00]
        u8 mu8Field1;       // [0x01]

        Relocator* Construct();
    };

    Relocator* Relocator::Construct()
    {
        mu8Field1 = 0;
        mu8Field0 = 0;
        return this;
    }
}
