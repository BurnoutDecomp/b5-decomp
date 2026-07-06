#pragma once

// CgsInput::InputPads -- the input module's owner of the physical controller pads, the
// per-player/per-port bind tables, and the jolt/rumble force-feedback effect tables.
//
// Recovered from BURNOUT_X360_ARTIST.XEX with member names/types from the DecFIGS DWARF
// (CgsInputPads.h). Every byte offset below is grounded in this TU's assembly
// (CgsInputPads.cpp Construct/Destruct/InitializePads/Prepare/FillRawData/PlayJoltEvent/
// PlayRumbleEvent/ChangeVolumeRumbleEvent/StopRumbleEvent/UpdatePadRumble @0x828E72xx /
// 0x828EFxxx / 0x828DCxxx). The X360 build supports KU_NUMBER_OF_PADS (==4) pads, so the
// per-pad arrays the DWARF dimensions [7] are dimensioned [KU_NUMBER_OF_PADS] here (the
// X360 Construct/InitializePads loops iterate exactly 4 times).
#include "types.hpp"
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"  // CgsInput::KU_NUMBER_OF_PADS, Device::WheelFFSpring
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h" // CgsInput::InputIO event payloads (JoltEffect, OutputBuffer, ...)
#include "GameShared/GameClasses/System/Input/Devices/X360/CgsInputDeviceX360Pad.h" // CgsInput::DeviceX360Pad (452-byte fielded layout)
#include "rw/rwcore_structs.h"                                   // rw::IResourceAllocator (Prepare param)

#include <cstddef>   // offsetof (the X360-pinned member-offset static_asserts below)
#include <cstring>   // std::memcpy (SetActionMapping)

// rw::core::controller::DeviceState -- the RenderWare controller device-state record the
// per-port override slots point at. Construct only stores null pointers and no body in this
// TU dereferences the type, so an incomplete forward declaration is sufficient. Promote to
// the real RenderWare layout when the controller-device-state TU lands.
namespace rw { namespace core { namespace controller { struct DeviceState; } } }

namespace CgsInput
{
    // KU_INPUT_MEMORY_BUFFER_SIZE / KU_MAX_NUMBER_OF_JOLT_EFFECTS et al. from the DWARF
    // (CgsInputPads.h:39-42). KU_MAX_NUMBER_OF_JOLT_EFFECTS / KU_MAX_NUMBER_OF_RUMBLE_EFFECTS
    // are the "2 effects per pad" dimension the asm loops (`cmplwi v6, 2`) confirm.
    const u32 KU_INPUT_MEMORY_BUFFER_SIZE          = 4096;
    const u32 KU_INPUT_MEMORY_EXTENDED_BUFFER_SIZE = 8192;
    const u32 KU_MAX_NUMBER_OF_JOLT_EFFECTS        = 2;
    const u32 KU_MAX_NUMBER_OF_RUMBLE_EFFECTS      = 2;

    // Debug "force-process this pad even when disconnected" rumble toggle. Read by
    // UpdatePadRumble (`if (byte_83085F80) lbProcess = true; else lbProcess = pad.IsConnected()`)
    // and by DeviceX360Pad::Update; it is the shared debug override that bypasses the connected
    // check. FLAGGED: no TU homes it yet; this TU is its provisional home (the rumble subsystem).
    // Promote/relocate when the debug-input override TU lands.
    extern bool gbForceRumbleOnDisconnectedPad;   // X360 global byte_83085F80

    // ---- Per-player bind record. maPlayers[player] indexed by player id. -------------------
    // Construct/Destruct write {mbBound=0/false @+0, miPort=-1 @+4} per record, stride 8.
    // PlayJoltEvent reads mbBound (`8*(player+241)` => +1928+8*player) then miPort
    // (`+1932+8*player`) to resolve a player id to its bound physical port.
    struct InputPlayer
    {
        bool mbBound;   // +0x00  set false in Construct/Destruct
        u8   mau8Pad01[3];
        s32  miPort;    // +0x04  resolved physical port, -1 when unbound
    };

    // ---- Per-port record (24-byte stride). InitializePads/Destruct zero {miField00 @+0,
    // mbField14 @+20}. Only the two written sub-fields are attested; the rest is opaque.
    struct InputPort
    {
        s32 miField00;          // +0x00  zeroed by InitializePads/Destruct
        u8  maOpaque04[0x14 - 0x04];
        u8  mbField14;          // +0x14  zeroed by InitializePads/Destruct
        u8  mau8Pad15[0x18 - 0x15];
    };

    // ============================================================================
    // CgsInput::InputPads -- 0x10D4-byte head + tail flag bytes.
    // ============================================================================
    class InputPads
    {
    public:
        // X360 0x828EFAF0. Zero/seed every per-pad bind + jolt/rumble table, then construct the
        // four physical DeviceX360Pads. (DWARF: void Construct(); the asm returns the last pad's
        // Construct result, which the caller ignores.)
        void Construct();

        // X360 0x828E72C0. Re-prime the jolt/rumble effect tables (times 0.0, ids/priorities -1),
        // mark the pads prepared, enable rumble (mbRumbleEnabled=true) and un-pause (mbRumblePaused
        // =false). Returns mbPrepared. The PS3 build passes a resource allocator; the X360 build
        // does not use it (the asm consumes only `this`).
        bool Prepare(rw::IResourceAllocator* lpInputGeneralAllocator);

        // X360 0x828EFBF0. Reset all per-player/per-port bind records to unbound.
        void Destruct();

        // X360 0x828DC128. Queue a jolt force-feedback effect on the requesting player's pad
        // (resolving player->port). No free slot or already-higher-priority slot => no-op.
        void PlayJoltEvent(const InputIO::PlayJoltEffectEvent& lJoltEvent);

        // X360 0x828DC208. Queue a rumble force-feedback effect (jolt envelope + id + volume).
        void PlayRumbleEvent(const InputIO::PlayRumbleEffectEvent& lRumbleEvent);

        // X360 0x828DC318. Update the volume of an already-playing rumble effect (matched by id).
        void ChangeVolumeRumbleEvent(const InputIO::ChangeVolumeRumbleEffectEvent& lChangeVolumeRumbleEvent);

        // X360 0x828DC3C0. Stop a playing rumble effect (matched by id): clear its slot.
        void StopRumbleEvent(const InputIO::StopRumbleEffectEvent& lStopRumbleEvent);

        // X360 0x823A5F38 (CgsInputPads.h:251) -- homed in CgsInputPads_GetDebugGamePad.cpp.
        // Returns the address of the liPortIndex-th physical pad record.
        DeviceX360Pad* GetDebugGamePad(s32 liPortIndex);

        // ---- WAVE54 accessors used by CgsInput::InputModule::ProcessMappingQueue / Release ----
        // The InputModule drains the post-world pad-mapping queue and copies each 112-byte
        // action-mapping payload into the addressed pad's private mapping storage; it also reads/
        // sets the prepared flag during Release. These accessors reproduce the exact stores the
        // X360 inlines (InputModule reaches maaMappingStorage @+0x7B8 and mbPrepared @+0xCFC
        // directly) while keeping the storage/flag private.

        // Copy the 112-byte (0x70) action-mapping payload into maaMappingStorage[liPort].
        void SetActionMapping(s32 liPort, const void* lpPayload)
        {
            std::memcpy(maaMappingStorage[liPort], lpPayload, 0x70);
        }

        // Read / set mbPrepared (@+0xCFC).
        bool IsPrepared() const   { return mbPrepared; }
        void SetPrepared(bool lbSet) { mbPrepared = lbSet; }

    private:
        // X360 0x828E7238. Zero the per-port records + the six head dwords, then construct the
        // four DeviceX360Pads (stride 452).
        void InitializePads();

        // X360 0x828E7350. Accumulate one pad's control floats into the per-action raw-button
        // array (max) and its axis floats into the running axis array (sum, clamped to [-1,1]).
        void FillRawData(f32* lafNewRawData, f32* lafAxisData, s32 liPortIndex);

        // X360 0x828EFC58. Per-pad: tick every active jolt + rumble effect through its envelope,
        // accumulate the strongest left/right motor magnitudes, age out finished effects, then
        // push the resulting magnitudes (or the wheel FF spring) to the DeviceX360Pad.
        void UpdatePadRumble(s32 liPort, s32 liPlayer, f32 lfTimeStep);

        // X360 0x828E7618. Evaluate one ADSR-style jolt envelope at lfTime (attack lerp / sustain
        // / release lerp / 0). lEnvelope = {attack, decay, sustain, release, peak, sustainLevel}.
        f32 UpdateJoltEnvelope(const InputIO::JoltEnvelope& lEnvelope, f32 lfTime);

        // Never-called layout pin so any drift in a member offset is a compile error.
        friend void _InputPads_AssertLayout();

        // ---- head (0x00 .. 0x78) ----
        InputPort     maPorts[KU_NUMBER_OF_PADS];   // +0x00  (24B stride; 4 records => +0x60)
        s32           miHead60;                     // +0x60  zeroed by InitializePads
        s32           miHead64;                     // +0x64
        s32           miHead68;                     // +0x68
        s32           miHead6C;                     // +0x6C
        s32           miHead70;                     // +0x70  zeroed by InitializePads/Destruct
        u8            mbHead74;                      // +0x74  zeroed by InitializePads/Destruct
        u8            mau8Pad75[0x78 - 0x75];        // +0x75  align to the pad array

        // ---- physical pad devices ----
        DeviceX360Pad maPads[KU_NUMBER_OF_PADS];    // +0x78, stride 0x1C4 => +0x788 (1928)

        // ---- per-player bind table ----
        InputPlayer   maPlayers[KU_NUMBER_OF_PADS]; // +0x788 (1928), stride 8 => +0x7A8 (1960)

        // ---- per-port resolved-player ids ----
        s32           maiPortToPlayer[KU_NUMBER_OF_PADS]; // +0x7A8 (1960), -1 when unbound => +0x7B8 (1976)

        // ---- per-pad action-mapping table (opaque; not indexed by any body in this TU) -------
        // Construct fills 112 (0x70) bytes per pad with 0xFF (the `stb -1` inner loop). The DWARF
        // names this maaMapping (ActionMapping[][]) but its element layout is not attested here and
        // no body in this TU indexes into it, so it is modelled as opaque per-pad storage that keeps
        // miCurrentSetOfStates at its asm offset (+0x97C / 2428... actually +0x978 / 2424).
        u8            maaMappingStorage[KU_NUMBER_OF_PADS][0x70]; // +0x7B8 (1976) => +0x978 (2424)

        s32           miCurrentSetOfStates;         // +0x978 (2424) zeroed by Construct

        // ---- per-frame action-down bitmap [2 sets][pads][112 actions] ----
        // Construct memsets 0x380 (896) bytes at +0x97C (2428): 2 * KU_NUMBER_OF_PADS * 112 = 896.
        bool          maaabActionsDown[2][KU_NUMBER_OF_PADS][112]; // +0x97C (2428) => +0xCFC (3324)

        bool          mbPrepared;                   // +0xCFC (3324) Construct=false, Prepare=true

        // ---- per-port device-state override pointers ----
        // X360 POINTER-WIDTH STORAGE: these are `const rw::core::controller::DeviceState*` on the X360
        // (4-byte pointers there), set to null by Construct and never dereferenced by any body in this TU.
        // Modelled as 32-bit slots (not native PC 8-byte pointers) so the following jolt/rumble tables keep
        // their X360 byte offsets (an 8-byte PC pointer array would push maJoltEffects off +0xD10). The
        // OverrideDeviceState setter (own TU) can store an index/handle here; promote when it lands.
        u32 mauOverrideDeviceState[KU_NUMBER_OF_PADS]; // +0xD00 (3328) => +0xD10 (3344); X360 const DeviceState*

        // ---- jolt effect tables [port][effect] (KU_MAX_NUMBER_OF_JOLT_EFFECTS == 2) ----
        InputIO::JoltEffect maJoltEffects[KU_NUMBER_OF_PADS][KU_MAX_NUMBER_OF_JOLT_EFFECTS]; // +0xD10 (3344), 48B => +0xE90 (3728)
        s32 maiJoltEffectPriorities[KU_NUMBER_OF_PADS][KU_MAX_NUMBER_OF_JOLT_EFFECTS];       // +0xE90 (3728) => +0xEB0 (3760)
        f32 mafJoltTime[KU_NUMBER_OF_PADS][KU_MAX_NUMBER_OF_JOLT_EFFECTS];                   // +0xEB0 (3760) => +0xED0 (3792)

        // ---- rumble effect tables [port][effect] (KU_MAX_NUMBER_OF_RUMBLE_EFFECTS == 2) ----
        InputIO::JoltEffect maRumbleEffects[KU_NUMBER_OF_PADS][KU_MAX_NUMBER_OF_RUMBLE_EFFECTS]; // +0xED0 (3792), 48B => +0x1050 (4176)
        f32 mafRumbleTime[KU_NUMBER_OF_PADS][KU_MAX_NUMBER_OF_RUMBLE_EFFECTS];                   // +0x1050 (4176) => +0x1070 (4208)
        f32 mafRumbleVolume[KU_NUMBER_OF_PADS][KU_MAX_NUMBER_OF_RUMBLE_EFFECTS];                 // +0x1070 (4208) => +0x1090 (4240)
        s32 maiRumbleIds[KU_NUMBER_OF_PADS][KU_MAX_NUMBER_OF_RUMBLE_EFFECTS];                    // +0x1090 (4240) => +0x10B0 (4272)
        s32 maiRumbleEffectPriorities[KU_NUMBER_OF_PADS][KU_MAX_NUMBER_OF_RUMBLE_EFFECTS];       // +0x10B0 (4272) => +0x10D0 (4304)

        // ---- rumble / force-feedback enable flags ----
        bool mbRumblePaused;        // +0x10D0 (4304) Construct=false, Prepare=false
        bool mbRumbleEnabled;       // +0x10D1 (4305) Construct=true,  Prepare=true
        bool mbWheelFFEnabled;      // +0x10D2 (4306) Construct=true

        // ---- wheel force-feedback spring (UpdatePadRumble reads mfSpringCoefficient/Saturation
        //      at +0x10D4 / +0x10D8 when publishing the wheel FF to a wheel pad) ----
        u8                          mau8Pad10D3[0x10D4 - 0x10D3]; // +0x10D3 align
        CgsInput::Device::WheelFFSpring mWheelFFSpring;           // +0x10D4 (4308)
    };
}

// Layout pin: never-called helper so any drift in a member offset is a compile error.
namespace CgsInput
{
    inline void _InputPads_AssertLayout()
    {
        static_assert(sizeof(InputPlayer) == 8, "InputPlayer must be 8 bytes");
        static_assert(sizeof(InputPort)   == 24, "InputPort must be 24 bytes (0x18 stride)");
        static_assert(offsetof(InputPads, maPads)                  == 0x78,   "maPads @ +0x78");
        static_assert(offsetof(InputPads, maPlayers)               == 0x788,  "maPlayers @ +0x788 (1928)");
        static_assert(offsetof(InputPads, maiPortToPlayer)         == 0x7A8,  "maiPortToPlayer @ +0x7A8 (1960)");
        static_assert(offsetof(InputPads, miCurrentSetOfStates)    == 0x978,  "miCurrentSetOfStates @ +0x978 (2424)");
        static_assert(offsetof(InputPads, maaabActionsDown)        == 0x97C,  "maaabActionsDown @ +0x97C (2428)");
        static_assert(offsetof(InputPads, mbPrepared)              == 0xCFC,  "mbPrepared @ +0xCFC (3324)");
        static_assert(offsetof(InputPads, mauOverrideDeviceState)  == 0xD00,  "mauOverrideDeviceState @ +0xD00 (3328)");
        static_assert(offsetof(InputPads, maJoltEffects)           == 0xD10,  "maJoltEffects @ +0xD10 (3344)");
        static_assert(offsetof(InputPads, maiJoltEffectPriorities) == 0xE90,  "maiJoltEffectPriorities @ +0xE90 (3728)");
        static_assert(offsetof(InputPads, mafJoltTime)             == 0xEB0,  "mafJoltTime @ +0xEB0 (3760)");
        static_assert(offsetof(InputPads, maRumbleEffects)         == 0xED0,  "maRumbleEffects @ +0xED0 (3792)");
        static_assert(offsetof(InputPads, mafRumbleTime)           == 0x1050, "mafRumbleTime @ +0x1050 (4176)");
        static_assert(offsetof(InputPads, mafRumbleVolume)         == 0x1070, "mafRumbleVolume @ +0x1070 (4208)");
        static_assert(offsetof(InputPads, maiRumbleIds)            == 0x1090, "maiRumbleIds @ +0x1090 (4240)");
        static_assert(offsetof(InputPads, maiRumbleEffectPriorities) == 0x10B0, "maiRumbleEffectPriorities @ +0x10B0 (4272)");
        static_assert(offsetof(InputPads, mbRumblePaused)          == 0x10D0, "mbRumblePaused @ +0x10D0 (4304)");
        static_assert(offsetof(InputPads, mbRumbleEnabled)         == 0x10D1, "mbRumbleEnabled @ +0x10D1 (4305)");
        static_assert(offsetof(InputPads, mbWheelFFEnabled)        == 0x10D2, "mbWheelFFEnabled @ +0x10D2 (4306)");
        static_assert(offsetof(InputPads, mWheelFFSpring)          == 0x10D4, "mWheelFFSpring @ +0x10D4 (4308)");
    }
}
