// CgsInput::DeviceX360Pad -- Xbox 360 physical-controller pad device.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. There is NO DecFIGS DWARF for this class (it is
// X360-only; the DecFIGS build is PS3). Every offset, store and constant below is grounded in
// the X360 assembly of:
//   Construct          @0x828DC578   (EXECUTED in the goal trace)
//   IsButtonPressed    @0x828DC800
//   GetAxisValue       @0x828DC870
//   DeadzoneAxis       @0x828DCB20
//   ClearCachedRumble  @0x828DCB90
//   BindToPort         @0x828DC8E8
//   Update             @0x828E7AB0
// baked __FILE__: ...gameshared\gameclasses\system\input\Devices/X360/CgsInputDeviceX360Pad.cpp
//
// FLAGGED rodata constants (see KF_FF_* below): the wheel force-feedback magnitude scale in
// Update reads several read-only-data floats (flt_82F34740, flt_820FA3F8, flt_82057DD8,
// flt_820049E0) whose numeric values are NOT recoverable from this packet (no stored immediate;
// they live in the rodata constant pool, not attested by the dump). They are isolated as named
// placeholders so they can be ground later; the clamp *bounds* (-100..100 / -200) ARE attested
// by the Hex-Rays-decoded literals.

#include "GameShared/GameClasses/System/Input/Devices/X360/CgsInputDeviceX360Pad.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "types.hpp"

// ---- Xbox 360 XDK entry points (real prototypes live in <xinput.h>/<xtl.h>). Declared as
// extern "C" free functions, mirroring the System/X360 precedent (CgsXOverlappedX360.cpp). The
// effect / overlapped arguments are passed by the void* address of the embedded members. ----
extern "C" u32 GetTickCount();
extern "C" void* CreateEventA(void* lpEventAttributes, s32 bManualReset, s32 bInitialState,
                              const char* lpName);
extern "C" u32 XInputFFResetDevice(u32 dwUserIndex, void* pOverlapped);
extern "C" u32 XInputFFSetDeviceGain(u32 dwUserIndex, u32 dwGain, void* pOverlapped);
extern "C" u32 XInputFFEnableMotors(u32 dwUserIndex, s32 fEnable, void* pOverlapped);
extern "C" u32 XInputFFSetEffect(u32 dwUserIndex, const void* pEffects, u32 dwCount, void* pOverlapped);
extern "C" u32 XInputFFEffectOperation(u32 dwUserIndex, const void* pEffects, u32 dwCount,
                                       u32 dwOperation, void* pOverlapped);
extern "C" u32 XInputFFUpdateEffect(u32 dwUserIndex, const void* pEffect, u32 dwUpdateMask,
                                    void* pOverlapped);

namespace CgsInput
{
    // ---- asm-attested constants -------------------------------------------------------------
    static const f32 KF_ZERO        = 0.0f;           // flt_82001CC0
    static const f32 KF_ONE         = 1.0f;           // flt_82001C98
    static const f32 KF_BYTE_SCALE  = 0.0039215689f;  // flt_82010C1C (= 1/255, trigger -> [0,1])
    static const f32 KF_AXIS_SCALE_NEG = 0.000030517578f; // flt_820AA8F8 / flt_820ADBE8 (= 1/32768, negative half)
    static const f32 KF_AXIS_SCALE_POS = 0.000030518509f; // flt_820037C8 (= 1/32767, positive half)
    static const f32 KF_GAIN_SCALE  = 31.0f;          // BindToPort: mfGain * 31.0 -> XInputFFSetDeviceGain
    static const u32 KU_FF_TIMEOUT_MS = 0x7D0;         // 2000 ms wheel-prime timeout
    static const u32 KU_ERROR_DEVICE_NOT_CONNECTED = 1460; // BindToPort SetEffect retry sentinel
    static const u32 KU_ERROR_IO_PENDING = 997;        // Update spring-overlapped pending sentinel
    static const u32 KU_FF_UPDATE_MASK   = 0x704;      // XInputFFUpdateEffect dwUpdateMask
    static const u32 KU_VALID_BUTTON_MAX  = 0x1B;      // IsButtonPressed: lcControl <= 0x1B
    static const u32 KU_VALID_AXIS_MAX    = 5;         // GetAxisValue: lcAxis <= 5
    static const u32 KU_VALID_PORT_MAX    = 3;         // BindToPort: lcPort <= 3

    // ---- FLAGGED unrecoverable rodata FF magnitude scales (see file header) -----------------
    static const f32 KF_FF_MOTOR_SCALE  = 1.0f; // flt_82F34740 -- PLACEHOLDER (rodata, not attested)
    static const f32 KF_FF_CLAMP_HI    = 100.0f;  // flt_820049E0 -- decoded literal 100.0
    static const f32 KF_FF_CLAMP_LO    = -100.0f; // flt_82057DD8 -- decoded literal -100.0
    static const f32 KF_FF_RUMBLE_Y_GAIN = -200.0f; // flt_820FA3F8 -- decoded literal -200.0 (mfRumbleY weight)

    // The XINPUT_GAMEPAD-shaped state record Update consumes (read-only). Only the fields this
    // TU reads are modelled; offsets match the lhz/lbz/lhz reads off the state pointer.
    struct GamepadState
    {
        u16 muReserved;     // +0x00
        u16 muReserved2;    // +0x02
        u16 muButtons;      // +0x04  wButtons bitfield
        u8  mbLeftTrigger;  // +0x06  bLeftTrigger  (unsigned, lbz)
        u8  mbRightTrigger; // +0x07  bRightTrigger (unsigned, lbz)
        s16 msThumbLX;      // +0x08  sThumbLX (signed, extsh)
        s16 msThumbLY;      // +0x0A  sThumbLY
        s16 msThumbRX;      // +0x0C  sThumbRX
        s16 msThumbRY;      // +0x0E  sThumbRY
    };

    // ============================================================================
    // Construct @0x828DC578 (EXECUTED in goal trace). Resets all state, seeds the deadzone/range
    // params, builds the three FF overlapped events, and primes the FF effect descriptor.
    // ============================================================================
    void DeviceX360Pad::Construct()
    {
        mbConnected = 0;                               // stb r30, 0x10

        // Zero the prev/current button bytes and the control floats (the 28-iteration triple loop).
        for (u32 i = 0; i < KU_NUMBER_OF_BUTTONS; ++i)
        {
            mabPrevButtons[i] = 0;                     // *(v6-28) = 0
            mabButtons[i]     = 0;                     // *v6 = 0
        }
        for (u32 i = 0; i < KU_NUMBER_OF_CONTROLS; ++i)
            mafControls[i] = KF_ZERO;                  // *v7++ = 0.0

        // Zero the 6 axis values (the 6-dword loop @0x828DC5D8).
        for (u32 i = 0; i < KU_NUMBER_OF_AXES; ++i)
            mafAxisValues[i] = KF_ZERO;

        meType          = 0;                           // stw r30, 4
        muCachedRumbleA = 0;                           // sth r30, 0xF4
        muCachedRumbleB = 0;                           // sth r30, 0xF6
        mePort          = -1;                          // stw -1, 0xF0

        // Deadzone / range params (flt_820F78xx pool @0x828DC608..).
        mfButtonThreshold = 0.2f;          // 0xD4
        mfTriggerOuter    = 0.94999999f;   // 0xD8
        mfTriggerInner    = 0.050000001f;  // 0xDC
        mfStickOuter      = 0.89999998f;   // 0xE0
        mfStickInner      = 0.1f;          // 0xE4
        mfDeadzoneOuter   = 0.89999998f;   // 0xE8
        mfDeadzoneInner   = 0.2f;          // 0xEC

        // Zero the three FF overlapped blocks (three 7-dword loops), then create their events.
        DeviceX360PadOverlapped* lapBlocks[3] =
            { &mFFOverlappedRumble, &mFFOverlappedGain, &mFFOverlappedSpring };
        for (u32 b = 0; b < 3; ++b)
        {
            u32* lpu32 = reinterpret_cast<u32*>(lapBlocks[b]);
            for (u32 i = 0; i < 7; ++i)
                lpu32[i] = 0;
        }

        // CreateEventA(NULL, FALSE, FALSE, NULL) x3; results land in each block's hEvent slot.
        mFFOverlappedRumble.mhEvent = reinterpret_cast<u32>(CreateEventA(nullptr, 0, 0, nullptr));
        mFFOverlappedSpring.mhEvent = reinterpret_cast<u32>(CreateEventA(nullptr, 0, 0, nullptr));
        mFFOverlappedGain.mhEvent   = reinterpret_cast<u32>(CreateEventA(nullptr, 0, 0, nullptr));

        CGS_ASSERT(mFFOverlappedRumble.mhEvent != 0, "mFFOverlappedRumble.hEvent != NULL");   // line 127
        CGS_ASSERT(mFFOverlappedGain.mhEvent   != 0, "mFFOverlappedGain.hEvent != NULL");     // line 128
        CGS_ASSERT(mFFOverlappedSpring.mhEvent != 0, "mFFOverlappedSpring.hEvent != NULL");   // line 129

        mfRumbleY = KF_ZERO;   // 0xC
        mfRumbleX = KF_ZERO;   // 0x8
        mfGain    = KF_ONE;    // 0x1C0

        // Prime the FF effect descriptor: memset 116 bytes then set the named integer fields.
        u8* lpEffect = reinterpret_cast<u8*>(&mFFEffect);
        for (u32 i = 0; i < sizeof(DeviceX360PadFFEffect); ++i)
            lpEffect[i] = 0;

        mFFEffect.miField00    = 1;     // 0x14C
        mFFEffect.miField04    = 8;     // 0x150
        mFFEffect.miField08    = 4;     // 0x154
        mFFEffect.miField0C    = 255;   // 0x158
        mFFEffect.miField10    = 0;     // 0x15C
        mFFEffect.miField14    = 0;     // 0x160
        mFFEffect.miField18    = 0;     // 0x164
        mFFEffect.miField1C    = 255;   // 0x168
        mFFEffect.miField20    = 255;   // 0x16C
        mFFEffect.miField24    = 0;     // 0x170
        mFFEffect.miSubMask    = 0;     // 0x174
        mFFEffect.miField2C    = 0;     // 0x178
        mFFEffect.miMotorLeft  = 30;    // 0x17C
        mFFEffect.miMotorRight = 30;    // 0x180
        mFFEffect.miField38    = 255;   // 0x184
        mFFEffect.miField3C    = 255;   // 0x188
        mFFEffect.miField40    = 0;     // 0x18C

        mbWheelStopped = 0;             // stb r30, 0x11
    }

    // ============================================================================
    // IsButtonPressed @0x828DC800. Asserts the control index then returns the live state byte.
    // ============================================================================
    bool DeviceX360Pad::IsButtonPressed(u32 luControl) const
    {
        CGS_ASSERT(luControl <= KU_VALID_BUTTON_MAX, "lbValidControl");   // line 455
        if (luControl > KU_VALID_BUTTON_MAX)
            return false;
        return mabButtons[luControl] != 0;            // lbz r3, 0x2E(this + luControl)
    }

    // ============================================================================
    // GetAxisValue @0x828DC870. Asserts the axis index then returns mafAxisValues[luAxis].
    // ============================================================================
    f32 DeviceX360Pad::GetAxisValue(u32 luAxis) const
    {
        CGS_ASSERT(luAxis <= KU_VALID_AXIS_MAX, "lbValidControl");        // line 548
        if (luAxis > KU_VALID_AXIS_MAX)
            return KF_ZERO;                            // invalid path returns flt_82001CC0 == 0.0
        return mafAxisValues[luAxis];                  // lfsx f1, this + (luAxis+47)*4 == mafAxisValues[luAxis]
    }

    // ============================================================================
    // DeadzoneAxis @0x828DCB20. Symmetric inner/outer deadzone+range remap of one raw axis.
    // Reads mfDeadzoneOuter (+0xE8) and mfDeadzoneInner (+0xEC).
    // ============================================================================
    f32 DeviceX360Pad::DeadzoneAxis(f32 lfRaw) const
    {
        const f32 lfOuter = mfDeadzoneOuter;

        if (lfRaw <= KF_ZERO)
        {
            f32 lfValue = lfRaw;
            if (lfValue < -lfOuter)
                lfValue = -lfOuter;
            const f32 lfShifted = mfDeadzoneInner + lfValue;
            if (lfShifted <= KF_ZERO)
                return (KF_ONE / (mfDeadzoneOuter - mfDeadzoneInner)) * lfShifted;
        }
        else
        {
            f32 lfValue = lfRaw;
            if (lfValue > lfOuter)
                lfValue = mfDeadzoneOuter;
            const f32 lfShifted = lfValue - mfDeadzoneInner;
            if (lfShifted >= KF_ZERO)
                return (KF_ONE / (mfDeadzoneOuter - mfDeadzoneInner)) * lfShifted;
        }
        return KF_ZERO;
    }

    // ============================================================================
    // ClearCachedRumble @0x828DCB90. Clears the two cached rumble half-words.
    // ============================================================================
    void DeviceX360Pad::ClearCachedRumble()
    {
        muCachedRumbleA = 0;   // sth 0, 0xF4
        muCachedRumbleB = 0;   // sth 0, 0xF6
    }

    // ============================================================================
    // BindToPort @0x828DC8E8. Binds to a physical port; for a wheel (leType == 2) primes the
    // XInput FF effect inside a 2-second timeout retry loop. Returns false on an invalid port.
    // ============================================================================
    bool DeviceX360Pad::BindToPort(u32 luPort, s32 leType)
    {
        CGS_ASSERT(luPort <= KU_VALID_PORT_MAX, "lbValidPort");           // line 1121
        if (luPort > KU_VALID_PORT_MAX)
            return false;

        mePort      = static_cast<s32>(luPort);   // stw a2, 0xF0
        meType      = leType;                      // stw a3, 0x04
        mbConnected = 1;                           // stw 1, 0x10

        if (leType == E_DEVICETYPE_WHEEL)
        {
            const u32 luStart = GetTickCount();
            while (true)
            {
                const u32 luReset = XInputFFResetDevice(static_cast<u32>(mePort), nullptr);
                if (GetTickCount() - luStart > KU_FF_TIMEOUT_MS)
                    break;
                if (luReset != 0)
                    continue;

                const u32 luGain = XInputFFSetDeviceGain(
                    static_cast<u32>(mePort),
                    static_cast<u32>(mfGain * KF_GAIN_SCALE),
                    nullptr);
                CGS_ASSERT(luGain == 0, "Wheel Error");                   // line 1155

                const u32 luMotors = XInputFFEnableMotors(static_cast<u32>(mePort), 1, nullptr);
                CGS_ASSERT(luMotors == 0, "Wheel Error");                 // line 1159

                while (true)
                {
                    const u32 luSetEffect = XInputFFSetEffect(
                        static_cast<u32>(mePort), &mFFEffect, 1u, nullptr);
                    if (GetTickCount() - luStart > KU_FF_TIMEOUT_MS)
                        return true;
                    if (luSetEffect != KU_ERROR_DEVICE_NOT_CONNECTED)
                    {
                        // dwOperation buffer { 1, 3 } (op start, loop count) per the asm v27[0]/v27[1].
                        const u32 lauOp[2] = { 1u, 3u };
                        const u32 luOp = XInputFFEffectOperation(
                            static_cast<u32>(mePort), lauOp, 1u, 1u, nullptr);
                        CGS_ASSERT(luOp == 0, "Wheel Error");             // line 1185
                        return true;
                    }
                }
            }
        }
        return true;
    }

    // ============================================================================
    // Update @0x828E7AB0. Per-frame: validate connection, latch the XINPUT button bits into the
    // control floats, deadzone-map the triggers and thumbsticks, copy current->prev button states
    // and recompute the current button bytes, then (for a wheel) push the FF effect.
    //
    // The decompiler's a6..a12 doubles are FP register-spill artifacts (DeadzoneAxis takes one
    // float); the real parameters are (lbConnected, lePort, leType, lpState).
    // ============================================================================
    void DeviceX360Pad::Update(bool lbConnected, s32 lePort, s32 leType, const void* lpState)
    {
        const GamepadState& lrState = *static_cast<const GamepadState*>(lpState);

        CGS_ASSERT(lbConnected, "lbConnected");                          // line 754
        CGS_ASSERT(IsConnected(), "IsConnected()");                      // line 755
        CGS_ASSERT(mePort == lePort, "mePort == lePort");                // line 756
        CGS_ASSERT(meType == leType, "meType == leType");                // line 757

        // -- latch the 14 mapped digital buttons (wButtons bits) into mafControls[0..13] --
        // bit order matches the rlwinm masks: 0,1,2,3,4,5,6,7,12,13,14,15,8,9.
        static const u32 KAU_BUTTON_BITS[14] =
            { 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
              0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200 };
        for (u32 i = 0; i < 14; ++i)
            mafControls[i] = (lrState.muButtons & KAU_BUTTON_BITS[i]) ? KF_ONE : KF_ZERO;

        // Trigger remap helper: scale a raw 0..255 trigger byte to [0,1] across the stick
        // range (the asm uses mfStickOuter/mfStickInner -- f12 @0xE0 / f13 @0xE4 -- for both
        // the wheel and gamepad trigger maps).
        const f32 lfRangeOuter = mfStickOuter;   // f12 @0xE0
        const f32 lfRangeInner = mfStickInner;   // f13 @0xE4

        if (meType == E_DEVICETYPE_WHEEL)
        {
            // -- wheel triggers --> mafControls[25] (left) / [24] (right); combined into axis[5] --
            f32 lfLeft = static_cast<f32>(lrState.mbLeftTrigger) * KF_BYTE_SCALE;
            if (lfLeft > lfRangeOuter)
                lfLeft = mfStickOuter;
            f32 lfLeftShift = lfLeft - lfRangeInner;
            mafControls[25] = (lfLeftShift >= KF_ZERO)
                ? (KF_ONE / (mfStickOuter - lfRangeInner)) * lfLeftShift : KF_ZERO;   // 0xB0

            f32 lfRight = static_cast<f32>(lrState.mbRightTrigger) * KF_BYTE_SCALE;
            if (lfRight > lfRangeOuter)
                lfRight = lfRangeOuter;
            f32 lfRightShift = lfRight - lfRangeInner;
            mafControls[24] = (lfRightShift >= KF_ZERO)
                ? (KF_ONE / (lfRangeOuter - lfRangeInner)) * lfRightShift : KF_ZERO;  // 0xAC

            mafAxisValues[5] = mafControls[24] - mafControls[25];                     // 0xD0 = 0xAC - 0xB0

            // -- wheel steer (sThumbLX): split signed value into mafControls[26]/[27], inline
            // [0,1]-clamped magnitude into axis[4]. (No DeadzoneAxis on the wheel path.) The
            // value pre-loaded from axis[0] (0xBC) is the stale prior value, mirrored to the
            // inactive half exactly as the asm does, then axis[0] is cleared below.
            const s16 lsLX = lrState.msThumbLX;
            const f32 lfOldAxis0 = mafAxisValues[0];   // lfs f0, 0xBC (prior value)
            f32 lfClamped;
            if (lsLX > 0)
            {
                mafControls[27] = lfOldAxis0;          // 0xB8
                mafControls[26] = KF_ZERO;             // 0xB4
                f32 lfScaled = static_cast<f32>(lsLX) * KF_AXIS_SCALE_POS;
                lfScaled = (-lfScaled >= KF_ZERO) ? KF_ZERO : lfScaled;   // fsel f13,f11,f0
                lfClamped = ((KF_ONE - lfScaled) >= KF_ZERO) ? lfScaled : KF_ONE; // clamp to <=1
            }
            else
            {
                mafControls[26] = -lfOldAxis0;         // 0xB4
                mafControls[27] = KF_ZERO;             // 0xB8
                f32 lfScaled = static_cast<f32>(lsLX) * KF_AXIS_SCALE_NEG;
                lfScaled = ((KF_ONE - lfScaled) >= KF_ZERO) ? lfScaled : KF_ONE;  // clamp to <=1
                lfClamped = (-lfScaled >= KF_ZERO) ? KF_ZERO : lfScaled;          // clamp to >=0
            }
            mafAxisValues[4] = lfClamped;   // 0xCC
            mafAxisValues[0] = KF_ZERO;     // 0xBC
            mafControls[14]  = KF_ZERO;     // 0x84
            mafControls[15]  = KF_ZERO;     // 0x88
            mafControls[19]  = KF_ZERO;     // 0x98
            mafControls[18]  = KF_ZERO;     // 0x94
        }
        else
        {
            // -- gamepad triggers --> mafControls[14] (left) / [15] (right) --
            f32 lfLeft = static_cast<f32>(lrState.mbLeftTrigger) * KF_BYTE_SCALE;
            if (lfLeft > lfRangeOuter)
                lfLeft = mfStickOuter;
            f32 lfLeftShift = lfLeft - lfRangeInner;
            mafControls[14] = (lfLeftShift >= KF_ZERO)
                ? (KF_ONE / (mfStickOuter - lfRangeInner)) * lfLeftShift : KF_ZERO;   // 0x84

            f32 lfRight = static_cast<f32>(lrState.mbRightTrigger) * KF_BYTE_SCALE;
            if (lfRight > lfRangeOuter)
                lfRight = lfRangeOuter;
            f32 lfRightShift = lfRight - lfRangeInner;
            mafControls[15] = (lfRightShift >= KF_ZERO)
                ? (KF_ONE / (lfRangeOuter - lfRangeInner)) * lfRightShift : KF_ZERO;  // 0x88

            // -- gamepad left-stick X (sThumbLX) through DeadzoneAxis --> mafControls[18]/[19] + axis[0] --
            const s16 lsLX = lrState.msThumbLX;
            const f32 lfLX = DeadzoneAxis(static_cast<f32>(lsLX) *
                                          (lsLX <= 0 ? KF_AXIS_SCALE_NEG : KF_AXIS_SCALE_POS));
            if (lsLX > 0)
            {
                mafControls[18] = KF_ZERO;   // 0x94
                mafControls[19] = lfLX;      // 0x98
            }
            else
            {
                mafControls[19] = KF_ZERO;   // 0x98
                mafControls[18] = -lfLX;     // 0x94
            }
            mafAxisValues[0] = lfLX;   // 0xBC
            mafAxisValues[5] = KF_ZERO; // 0xD0
            mafAxisValues[4] = KF_ZERO; // 0xCC
            mafControls[25]  = KF_ZERO; // 0xB0
            mafControls[24]  = KF_ZERO; // 0xAC
            mafControls[27]  = KF_ZERO; // 0xB8
            mafControls[26]  = KF_ZERO; // 0xB4
        }

        // -- the remaining three analogue axes, always through DeadzoneAxis --
        //   sThumbLY  -> mafControls[16](pos 0x8C) / [17](neg 0x90), axis[1] (0xC0)
        //   sThumbRX  -> mafControls[23](pos 0xA8) / [22](neg 0xA4), axis[2] (0xC4)
        //   sThumbRY  -> mafControls[20](pos 0x9C) / [21](neg 0xA0), axis[3] (0xC8)
        struct AxisMap { s16 lsRaw; u32 luPosCtrl; u32 luNegCtrl; u32 luAxis; };
        const AxisMap laMaps[3] =
        {
            { lrState.msThumbLY, 16, 17, 1 },   // pos 0x8C / neg 0x90, axis[1] 0xC0
            { lrState.msThumbRX, 23, 22, 2 },   // pos 0xA8 / neg 0xA4, axis[2] 0xC4
            { lrState.msThumbRY, 20, 21, 3 },   // pos 0x9C / neg 0xA0, axis[3] 0xC8
        };
        for (u32 m = 0; m < 3; ++m)
        {
            const s16 lsRaw = laMaps[m].lsRaw;
            const f32 lf = DeadzoneAxis(static_cast<f32>(lsRaw) *
                                        (lsRaw <= 0 ? KF_AXIS_SCALE_NEG : KF_AXIS_SCALE_POS));
            if (lsRaw > 0)
            {
                mafControls[laMaps[m].luNegCtrl] = KF_ZERO;
                mafControls[laMaps[m].luPosCtrl] = lf;
            }
            else
            {
                mafControls[laMaps[m].luPosCtrl] = KF_ZERO;
                mafControls[laMaps[m].luNegCtrl] = -lf;
            }
            mafAxisValues[laMaps[m].luAxis] = lf;
        }

        // -- copy current button states to prev, recompute current from the control floats --
        for (u32 i = 0; i < KU_NUMBER_OF_BUTTONS; ++i)
        {
            mabPrevButtons[i] = mabButtons[i];
            mabButtons[i] = (mafControls[i] > mfButtonThreshold) ? 1 : 0;
        }

        // -- wheel force-feedback: push the FF effect via XInputFFUpdateEffect --
        if (meType == E_DEVICETYPE_WHEEL && mFFOverlappedSpring.muStatus != KU_ERROR_IO_PENDING)
        {
            s32 liMagnitude;
            if (mbWheelStopped)
            {
                mFFEffect.miField2C = 0;   // +0x178 (asm stw r8,0x178)
                liMagnitude = static_cast<s32>(KF_FF_MOTOR_SCALE);
            }
            else
            {
                // mfRumbleX * scale -> motor; bias = clamp(mfRumbleY * -200, -100, 100) -> +0x178.
                const s32 liMotor = static_cast<s32>(mfRumbleX * KF_FF_MOTOR_SCALE);
                f32 lfBias = mfRumbleY * KF_FF_RUMBLE_Y_GAIN;
                lfBias = (KF_FF_CLAMP_LO >= lfBias) ? KF_FF_CLAMP_LO : lfBias;        // fsel: max(bias,-100)
                lfBias = ((KF_FF_CLAMP_HI - lfBias) >= KF_ZERO) ? lfBias : KF_FF_CLAMP_HI; // fsel: min(bias,100)
                mFFEffect.miField2C = static_cast<s32>(lfBias);   // +0x178
                liMagnitude = liMotor;
            }
            mFFEffect.miMotorRight = liMagnitude;   // 0x180
            mFFEffect.miMotorLeft  = liMagnitude;   // 0x17C
            XInputFFUpdateEffect(static_cast<u32>(mePort), &mFFEffect, KU_FF_UPDATE_MASK,
                                 &mFFOverlappedSpring);
        }
    }
}
