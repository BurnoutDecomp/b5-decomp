#pragma once

// BrnReplays::SoundSerialiserFrame -- the per-frame snapshot the sound replay
// serialiser records/plays back. No DWARF or prior source recovered for this type,
// so its internal field layout is unknown; only its copy-assignment behaviour is
// recovered (from the X360 build). It is reconstructed here as an opaque,
// trivially-copyable aggregate of the exact byte extent the copy touches.
//
// X360 reference: operator= @0x8264CA58 (called by SoundSerialiser::Read/Write).

#include "types.hpp"

namespace BrnReplays
{
    // Opaque per-frame sound state. The copy-assignment @0x8264CA58 copies the full
    // object byte range [0x000 .. 0x230) (560 bytes) -- vmx128 16-byte block copies +
    // scalar lwz/stw + a final stfs, a contiguous trivially-copyable aggregate. Because
    // no member names/types were recovered, the payload is held as a single named byte
    // buffer of that exact size and the copy is reconstructed as a full-object
    // (memberwise-equivalent) copy. All access is BY NAME (maPayload); no raw-offset
    // member casts. GROW this home into named fields if/when a DWARF layout is recovered.
    struct SoundSerialiserFrame
    {
        // Total recorded extent of operator= @0x8264CA58: the asm's last store is the
        // scalar at +0x228 / stfs at +0x22C -> object extent 0x230 bytes.
        static const u32 KU_FRAME_SIZE = 0x230;

        u8 maPayload[KU_FRAME_SIZE];

        // @0x8264CA58 -- full-object copy assignment (declared here, defined in
        // BrnReplaySoundSerialiserFrame.cpp).
        SoundSerialiserFrame& operator=(const SoundSerialiserFrame& lOther);
    };
}
