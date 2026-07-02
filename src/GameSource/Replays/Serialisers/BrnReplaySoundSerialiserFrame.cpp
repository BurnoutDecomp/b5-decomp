#include "GameSource/Replays/Serialisers/BrnReplaySoundSerialiserFrame.h"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::SoundSerialiserFrame).
//
//   operator= @ 0x8264CA58
//
// The X360 body is the compiler-emitted copy assignment of a trivially-copyable
// aggregate: it copies the entire object [0x000 .. 0x560) via a mix of 128-bit
// vector loads/stores (lvx128/stvx128) and scalar copies, with no field-specific
// logic. With no recovered field layout, the semantically-faithful reconstruction is
// a single full-object copy of the same byte extent.

namespace BrnReplays
{
    // @ 0x8264CA58
    SoundSerialiserFrame& SoundSerialiserFrame::operator=(const SoundSerialiserFrame& lOther)
    {
        // The X360 asm copies unconditionally with no self-assignment check.
        std::memcpy(maPayload, lOther.maPayload, sizeof(maPayload));
        return *this;
    }
}
