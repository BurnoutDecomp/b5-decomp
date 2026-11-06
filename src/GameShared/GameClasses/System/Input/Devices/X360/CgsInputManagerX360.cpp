// ============================================================================
// CgsInput::ManagerX360 -- the Xbox 360 input manager. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (asm authoritative; no DecFIGS DWARF for this X360-only class).
//
// The existing exported methods here are UpdateConnectedDevices (@0x828DC480), BindToPort
// (@0x828E7718) and UnbindFromPort (@0x828E7808); Update is recovered at @0x828F0028. The
// manager reset methods wrap the stores inlined in InputPads::InitializePads/Destruct.
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

    // The following lifecycle stores are inlined in InputPads::InitializePads @0x828E7238
    // and InputPads::Destruct @0x828EFBF0; ARTIST has no separate ManagerX360 constructor or
    // destructor call. These methods expose those exact manager-owned stores to InputPads.
    void ManagerX360::Construct()
    {
        miNumBoundPads = 0;               // InputPads::InitializePads: stw 0, +0x70
        mu8Unknown74 = 0;                  // InputPads::InitializePads: stb 0, +0x74
        for (u32 luPort = 0; luPort < KU_NUMBER_OF_INPUT_PORTS; ++luPort)
        {
            mapBoundPads[luPort] = 0;      // +0x60 .. +0x6C
        }
        for (u32 luPort = 0; luPort < KU_NUMBER_OF_INPUT_PORTS; ++luPort)
        {
            maDevices[luPort].meDeviceType = 0;
            maDevices[luPort].mbConnected = 0;
        }
    }

    void ManagerX360::Destruct()
    {
        for (u32 luPort = 0; luPort < KU_NUMBER_OF_INPUT_PORTS; ++luPort)
        {
            maDevices[luPort].meDeviceType = 0;
            maDevices[luPort].mbConnected = 0;
        }
        miNumBoundPads = 0;
        mu8Unknown74 = 0;
        // InputPads::Destruct does not clear mapBoundPads.
    }

    CgsInput::Device::EType ManagerX360::GetConnectedDeviceType(s32 liPort)
    {
        return static_cast<CgsInput::Device::EType>(maDevices[liPort].meDeviceType);
    }

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
    // Update @0x828F0028. The native loop walks the bound-pad pointer table, then uses each
    // non-null pad's stored port (+0xF0) to select the slot record. It calls DeviceX360Pad::Update
    // with the slot's connected flag, saved port, type and state pointer in that order.
    // ============================================================================
    void ManagerX360::Update()
    {
        for (u32 luPort = 0; luPort < KU_NUMBER_OF_INPUT_PORTS; ++luPort)
        {
            DeviceX360Pad* lpPad = mapBoundPads[luPort];
            if (lpPad != 0)
            {
                const s32 liPadPort = lpPad->mePort;
                ManagerX360Device& lrDevice = maDevices[liPadPort];
                lpPad->Update(lrDevice.mbConnected != 0,
                              liPadPort,
                              lrDevice.meDeviceType,
                              lrDevice.maState);
            }
        }
    }

    // ============================================================================
    // BindToPort @0x828E7718. Bind port lePort to physical pad lpPad. Returns true (and binds)
    // only when the slot record has a resolved device type; false otherwise.
    // ============================================================================
    bool ManagerX360::BindToPort(DeviceX360Pad* lpPad, u32 lePort)
    {
        CGS_ASSERT(lpPad != 0, "lpPad");                                              // line 288
        CGS_ASSERT(lePort < KU_NUMBER_OF_INPUT_PORTS,
                   "(lePort >= E_INPUTPORT_FIRST) && (lePort < E_INPUTPORT_COUNT)");        // line 289
        CGS_ASSERT(mapBoundPads[lePort] == 0, "mapBoundPads[lePort] == NULL");        // line 290

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
        CGS_ASSERT(lpPad != 0, "lpPad");                                              // line 325
        CGS_ASSERT(lePort < KU_NUMBER_OF_INPUT_PORTS,
                   "(lePort >= E_INPUTPORT_FIRST) && (lePort < E_INPUTPORT_COUNT)");        // line 326
        CGS_ASSERT(mapBoundPads[lePort] == lpPad, "mapBoundPads[lePort] == lpPad");         // line 327

        lpPad->mbConnected = 0;             // *(lpPad+0x10) = 0
        lpPad->mePort      = -1;            // *(lpPad+0xF0) = -1
        mapBoundPads[lePort] = 0;     // v4[lePort+24] = 0
        --miNumBoundPads;                   // --*(this+0x70)
    }
}
