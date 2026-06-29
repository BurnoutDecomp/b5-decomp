#pragma once

// CgsInput::InputPads -- the input module's owner of the four physical controller pads.
// Recovered from BURNOUT_X360_ARTIST.XEX with member names/types from the DecFIGS DWARF
// (CgsInputPads.h). This header models only the slice needed by GetDebugGamePad (the
// per-port pad array); the rest of InputPads (the per-pad bind/rumble state at the head,
// the surface-rumble timing tables, the enable/pause/force-feedback flags) is reconstructed
// in the full CgsInputPads.cpp TU.
#include "types.hpp"
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"  // CgsInput::KU_NUMBER_OF_PADS
#include "GameShared/GameClasses/System/Input/Devices/X360/CgsInputDeviceX360Pad.h" // CgsInput::DeviceX360Pad (452-byte fielded layout)

namespace CgsInput
{
    // The per-port physical-pad device record CgsInput::DeviceX360Pad is now the real fielded
    // 452-byte (0x1C4) class, homed in CgsInputDeviceX360Pad.h (its reconstruction TU). InputPads
    // owns an inline array of four of these starting at byte offset 0x78; the 452-byte stride both
    // GetDebugGamePad (`452 * liPortIndex + this + 120` @0x823A5F38) and InputPads::Construct
    // (`v3 = this + 120; DeviceX360Pad::Construct(v3); v3 += 452` x4 @0x828E7238) use is preserved
    // by the layout pin in that header (sizeof == 452).

    // INPUT-PADS SLICE (FLAGGED): only the per-port pad array + GetDebugGamePad are modelled.
    // The 0x78-byte head (the four per-pad {player,port,...} bind records the Construct/Bind
    // path writes via `this+0`, `this+1928`, `this+1976`, ...) and the tail (surface-rumble
    // timing floats + the enable/pause/force-feedback flag bytes Construct sets at this+4304..06)
    // are owned by the full CgsInputPads.cpp TU. maPadsHeaderStorage holds the head as opaque
    // bytes so the maPads array sits at its X360 byte offset 0x78.
    class InputPads
    {
    public:
        // X360 0x823A5F38 (CgsInputPads.h:251). Returns the address of the liPortIndex-th physical
        // pad record (`452 * liPortIndex + this + 120`). The two range asserts (liPortIndex >= 0,
        // liPortIndex < KU_NUMBER_OF_PADS) are non-gating tripwires. Hex-Rays names the result
        // "DebugGamePad" because the sole caller (BrnGameModule::HandleInputBindResult @0x823A8C38)
        // hands it to CgsDev::DebugManager::SetGamePad as the active debug controller pad.
        DeviceX360Pad* GetDebugGamePad(s32 liPortIndex);

    private:
        u8            maPadsHeaderStorage[0x78];      // @ +0x00 .. +0x78 (bind/rumble head -- full TU)
        DeviceX360Pad maPads[KU_NUMBER_OF_PADS];      // @ +0x78, stride 0x1C4
        // surface-rumble timing tables + enable/pause/force-feedback flags follow -- full TU.
    };
}
