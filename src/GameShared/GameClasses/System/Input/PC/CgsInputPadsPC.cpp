#include "GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h"

#include <cstring>   // std::memset
#include <cstdlib>   // std::getenv (the harness focus-gate bypass)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog (the pause gate's one-shot)

// ============================================================================
// FLAG PC-platform leaf (whole file): the host pad source standing in for the
// unreconstructed console pad fill (CgsInput::InputPads::Update + the binding
// tables). See the header for the contract. Win32/XInput imports are declared
// locally (no <Windows.h> -- its NOUSER/NOGDI lean-defines conflict with the
// game TUs; same pattern as the BrnGuiModule.cpp bring-up helpers this replaces).
//
// ---------------------------------------------------------------------------
// THE CONSOLE FILL PATH THIS LEAF STANDS IN FOR (X360 evidence, 2026-08-11)
// ---------------------------------------------------------------------------
// CgsInput::ManagerX360::Update @0x828F0028
//   -> CgsInput::DeviceX360Pad::Update @0x828E7AB0
//        reads an XINPUT_STATE by field (wButtons @+4, bLeftTrigger @+6,
//        bRightTrigger @+7, sThumbLX/LY/RX/RY @+8/+10/+12/+14) and fills the
//        device's 28 raw control floats (CgsInput::EPadButton) + 6 raw axes
//        (CgsInput::EPadAxis), applying the deadzone/saturation curves below.
// CgsInput::InputPads::Update @0x828F8690      (DWARF CgsInputPads.cpp:216)
//   -> CgsInput::InputPads::FillRawData @0x828E7350
//        accumulates every device bound to the port into
//        `float32_t lafNewRawButtonData[34]` -- buttons MAXed, axes SUMMED then
//        rw::math::fpu::Clamp'd to [-1,+1] -- then walks
//        gaDefaultGameInputMapping (ActionMapping[34], each entry an int8_t[4]
//        of EGameInputActions ids) and per mapped action calls
//        ActionInfo::SetValue + SetAsDown/SetAsPressed/SetAsReleased on
//        PadOutputInformation::GetActionInfo(), plus SetPlayerId / SetType /
//        SetPadIdle. That record is what this leaf writes directly.
//
// ⚠️ HONEST PARK -- gaDefaultGameInputMapping IS NOT RECOVERED.
// InputPads::Update @0x828F8690 is a HOLE in the IDA export set (no
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x828F8690.json; it is only reachable as
// an xref name from FillRawData), and gaDefaultGameInputMapping is a rodata DATA
// symbol with no export at all, so the console's control->action table cannot be
// read on this host (no IDA install; only .i64 databases are present). The
// per-action SEMANTICS are attested -- EGameInputActions
// (references/DecFIGS/dwarfdump/GameSource/Input/GameInputActions.h:24) and
// EPadButton/EPadAxis (.../Devices/PS3/CgsInputDevicePS3Pad.h:40/:84) -- and the
// CONSUMER side is attested store-for-store by BridgeControllerToWorld
// @0x823CD890. What is NOT attested is which pad control the console binds to
// which action; the KA_BINDINGS table below is therefore a PC-side binding
// choice, flagged as such, and is the ONE place any binding lives.
// ============================================================================

extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
extern "C" __declspec(dllimport) void* __stdcall GetForegroundWindow(void);
extern "C" __declspec(dllimport) unsigned long __stdcall GetWindowThreadProcessId(void* hWnd, unsigned long* lpdwProcessId);
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentProcessId(void);
extern "C" __declspec(dllimport) void* __stdcall LoadLibraryA(const char* lpLibFileName);
extern "C" __declspec(dllimport) void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
extern "C" __declspec(dllimport) void* __stdcall OpenEventA(unsigned long dwDesiredAccess,
                                                              int bInheritHandle,
                                                              const char* lpName);
extern "C" __declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void* hHandle,
                                                                              unsigned long dwMilliseconds);

namespace
{
    // ---- XInput pad 0 (dynamic bind -- the exe does not link an XInput import lib) ----
    // The record layout is the one CgsInput::DeviceX360Pad::Update @0x828E7AB0 reads by
    // field offset on the console (wButtons @+4, triggers @+6/+7, thumbs @+8..+14).
    struct XInputGamepad
    {
        unsigned short wButtons;
        unsigned char  bLeftTrigger;
        unsigned char  bRightTrigger;
        short          sThumbLX;
        short          sThumbLY;
        short          sThumbRX;
        short          sThumbRY;
    };
    struct XInputState
    {
        unsigned long dwPacketNumber;
        XInputGamepad Gamepad;
    };
    typedef unsigned long(__stdcall* XInputGetStateFn)(unsigned long dwUserIndex, XInputState* pState);

    XInputGetStateFn ResolveXInputGetState()
    {
        static XInputGetStateFn spfGetState = 0;
        static bool sbResolved = false;
        if (!sbResolved)
        {
            sbResolved = true;
            // The system XInput generations, newest first (9_1_0 ships with the OS).
            static const char* KAPC_DLLS[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
            for (unsigned i = 0; i < sizeof(KAPC_DLLS) / sizeof(KAPC_DLLS[0]) && !spfGetState; ++i)
            {
                void* lpModule = LoadLibraryA(KAPC_DLLS[i]);
                if (lpModule)
                    spfGetState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(lpModule, "XInputGetState"));
            }
        }
        return spfGetState;
    }

    // XINPUT_GAMEPAD wButtons bits, with the CgsInput::EPadButton control each one drives
    // on the console (DeviceX360Pad::Update @0x828E7AB0 store order, device float array base
    // this+76: mask 0x1 -> +76 = control 0, 0x2 -> +80 = 1, ... 0x200 -> +128 = 13).
    const unsigned short KU_XPAD_DPAD_UP    = 0x0001; // E_PADBUTTON_UP        (control 0)
    const unsigned short KU_XPAD_DPAD_DOWN  = 0x0002; // E_PADBUTTON_DOWN      (control 1)
    const unsigned short KU_XPAD_DPAD_LEFT  = 0x0004; // E_PADBUTTON_LEFT      (control 2)
    const unsigned short KU_XPAD_DPAD_RIGHT = 0x0008; // E_PADBUTTON_RIGHT     (control 3)
    const unsigned short KU_XPAD_START      = 0x0010; // E_PADBUTTON_START     (control 4)
    const unsigned short KU_XPAD_BACK       = 0x0020; // E_PADBUTTON_SELECT    (control 5)
    const unsigned short KU_XPAD_LTHUMB     = 0x0040; // E_PADBUTTON_LTHUMB    (control 6)
    const unsigned short KU_XPAD_RTHUMB     = 0x0080; // E_PADBUTTON_RTHUMB    (control 7)
    const unsigned short KU_XPAD_LSHOULDER  = 0x0100; // E_PADBUTTON_L1        (control 12)
    const unsigned short KU_XPAD_RSHOULDER  = 0x0200; // E_PADBUTTON_R1        (control 13)
    const unsigned short KU_XPAD_A          = 0x1000; // E_PADBUTTON_CROSS     (control 8)
    const unsigned short KU_XPAD_B          = 0x2000; // E_PADBUTTON_CIRCLE    (control 9)
    const unsigned short KU_XPAD_X          = 0x4000; // E_PADBUTTON_SQUARE    (control 10)
    const unsigned short KU_XPAD_Y          = 0x8000; // E_PADBUTTON_TRIANGLE  (control 11)

    // ------------------------------------------------------------------------------------
    // The console analogue conventions. Every constant below is read out of
    // CgsInput::DeviceX360Pad::Construct @0x828DC578 and applied exactly the way
    // CgsInput::DeviceX360Pad::Update @0x828E7AB0 / ::DeadzoneAxis @0x828DCB20 apply it,
    // because the console applies the curve in the DEVICE layer -- i.e. BEFORE the pad
    // record this leaf writes. Feeding raw values here would be a different signal.
    // ------------------------------------------------------------------------------------
    const f32 KF_CONTROL_DOWN_THRESHOLD = 0.2f;        // Construct this+212: raw > 0.2 => "down"
    const f32 KF_TRIGGER_SATURATION     = 0.9f;        // Construct this+224
    const f32 KF_TRIGGER_DEADZONE       = 0.1f;        // Construct this+228
    const f32 KF_STICK_SATURATION       = 0.9f;        // Construct this+232
    const f32 KF_STICK_DEADZONE         = 0.2f;        // Construct this+236

    // CgsInput::DeviceX360Pad::DeadzoneAxis @0x828DCB20, sign-preserving, de-optimised
    // (the X360 emits the reciprocal `1.0 / (max - min)` as a multiply; the division is
    // the same value written the way the source had it).
    f32 ApplyStickDeadzone(f32 lfRaw)
    {
        if (lfRaw <= 0.0f)
        {
            if (lfRaw < -KF_STICK_SATURATION)
                lfRaw = -KF_STICK_SATURATION;
            const f32 lfOverDeadzone = KF_STICK_DEADZONE + lfRaw;
            if (lfOverDeadzone <= 0.0f)
                return lfOverDeadzone / (KF_STICK_SATURATION - KF_STICK_DEADZONE);
            return 0.0f;
        }

        if (lfRaw > KF_STICK_SATURATION)
            lfRaw = KF_STICK_SATURATION;
        const f32 lfOverDeadzone = lfRaw - KF_STICK_DEADZONE;
        if (lfOverDeadzone >= 0.0f)
            return lfOverDeadzone / (KF_STICK_SATURATION - KF_STICK_DEADZONE);
        return 0.0f;
    }

    // The thumb-word normalisation the console uses before the deadzone: the negative half
    // is scaled by 1/32768 (X360 literal 0.000030517578) and the positive half by 1/32767
    // (X360 literal 0.000030518509), so both halves reach exactly 1.0 at full deflection.
    f32 NormaliseThumb(short liThumb)
    {
        const f32 lfRaw = (liThumb <= 0) ? (liThumb * (1.0f / 32768.0f))
                                         : (liThumb * (1.0f / 32767.0f));
        return ApplyStickDeadzone(lfRaw);
    }

    // The trigger normalisation from the non-wheel arm of DeviceX360Pad::Update: raw/255
    // (X360 literal 0.0039215689), clamped at the saturation, shifted by the deadzone and
    // rescaled -- the trigger has no negative half, so no sign handling.
    f32 NormaliseTrigger(unsigned char lucTrigger)
    {
        f32 lfValue = lucTrigger * (1.0f / 255.0f);
        if (lfValue > KF_TRIGGER_SATURATION)
            lfValue = KF_TRIGGER_SATURATION;
        const f32 lfOverDeadzone = lfValue - KF_TRIGGER_DEADZONE;
        if (lfOverDeadzone >= 0.0f)
            return lfOverDeadzone / (KF_TRIGGER_SATURATION - KF_TRIGGER_DEADZONE);
        return 0.0f;
    }

    // FOCUS GATE: GetAsyncKeyState reads the GLOBAL key state and XInputGetState reads the
    // pad no matter which window owns the desktop, so without a foreground check the game
    // reacts to input meant for another app (verified: terminal Enters accepted the title
    // menu). Both host sources are gated on this process being foreground.
    // FLAG PC-platform leaf: BRN_INPUT_ALLOW_BACKGROUND=1 (set only by the scripted
    // boot-validation harness at launch) bypasses the gate so the harness's injected
    // key events land without fighting the desktop for foreground (the injection is
    // still deliberate keybd_event state, not stray typing -- see boot_test.ps1).
    bool IsProcessForeground()
    {
        static const bool s_bAllowBackground =
            (std::getenv("BRN_INPUT_ALLOW_BACKGROUND") != nullptr);
        if (s_bAllowBackground)
            return true;
        void* lpForeground = GetForegroundWindow();
        if (lpForeground == 0)
            return false;
        unsigned long luPid = 0;
        GetWindowThreadProcessId(lpForeground, &luPid);
        return luPid == GetCurrentProcessId();
    }

    // ====================================================================================
    // THE ONE BINDING TABLE (host device -> EGameInputActions slot).
    //
    // This is the PC stand-in for gaDefaultGameInputMapping (see the park at the top of
    // this file). Action ids are EGameInputActions
    // (references/DecFIGS/dwarfdump/GameSource/Input/GameInputActions.h:24) -- the same
    // ids CgsInput::InputIO::PadOutputInformation::maActionInfo[] is indexed by and the
    // ids BrnGame::BrnGameModule::BridgeControllerToWorld @0x823CD890 /
    // ::BridgeControllerToGui @0x823E6B18 read back out.
    //
    // The DRIVING rows exist because BridgeControllerToWorld reads exactly these slots
    // (asm-attested, store-for-store, into BrnWorld::PlayerVehicleControls):
    //     action  0 ACCELERATE   .mfValue        -> mfAcceleration
    //     action  1 BRAKE        .mfValue        -> mfBraking
    //     action  2 HANDBRAKE    .mfValue        -> mfHandBrake
    //     action  3 BOOST        .muStatus bit0  -> mbBoost, bit1 -> mbBoostBounce
    //     action  5 CHANGEVIEW   .muStatus bit1  -> mbChangeView
    //     action  7 RESET        .muStatus bit0  -> mbReset
    //     action  8 START        .muStatus bit1  -> mbStart
    //     action 10 POWERSWERVE_R .muStatus bit0 -> mbToggle        (left unbound, see below)
    //     action 13 HORN         .muStatus bit0  -> mbHorn
    //     actions 55/54 GUI_R/LSHOULDER .mfValue -> mfSpin = a55 - a54
    //   plus mfStickLX, run through the steering response curve -> mfSteering.
    //
    // ⚠️ KEYBOARD/MENU OVERLAP IS DELIBERATE AND MATCHES THE CONSOLE. On the pad the same
    // control feeds several actions at once (one ActionMapping entry carries FOUR action
    // ids), and the game's current mode decides which consumer reads them -- the left
    // stick both steers the car and moves the menu cursor. So on PC the arrow keys drive
    // BOTH the menu rows (41/42) and the steering/throttle axes, Return/Space stay on the
    // menu accept row, and Escape stays on the menu stop row. Nothing is stolen from the
    // verified boot chain: every pre-existing row below is byte-identical to what it was.
    //
    // ⚠️ PRE-EXISTING DEFECT, NOT TOUCHED HERE (it is its own task, and the tree already
    // documents it at BrnCarSelectVehicle_Input.cpp:63): the console vocabulary is
    // 49 GUI_SELECT = accept and 50 GUI_CANCEL = back, but the four rows below bind the
    // accept key to 45 GUI_START and the Escape key to 49 GUI_SELECT. Several GUI screens
    // carry a compensating "accept is 45 on PC" arm. Repairing it moves every boot-chain
    // press at once, so it is left exactly as the boot-verified build has it.
    //
    // ⚠️ NOT BOUND (honest gaps, all of them consumed by the bridge but with no defensible
    // host control): action 10 POWERSWERVE_R -> mbToggle, action 4 CRASHBREAKER, action 6
    // LOOKBACK, action 9 POWERSWERVE_L, action 11 DIRTY_TRICK, action 12 SCREENSHOT, and
    // the GUI_OPTION*/EVENT_DETAILS/CAR_LOG rows. They stay 0 rather than guessed.
    // ====================================================================================

    // Which XInput analogue axis, if any, additionally feeds an action's value.
    enum EPcAnalogueSource
    {
        E_PCANALOGUE_NONE     = 0,
        E_PCANALOGUE_LTRIGGER = 1,   // XINPUT bLeftTrigger  (E_PADBUTTON_L2, control 14)
        E_PCANALOGUE_RTRIGGER = 2    // XINPUT bRightTrigger (E_PADBUTTON_R2, control 15)
    };

    struct PcActionBinding
    {
        s32              iActionId;    // EGameInputActions slot in maActionInfo[]
        const int*       paiVKeys;     // virtual keys mapped to this action (0-terminated)
        unsigned short   uXPadButtons; // XINPUT wButtons mask mapped to this action
        EPcAnalogueSource eXPadAnalogue;
    };

    // ---- menu rows (unchanged from the boot-verified build) ----
    const int KAI_KEYS_ACCEPT[] = { 0x0D /*VK_RETURN*/, 0x20 /*VK_SPACE*/, 0 };
    const int KAI_KEYS_STOP[]   = { 0x1B /*VK_ESCAPE*/, 0 };
    const int KAI_KEYS_NEXT[]   = { 0x28 /*VK_DOWN*/, 0x27 /*VK_RIGHT*/, 0 };
    const int KAI_KEYS_PREV[]   = { 0x26 /*VK_UP*/, 0x25 /*VK_LEFT*/, 0 };
    // ---- driving rows ----
    const int KAI_KEYS_ACCELERATE[] = { 0x26 /*VK_UP*/,   'W', 0 };
    const int KAI_KEYS_BRAKE[]      = { 0x28 /*VK_DOWN*/, 'S', 0 };
    const int KAI_KEYS_HANDBRAKE[]  = { 0xA2 /*VK_LCONTROL*/, 0 };
    const int KAI_KEYS_BOOST[]      = { 0xA0 /*VK_LSHIFT*/, 0 };
    const int KAI_KEYS_CHANGEVIEW[] = { 'C', 0 };
    const int KAI_KEYS_RESET[]      = { 'R', 0 };
    const int KAI_KEYS_START[]      = { 'P', 0 };
    const int KAI_KEYS_HORN[]       = { 'H', 0 };
    const int KAI_KEYS_SPIN_LEFT[]  = { 'Q', 0 };
    const int KAI_KEYS_SPIN_RIGHT[] = { 'E', 0 };
    // FLAG PC binding choice (this table is the one place bindings live -- see the banner):
    // action 46 == KI_ACTION_PAUSE_MAIN_MAP, the OFFLINE pause. 'M' for map. Deliberately NOT
    // on a pad button: KU_XPAD_START is already action 8 and KU_XPAD_BACK already action 7,
    // and stacking pause onto either would fire two actions from one press.
    const int KAI_KEYS_PAUSE_MAP[]  = { 'M', 0 };

    const PcActionBinding KA_BINDINGS[] =
    {
        // -- the four menu rows the boot chain was verified on (order fixed: the harness
        //    event channel below indexes this table and only knows these four) ----------
        { 45, KAI_KEYS_ACCEPT, static_cast<unsigned short>(KU_XPAD_A | KU_XPAD_START), E_PCANALOGUE_NONE },
        { 49, KAI_KEYS_STOP,   KU_XPAD_B,                                              E_PCANALOGUE_NONE },
        { 41, KAI_KEYS_NEXT,   static_cast<unsigned short>(KU_XPAD_DPAD_DOWN | KU_XPAD_DPAD_RIGHT), E_PCANALOGUE_NONE },
        { 42, KAI_KEYS_PREV,   static_cast<unsigned short>(KU_XPAD_DPAD_UP   | KU_XPAD_DPAD_LEFT),  E_PCANALOGUE_NONE },

        // -- driving ------------------------------------------------------------------
        //  id  EGameInputActions       keyboard         pad button        pad analogue
        {  0, KAI_KEYS_ACCELERATE, 0,                E_PCANALOGUE_RTRIGGER }, // ACCELERATE  (RT / Up,W)
        {  1, KAI_KEYS_BRAKE,      0,                E_PCANALOGUE_LTRIGGER }, // BRAKE       (LT / Down,S)
        {  2, KAI_KEYS_HANDBRAKE,  KU_XPAD_X,        E_PCANALOGUE_NONE     }, // HANDBRAKE   (X / LCtrl)
        {  3, KAI_KEYS_BOOST,      KU_XPAD_A,        E_PCANALOGUE_NONE     }, // BOOST       (A / LShift)
        {  5, KAI_KEYS_CHANGEVIEW, KU_XPAD_Y,        E_PCANALOGUE_NONE     }, // CHANGEVIEW  (Y / C)
        {  7, KAI_KEYS_RESET,      KU_XPAD_BACK,     E_PCANALOGUE_NONE     }, // RESET       (Back / R)
        {  8, KAI_KEYS_START,      KU_XPAD_START,    E_PCANALOGUE_NONE     }, // START       (Start / P)
        { 13, KAI_KEYS_HORN,       KU_XPAD_LTHUMB,   E_PCANALOGUE_NONE     }, // HORN        (L3 / H)
        { 54, KAI_KEYS_SPIN_LEFT,  KU_XPAD_LSHOULDER, E_PCANALOGUE_NONE    }, // GUI_LSHOULDER -> -mfSpin
        { 55, KAI_KEYS_SPIN_RIGHT, KU_XPAD_RSHOULDER, E_PCANALOGUE_NONE    }, // GUI_RSHOULDER -> +mfSpin

        // -- the offline pause (pause wave, 2026-08-26) --------------------------------
        // ADDITIVE. Action 46 was ABSENT FROM THIS TABLE ENTIRELY, which is why the offline
        // pause could not be reached from a PC keyboard at all: InGame::HandleControllerInput
        // has had `case KI_ACTION_PAUSE_MAIN_MAP: PauseGame(true,false)` all along
        // (BrnInGame.cpp:436) and nothing could ever deliver a 46.
        // ⚠️ This is NOT the documented 45-vs-49 accept-vocabulary defect and it does not touch
        // row 45 -- that repair is a separate decision and is deliberately not made in passing.
        { 46, KAI_KEYS_PAUSE_MAP,  0,                 E_PCANALOGUE_NONE    }, // PAUSE_MAIN_MAP (M)
    };
    const u32 KU_NUM_BINDINGS = sizeof(KA_BINDINGS) / sizeof(KA_BINDINGS[0]);

    // ---- keyboard overlay for the two left-stick axes ----------------------------------
    // The console accumulates every device bound to a port into one axis value
    // (InputPads::FillRawData @0x828E7350 SUMS the axes then clamps to [-1,+1]), so the
    // keyboard is summed onto the pad's stick exactly the same way -- a second device, not
    // an override. Full deflection is 1.0 because a key has no travel.
    const int KAI_KEYS_STEER_LEFT[]  = { 0x25 /*VK_LEFT*/,  'A', 0 };
    const int KAI_KEYS_STEER_RIGHT[] = { 0x27 /*VK_RIGHT*/, 'D', 0 };

    bool AnyKeyDown(const int* lpiKeys)
    {
        for (const int* lpiKey = lpiKeys; *lpiKey != 0; ++lpiKey)
        {
            if ((GetAsyncKeyState(*lpiKey) & 0x8000) != 0)
                return true;
        }
        return false;
    }

    // InputPads::FillRawData's axis accumulation tail: rw::math::fpu::Clamp(sum, -1, +1).
    f32 ClampAxis(f32 lfValue)
    {
        if (lfValue < -1.0f)
            return -1.0f;
        if (lfValue > 1.0f)
            return 1.0f;
        return lfValue;
    }

    // FLAG PC-platform leaf: the unattended boot harness signals one named event per host
    // action. Unlike synthetic desktop keystrokes these survive a locked or disconnected
    // desktop, and -- because they are named kernel objects rather than global key state --
    // they cannot leak into whatever other window happens to own the desktop. The channel is
    // enabled only with the harness's BRN_INPUT_ALLOW_BACKGROUND environment marker.
    //
    // ⭐ THE RESET MODE LIVES ENTIRELY ON THE HARNESS SIDE; this code is one zero-timeout wait
    // either way, and each channel is sampled exactly ONCE per input update (see UpdatePlayer0):
    //   * the four MENU channels are created AUTO-RESET, so one Set() is observed by exactly one
    //     input update -- a tap, which is what a menu press is;
    //   * the DRIVING channels are created MANUAL-RESET, so they read as active on every input
    //     update between the harness's Set() and its Reset() -- a HOLD, which is what a throttle
    //     is. A pedal you can only tap for one frame cannot drive a car.
    // Nothing here distinguishes the two: WaitForSingleObject(h, 0) == WAIT_OBJECT_0 is "this
    // control is down right now" in both cases.
    //
    // The driving rows deliberately mirror a PAD player, not a keyboard one: ACCELERATE/BRAKE
    // land in their maActionInfo slots (where the triggers put them) and steering lands on
    // mfStickLX (where the left stick puts it) -- see HarnessSteerActive below. So the game
    // sees an ordinary controller, through the ordinary bridge.
    bool ConsumeHarnessAction(s32 liActionId)
    {
        static const bool s_bHarnessEnabled =
            (std::getenv("BRN_INPUT_ALLOW_BACKGROUND") != nullptr);
        if (!s_bHarnessEnabled)
            return false;

        const char* lpcEventName = 0;
        switch (liActionId)
        {
        case 45: lpcEventName = "Local\\BurnoutPC_Input_Accept";     break;
        case 49: lpcEventName = "Local\\BurnoutPC_Input_Stop";       break;
        case 41: lpcEventName = "Local\\BurnoutPC_Input_Next";       break;
        case 42: lpcEventName = "Local\\BurnoutPC_Input_Prev";       break;
        // -- driving. These three ids are the rows BridgeControllerToWorld reads straight out
        //    of maActionInfo[] into PlayerVehicleControls (asm-attested: [0].mfValue ->
        //    mfAcceleration, [1] -> mfBraking, [2] -> mfHandBrake), i.e. exactly the slots the
        //    right trigger / left trigger / X button fill.
        case  0: lpcEventName = "Local\\BurnoutPC_Input_Accelerate"; break;
        case  1: lpcEventName = "Local\\BurnoutPC_Input_Brake";      break;
        case  2: lpcEventName = "Local\\BurnoutPC_Input_HandBrake";  break;
        // -- the offline pause. AUTO-RESET like the four menu channels (a TAP): one press
        //    opens the map, one press of Accept inside it comes back out.
        case 46: lpcEventName = "Local\\BurnoutPC_Input_PauseMap";   break;
        default: return false;
        }

        // SYNCHRONIZE == 0x00100000; WAIT_OBJECT_0 == 0. The harness creates every event
        // before launching the game, so a successful zero-time wait says the control is down
        // this update (and, for the auto-reset menu channels, consumes the one request).
        // The handle cache is indexed by KA_BINDINGS row, so every id in the switch above must
        // also be a bound row -- 45/49/41/42 are rows 0..3 and 0/1/2 are rows 4..6. Action 46
        // is the appended last row; the lookup below is BY ACTION ID, not by a fixed index, so
        // appending grows the cache without disturbing any existing row.
        static void* sapHarnessEvents[KU_NUM_BINDINGS] = {};
        u32 luBinding = 0;
        while (luBinding < KU_NUM_BINDINGS
               && KA_BINDINGS[luBinding].iActionId != liActionId)
            ++luBinding;
        if (luBinding >= KU_NUM_BINDINGS)
            return false;
        if (sapHarnessEvents[luBinding] == 0)
            sapHarnessEvents[luBinding] = OpenEventA(0x00100000u, 0, lpcEventName);
        return sapHarnessEvents[luBinding] != 0
            && WaitForSingleObject(sapHarnessEvents[luBinding], 0) == 0;
    }

    // The harness's two STEERING channels. Steering is not an action -- on a pad it is the left
    // stick, and BridgeControllerToWorld takes mfStickLX (not any maActionInfo slot) through the
    // response curve into mfSteering. So these are summed onto mfStickLX in exactly the place,
    // and by exactly the +/-1.0 full deflection, that the keyboard's A/D rows use; a second
    // device on the same port, which is what InputPads::FillRawData @0x828E7350 does with one.
    // They carry no action id and therefore no KA_BINDINGS row, hence their own handle pair.
    bool HarnessSteerActive(bool lbRight)
    {
        static const bool s_bHarnessEnabled =
            (std::getenv("BRN_INPUT_ALLOW_BACKGROUND") != nullptr);
        if (!s_bHarnessEnabled)
            return false;

        static void* sapSteerEvents[2] = {};
        const u32 luIndex = lbRight ? 1u : 0u;
        if (sapSteerEvents[luIndex] == 0)
        {
            sapSteerEvents[luIndex] = OpenEventA(0x00100000u, 0,
                    lbRight ? "Local\\BurnoutPC_Input_SteerRight"
                            : "Local\\BurnoutPC_Input_SteerLeft");
        }
        return sapSteerEvents[luIndex] != 0
            && WaitForSingleObject(sapSteerEvents[luIndex], 0) == 0;
    }
}

namespace CgsInput
{
    void InputPadsPC::UpdatePlayer0(InputIO::OutputBuffer* lpOutput)
    {
        InputIO::PadOutputInformation& lrPad = lpOutput->maPadOutputInformation[0];

        // One-time record bring-up (the console fill inherits Construct's zeroed record).
        static bool sbInitialised = false;
        if (!sbInitialised)
        {
            std::memset(&lrPad, 0, sizeof(lrPad));
            sbInitialised = true;
        }

        // Host device reads. Both sources are focus-gated (see IsProcessForeground).
        const bool lbForeground = IsProcessForeground();
        XInputState lXState;
        std::memset(&lXState, 0, sizeof(lXState));
        bool lbXPad = false;
        if (lbForeground)
        {
            if (XInputGetStateFn lpfGetState = ResolveXInputGetState())
                lbXPad = (lpfGetState(0, &lXState) == 0);   // ERROR_SUCCESS
        }

        // ---- the analogue axis block (CgsInput::EPadAxis, the record's leading floats) ----
        // E_PADAXIS_0_X/0_Y are the left stick, E_PADAXIS_1_X/1_Y the right stick; the two
        // trailing axes are E_WHEELAXIS_STEERING / E_WHEELAXIS_PEDALS, which
        // DeviceX360Pad::Update writes as 0 for every non-wheel device type (they are only
        // filled on its `meType == 2` wheel arm, and only that arm is read by the bridges'
        // meControllerState == 2 paths). Pad deflections carry the console deadzone curve;
        // the keyboard is summed on as a second device and the sum is clamped, exactly as
        // InputPads::FillRawData does.
        f32 lfStickLX = lbXPad ? NormaliseThumb(lXState.Gamepad.sThumbLX) : 0.0f;
        f32 lfStickLY = lbXPad ? NormaliseThumb(lXState.Gamepad.sThumbLY) : 0.0f;
        if (lbForeground)
        {
            if (AnyKeyDown(KAI_KEYS_STEER_LEFT))    lfStickLX -= 1.0f;
            if (AnyKeyDown(KAI_KEYS_STEER_RIGHT))   lfStickLX += 1.0f;
            if (AnyKeyDown(KAI_KEYS_ACCELERATE))    lfStickLY += 1.0f;
            if (AnyKeyDown(KAI_KEYS_BRAKE))         lfStickLY -= 1.0f;
        }
        // The harness steering channels sum onto the same stick as a further device. Sampled
        // once each, here, because mfStickLX is the ONLY place steering reaches the bridge.
        // They do NOT touch mfStickLY: a pad player's throttle is the trigger (action 0), not
        // the stick, and mfStickLY is the bridge's mfForwardSteering (in-air pitch), not gas.
        if (HarnessSteerActive(false)) lfStickLX -= 1.0f;
        if (HarnessSteerActive(true))  lfStickLX += 1.0f;

        lrPad.mfStickLX = ClampAxis(lfStickLX);                                        // E_PADAXIS_0_X
        lrPad.mfStickLY = ClampAxis(lfStickLY);                                        // E_PADAXIS_0_Y
        lrPad.mfStickRX = lbXPad ? NormaliseThumb(lXState.Gamepad.sThumbRX) : 0.0f;     // E_PADAXIS_1_X
        lrPad.mfStickRY = lbXPad ? NormaliseThumb(lXState.Gamepad.sThumbRY) : 0.0f;     // E_PADAXIS_1_Y
        lrPad.mfAxis10  = 0.0f;   // E_WHEELAXIS_STEERING -- wheel devices only
        lrPad.mfAxis14  = 0.0f;   // E_WHEELAXIS_PEDALS   -- wheel devices only

        // ---- the per-action {value, status} table -----------------------------------------
        // Per action: the value is the raw control value (a key or pad button has no travel,
        // so it is full scale; a trigger carries its normalised curve), and the status is the
        // console muStatus contract (bit0 held, bit1 pressed-this-frame, bit2 released-this-
        // frame) with "held" being DeviceX360Pad::Update's `raw > 0.2` control-down test.
        // ✅✅ THE OFFLINE PAUSE SHIPS (pauseresume wave, 2026-08-27). Action 46 is bound above
        // and the whole chain behind it works, pause AND resume, repeatedly:
        //     action 46 -> CrashNavMapMain::OnEnter -> GuiEventActivateCrashNav(false) ->
        //     game event 93 -> RequestPause(4) -> action 86 -> BrnGameModule::CheckGameActions
        //     sets mbSimPaused + stops the sim timer -> ConstructUpdateSetFromFsm raises
        //     update-set bit 0x1 -> the in-game set goes 0x88 -> 0x89 and the WORLD FREEZES.
        // Accept (action 45) walks it back: 0x89 -> 0x88, and the world runs again.
        //
        // ✅ BRN_ENABLE_PAUSE IS DELETED (2026-08-27), the same retirement BRN_ENABLE_CRASH_ENTRY
        // got a day earlier. It gated the KEY -- not the chain -- for one reason: the resume frame
        // tripped a halting dev assert (`mpData != NULL`, BrnContactSpyInterface.h:82, under
        // PropEntityModule::ProcessContacts). That reason is GONE, and the fix was not here.
        //
        // ⭐⭐ WHAT IT ACTUALLY WAS, and it took two waves to find because the first two answers
        // were both wrong in an instructive way:
        //   * REFUTED (pausebit wave): "update-set bit 0x1 is read under TWO different meanings".
        //     It is not. Settled from the X360 asm at three independent sites -- PhysicsModule::
        //     Update @0x825B0640 (`clrlwi r30, r18, 31` @0x825B0688 is the ONLY mask ever applied
        //     to the update-set argument, tested three times, the third skipping
        //     BridgeSimulationToOutput @0x825B2304, the SOLE binder of the contact-spy interface),
        //     PhysicsModule::PostSceneUpdate @0x825ABC10, and PropEntityModule::PostPhysicsUpdate
        //     @0x823031D8. ONE bit, ONE meaning: "this frame carries no physics result", with every
        //     consumer of the interface gated on it. The assert is that invariant's guard, and this
        //     tree already reproduced every gate faithfully. (The "two meanings" story came from
        //     reading two line numbers in one TU as one call chain; they were two functions.)
        //   * THE ACTUAL CAUSE (this wave): a PC-BUILD GUARD **WE** ADDED at the top of
        //     PhysicsModule::Update -- `if (!GetSimTimerStatus()->IsRunning()) return;` -- made a
        //     one-frame-stale MIRROR of the timer load-bearing. On the resume frame
        //     ConstructUpdateSetFromFsm reads mbSimPaused LIVE, so bit 0 clears at once, while the
        //     physics module's timer SNAPSHOT still said stopped: physics went inert on a frame
        //     whose bit 0 said "results are coming". A THIRD state the console never has.
        //     The console's own Update never reads that flag at all (exhaustively: every access to
        //     the sim TimerStatus in all 1999 instructions is +4/+8/+0x10/+0x14 -- never +0xC).
        //     THE GUARD IS DELETED; its full obituary, asm proof and measurement live at the site,
        //     in BrnPhysicsModuleUpdateFunctions.cpp.
        // ⭐ INVENTED-ARM class, third sighting: defensive code we added that the console lacks.
        // The fix was to delete the invention, NOT to silence the assert or null-check mpData --
        // that assert is the console's and was correctly reporting a genuinely unbound interface.
        //
        // ⚠️ THE SECOND HALF OF THE FIX IS NOT IN THIS FILE EITHER. Deleting the physics guard
        // exposed a real reconstruction gap one module over: TrafficEntityModule::PreSceneUpdate
        // guarded its block on `!IsPaused()` alone, dropping the console's `&& !lbSimPaused`, so
        // the traffic frame-clock FREE-RAN through a pause and the first decision frame after the
        // resume tripped `muLastParamCalculated >= KU_MAX_PARAMS`. Restored (asm-attested) in
        // BrnTrafficEntityModule_wT1_02.cpp; the full note is there.
        //
        // ⭐⭐ THE CONTROL THAT KEPT TWO WRONG ANSWERS FROM BEING PUBLISHED, and it is worth
        // keeping: a frozen 3D frame CANNOT tell a pause from a hang. Measure the debug-overlay
        // strip separately from the world -- a paused WORLD under a LIVE renderer freezes one band
        // and not the other, while a hang freezes both. Measured on this build over three
        // pause/resume cycles: inside a pause the world band's frame-to-frame mean |luma delta|
        // sits at 0.018-0.024 while the overlay band stays at 2.3-3.1 (the running-world floor is
        // ~0.15, so the world is an order of magnitude below it and the overlay is untouched).
        // The 2026-08-26 assert FAILED that test -- BOTH bands went to 0.000 for the remaining
        // ~2900 presents. ⛔ The absolute numbers are band-definition-specific; what transfers is
        // the SPLIT, never a threshold copied between waves.
        static bool sabWasDown[KU_NUM_BINDINGS] = {};
        for (u32 luBind = 0; luBind < KU_NUM_BINDINGS; ++luBind)
        {
            const PcActionBinding& lrBinding = KA_BINDINGS[luBind];

            f32 lfValue = 0.0f;
            if (lbForeground && AnyKeyDown(lrBinding.paiVKeys))
                lfValue = 1.0f;
            if (lfValue < 1.0f && ConsumeHarnessAction(lrBinding.iActionId))
                lfValue = 1.0f;
            if (lbXPad)
            {
                if (lrBinding.uXPadButtons != 0
                    && (lXState.Gamepad.wButtons & lrBinding.uXPadButtons) != 0)
                {
                    lfValue = 1.0f;
                }
                // FillRawData accumulates several controls onto one action by MAX, so an
                // analogue source only raises the value a digital source already set.
                f32 lfAnalogue = 0.0f;
                if (lrBinding.eXPadAnalogue == E_PCANALOGUE_LTRIGGER)
                    lfAnalogue = NormaliseTrigger(lXState.Gamepad.bLeftTrigger);
                else if (lrBinding.eXPadAnalogue == E_PCANALOGUE_RTRIGGER)
                    lfAnalogue = NormaliseTrigger(lXState.Gamepad.bRightTrigger);
                if (lfAnalogue > lfValue)
                    lfValue = lfAnalogue;
            }

            const bool lbDown = (lfValue > KF_CONTROL_DOWN_THRESHOLD);

            u32 luStatus = 0;
            if (lbDown)
                luStatus |= 1u;                          // held
            if (lbDown && !sabWasDown[luBind])
                luStatus |= 2u;                          // pressed edge
            if (!lbDown && sabWasDown[luBind])
                luStatus |= 4u;                          // released edge
            sabWasDown[luBind] = lbDown;

            InputIO::ActionInfo& lrAction = lrPad.maActionInfo[lrBinding.iActionId];
            lrAction.mfValue  = lfValue;
            lrAction.muStatus = luStatus;
        }

        // Connection/state tail: the pad is present and assigned to player 0.
        // ⓘ The DWARF names these three members miPlayerId / meControllerType (a
        // CgsInput::Device::EType) / mbPadIdle -- see the note in CgsInputModuleIO.h.
        lrPad.muConnectionWord  = 0;   // player 0 (GetPadInfoForPlayer0's gate)
        lrPad.meControllerState = 1;   // a standard pad (2 == wheel: the bridges' wheel arms)
        lrPad.mbDisconnected    = 0;   // not idle
    }
}
