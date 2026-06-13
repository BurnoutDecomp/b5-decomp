#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnReplays::RaceCarEntitySerialiser)
//
//   Construct @ 0x8264C4C0:
//       result = BrnReplays::BaseSerialiser::Construct(
//                    this, 0, 0, 110592, 110592, "RaceCarEntity", 1);
//       *(this + 92) = 0;
//       return result;
//
// Like its sibling serialisers it forwards to BaseSerialiser::Construct (stream
// id 0, 0x1B000-byte buffers, "RaceCarEntity", flag 1), then clears a 32-bit
// field at offset 0x5C of the object before returning the base result.
// BaseSerialiser is reconstructed by its own TU; declared here for the
// compile-only gate.

namespace BrnReplays
{
    struct BaseSerialiser
    {
        u8   mPad[92];      // [0x00] opaque base fields
        u32  muField5C;     // [0x5C] cleared after Construct by this serialiser

        int Construct(int nId, int nMode, int nBufferSize, int nMaxSize,
                      const char* pName, int nFlag);
    };

    struct RaceCarEntitySerialiser : BaseSerialiser
    {
        int Construct();
    };

    int RaceCarEntitySerialiser::Construct()
    {
        int nResult = BaseSerialiser::Construct(0, 0, 110592, 110592, "RaceCarEntity", 1);
        muField5C = 0;
        return nResult;
    }
}
