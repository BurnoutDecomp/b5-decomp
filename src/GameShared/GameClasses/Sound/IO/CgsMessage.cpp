#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82689868
//   (CgsSound::Io::MessageHeader::Destruct)
//
// Behaviour-faithful to the X360 pseudocode:
//     *(this + 8) = 0xFFFF;        // reset the 16-bit id field to "invalid"
//     return this;

namespace CgsSound
{
    namespace Io
    {
        struct MessageHeader
        {
            u8   mPad[8];       // [0x00] opaque
            u16  mu16Id;        // [0x08] message id, reset to 0xFFFF on destruct

            MessageHeader* Destruct();
        };

        MessageHeader* MessageHeader::Destruct()
        {
            mu16Id = 0xFFFF;
            return this;
        }
    }
}
