#pragma once

// CgsInput::ManagerX360 -- the Xbox 360 input manager: it polls the four physical XInput
// user slots each frame, tracks per-slot connection state + device sub-type, and binds a
// connected slot to a CgsInput::DeviceX360Pad when the higher-level InputPads asks.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. There is NO DecFIGS DWARF for this class (it
// is X360-only; the DecFIGS build is PS3). Every offset, store and constant below is
// grounded in this TU's assembly:
//   UpdateConnectedDevices @0x828DC480
//   BindToPort             @0x828E7718
//   UnbindFromPort         @0x828E7808
// baked __FILE__: ...gameshared\gameclasses\system\input\Devices/X360/CgsInputManagerX360.cpp
//
// Layout facts (all asm-attested):
//   * A four-element array of 24-byte (0x18) per-slot device records lives at +0x00. The
//     stride is proven by UpdateConnectedDevices (`r31 += 0x18` per iteration) and by
//     BindToPort's `24 * lePort` index. Each record is { meDeviceType @+0x00 (s32),
//     maState @+0x04 (16-byte XINPUT_STATE), mbConnected @+0x14 (u8) }.
//   * A four-element mapBoundPads[] array of DeviceX360Pad* lives at +0x60. BindToPort
//     indexes it as 4 * (lePort + 24) == 0x60 + 4*lePort; UnbindFromPort as v4[lePort+24].
//   * miNumBoundPads @+0x70: bumped on bind (BindToPort ++*(this+0x70)), decremented on
//     unbind (UnbindFromPort --*(this+0x70)).
//
// WAVE54 NOTE: only BindToPort (@0x828E7718) is homed in this wave. UpdateConnectedDevices
// and UnbindFromPort are declared-only here (their proposed bodies were rejected by the
// verifier and land in a later wave); UnbindFromPort's inlined pad writes will need a
// `friend class ManagerX360;` in DeviceX360Pad at that time.

#include "types.hpp"
#include "GameShared/GameClasses/System/Input/Devices/X360/CgsInputDeviceX360Pad.h" // CgsInput::DeviceX360Pad (BindToPort)

#include <cstddef>   // offsetof (the X360-pinned member-offset static_asserts below)

namespace CgsInput
{
    // Number of physical XInput user slots the X360 build polls / binds. The four-iteration
    // UpdateConnectedDevices loop (`cmpwi lePort, 4`) and the (lePort < 4) bind/unbind range
    // asserts ("(lePort >= E_INPUTPORT_FIRST) && (lePort < E_INPUTPORT_COUNT)") attest 4.
    static const u32 KU_NUMBER_OF_INPUT_PORTS = 4;

    // ---- Per-slot device record (24-byte / 0x18 stride) ------------------------------------
    // meDeviceType holds the XInput device sub-type UpdateConnectedDevices resolves from
    // XINPUT_CAPABILITIES::SubType (1 == gamepad, 2 == wheel); it is later passed straight to
    // DeviceX360Pad::BindToPort as its leType argument. maState is the raw 16-byte XINPUT_STATE
    // buffer XInputGetState fills (at record +0x04). mbConnected is the live connection flag.
    struct ManagerX360Device
    {
        s32 meDeviceType;   // +0x00  XInput SubType (0 none / 1 gamepad / 2 wheel); passed as leType to the pad
        u8  maState[16];    // +0x04  raw XINPUT_STATE polled by XInputGetState
        u8  mbConnected;    // +0x14  1 when the slot currently has a device, 0 otherwise
        u8  mau8Pad15[3];   // +0x15  align to the 24-byte stride
    };

    // ============================================================================
    // CgsInput::ManagerX360 -- head 0x74 bytes (X360).
    // ============================================================================
    class ManagerX360
    {
    public:
        // X360 0x828DC480. Poll each of the four XInput slots: on a successful XInputGetState,
        // resolve/refresh the device sub-type via XInputGetCapabilities and (re)latch the raw
        // state; on failure clear the slot. Updates each slot record's type + connected flag.
        // (Declared-only in wave54 -- body lands in a later wave.)
        void UpdateConnectedDevices();

        // X360 0x828E7718. Bind port lePort to physical pad lpPad. Returns true when the slot
        // holds a device (its resolved sub-type is passed to DeviceX360Pad::BindToPort); false
        // when the slot has no device type (meDeviceType == 0), in which case nothing is bound.
        bool BindToPort(DeviceX360Pad* lpPad, u32 lePort);

        // X360 0x828E7808. Unbind port lePort from lpPad: clear the pad's connected flag and
        // port (two inlined pad writes), clear the bound-pad slot, and decrement the count.
        // (Declared-only in wave54 -- body lands in a later wave.)
        void UnbindFromPort(DeviceX360Pad* lpPad, u32 lePort);

    private:
        // Never-called layout pin so any drift in a member offset is a compile error.
        friend void _ManagerX360_AssertLayout();

        // ---- per-slot device records (24-byte stride) ----
        ManagerX360Device maDevices[KU_NUMBER_OF_INPUT_PORTS];   // +0x00 .. 0x5F

        // ---- port -> bound pad table ----
        // X360 4-byte pointers (mapBoundPads @+0x60 .. 0x6F, one word per port). On the 64-bit
        // host these pointers widen to 8 bytes, so mapBoundPads/miNumBoundPads no longer sit at
        // their X360 byte offsets -- the layout pin below therefore asserts only the pointer-free
        // maDevices sub-record + the mapBoundPads START (stable because maDevices is pointer-free
        // and exactly 0x60). BindToPort only stores into this table and UnbindFromPort only
        // compares against it; neither dereferences a slot as data, so the widened pointer is
        // behaviourally faithful.
        DeviceX360Pad*    mapBoundPads[KU_NUMBER_OF_INPUT_PORTS]; // +0x60 (X360)

        // ---- number of currently bound pads ----
        s32               miNumBoundPads;                        // +0x70 (X360)
    };
}

// Layout pin: never-called helper so any drift in a member offset is a compile error.
namespace CgsInput
{
    inline void _ManagerX360_AssertLayout()
    {
        static_assert(sizeof(ManagerX360Device) == 24, "ManagerX360Device must be 24 bytes (0x18 stride)");
        static_assert(offsetof(ManagerX360Device, meDeviceType) == 0x00, "meDeviceType @ +0x00");
        static_assert(offsetof(ManagerX360Device, maState)      == 0x04, "maState @ +0x04");
        static_assert(offsetof(ManagerX360Device, mbConnected)  == 0x14, "mbConnected @ +0x14");
        static_assert(offsetof(ManagerX360, maDevices)    == 0x00, "maDevices @ +0x00");
        static_assert(offsetof(ManagerX360, mapBoundPads) == 0x60, "mapBoundPads @ +0x60 (X360; host pointers widen this)");
    }
}
