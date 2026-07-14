// ============================================================================
// CgsInput::ManagerX360 -- the Xbox 360 input manager. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (asm authoritative; no DecFIGS DWARF for this X360-only class).
//
// All three methods are now homed here: UpdateConnectedDevices (@0x828DC480),
// BindToPort (@0x828E7718) and UnbindFromPort (@0x828E7808). UnbindFromPort's two inlined
// pad writes reach DeviceX360Pad's private members through the `friend class ManagerX360;`
// grant in CgsInputDeviceX360Pad.h (anticipated by that header's WAVE54 note).
// ============================================================================

#include "GameShared/GameClasses/System/Input/Devices/X360/CgsInputManagerX360.h"
#include "GameShared/GameClasses/System/Input/Devices/X360/CgsInputDeviceX360Pad.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "types.hpp"

// ---- Xbox 360 XDK entry points (real prototypes live in <xinput.h>/<xtl.h>). Declared as
// extern "C" free functions, mirroring the sibling device TU (CgsInputDeviceX360Pad.cpp). The
// state / capabilities buffers are passed by the void* address of the local scratch record. ----
extern "C" u32 XInputGetState(u32 dwUserIndex, void* pState);
extern "C" u32 XInputGetCapabilities(u32 dwUserIndex, u32 dwFlags, void* pCapabilities);

namespace CgsInput
{
    // XINPUT_CAPABILITIES-shaped scratch buffer XInputGetCapabilities fills. Only mbSubType is
    // asm-attested (UpdateConnectedDevices reads the SubType byte @+0x01: 1 == gamepad, 2 ==
    // wheel); the remaining fields are the canonical XDK tail, present only to size the buffer.
    struct InputCapabilities
    {
        u8  mbType;         // +0x00  XINPUT_CAPABILITIES::Type
        u8  mbSubType;      // +0x01  XINPUT_CAPABILITIES::SubType (v7 in the asm)
        u16 muFlags;        // +0x02  XINPUT_CAPABILITIES::Flags
        u8  maTail[16];     // +0x04  XINPUT_GAMEPAD + XINPUT_VIBRATION remainder (20-byte struct)
    };

    // XInputGetCapabilities dwFlags: XINPUT_FLAG_GAMEPAD (the asm passes the immediate 1).
    static const u32 KU_XINPUT_FLAG_GAMEPAD = 1;
    // ============================================================================
    // UpdateConnectedDevices @0x828DC480. Poll each of the four XInput slots. On a failed
    // XInputGetState the slot is cleared. On success: if the slot is already connected (or the
    // capabilities query fails) the existing sub-type is kept; otherwise the freshly-resolved
    // SubType (1 gamepad / 2 wheel) is latched. Any connected slot re-latches its raw state into
    // the record; every other outcome clears both the connected flag and the device type.
    // ============================================================================
    void ManagerX360::UpdateConnectedDevices()
    {
        for (u32 i = 0; i < KU_NUMBER_OF_INPUT_PORTS; ++i)
        {
            ManagerX360Device& lrDevice = maDevices[i];

            u8  lauState[16];                                   // v5: XINPUT_STATE scratch
            u32 luResult = XInputGetState(i, lauState);         // poll the slot
            if (luResult != 0)
            {
                // Slot poll failed -> no device present.
                lrDevice.mbConnected  = 0;                      // *(a1+20) = 0
                lrDevice.meDeviceType = 0;                      // *a1 = 0
                continue;                                       // LABEL_16: a1 += 24
            }

            InputCapabilities lCaps;                            // v6/v7
            if (lrDevice.mbConnected ||
                (luResult = XInputGetCapabilities(i, KU_XINPUT_FLAG_GAMEPAD, &lCaps)) != 0)
            {
                // Already connected this slot, or the capabilities query failed.
                if (lrDevice.meDeviceType != 0 && luResult == 0)
                {
                    lrDevice.mbConnected = 1;                   // *v4 = 1
                    XInputGetState(i, lrDevice.maState);        // re-latch into the record (a1+4)
                    continue;                                   // goto LABEL_16
                }
            }
            else
            {
                // Freshly connected -> resolve the device sub-type from the capabilities.
                if (lCaps.mbSubType == 1)                       // v7 == 1
                {
                    lrDevice.meDeviceType = 1;                  // *a1 = 1
                    lrDevice.mbConnected  = 1;                  // *v4 = 1
                    XInputGetState(i, lrDevice.maState);        // re-latch (a1+4)
                    continue;                                   // goto LABEL_16
                }
                if (lCaps.mbSubType == 2)                       // v7 == 2
                {
                    lrDevice.meDeviceType = 2;                  // *a1 = 2
                    lrDevice.mbConnected  = 1;                  // *v4 = 1
                    XInputGetState(i, lrDevice.maState);        // re-latch (a1+4)
                    continue;                                   // goto LABEL_16
                }
                lrDevice.meDeviceType = 0;                      // *a1 = 0 (unknown sub-type)
            }

            lrDevice.mbConnected  = 0;                          // *v4 = 0
            lrDevice.meDeviceType = 0;                          // *a1 = 0
        }
    }

    // ============================================================================
    // BindToPort @0x828E7718. Bind port lePort to physical pad lpPad. Returns true (and binds)
    // only when the slot record has a resolved device type; false otherwise.
    // ============================================================================
    bool ManagerX360::BindToPort(DeviceX360Pad* lpPad, u32 lePort)
    {
        CGS_ASSERT(lpPad != nullptr, "lpPad");                                              // line 288
        CGS_ASSERT(lePort < KU_NUMBER_OF_INPUT_PORTS,
                   "(lePort >= E_INPUTPORT_FIRST) && (lePort < E_INPUTPORT_COUNT)");        // line 289
        CGS_ASSERT(mapBoundPads[lePort] == nullptr, "mapBoundPads[lePort] == NULL");        // line 290

        if (maDevices[lePort].meDeviceType == 0)
            return false;

        mapBoundPads[lePort] = lpPad;                                    // stwx lpPad, mapBoundPads[lePort]
        ++miNumBoundPads;                                                // ++*(this+0x70)
        lpPad->BindToPort(lePort, maDevices[lePort].meDeviceType);       // pad.BindToPort(lePort, subType)
        return true;
    }

    // ============================================================================
    // UnbindFromPort @0x828E7808. Unbind port lePort from lpPad: clear the pad's connected flag
    // and bound-port (two inlined writes into the pad's private members), clear the bound-pad
    // table slot, and decrement the bound count.
    // ============================================================================
    void ManagerX360::UnbindFromPort(DeviceX360Pad* lpPad, u32 lePort)
    {
        CGS_ASSERT(lpPad != nullptr, "lpPad");                                              // line 325
        CGS_ASSERT(lePort < KU_NUMBER_OF_INPUT_PORTS,
                   "(lePort >= E_INPUTPORT_FIRST) && (lePort < E_INPUTPORT_COUNT)");        // line 326
        CGS_ASSERT(mapBoundPads[lePort] == lpPad, "mapBoundPads[lePort] == lpPad");         // line 327

        lpPad->mbConnected = 0;             // *(lpPad+0x10) = 0
        lpPad->mePort      = -1;            // *(lpPad+0xF0) = -1
        mapBoundPads[lePort] = nullptr;     // v4[lePort+24] = 0
        --miNumBoundPads;                   // --*(this+0x70)
    }
}
