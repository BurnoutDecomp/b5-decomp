#include "GameShared/GameClasses/RenderWare/FixableVolume.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   BrnPhysics::Props::FixableVolume::FixUp   @ 0x828A87A0
//   BrnPhysics::Props::FixableVolume::FixDown @ 0x828A8830
//
// Both functions read/write the volume type slot at byte +0x40 of the embedded
// rw::collision::Volume. The X360 build uses a single u32-sized slot there (32-bit
// host): on disk it holds the volume-type enum (1..5); in memory it holds the shared
// runtime Volume-handler pointer gVolumeVTable[enum].

namespace rw
{
namespace collision
{
    // The shared Volume processing vtable (dword_8327EEE0), defined once in
    // SDKs/EATech/rwcollision/volume.cpp (external linkage). Slot 0 is null; slots
    // 1..6 hold the per-type handler entry points. Referenced (never redefined) here.
    extern void* gVolumeVTable[7];
}
}

namespace BrnPhysics
{
namespace Props
{
    // Byte offset of the volume type slot within the Volume payload (X360 +0x40).
    static const u32 KU_VOLUME_TYPE_OFFSET = 0x40;

    // @ 0x828A87A0
    // r11 = *(this+0x40)            ; the on-disk type enum
    // r11 = gVolumeVTable[enum]     ; lwzx r11, enum*4, &gVolumeVTable[0]
    // *(this+0x40) = r11            ; store the handler pointer back
    // switch(enum) { case 1..5: ok; default: ASSERT "Unsupported type\n" (line 92) }
    void FixableVolume::FixUp()
    {
        u8* lpThis = reinterpret_cast<u8*>(this);
        void** lppSlot = reinterpret_cast<void**>(lpThis + KU_VOLUME_TYPE_OFFSET);

        const u32 luType = *reinterpret_cast<u32*>(lppSlot);
        *lppSlot = rw::collision::gVolumeVTable[luType];

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
    // r11 = *(this+0x40)            ; the runtime handler pointer
    // r11 = *r11                    ; lwz r11,0(r11) — first word == the type enum
    // *(this+0x40) = r11            ; store the enum back
    // switch(enum) { case 1..5: ok; default: ASSERT "Unsupported type\n" (line 156) }
    void FixableVolume::FixDown()
    {
        u8* lpThis = reinterpret_cast<u8*>(this);
        u32* lpSlot = reinterpret_cast<u32*>(lpThis + KU_VOLUME_TYPE_OFFSET);

        void* lpHandler = *reinterpret_cast<void**>(lpSlot);
        const u32 luType = *reinterpret_cast<u32*>(lpHandler);
        *lpSlot = luType;

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
