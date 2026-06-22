#pragma once

// MINIMAL SLICE of BrnReplays::BaseSerialiser for the RequestInterface TU.
// DWARF home: GameSource/Replays/BrnReplayBaseSerialiser.h:51.
//
// BaseSerialiser is the abstract base for all replay serialisers; its full layout
// (record/playback buffers, mode state, name, etc.) is reconstructed by its OWN
// ledger TU. RequestInterface (this TU's owner) only stores BaseSerialiser* slots
// and reads each serialiser's id via GetId(), so this header carries just the id
// member + accessor needed to body RequestInterface. When the full BaseSerialiser
// TU lands, GROW this canonical home additively -- do NOT fork it.
//
// The id member meId lives at +0x28 (X360 asm: RegisterSerialiser reads
// `lwz r11, 0x28(r31)` for the serialiser id). The preceding 0x28 bytes are the
// mode/lock/buffer state members enumerated by the DWARF (meMode, mbLocked,
// mpBuffer, miBufferSize, miBufferUsed, miBufferRead, mpStaticBuffer,
// miStaticBufferSize); represented here as an opaque header blob so meId sits at
// the asm-attested offset without fabricating the (other-TU-owned) member detail.

#include "types.hpp"
#include "GameSource/Replays/BrnReplayShared.h"

namespace BrnReplays
{
    // DWARF: BrnReplayBaseSerialiser.h:51
    class BaseSerialiser
    {
    public:
        // GetId @ inline (BrnReplayBaseSerialiser.h:173) -- returns meId.
        ESerialiserId GetId() const { return meId; }

    protected:
        // Mode/lock/buffer state owned by BaseSerialiser's own TU. Opaque here so
        // meId sits at its asm-attested +0x28 offset. (8 fields x 4 bytes == 0x20,
        // i.e. through miStaticBufferSize per the DWARF member order.)
        unsigned char maHeaderState[0x28]; // @0x00 -- grown/named by own TU

        ESerialiserId meId;                // @0x28 -- read by RequestInterface
    };
}
