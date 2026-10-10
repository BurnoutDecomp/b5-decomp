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
        // CgsInputModuleIO.h -- one action's game-id slots (four signed bytes).
        struct ActionMapping
        {
            s8 maiGameIds[4];
        };

        // CgsInputModuleIO.h -- the pad-mapping request record carried by the
        // post-world buffer's EventQueue<PadMapping, 4>. The record is 116 bytes (0x74): the
        // AddEvent element stride (`li r5, 0x74` into memcpy, `mulli r11, r11, 0x74`).
        // PostWorldInputBuffer::PostMappingRequest builds it as the target port word followed
        // by a 0x70-byte copy of the caller's mapping table, so this build carries 28
        // ActionMappings (the original header's [34] is the other platform's table).
        struct PadMapping
        {
            static const s32 KI_NUM_ACTION_MAPPINGS = 28;

            s32           miPortId;                            // +0x00 (-1 == every pad)
            ActionMapping maMapping[KI_NUM_ACTION_MAPPINGS];   // +0x04 .. +0x74
        };
    }

    namespace Device
    {
        // CgsInput::Device::EType -- DWARF CgsInputDevice.h:12 (verbatim). The type byte a device
        // is bound with: the XInput SubType the X360 ManagerX360 scan reads (XINPUT_DEVSUBTYPE_GAMEPAD
        // 1, _WHEEL 2) and the `cmpwi r11, 2` wheel test of DeviceX360Pad::Update / SetRumble and
        // InputPads::UpdatePadRumble.
        enum EType : s32
        {
            E_NO_DEVICE_TYPE    = 0,
            E_PAD_DEVICE_TYPE   = 1,
            E_WHEEL_DEVICE_TYPE = 2,
            E_NUM_DEVICE_TYPES  = 3
        };

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
