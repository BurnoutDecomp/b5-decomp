#pragma once

#include "types.hpp"

// BrnPhysics::Props::FixableVolume — a thin, fix-up/fix-down-aware view over an
// rw::collision::Volume embedded in a serialised prop-physics resource. The on-disk
// form stores a small volume-type enum (1..5) in the Volume's type slot at +0x40;
// FixUp() rebinds that enum to the shared runtime Volume vtable pointer
// (rw::collision::gVolumeVTable[enum]) so the loaded volume is usable, and FixDown()
// reverses it (reading the type enum back out of the handler the pointer refers to)
// so the resource can be re-serialised. Recovered store-for-store from the X360
// ARTIST build:
//   FixUp   @ 0x828A87A0   (called by PropPhysicsDataHeader::FixUp)
//   FixDown @ 0x828A8830   (called by PropPhysicsDataHeader::FixDown)
//
// LAYOUT: the X360 asm only touches the type slot at byte +0x40 of the Volume; the
// Volume itself is the canonical 96-byte rw::collision payload (see
// SDKs/EATech/rwcollision/volume.cpp, `maPayload[96]`). FixableVolume adds no data
// members. The base Volume is modelled here as an opaque 96-byte payload so this
// header does not depend on (or re-declare) the rwcollision Volume class that is
// defined privately in volume.cpp; the only field the relocation logic reads/writes
// is the type slot at +0x40, accessed by that byte offset (serialised-payload
// raw-offset precedent).

namespace rw { namespace collision { class Volume; } }

namespace BrnPhysics
{
namespace Props
{
    // Opaque rw::collision::Volume payload (96 bytes). Mirrors volume.cpp's
    // `u8 maPayload[96]`; the type slot the relocation logic touches sits at +0x40.
    struct FixableVolume
    {
        // @ 0x828A87A0 — disk enum (+0x40) -> runtime vtable pointer
        // (gVolumeVTable[enum]). Asserts the enum is a supported type (1..5).
        void FixUp();

        // @ 0x828A8830 — runtime vtable pointer (+0x40) -> disk enum (the type word
        // the pointed-at handler carries). Asserts the recovered enum is 1..5.
        void FixDown();

    private:
        u8 maPayload[96];
    };
}
}
