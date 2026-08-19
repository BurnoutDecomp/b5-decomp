#include "GameShared/GameClasses/RenderWare/FixableVolume.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   BrnPhysics::Props::FixableVolume::FixUp   @ 0x828A87A0
//   BrnPhysics::Props::FixableVolume::FixDown @ 0x828A8830
//
// Both functions read/write the volume type slot at byte +0x40 of the embedded
// rw::collision::Volume. The X360 build uses a single u32-sized slot there (32-bit
// host): on disk it holds the volume-type enum (1..5); in memory it holds the shared
// runtime Volume-handler pointer gVolumeVTable[enum] (dword_8327EEE0, filled by
// Volume::InitializeVTable @0x82BB03A8 in SDKs/EATech/rwcollision/volume.cpp).
//
// HOST REPRESENTATION (2026-08-18, wave Q5 integration): an x64 pointer is 8 bytes and
// the slot is 4 (it is immediately followed by the +0x44 box half-extent X lane, and the
// record is the 96-byte serialised stride -- it cannot grow). The previous host body
// stored gVolumeVTable[type] through a `void**` and CLOBBERED +0x44..+0x47 of every
// prop volume. The host therefore keeps the on-disk TYPE ENUM in the slot for the whole
// lifetime of the record and every reader recovers the console's pointer as
// gVolumeVTable[type] (SDKs/EATech/rwcollision/volume_debug_access.h Volume::GetVTable,
// the vendor/renderware/collision TU-local descriptor views). FixUp and FixDown are
// consequently the console's VALIDATION half only: the enum<->pointer swap is the
// identity on the host. Derivation: scratchpad/waveQ5/rwc3.owner.md section 7.

namespace BrnPhysics
{
namespace Props
{
    // Byte offset of the volume type slot within the Volume payload (X360 +0x40).
    static const u32 KU_VOLUME_TYPE_OFFSET = 0x40;

    // @ 0x828A87A0
    // r11 = *(this+0x40)            ; the on-disk type enum
    // r11 = gVolumeVTable[enum]     ; lwzx r11, enum*4, &gVolumeVTable[0]   (host: identity)
    // *(this+0x40) = r11            ; store the handler pointer back          (host: identity)
    // switch(enum) { case 1..5: ok; default: ASSERT "Unsupported type\n" (line 92) }
    void FixableVolume::FixUp()
    {
        u8* lpThis = reinterpret_cast<u8*>(this);
        u32* lpSlot = reinterpret_cast<u32*>(lpThis + KU_VOLUME_TYPE_OFFSET);

        const u32 luType = *lpSlot;
        *lpSlot = luType;   // console: gVolumeVTable[luType]; host: the enum stays

        switch (luType)
        {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                return;
            default:
                CGS_ASSERT(false, "Unsupported type\n");
                return;
        }
    }

    // @ 0x828A8830
    // r11 = *(this+0x40)            ; the runtime handler pointer          (host: the enum)
    // r11 = *r11                    ; lwz r11,0(r11) — first word == the type enum
    //                               ; (descriptor word 0 IS typeID; host: identity)
    // *(this+0x40) = r11            ; store the enum back
    // switch(enum) { case 1..5: ok; default: ASSERT "Unsupported type\n" (line 156) }
    void FixableVolume::FixDown()
    {
        u8* lpThis = reinterpret_cast<u8*>(this);
        u32* lpSlot = reinterpret_cast<u32*>(lpThis + KU_VOLUME_TYPE_OFFSET);

        const u32 luType = *lpSlot;
        *lpSlot = luType;   // console: gVolumeVTable[luType]->typeID == luType

        switch (luType)
        {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                return;
            default:
                CGS_ASSERT(false, "Unsupported type\n");
                return;
        }
    }
}
}
