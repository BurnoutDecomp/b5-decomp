#pragma once

// CgsInput::DeviceX360Pad -- the Xbox 360 physical-controller pad device.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360-only device; there is NO DecFIGS
// DWARF entry -- the DecFIGS PS3 build has no X360 pad). Every member offset, every stored
// immediate and every constant below is grounded in this TU's assembly
// (CgsInputDeviceX360Pad.cpp methods @0x828DC578 / 0x828DC800 / 0x828DC870 / 0x828DCB20 /
// 0x828DCB90 / 0x828DC8E8 / 0x828E7AB0). The record is exactly 452 bytes (0x1C4) -- the
// stride InputPads::Construct and GetDebugGamePad use over the four-pad array -- so the
// layout below is byte-exact and replaces the opaque-span placeholder in CgsInputPads.h.
//
// Source path mirrored from the asserts' baked __FILE__:
//   d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\system\input\Devices/X360/CgsInputDeviceX360Pad.cpp

#include "types.hpp"

namespace CgsInput
{
    // ---- Number of logical controls / buttons / axes the X360 pad exposes ----
    // Grounded in the range asserts and Construct loops:
    //   IsButtonPressed asserts (lcControl <= 0x1B) and the button-copy loop runs 28 (0x1C)
    //   times -> 28 button-state bytes.  Construct zero-fills 28 control floats (loop count 0x1C).
    //   GetAxisValue asserts (lcAxis <= 5) and reads from a 6-float array -> 6 axes.
    static const u32 KU_NUMBER_OF_BUTTONS  = 28;  // 0x1C, from the button loop / IsButtonPressed assert (<= 0x1B)
    static const u32 KU_NUMBER_OF_CONTROLS = 28;  // 0x1C, from Construct's control-float zero-fill loop
    static const u32 KU_NUMBER_OF_AXES     = 6;   // from GetAxisValue assert (<= 5) and the 6-dword axis loop

    // The device type stored at +0x04 / passed to BindToPort & checked in Update.
    // Only value attested by the asm is 2 (cmpwi r11, 2 -> "wheel" force-feedback path in
    // Update @0x828E8138 and BindToPort @0x828DC...). The non-2 path is the plain gamepad
    // path. The enum value 2 (E_DEVICETYPE_WHEEL) is asm-grounded; the 0/1 enumerators are
    // a FLAGGED placeholder set (no other compare-immediates against +0x04 appear in this TU).
    enum EDeviceType : s32
    {
        E_DEVICETYPE_GAMEPAD = 0,  // PLACEHOLDER: default; not asm-attested in this TU
        E_DEVICETYPE_UNKNOWN = 1,  // PLACEHOLDER: not asm-attested in this TU
        E_DEVICETYPE_WHEEL   = 2   // ASM-GROUNDED: cmpwi against +0x04 (meType) selects the FFB path
    };

    // ---- Minimal XDK FF/overlapped platform slices ----
    // These are pure Xbox 360 XDK types (real layouts in <xinput.h>/<xtl.h>); reconstructed
    // here as size/offset-exact local slices because the X360 build embeds them by value in
    // the pad record. Sizes/offsets are the only load-bearing facts and are asm-attested.

    // XINPUT_FF_EFFECT-like force-feedback effect descriptor, embedded at pad +0x14C.
    // 116 bytes (0x74) -- the memset stride in Construct @0x828DC774 (`li r5, 0x74`). Construct
    // sets the integer fields below; Update writes the two motor magnitudes + sub-mask.
    struct DeviceX360PadFFEffect
    {
        s32 miField00;      // +0x00 (0x14C)  = 1   (effect/sub-effect id)
        s32 miField04;      // +0x04 (0x150)  = 8
        s32 miField08;      // +0x08 (0x154)  = 4
        s32 miField0C;      // +0x0C (0x158)  = 255
        s32 miField10;      // +0x10 (0x15C)  = 0
        s32 miField14;      // +0x14 (0x160)  = 0
        s32 miField18;      // +0x18 (0x164)  = 0
        s32 miField1C;      // +0x1C (0x168)  = 255
        s32 miField20;      // +0x20 (0x16C)  = 255
        s32 miField24;      // +0x24 (0x170)  = 0
        s32 miSubMask;      // +0x28 (0x174)  = 0   (Update writes 0 here on the "stop" branch)
        s32 miField2C;      // +0x2C (0x178)  = 0   (Update writes a motor magnitude here)
        s32 miMotorLeft;    // +0x30 (0x17C)  = 30  (Update writes v91 here)
        s32 miMotorRight;   // +0x34 (0x180)  = 30  (Update writes v91 here)
        s32 miField38;      // +0x38 (0x184)  = 255
        s32 miField3C;      // +0x3C (0x188)  = 255
        s32 miField40;      // +0x40 (0x18C)  = 0
        u8  maTail[0x74 - 0x44]; // +0x44..0x73 -- remainder zero-filled by the memset; not individually written
    };

    // XOVERLAPPED-like async handle block, embedded by value (three of them) in the pad
    // record. 28 bytes (7 DWORDs) -- the zero-fill loops in Construct (count 7). CreateEventA's
    // result is stored at +0x0C of each block (the X360 build's hEvent slot for these blocks).
    struct DeviceX360PadOverlapped
    {
        u32 muStatus;       // +0x00  (Update tests mFFOverlappedSpring.muStatus != 997 == ERROR_IO_PENDING)
        u32 muField04;      // +0x04
        u32 muField08;      // +0x08
        u32 mhEvent;        // +0x0C  (CreateEventA result)
        u32 muField10;      // +0x10
        u32 muField14;      // +0x14
        u32 muField18;      // +0x18
    };

    // ============================================================================
    // CgsInput::DeviceX360Pad -- 452 bytes (0x1C4).
    // ============================================================================
    class DeviceX360Pad
    {
    public:
        // X360 0x828DC578. Resets all per-pad state and creates the three force-feedback
        // overlapped events. (a2..a5 in the asm are the CreateEventA name args, all 0.)
        void Construct();

        // X360 0x828DC800. Returns the live state byte of button lcControl (asserts lcControl <= 0x1B).
        bool IsButtonPressed(u32 luControl) const;

        // X360 0x828DC870. Returns axis value luAxis (asserts luAxis <= 5); reads mafAxisValues[luAxis].
        f32 GetAxisValue(u32 luAxis) const;

        // X360 0x828DCB20. Applies the symmetric inner/outer deadzone+range remap to one raw axis.
        f32 DeadzoneAxis(f32 lfRaw) const;

        // X360 0x828DCB90. Clears the two cached rumble half-words (+0xF4/+0xF6).
        void ClearCachedRumble();

        // X360 0x828DC8E8. Binds the device to a physical port (asserts luPort <= 3); for a wheel
        // (leType == 2) primes the XInput force-feedback effect with a 2s timeout retry loop.
        bool BindToPort(u32 luPort, s32 leType);

        // X360 0x828E7AB0. Per-frame update: latches button bits, deadzone-maps the axes, copies
        // previous button states, and (for a wheel) pushes the FF effect via XInputFFUpdateEffect.
        void Update(bool lbConnected, s32 lePort, s32 leType, const void* lpState);

        bool IsConnected() const { return mbConnected != 0; }

    private:
        // Never-called layout pin (defined in the header below) needs member-offset access.
        friend void _DeviceX360Pad_AssertLayout();

        // ---- head ----
        u8  mau8Pad00[4];         // +0x00 .. 0x03  (bind/player head -- not touched by this TU's bodies)
        s32 meType;               // +0x04          device type (Construct = 0; checked == 2 for wheel)
        f32 mfRumbleX;            // +0x08          (Construct = 0.0; used in the wheel FF magnitude math)
        f32 mfRumbleY;            // +0x0C          (Construct = 0.0; used in the wheel FF magnitude math)
        u8  mbConnected;          // +0x10          set/cleared connect flag (Construct = 0)
        u8  mbWheelStopped;       // +0x11          wheel "stop FF" flag (Construct = 0)

        // ---- button state ----
        u8  mabPrevButtons[KU_NUMBER_OF_BUTTONS]; // +0x12 .. 0x2D  previous-frame button states (28)
        u8  mabButtons[KU_NUMBER_OF_BUTTONS];     // +0x2E .. 0x49  current-frame button states  (28)
        u8  mau8Pad4A[2];                         // +0x4A .. 0x4B  alignment to the f32 control array

        // ---- control values ----
        f32 mafControls[KU_NUMBER_OF_CONTROLS];   // +0x4C .. 0xBB  per-control float values (28)

        // ---- axis values ----
        f32 mafAxisValues[KU_NUMBER_OF_AXES];     // +0xBC .. 0xD3  remapped axis values (6); GetAxisValue reads here

        // ---- deadzone / range params (Construct seeds these) ----
        f32 mfButtonThreshold;    // +0xD4 = 0.2          (control > this => button pressed)
        f32 mfTriggerOuter;       // +0xD8 = 0.94999999   trigger range outer
        f32 mfTriggerInner;       // +0xDC = 0.050000001  trigger range inner
        f32 mfStickOuter;         // +0xE0 = 0.89999998   stick range outer
        f32 mfStickInner;         // +0xE4 = 0.1          stick range inner
        f32 mfDeadzoneOuter;      // +0xE8 = 0.89999998   DeadzoneAxis outer (reads +0xE8)
        f32 mfDeadzoneInner;      // +0xEC = 0.2          DeadzoneAxis inner (reads +0xEC)

        s32 mePort;               // +0xF0 = -1           bound physical port (BindToPort sets, Update checks)
        u16 muCachedRumbleA;      // +0xF4 = 0            ClearCachedRumble target
        u16 muCachedRumbleB;      // +0xF6 = 0            ClearCachedRumble target

        // ---- force-feedback overlapped blocks (Construct zero-fills + CreateEventA into each) ----
        DeviceX360PadOverlapped mFFOverlappedRumble; // +0xF8  (28 bytes; hEvent stored @+0x104)
        DeviceX360PadOverlapped mFFOverlappedGain;   // +0x114 (28 bytes; hEvent stored @+0x120)
        DeviceX360PadOverlapped mFFOverlappedSpring; // +0x130 (28 bytes; hEvent stored @+0x13C)

        // ---- force-feedback effect descriptor ----
        DeviceX360PadFFEffect mFFEffect;             // +0x14C (116 bytes)

        // ---- gain ----
        f32 mfGain;               // +0x1C0 = 1.0         FF device gain (BindToPort multiplies by 31.0)
    };
}

// Layout pin: never-called helper so any drift in a member offset is a compile error.
// (offsetof on a standard-layout struct; constants are the asm-attested byte offsets.)
#include <cstddef>
namespace CgsInput
{
    inline void _DeviceX360Pad_AssertLayout()
    {
        static_assert(sizeof(DeviceX360PadOverlapped) == 28, "overlapped block must be 28 bytes");
        static_assert(sizeof(DeviceX360PadFFEffect)   == 116, "FF effect must be 116 bytes (0x74)");
        static_assert(offsetof(DeviceX360Pad, meType)              == 0x04, "meType @ +0x04");
        static_assert(offsetof(DeviceX360Pad, mbConnected)         == 0x10, "mbConnected @ +0x10");
        static_assert(offsetof(DeviceX360Pad, mbWheelStopped)      == 0x11, "mbWheelStopped @ +0x11");
        static_assert(offsetof(DeviceX360Pad, mabPrevButtons)      == 0x12, "mabPrevButtons @ +0x12");
        static_assert(offsetof(DeviceX360Pad, mabButtons)          == 0x2E, "mabButtons @ +0x2E");
        static_assert(offsetof(DeviceX360Pad, mafControls)         == 0x4C, "mafControls @ +0x4C");
        static_assert(offsetof(DeviceX360Pad, mafAxisValues)       == 0xBC, "mafAxisValues @ +0xBC");
        static_assert(offsetof(DeviceX360Pad, mfDeadzoneOuter)     == 0xE8, "mfDeadzoneOuter @ +0xE8");
        static_assert(offsetof(DeviceX360Pad, mfDeadzoneInner)     == 0xEC, "mfDeadzoneInner @ +0xEC");
        static_assert(offsetof(DeviceX360Pad, mePort)              == 0xF0, "mePort @ +0xF0");
        static_assert(offsetof(DeviceX360Pad, mFFOverlappedRumble) == 0xF8, "rumble overlapped @ +0xF8");
        static_assert(offsetof(DeviceX360Pad, mFFOverlappedSpring) == 0x130, "spring overlapped @ +0x130");
        static_assert(offsetof(DeviceX360Pad, mFFEffect)           == 0x14C, "FF effect @ +0x14C");
        static_assert(offsetof(DeviceX360Pad, mfGain)              == 0x1C0, "mfGain @ +0x1C0");
        static_assert(sizeof(DeviceX360Pad) == 452, "DeviceX360Pad must be 452 bytes (0x1C4)");
    }
}
