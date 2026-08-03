#pragma once

// Input-system result enums. Recovered from the DecFIGS DWARF (CgsInputTypes.h).
#include "types.hpp"

namespace CgsInput
{
    // Number of physical controller pads the input system supports. The pad-index asserts in the
    // input IO accessors + the controller bridges check ports against this (X360 GetPadInfo asserts
    // `iPort < 4`; GetPadInfoForPlayer0 asserts `miPlayer0ControllerPort <= CgsInput::KU_NUMBER_OF_PADS`).
    const u32 KU_NUMBER_OF_PADS = 4;

    enum EBindResult : s32
    {
        E_BINDRESULTOK                 = 0,
        E_BINDRESULTPLAYERALREADYBOUND = 1,
        E_BINDRESULTPORTALREADYBOUND   = 2,
        E_BINDRESULTINVALIDPLAYER      = 3,
        E_BINDRESULTINVALIDPORT        = 4
    };

    enum EUnbindResult : s32
    {
        E_UNBINDRESULTOK            = 0,
        E_UNBINDRESULTINVALIDPLAYER = 1,
        E_UNBINDRESULTPLAYERNOTBOUND = 2
    };

    namespace InputIO
    {
        // Pad-mapping request record carried by the input module's PadMapping event queues
        // (EventQueue<PadMapping, 4> and EventQueue<PadMapping, 7>).
        //
        // SIZE-ONLY HOME (FLAGGED): the X360 enqueue path block-copies this record whole --
        // CgsInput::InputIO::PadMapping>::AddEvent @0x828EA820 uses a 116-byte (0x74) element
        // stride (`li r5, 0x74 # Size` into memcpy and `mulli r11, r11, 0x74` for the slot
        // address), and PadMapping,4>::Construct @0x828F2D00 places its inline maEvents buffer
        // at this+0xC, so the record is exactly 116 bytes. The internal field layout is NOT
        // attested by this TU's assembly (AddEvent only block-copies; the boot trace never
        // enters a field-level body) and the full layout drags in the un-homed ActionMapping[34]
        // table -- modelled here as an opaque 116-byte span so the queue instantiations compile
        // and round-trip the record byte-exactly. Promote to the real fielded layout when the
        // PadMapping full-reconstruction TU (ActionMapping[34]) lands; its size must stay 116.
        struct PadMapping
        {
            u8 maOpaque[116]; // 0x74 -- attested by the AddEvent memcpy stride @0x828EA820
        };
    }

    namespace Device
    {
        // CgsInput::Device::WheelFFSpring - the wheel force-feedback "spring" centring
        // parameters the post-world input pass copies out of the vehicle output interface
        // and republishes for the rumble/FFB driver.
        //
        // SIZE FROM ASM: PostWorldInputBuffer::Set/GetWheelFFSpring (X360 0x823B0F80 / 0x828E6C80)
        // and the InputModule::PostWorldUpdate consumer (0x828F8478) move exactly two 32-bit
        // words (`lwz`/`stw` pairs at +0 and +4) -- so the record is 8 bytes. The producer is the
        // vehicle output interface (DoUpdate_InputPostWorld passes `Veh + 2164`).
        //
        // FLAGGED: the two words are copied as raw 32-bit words (the X360 compiler emitted integer
        // load/store for this trivially-copyable POD), so the asm does not prove float vs int. The
        // FFB "spring" domain (a centring force + its strength/damping coefficient) makes a
        // {coefficient, saturation} float pair the most likely intent; modelled as two f32 with
        // inferred names. Promote names/types when the vehicle-output WheelFFSpring producer TU
        // (the +2164 sub-record of BrnVehicleOutputInterface) lands. Size must stay 8 bytes.
        //
        // ⭐ 2026-08-03 (VehiclePhysics own-block wave): the DWARF NAMES ARE NOW KNOWN and they are
        // NOT these. references/DecFIGS/dwarfdump/.../Input/Devices/CgsInputDevice.h:57-63 declares
        //     struct WheelFFSpring { float32_t mfStrength; float32_t mfOffset; }
        // -- two f32, so the SIZE and the TYPES above are confirmed (and independently so: the
        // producer VehiclePhysics::UpdateDriving @0x82638720/@0x826387D0 writes them with `stfs` at
        // this+0x13D0 and this+0x13D4, floats, and VehiclePhysics::mbRollingInAir sits at 0x13D8).
        // Only the two NAMES are wrong, and "offset" (a centring offset) is a different quantity
        // from "saturation". NOT renamed here: this type has consumers in the input layer outside
        // this wave's scope, and a rename is a mechanical follow-up, not a discovery.
        struct WheelFFSpring
        {
            f32 mfSpringCoefficient;   // +0x00  DWARF name: mfStrength
            f32 mfSpringSaturation;    // +0x04  DWARF name: mfOffset
        };
    }
}
