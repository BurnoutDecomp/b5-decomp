#include "GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h"

#include <cstring>   // std::memset
#include <cstdlib>   // std::getenv (the harness focus-gate bypass)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog (the pause gate's one-shot)
#include "GameShared/GameClasses/System/CgsHarnessSlot.h"     // BRN_HARNESS_SLOT channel-name suffix (parallel harness slots)
#include "GameSource/Input/GameInputActions.h"                // EGameInputActions -- the action vocabulary KA_BINDINGS binds to
#include "GameShared/GameClasses/System/Input/PC/CgsDebugKeyboardPC.h"

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
// ✅✅ gaDefaultGameInputMapping IS RECOVERED (bug-test wave, 2026-09-06, lane input).
// This banner used to read "IS NOT RECOVERED ... cannot be read on this host", and on the
// strength of that park EVERY pad binding in this file was a PC guess. Both halves of the
// park were wrong, and the fix was reading the image, not reasoning about it:
//   * InputPads::Update @0x828F8690 is indeed an export HOLE -- but tools/re/ppcdis.py
//     disassembles it straight out of the flat image, and it is the function that walks the
//     table (`addi r9, r10, 0x7BA` @0x828F88D8, 28 iterations of `lbz`/`extsb`/`slwi 3`/
//     `lfsx`/`fcmpu`/`stfsx` -- i.e. per raw control i, per action id in entry i,
//     maActionInfo[id].mfValue = MAX(mfValue, rawControl[i]); -1 == no action).
//   * gaDefaultGameInputMapping has no data export, but it does not need one: it is the
//     LITERAL ARGUMENT of BrnGame::BrnGameModule::PrepareInitialInputMapping @0x823BCF40 --
//         PostWorldInputBuffer::PostMappingRequest(lpInputBuffer, &unk_82CDBEB8, -1)
//     which memcpy's exactly 112 bytes from 0x82CDBEB8, and
//     InputModule::ProcessMappingQueue @0x828E7098 copies those 112 bytes into all four
//     ports (pad id -1 == "all pads"). It is the ONLY PostMappingRequest call site in the
//     build, so 0x82CDBEB8 is the console's whole control->action map. 112 bytes / 4 == the
//     28 pad controls of CgsInput::EPadButton; InputPads::Construct @0x828EFAF0 pre-fills
//     each port's copy with 112 bytes of -1, so nothing else contributes.
// The table is transcribed verbatim into KA_DEFAULT_GAME_INPUT_MAPPING below and is now the
// ONLY thing that decides what the PAD does. tools/tests/offline/input_mapping_coverage.py
// re-reads 0x82CDBEB8 out of the image and diffs it against this build's own
// `[input-map]` dump, so a future drift is a red test rather than a comment.
// The per-action SEMANTICS were already attested -- EGameInputActions
// (references/DecFIGS/dwarfdump/GameSource/Input/GameInputActions.h:24) and
// EPadButton/EPadAxis (.../Devices/PS3/CgsInputDevicePS3Pad.h:40/:84) -- as was the
// CONSUMER side, store-for-store, by BridgeControllerToWorld @0x823CD890.
// ⚠️ The KEYBOARD half (KA_BINDINGS) is still a PC binding choice: the console has no
// keyboard and there is no table to recover for one. It is flagged as such at its own banner.
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

// THE GAME'S OWN WINDOW, by handle -- the thing the focus gate below compares against.
// It is the SAME OBJECT as `renderengine::hWnd` in pc/gcm/renderengine/device.h (defined in
// device.cpp, created by CgsSystem::HardwareInit::InitializeHardware), re-declared here so this
// file need not pull <Windows.h> in -- exactly the pattern Xbox2SurfaceShims.h already uses for
// renderengine::gAntiAliasing, and for the same reason (the lean NOUSER/NOGDI defines the game
// TUs carry conflict with it). ⚠️ `HWND` IS `struct HWND__*`, so the forward declaration has to
// be `struct HWND__` and the type has to be spelled `HWND__*`: a `void*` here would mangle to a
// DIFFERENT decorated name and link against nothing (the same class-key trap as `class X;` for a
// `struct X`).
struct HWND__;
namespace renderengine { extern HWND__* hWnd; }

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
    // ⚠️ TWO DIFFERENT "down" THRESHOLDS, and this file used the wrong one for the actions.
    //   * 0.2 (DeviceX360Pad::Construct this+212) is the DEVICE's per-CONTROL threshold. It
    //     feeds only the device's own 28-bool is-down/was-down array (Update @0x828E7AB0's
    //     `*v89 = *v88 > *(a1 + 212)` tail), which nothing in the action path reads --
    //     FillRawData @0x828E7350 accumulates the raw FLOATS (device+76+4i), not the bools.
    //   * 0.1 (the literal at rodata 0x820F78E4) is the threshold InputPads::Update
    //     @0x828F8690 compares each ACTION's accumulated mfValue against before setting
    //     muStatus bit0/bit1/bit2 (`lfs f0, 0x78e4(r17)` / `fcmpu` / `blt` @0x828F89B4).
    // This leaf used 0.2 for the action test, so an analogue source (a trigger) counted as
    // held later than the console counts it. The device constant is kept for the curve
    // constants around it; the action test now uses the console's own 0.1.
    const f32 KF_ACTION_DOWN_THRESHOLD  = 0.1f;        // 0x820F78E4, InputPads::Update
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

    // ====================================================================================
    // THE FOCUS GATE, IN TWO PIECES -- and the split is the whole point.
    //
    // GetAsyncKeyState reads the GLOBAL key state: it answers "is this key physically down
    // anywhere on this machine", with no notion of which window the person meant to type into.
    // XInputGetState is different in kind -- it reads a DEVICE somebody deliberately picked up,
    // and Windows has no per-window notion of a pad at all.
    //
    // ⭐⭐ 2026-09-06 (lane quiet). The user's report is verbatim: "only uses input on the actual
    //   window not system wide, so that i can scroll on x without it going all over the place".
    //   The single gate that used to live here answered that with `return true` whenever
    //   BRN_INPUT_ALLOW_BACKGROUND was set -- which is EVERY harness run -- so the keyboard half
    //   was ungated for the entire class of runs the complaint is about.
    //   ⭐ MEASURED, RED: scratch\bugtest\runs\quiet_focus_input\20260906_145815. A boot with no
    //   -Drive and NOTHING pressed by the harness, while a topmost window (not the game) held
    //   the foreground for 117 of 117 samples and W+A were physically down for all 117: the
    //   game's own [motion] probe reports gas rising to 1.000 and steering to 0.393, and the car
    //   DROVE 20.85 m. Keys typed into another window drove the test car, exactly as reported.
    //
    // ⭐ THE RULE, and it holds with AND without BRN_INPUT_ALLOW_BACKGROUND:
    //   keyboard state is consumed ONLY while GetForegroundWindow() is the game's OWN window.
    //   BRN_INPUT_ALLOW_BACKGROUND exists so the HARNESS's named-event channels and an attached
    //   XInput pad are accepted while the window sits in the background; it never meant "read
    //   the person's keyboard", and it no longer can.
    // ⚠️ GetForegroundWindow can return NULL during a focus transition (and does, while another
    //   process is being activated). NULL is treated as NOT US -- the safe direction, because the
    //   alternative is a window-less instant in which every global key is live.
    // ⚠️ COMPARED BY HWND, not by process id. The window is owned by the render/hardware leaf and
    //   is looked up as a pointer, not re-found from a title string per poll.
    // ⓘ MOUSE: there is nothing to gate. This leaf consumes no mouse state at all -- no
    //   GetCursorPos, no raw input, no WM_MOUSE* consumer, and no KA_BINDINGS row carries
    //   VK_LBUTTON/VK_RBUTTON/VK_MBUTTON -- so "the mouse only when focused" holds because the
    //   mouse is never read. Anything added later belongs behind IsGameWindowForeground().
    // ====================================================================================
    bool IsGameWindowForeground()
    {
        void* lpForeground = GetForegroundWindow();
        if (lpForeground == 0)
            return false;                              // focus transition -- not us
        if (renderengine::hWnd != 0)
            return lpForeground == static_cast<void*>(renderengine::hWnd);
        // Before InitializeHardware has created the window there is no handle to compare, so
        // fall back to the process test this gate has always used. It cannot let a foreign
        // window's keys in (a different process fails it), and no input is consumed this early
        // anyway -- UpdatePlayer0 does not run before the game module exists.
        unsigned long luPid = 0;
        GetWindowThreadProcessId(lpForeground, &luPid);
        return luPid == GetCurrentProcessId();
    }

    // The PAD gate. FLAG PC-platform leaf: BRN_INPUT_ALLOW_BACKGROUND=1 (set only by the scripted
    // harness at launch) keeps the XInput read alive while the window is in the background, so a
    // scripted run is not fighting the desktop for foreground. Left as it was, deliberately: a
    // pad is a device in someone's hands, not a window, and gating it on focus would break every
    // unattended run that uses one without answering the complaint above, which is about the
    // keyboard. A pad in the user's hand while a harness run is up is still theirs to hold.
    bool IsPadReadAllowed()
    {
        static const bool s_bAllowBackground =
            (std::getenv("BRN_INPUT_ALLOW_BACKGROUND") != nullptr);
        if (s_bAllowBackground)
            return true;
        return IsGameWindowForeground();
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
    // ✅✅ THE ACCEPT-VOCABULARY DEFECT IS FIXED (input-vocabulary wave, 2026-08-29). It used
    // to read "PRE-EXISTING DEFECT, NOT TOUCHED HERE" and four screens carried an invented
    // `case 45` arm to work around it. What was wrong, and what it is now:
    //
    //   was:  row 45 <- Return/Space + pad A + pad START      (accept fired GUI_START)
    //         row 49 <- Escape       + pad B                  (back   fired GUI_SELECT)
    //         50 GUI_CANCEL bound NOWHERE -- unreachable on PC by any device, although it is
    //         the id half the CrashNav family branches on for "back".
    //   now:  row 49 GUI_SELECT <- Return/Space  + pad A      (accept, the console's A)
    //         row 50 GUI_CANCEL <- Escape/Bksp   + pad B      (back,   the console's B)
    //         row 45 GUI_START  <- 'P'           + pad START  (a THIRD thing, not accept)
    //
    // The names come from the DWARF (GameSource/Input/GameInputActions.h, now landed in the
    // tree and included above); every consumer that used to recognise only 45 also already
    // recognised 49, so moving the accept key breaks nothing, and the four compensating arms
    // (BrnBootProfile / BrnCarSelectVehicle_Input / BrnCarSelectLivery_Input / BrnIntro) were
    // asm-checked against their console bodies -- none of those four consoles handles 45 --
    // and DELETED in the same wave. BrnCarSelectUnlock's `45 || 49` is genuine (X360
    // @0x824CA420 tests both) and stays.
    //
    // ⭐ Why 45 gets its own KEY rather than doubling up on Enter: BrnBootLegal @0x82477270
    // routes a 45 to BOTH the back/accept path and the press-start path, and the map routes
    // 45 to EXIT. Enter on both 45 and 49 would, on the map, fire ToggleRoadPanelScores AND
    // "GO_BACK" from one press. 'P' is already action 8 (world START) and pad START already
    // emits 8, so 'P'/START emitting 8 + 45 together IS the console control.
    //
    // ⛔⛔ THE "ONE PHYSICAL PRESS => EXACTLY ONE GUI ACTION ID" RULE WAS A GUESS, AND THE
    // CONSOLE REFUTES IT. It used to stand here and it is why this table did not multi-map:
    // DPAD_UP was given 41 only, 37/38 were keyboard-only, 43/44 were ',' and '.', 56/57 were
    // keyboard-only "because stacking them on the triggers would emit two action ids from one
    // pull". gaDefaultGameInputMapping (now read out of the image, see the banner at the top)
    // does exactly what the rule forbade: entry 0 (E_PADBUTTON_UP) is { 37, 41, 58, 14 } --
    // ONE dpad press emits FOUR action ids, and every other entry carries two or three. The
    // multi-id fan-out IS the console's design; screens ignore the ids they do not handle.
    // The measured cost of the rule was 29 player-visible pad bindings that simply did not
    // exist on PC -- LOOKBACK on L1, RESET on R1, map zoom on L2, event inspect on R2, the
    // three GUI_OPTION rows, DIRTY_TRICK, both thumb-stick clicks, menu left/right on the dpad
    // and menu navigation on the left stick, which is the bug this file was fixed for.
    //
    // FLAG PC binding choice: only the KEYBOARD rows below are a PC choice -- the console has
    // no keyboard, so there is no table to recover for one. The PAD is no longer a choice at
    // all: it is KA_DEFAULT_GAME_INPUT_MAPPING, transcribed from 0x82CDBEB8.
    // ====================================================================================

    // ====================================================================================
    // gaDefaultGameInputMapping -- the console's ActionMapping[28], each entry four
    // EGameInputActions ids (-1 == unused), transcribed byte-for-byte from the X360 image at
    // 0x82CDBEB8 (file offset 0xCDBEB8 in the flat dump; see the banner at the top of this
    // file for the three console functions that produce, distribute and consume it).
    // Row i is CgsInput::EPadButton i; the comment names it. Verified against the image by
    // tools/tests/offline/input_mapping_coverage.py, which re-reads those 112 bytes.
    //
    // ⓘ Rows 24..27 are the WHEEL controls. DeviceX360Pad::Update @0x828E7AB0 writes 0.0 into
    // them for every non-wheel device (its `meType == 2` arm is the only writer), so they are
    // inert for a pad -- they are kept because the table is transcribed, not curated.
    // ====================================================================================
    const u32 KU_NUM_PAD_CONTROLS = 28;   // FillRawData @0x828E7350's `i < 28` + lbValidControl
    const u32 KU_MAPPING_SLOTS    = 4;    // int8_t[4] per entry; 28 * 4 == the 112-byte memcpy

    const s8 KA_DEFAULT_GAME_INPUT_MAPPING[KU_NUM_PAD_CONTROLS][KU_MAPPING_SLOTS] =
    {
        { 37, 41, 58, 14 },   //  0 E_PADBUTTON_UP        GUI_DPAD_UP, GUI_UP, GUI_EVENT_DETAILS, dbg
        { 38, 42, 60, 15 },   //  1 E_PADBUTTON_DOWN      GUI_DPAD_DOWN, GUI_DOWN, SET_PLAYERSTATS_MAX, dbg
        { 39, 43, 59, 16 },   //  2 E_PADBUTTON_LEFT      GUI_DPAD_LEFT, GUI_LEFT, GUI_CAR_LOG, dbg
        { 40, 44, -1, 17 },   //  3 E_PADBUTTON_RIGHT     GUI_DPAD_RIGHT, GUI_RIGHT, dbg
        {  8, 45, -1, -1 },   //  4 E_PADBUTTON_START     START, GUI_START
        { 50, 46, -1, 36 },   //  5 E_PADBUTTON_SELECT    GUI_CANCEL, GUI_BACK, DEBUG_STEP   (the Back button)
        { 13, 47, -1, 34 },   //  6 E_PADBUTTON_LTHUMB    HORN, GUI_LTHUMB, dbg              (L3)
        { 12, 48, -1, 35 },   //  7 E_PADBUTTON_RTHUMB    SCREENSHOT, GUI_RTHUMB, dbg        (R3)
        {  3, 49, -1, 19 },   //  8 E_PADBUTTON_CROSS     BOOST, GUI_SELECT, dbg             (A)
        { 11, 53, 50, 21 },   //  9 E_PADBUTTON_CIRCLE    DIRTY_TRICK, GUI_OPTION2, GUI_CANCEL, dbg (B)
        {  2, 51, -1, 20 },   // 10 E_PADBUTTON_SQUARE    HANDBRAKE, GUI_OPTION0, dbg        (X)
        {  5, 52, -1, 18 },   // 11 E_PADBUTTON_TRIANGLE  CHANGEVIEW, GUI_OPTION1, dbg       (Y)
        {  6, 54, -1, 24 },   // 12 E_PADBUTTON_L1        LOOKBACK, GUI_LSHOULDER, dbg
        {  7, 55, -1, 25 },   // 13 E_PADBUTTON_R1        RESET, GUI_RSHOULDER, dbg
        {  1, 56, -1, 22 },   // 14 E_PADBUTTON_L2        BRAKE, GUI_LTRIGGER, dbg           (left trigger)
        {  0, 57, -1, 23 },   // 15 E_PADBUTTON_R2        ACCELERATE, GUI_RTRIGGER, dbg      (right trigger)
        { -1, 41, -1, 26 },   // 16 E_PADBUTTON_ANALOGUE_0_UP     GUI_UP, dbg               (left stick)
        { -1, 42, -1, 27 },   // 17 E_PADBUTTON_ANALOGUE_0_DOWN   GUI_DOWN, dbg
        { -1, 43, -1, 28 },   // 18 E_PADBUTTON_ANALOGUE_0_LEFT   GUI_LEFT, dbg
        { -1, 44, -1, 29 },   // 19 E_PADBUTTON_ANALOGUE_0_RIGHT  GUI_RIGHT, dbg
        { -1, -1, -1, 30 },   // 20 E_PADBUTTON_ANALOGUE_1_UP     dbg only                  (right stick)
        { -1, -1, -1, 31 },   // 21 E_PADBUTTON_ANALOGUE_1_DOWN   dbg only
        { -1, -1, -1, 32 },   // 22 E_PADBUTTON_ANALOGUE_1_LEFT   dbg only
        { -1, -1, -1, 33 },   // 23 E_PADBUTTON_ANALOGUE_1_RIGHT  dbg only
        {  0, 41, -1, 30 },   // 24 E_WHEELBUTTON_ACCELERATOR     wheel only (pad writes 0)
        {  1, 42, -1, 31 },   // 25 E_WHEELBUTTON_BRAKE           wheel only
        { -1, 43, -1, 32 },   // 26 E_WHEELBUTTON_LEFT_PADDLE     wheel only
        { -1, 44, -1, 33 },   // 27 E_WHEELBUTTON_RIGHT_PADDLE    wheel only
    };

    // CgsInput::EPadButton (CgsInputDevicePS3Pad.h:40) -- names for the [input-map] dump only.
    const char* const KAPC_CONTROL_NAMES[KU_NUM_PAD_CONTROLS] =
    {
        "E_PADBUTTON_UP", "E_PADBUTTON_DOWN", "E_PADBUTTON_LEFT", "E_PADBUTTON_RIGHT",
        "E_PADBUTTON_START", "E_PADBUTTON_SELECT", "E_PADBUTTON_LTHUMB", "E_PADBUTTON_RTHUMB",
        "E_PADBUTTON_CROSS", "E_PADBUTTON_CIRCLE", "E_PADBUTTON_SQUARE", "E_PADBUTTON_TRIANGLE",
        "E_PADBUTTON_L1", "E_PADBUTTON_R1", "E_PADBUTTON_L2", "E_PADBUTTON_R2",
        "E_PADBUTTON_ANALOGUE_0_UP", "E_PADBUTTON_ANALOGUE_0_DOWN",
        "E_PADBUTTON_ANALOGUE_0_LEFT", "E_PADBUTTON_ANALOGUE_0_RIGHT",
        "E_PADBUTTON_ANALOGUE_1_UP", "E_PADBUTTON_ANALOGUE_1_DOWN",
        "E_PADBUTTON_ANALOGUE_1_LEFT", "E_PADBUTTON_ANALOGUE_1_RIGHT",
        "E_WHEELBUTTON_ACCELERATOR", "E_WHEELBUTTON_BRAKE",
        "E_WHEELBUTTON_LEFT_PADDLE", "E_WHEELBUTTON_RIGHT_PADDLE",
    };

    // ------------------------------------------------------------------------------------
    // The 28 raw control floats a pad device publishes, filled the way
    // CgsInput::DeviceX360Pad::Update @0x828E7AB0 fills them on its NON-WHEEL arm
    // (`meType != 2`), because that arm is the one an ordinary XInput pad takes:
    //   controls 0..13   the 14 wButtons bits, 1.0 / 0.0            (stores this+76 .. this+128)
    //   control 14/15    bLeftTrigger / bRightTrigger, normalised   (this+132 / this+136)
    //   controls 16..19  the LEFT stick split into four one-sided magnitudes; the sign test
    //                    is on the raw thumb word and the deadzone curve is applied to the
    //                    magnitude, exactly as the two `if (thumb <= 0)` arms do
    //                    (this+140/144 from sThumbLY, this+148/152 from sThumbLX)
    //   controls 20..23  the RIGHT stick, same split (this+156/160 from RY, +164/168 from RX)
    //   controls 24..27  0.0 -- wheel-only, written only by the `meType == 2` arm
    // ⚠️ The stick DIRECTIONS are controls, not axes: they are what makes the left stick a
    // menu navigator (mapping rows 16..19 -> GUI_UP/DOWN/LEFT/RIGHT). The signed axis value
    // the same stick also produces goes to the record's leading floats, not through here.
    // ------------------------------------------------------------------------------------
    void FillPadRawControls(const XInputGamepad& lrGamepad, f32 lafRawControls[KU_NUM_PAD_CONTROLS])
    {
        for (u32 luControl = 0; luControl < KU_NUM_PAD_CONTROLS; ++luControl)
            lafRawControls[luControl] = 0.0f;

        // The wButtons bit -> EPadButton control index order of DeviceX360Pad::Update's
        // fourteen store pairs (0x1 -> +76 == control 0 ... 0x200 -> +128 == control 13).
        static const unsigned short KAU_BUTTON_MASKS[14] =
        {
            KU_XPAD_DPAD_UP, KU_XPAD_DPAD_DOWN, KU_XPAD_DPAD_LEFT, KU_XPAD_DPAD_RIGHT,
            KU_XPAD_START,   KU_XPAD_BACK,      KU_XPAD_LTHUMB,    KU_XPAD_RTHUMB,
            KU_XPAD_A,       KU_XPAD_B,         KU_XPAD_X,         KU_XPAD_Y,
            KU_XPAD_LSHOULDER, KU_XPAD_RSHOULDER
        };
        for (u32 luButton = 0; luButton < 14; ++luButton)
        {
            if ((lrGamepad.wButtons & KAU_BUTTON_MASKS[luButton]) != 0)
                lafRawControls[luButton] = 1.0f;
        }

        lafRawControls[14] = NormaliseTrigger(lrGamepad.bLeftTrigger);   // E_PADBUTTON_L2
        lafRawControls[15] = NormaliseTrigger(lrGamepad.bRightTrigger);  // E_PADBUTTON_R2

        // One-sided stick magnitudes. NormaliseThumb already carries the sign-preserving
        // deadzone curve, so the magnitude is |curve(thumb)| on the half the stick is on.
        const f32 lfLeftY  = NormaliseThumb(lrGamepad.sThumbLY);
        const f32 lfLeftX  = NormaliseThumb(lrGamepad.sThumbLX);
        const f32 lfRightY = NormaliseThumb(lrGamepad.sThumbRY);
        const f32 lfRightX = NormaliseThumb(lrGamepad.sThumbRX);
        if (lrGamepad.sThumbLY > 0) lafRawControls[16] =  lfLeftY;  else lafRawControls[17] = -lfLeftY;
        if (lrGamepad.sThumbLX > 0) lafRawControls[19] =  lfLeftX;  else lafRawControls[18] = -lfLeftX;
        if (lrGamepad.sThumbRY > 0) lafRawControls[20] =  lfRightY; else lafRawControls[21] = -lfRightY;
        if (lrGamepad.sThumbRX > 0) lafRawControls[23] =  lfRightX; else lafRawControls[22] = -lfRightX;
    }

    // A KEYBOARD row: the host keys that stand in for one action. The pad column that used to
    // live here is gone -- the pad comes from KA_DEFAULT_GAME_INPUT_MAPPING above, so there is
    // exactly ONE place a pad binding can come from.
    // ⓘ The harness channel (ConsumeHarnessAction) still looks a row up BY ACTION ID in this
    // table, so every id it can deliver must appear here; that is unchanged.
    struct PcActionBinding
    {
        s32        iActionId;    // EGameInputActions slot in maActionInfo[]
        const int* paiVKeys;     // virtual keys mapped to this action (0-terminated)
    };

    // ---- menu rows ----
    // 49 GUI_SELECT (accept) and 50 GUI_CANCEL (back). The names below are the CHANNEL names
    // the harness uses, deliberately kept stable across the 45/49 -> 49/50 repair: "Accept"
    // is still the accept control, it just carries the right action id now.
    const int KAI_KEYS_ACCEPT[] = { 0x0D /*VK_RETURN*/, 0x20 /*VK_SPACE*/, 0 };
    const int KAI_KEYS_STOP[]   = { 0x1B /*VK_ESCAPE*/, 0x08 /*VK_BACK*/, 0 };
    // 41 GUI_UP == HighlightPrevious, 42 GUI_DOWN == HighlightNext -- settled against every
    // real consumer (see GameInputActions.h). ⚠️ THESE TWO WERE INVERTED: Down/Right used to
    // emit 41 and Up/Left 42, so pressing Down moved the highlight UP on every screen with
    // more than two rows. It went unnoticed because the only boot-chain consumer was
    // BootLegal's two-row WRAPPING title menu, where up and down are the same move.
    const int KAI_KEYS_PREV[]   = { 0x26 /*VK_UP*/,   0x25 /*VK_LEFT*/,  0 };
    const int KAI_KEYS_NEXT[]   = { 0x28 /*VK_DOWN*/, 0x27 /*VK_RIGHT*/, 0 };
    // 37..40 GUI_DPAD_* -- the map's road-rule filter panel family (CrashNavPanel
    // @0x824408E0 reads ONLY these four) and PauseScreen's 38 "TO_COLOUR" hand-off.
    // FLAG PC binding choice: the numpad cluster, so the whole family is reachable from a
    // keyboard without stealing the arrow keys from 41/42.
    // FLAG PC binding choice: the numpad cluster mirrors the dpad shape, but
    // GetAsyncKeyState(VK_NUMPADx) reads 0 with NumLock off and laptop keyboards have no
    // numpad at all -- so each row also carries a numpad-independent letter alias (I/K/J/L,
    // the right-hand inverse-T). Collision-checked against every other bound key; the
    // one-press-one-GUI-id rule holds.
    const int KAI_KEYS_DPAD_UP[]    = { 0x68 /*VK_NUMPAD8*/, 'I', 0 };
    const int KAI_KEYS_DPAD_DOWN[]  = { 0x62 /*VK_NUMPAD2*/, 'K', 0 };
    const int KAI_KEYS_DPAD_LEFT[]  = { 0x64 /*VK_NUMPAD4*/, 'J', 0 };
    const int KAI_KEYS_DPAD_RIGHT[] = { 0x66 /*VK_NUMPAD6*/, 'L', 0 };
    // 43/44 GUI_LEFT/GUI_RIGHT -- legend prev/next on the map, option decrement/increment on
    // Options/Trax/ColourCalibrate, carousel step on CarSelectVehicle.
    // FLAG PC binding choice: ',' and '.' -- see the one-press-one-id note in the banner.
    const int KAI_KEYS_GUI_LEFT[]  = { 0xBC /*VK_OEM_COMMA*/,  0 };
    const int KAI_KEYS_GUI_RIGHT[] = { 0xBE /*VK_OEM_PERIOD*/, 0 };
    // 56/57 GUI_L/RTRIGGER -- map zoom and inspect-hovered-event.
    // FLAG PC binding choice: KEYBOARD ONLY. The pad triggers are actions 0/1 (accelerate /
    // brake) and giving them 56/57 as well would emit two action ids from one pull.
    const int KAI_KEYS_GUI_LTRIGGER[] = { 0x21 /*VK_PRIOR  (PageUp)*/,   0 };
    const int KAI_KEYS_GUI_RTRIGGER[] = { 0x22 /*VK_NEXT   (PageDown)*/, 0 };
    // 58 GUI_EVENT_DETAILS -- InGame::OpenEventMap() -> "MAP_EVENT". FLAG PC binding choice.
    const int KAI_KEYS_EVENT_DETAILS[] = { 'N', 0 };
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
    // FLAG PC binding choice: action 46 == EGameInputActions GUI_BACK, the Back button, which
    // InGame turns into the OFFLINE pause -> main map. 'M' for map on the keyboard. On the PAD
    // it is mapping entry 5 (E_PADBUTTON_SELECT, the Back button) = { 50, 46, -1, 36 }, i.e.
    // the console's Back both cancels and opens the map -- which is where the old "one press,
    // one id" worry about stacking 50 onto Back was answered by the table itself.
    const int KAI_KEYS_PAUSE_MAP[]  = { 'M', 0 };

    // ⭐ KEYBOARD ONLY. Every pad column that used to sit in these rows is gone -- the pad is
    // KA_DEFAULT_GAME_INPUT_MAPPING now. What a key does is still a PC choice; what a BUTTON
    // does is the console's table. The keys themselves are byte-identical to what they were,
    // so nothing about the verified boot chain or the harness scripts moved.
    const PcActionBinding KA_BINDINGS[] =
    {
        // -- the GUI rows. The harness channel below looks a row up BY ACTION ID, never by
        //    index, so this block may be reordered or grown freely. -----------------------
        //  id  EGameInputActions              keyboard
        { E_GAMEINPUTACTIONS_GUI_SELECT, KAI_KEYS_ACCEPT }, // 49 accept (Enter/Space; pad A)
        { E_GAMEINPUTACTIONS_GUI_CANCEL, KAI_KEYS_STOP   }, // 50 back   (Esc/Bksp;    pad B + Back)
        { E_GAMEINPUTACTIONS_GUI_UP,     KAI_KEYS_PREV   }, // 41 HighlightPrevious
        { E_GAMEINPUTACTIONS_GUI_DOWN,   KAI_KEYS_NEXT   }, // 42 HighlightNext
        // 45 GUI_START -- the START button, NOT accept. It SHARES KAI_KEYS_START with driving
        // row 8 on purpose: on the console the one START control emits BOTH 8 (world start) and
        // 45 (GUI start) -- mapping entry 4 is literally { 8, 45, -1, -1 } -- so 'P' firing two
        // action ids here is the console control, not a double-fire bug.
        { E_GAMEINPUTACTIONS_GUI_START,  KAI_KEYS_START  }, // 45 START (P)

        // -- the dpad family (37..40). On the pad these ride the dpad ALONGSIDE 41..44, which
        //    is what mapping entries 0..3 do; the numpad/IJKL cluster is the keyboard's way in.
        { E_GAMEINPUTACTIONS_GUI_DPAD_UP,    KAI_KEYS_DPAD_UP    }, // 37
        { E_GAMEINPUTACTIONS_GUI_DPAD_DOWN,  KAI_KEYS_DPAD_DOWN  }, // 38 (PauseScreen TO_COLOUR)
        { E_GAMEINPUTACTIONS_GUI_DPAD_LEFT,  KAI_KEYS_DPAD_LEFT  }, // 39
        { E_GAMEINPUTACTIONS_GUI_DPAD_RIGHT, KAI_KEYS_DPAD_RIGHT }, // 40

        // -- horizontal nav / option adjust / legend ------------------------------------
        { E_GAMEINPUTACTIONS_GUI_LEFT,  KAI_KEYS_GUI_LEFT  }, // 43 (',';  pad dpad-left + stick)
        { E_GAMEINPUTACTIONS_GUI_RIGHT, KAI_KEYS_GUI_RIGHT }, // 44 ('.';  pad dpad-right + stick)

        // -- map zoom / event inspect / event details -----------------------------------
        { E_GAMEINPUTACTIONS_GUI_LTRIGGER,      KAI_KEYS_GUI_LTRIGGER  }, // 56 PageUp   (pad LT)
        { E_GAMEINPUTACTIONS_GUI_RTRIGGER,      KAI_KEYS_GUI_RTRIGGER  }, // 57 PageDown (pad RT)
        { E_GAMEINPUTACTIONS_GUI_EVENT_DETAILS, KAI_KEYS_EVENT_DETAILS }, // 58 'N'      (pad dpad-up)

        // -- driving --------------------------------------------------------------------
        //  id  EGameInputActions       keyboard              (the pad control, for orientation)
        {  0, KAI_KEYS_ACCELERATE }, // ACCELERATE  (Up,W)   pad R2
        {  1, KAI_KEYS_BRAKE      }, // BRAKE       (Down,S) pad L2
        {  2, KAI_KEYS_HANDBRAKE  }, // HANDBRAKE   (LCtrl)  pad X
        {  3, KAI_KEYS_BOOST      }, // BOOST       (LShift) pad A
        {  5, KAI_KEYS_CHANGEVIEW }, // CHANGEVIEW  (C)      pad Y
        {  7, KAI_KEYS_RESET      }, // RESET       (R)      pad R1  -- restored by the console table
        {  8, KAI_KEYS_START      }, // START       (P)      pad START
        { 13, KAI_KEYS_HORN       }, // HORN        (H)      pad L3
        { 54, KAI_KEYS_SPIN_LEFT  }, // GUI_LSHOULDER -> -mfSpin (Q) pad L1
        { 55, KAI_KEYS_SPIN_RIGHT }, // GUI_RSHOULDER -> +mfSpin (E) pad R1

        // -- the offline pause / open-the-map (pause wave, 2026-08-26) -------------------
        // Action 46 GUI_BACK was ABSENT FROM THIS TABLE ENTIRELY, which is why the offline
        // pause could not be reached from a PC keyboard at all: InGame::HandleControllerInput
        // has had `case E_GAMEINPUTACTIONS_GUI_BACK: PauseGame(true,false)` all along
        // (BrnInGame.cpp) and nothing could ever deliver a 46. On the pad it is mapping entry
        // 5 (the Back button), which also carries 50 GUI_CANCEL.
        { E_GAMEINPUTACTIONS_GUI_BACK, KAI_KEYS_PAUSE_MAP }, // 46 (M)
    };
    const u32 KU_NUM_BINDINGS = sizeof(KA_BINDINGS) / sizeof(KA_BINDINGS[0]);

    // ---- [input-map] the build says what it binds -----------------------------------------
    // DIAG. NOT IN THE X360 BINARY. Opt-in (BRN_INPUT_MAP_DUMP=1), printed ONCE at the first
    // input update, 28 + N lines and never again -- there is no per-frame budget here at all.
    // ⭐ WHY IT EXISTS: "the pad is missing controls" was un-measurable for as long as the
    // console's table was believed unreadable. It is readable, and this makes the two
    // comparable without a screenshot or a human with a controller:
    // tools/tests/offline/input_mapping_coverage.py re-reads the same 112 bytes out of the
    // image and diffs them against these lines (case tools/tests/cases/input_pad_coverage.ps1).
    // A future edit that drops a binding is then a red test, not a comment nobody re-checks.
    void DumpInputMapOnce()
    {
        static bool sbDumped = false;
        if (sbDumped)
            return;
        static const bool s_bWanted = (std::getenv("BRN_INPUT_MAP_DUMP") != 0);
        if (!s_bWanted)
        {
            sbDumped = true;      // never asked for; stop looking
            return;
        }
        // ⚠️ DO NOT latch before the log stream exists. The first input update can run before
        // CgsDev::Log::gpDebugPrint is bound, and latching there would make the witness
        // silently never appear -- which reads in a test exactly like a missing binding.
        if (CgsDev::Log::gpDebugPrint == 0)
            return;
        sbDumped = true;

        *CgsDev::Log::gpDebugPrint
            << "[input-map] pad = gaDefaultGameInputMapping @0x82CDBEB8 (X360), "
            << static_cast<s32>(KU_NUM_PAD_CONTROLS) << " controls x "
            << static_cast<s32>(KU_MAPPING_SLOTS) << " action slots; -1 == unbound\n";
        for (u32 luControl = 0; luControl < KU_NUM_PAD_CONTROLS; ++luControl)
        {
            *CgsDev::Log::gpDebugPrint
                << "[input-map] control " << static_cast<s32>(luControl) << " "
                << KAPC_CONTROL_NAMES[luControl] << " -> actions "
                << static_cast<s32>(KA_DEFAULT_GAME_INPUT_MAPPING[luControl][0]) << ","
                << static_cast<s32>(KA_DEFAULT_GAME_INPUT_MAPPING[luControl][1]) << ","
                << static_cast<s32>(KA_DEFAULT_GAME_INPUT_MAPPING[luControl][2]) << ","
                << static_cast<s32>(KA_DEFAULT_GAME_INPUT_MAPPING[luControl][3]) << "\n";
        }
        for (u32 luBind = 0; luBind < KU_NUM_BINDINGS; ++luBind)
        {
            *CgsDev::Log::gpDebugPrint
                << "[input-map] keyboard action " << KA_BINDINGS[luBind].iActionId
                << " <- vk (decimal)";
            // ⚠️ DECIMAL, and the label says so. This stream has no hex manipulator, so a
            // literal "0x" prefix in front of it would print `0x13` for VK_RETURN (13) --
            // a number that is wrong in both bases. (The pre-existing [input-src] line
            // below has exactly that wart on its padButtons word; left alone here because
            // it is not this lane's line, but do not copy the pattern.)
            for (const int* lpiKey = KA_BINDINGS[luBind].paiVKeys; *lpiKey != 0; ++lpiKey)
                *CgsDev::Log::gpDebugPrint << " " << *lpiKey;
            *CgsDev::Log::gpDebugPrint << "\n";
        }
    }

    // ---- keyboard overlay for the two left-stick axes ----------------------------------
    // The console accumulates every device bound to a port into one axis value
    // (InputPads::FillRawData @0x828E7350 SUMS the axes then clamps to [-1,+1]), so the
    // keyboard is summed onto the pad's stick exactly the same way -- a second device, not
    // an override. Full deflection is 1.0 because a key has no travel.
    const int KAI_KEYS_STEER_LEFT[]  = { 0x25 /*VK_LEFT*/,  'A', 0 };
    const int KAI_KEYS_STEER_RIGHT[] = { 0x27 /*VK_RIGHT*/, 'D', 0 };

    // ⛔⛔⛔ AN UNATTENDED RUN MUST NOT READ THE PHYSICAL KEYBOARD (2026-09-03, harness wave).
    //   GetAsyncKeyState is GLOBAL key state. The only thing standing between it and this game's
    //   controls is the focus gate (IsGameWindowForeground) -- and an unattended harness run
    //   ALWAYS has the game foreground, for minutes, while the box is being used for everything
    //   else. So any 'M' or
    //   'P' typed anywhere on the machine lands on action 46 GUI_BACK / 45 GUI_START, which are
    //   InGame::PauseGame(true,false) / (true,true): the map or the driver-details screen opens,
    //   GuiEventActivateCrashNav(false) becomes game event 93 -> RequestPause(4) -> mbSimPaused,
    //   AND THE WORLD FREEZES for the rest of the run.
    //   ⭐ THIS IS MEASURED, not theorised, and the measurement is the [input-src] line below it:
    //       [input-src] action 46 PRESSED -- key 1 padbtn 0 harness 0 (foreground 1 xpad 0 ...)
    //   Three such presses in scratch/flow_run/hw1 and three in scratch/flow_run/tcrash1, with no
    //   controller attached and the harness never touching either channel. In
    //   scratch/flow_run/stA_right the same thing fired EARLY and voided the entire run: 145
    //   BIT-IDENTICAL [motion] samples that three separate waves read as a physics or teleport
    //   failure, and that one wave turned into a published "harness steering does essentially
    //   nothing" conclusion. A stimulus that silently did not happen is worse than no stimulus.
    //   ⭐ THE RULE: when BRN_INPUT_ALLOW_BACKGROUND is set, this process is being driven by the
    //   named-event channel and by nothing else, so the host keyboard is not an input device --
    //   it is noise from whatever else the box is doing. Suppressed, announced ONCE (a silent
    //   behaviour change is the thing this whole file's banners exist to prevent), and escapable
    //   with BRN_INPUT_KEEP_KEYBOARD=1 for anyone who wants to type at a harness-launched build.
    //   ⭐⭐ THIS IS NO LONGER THE ONLY THING BETWEEN A HARNESS RUN AND THE USER'S KEYBOARD, and
    //   it never should have been (2026-09-06, lane quiet). It is a blanket refusal keyed on
    //   "am I a harness run", which is a different question from "did the person mean to type at
    //   this window" -- so BRN_INPUT_KEEP_KEYBOARD used to hand back a keyboard that was read
    //   SYSTEM-WIDE, and every non-harness build was gated on the process rather than the window.
    //   IsGameWindowForeground() above is now the primary rule and applies to every build and
    //   every run; this remains as the stricter harness default on top of it, so KEEP_KEYBOARD
    //   now means "read the keyboard, while the game's window has the focus" rather than
    //   "read the whole machine's keyboard".
    //   ⚠️ IT DOES NOT TOUCH THE ASSERT-DISMISS FALLBACK. flow_run.ps1's [KBFLOW]::Tap(0x23) is
    //   read by CgsAssertManager.cpp:316/323 with its own GetAsyncKeyState(VK_END), not through
    //   KA_BINDINGS, and VK_END is in no KAI_KEYS_* row. Checked, not assumed.
    //   ⚠️ AND IT DOES NOT TOUCH THE PAD. XInput is a real device someone deliberately attached;
    //   only the global-key-state read is noise. The [input-src] line still names both sources.
    bool HostKeyboardSuppressed()
    {
        // FLAG PC-platform leaf: menu/console navigation owns the shared host keyboard.
        // The controller keeps ownership through the closing key's release.
        if (CgsDev::IsDebugKeyboardCapturedPC())
            return true;
        static s32 siSuppressed = -1;
        if (siSuppressed < 0)
        {
            const bool lbHarness = (std::getenv("BRN_INPUT_ALLOW_BACKGROUND") != nullptr);
            const bool lbKeep    = (std::getenv("BRN_INPUT_KEEP_KEYBOARD") != nullptr);
            siSuppressed = (lbHarness && !lbKeep) ? 1 : 0;
            if (siSuppressed == 1)
            {
                CgsDev::Log::WriteToLog(
                    "[input] HOST KEYBOARD SUPPRESSED -- BRN_INPUT_ALLOW_BACKGROUND is set, so this "
                    "run takes input ONLY from the harness's named-event channel (and an XInput pad "
                    "if one is attached). A stray 'M'/'P' on this box can no longer open the map or "
                    "driver details and freeze the world mid-measurement. BRN_INPUT_KEEP_KEYBOARD=1 "
                    "restores the old behaviour.\n");
            }
        }
        return siSuppressed == 1;
    }

    bool AnyKeyDown(const int* lpiKeys)
    {
        if (HostKeyboardSuppressed())
            return false;
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
        // ⭐ THE CHANNEL MEANINGS ARE STABLE ACROSS THE 2026-08-29 VOCABULARY REPAIR; only the
        //    action ids they carry were corrected, so every existing harness script
        //    (tools/diagnostics/flow_run.ps1, boot_test.ps1) keeps working unchanged:
        //      Accept   = "the accept control"  -- was 45 GUI_START, is now 49 GUI_SELECT
        //      Stop     = "the back control"    -- was 49 GUI_SELECT, is now 50 GUI_CANCEL
        //      Next     = "highlight next"      -- was 41, is now 42 GUI_DOWN (the pair was
        //                                          inverted; the CHANNEL always meant "next")
        //      Prev     = "highlight previous"  -- was 42, is now 41 GUI_UP
        //      PauseMap = "open the main map"   -- 46 GUI_BACK, unchanged
        //      Start    = "the START button"    -- NEW: 45 GUI_START, which is no longer the
        //                                          accept control and so needs its own channel
        //                                          (BootLegal's press-start path reads 45).
        //    All six are AUTO-RESET taps, like a menu press.
        case E_GAMEINPUTACTIONS_GUI_SELECT: lpcEventName = "Local\\BurnoutPC_Input_Accept";   break;
        case E_GAMEINPUTACTIONS_GUI_CANCEL: lpcEventName = "Local\\BurnoutPC_Input_Stop";     break;
        case E_GAMEINPUTACTIONS_GUI_DOWN:   lpcEventName = "Local\\BurnoutPC_Input_Next";     break;
        case E_GAMEINPUTACTIONS_GUI_UP:     lpcEventName = "Local\\BurnoutPC_Input_Prev";     break;
        // PC harness: option changes use the same actions as comma/period and pad left/right.
        case E_GAMEINPUTACTIONS_GUI_LEFT:  lpcEventName = "Local\\BurnoutPC_Input_OptionPrev"; break;
        case E_GAMEINPUTACTIONS_GUI_RIGHT: lpcEventName = "Local\\BurnoutPC_Input_OptionNext"; break;
        case E_GAMEINPUTACTIONS_GUI_START:  lpcEventName = "Local\\BurnoutPC_Input_Start";    break;
        // -- driving. These three ids are the rows BridgeControllerToWorld reads straight out
        //    of maActionInfo[] into PlayerVehicleControls (asm-attested: [0].mfValue ->
        //    mfAcceleration, [1] -> mfBraking, [2] -> mfHandBrake), i.e. exactly the slots the
        //    right trigger / left trigger / X button fill.
        case  0: lpcEventName = "Local\\BurnoutPC_Input_Accelerate"; break;
        case  1: lpcEventName = "Local\\BurnoutPC_Input_Brake";      break;
        case  2: lpcEventName = "Local\\BurnoutPC_Input_HandBrake";  break;
        // ⭐ -- BOOST (showtime terminator wave, 2026-08-29). Same shape as the shoulder rows
        //    below: NO NEW KA_BINDINGS ROW IS ADDED AND NONE IS NEEDED -- action 3 has been in
        //    that table all along ({3, KAI_KEYS_BOOST, KU_XPAD_A} above), and this lookup is BY
        //    ACTION ID, so the existing row is found unchanged. The game sees an ordinary pad
        //    holding A / LShift.
        //    ⭐⭐ WHY IT IS WORTH A CHANNEL: BridgeControllerToWorld reads this action's
        //    muStatus bit0 into PlayerVehicleControls::mbBoost (see the table at :218), and
        //    that is the ONLY thing that spends boost during showtime -- the console's showtime
        //    bar is drained by BOUNCING and by nothing else (CrashPlayManager::OnBounce
        //    @0x822A7EF8 subtracts per bounce; nothing drains it on a timer). Since
        //    CrashModeScoring::HasCrashModeEnded's idle ladder will not fire until the boost
        //    percentage settles to ~0, a showtime run that never presses this button cannot end
        //    -- ON THE CONSOLE EITHER. Every showtime measurement taken before this channel
        //    existed was therefore measuring the harness, not the game.
        //    ⚠️ MANUAL-RESET on the harness side, like the other driving rows: bouncing is a
        //    press the game samples on the frames between Set() and Reset(), and a one-frame
        //    auto-reset tap cannot express a hold. See the driving-row banner above.
        case  3: lpcEventName = "Local\\BurnoutPC_Input_Boost";      break;
        // -- the offline pause. AUTO-RESET like the other menu channels (a TAP): one press
        //    opens the map, one press of Stop (50 GUI_CANCEL) or Start (45) inside it comes
        //    back out -- the map's exit arm is 45/50, never 49.
        case E_GAMEINPUTACTIONS_GUI_BACK: lpcEventName = "Local\\BurnoutPC_Input_PauseMap"; break;
        // ⭐⭐ -- the two SHOULDER rows (showtime S7b-a wave, 2026-08-27). MANUAL-RESET, i.e. a
        //    HOLD, because the control they stand in for is a hold: BrnGameStateModuleIO.cpp:92
        //    computes ControllerInput::mbCrashModePressed (+0x42) as
        //        (row 54 HELD) && (row 55 HELD)
        //    -- both bumpers down at the same time -- and that byte is the showtime/crash-mode
        //    gesture DetectModeStarts' `else` arm reads. A tap channel cannot express "both, at
        //    once, for a frame the game samples", which is exactly the distinction the driving
        //    rows above already document.
        //    ⚠️ NO NEW KA_BINDINGS ROW IS ADDED, AND NONE IS NEEDED: rows 54 and 55 have been in
        //    that table all along (LSHOULDER / RSHOULDER, plus their keyboard keys). All that was
        //    missing was a harness channel for them, and the lookup below is BY ACTION ID, so the
        //    existing rows are found unchanged. The game sees an ordinary pad holding both
        //    bumpers; nothing here writes a game-state flag.
        case 54: lpcEventName = "Local\\BurnoutPC_Input_ShoulderL";  break;
        case 55: lpcEventName = "Local\\BurnoutPC_Input_ShoulderR";  break;
        default: return false;
        }

        // SYNCHRONIZE == 0x00100000; WAIT_OBJECT_0 == 0. The harness creates every event
        // before launching the game, so a successful zero-time wait says the control is down
        // this update (and, for the auto-reset menu channels, consumes the one request).
        // The handle cache is indexed by KA_BINDINGS row, so every id in the switch above must
        // also be a bound row -- 49/50/41/42/45/46 and 0/1/2 and 54/55 all are. The lookup
        // below is BY ACTION ID, not by a fixed index, so growing or reordering the table
        // grows the cache without disturbing any existing row.
        static void* sapHarnessEvents[KU_NUM_BINDINGS] = {};
        u32 luBinding = 0;
        while (luBinding < KU_NUM_BINDINGS
               && KA_BINDINGS[luBinding].iActionId != liActionId)
            ++luBinding;
        if (luBinding >= KU_NUM_BINDINGS)
            return false;
        if (sapHarnessEvents[luBinding] == 0)
        {
            // ⭐ THE SLOT SUFFIX (CgsHarnessSlot.h). These channels are session-global named
            // events, so with several game instances on one box a single Set() would press the
            // control in EVERY one of them. BRN_HARNESS_SLOT unset or 0 yields the empty
            // suffix, i.e. the names every existing harness script already opens.
            char lacEventName[64];
            sapHarnessEvents[luBinding] = OpenEventA(0x00100000u, 0,
                CgsSystem::HarnessSlot::Name(lacEventName, sizeof(lacEventName), lpcEventName));
        }
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
            // Slot-suffixed like the action channels above (CgsHarnessSlot.h).
            char lacEventName[64];
            sapSteerEvents[luIndex] = OpenEventA(0x00100000u, 0,
                    CgsSystem::HarnessSlot::Name(lacEventName, sizeof(lacEventName),
                            lbRight ? "Local\\BurnoutPC_Input_SteerRight"
                                    : "Local\\BurnoutPC_Input_SteerLeft"));
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
        DumpInputMapOnce();   // opt-in [input-map] witness; see its banner

        // Host device reads, through the TWO gates (see their banner at IsGameWindowForeground):
        //   lbForeground  -- the game's own window has the focus. The ONLY gate the keyboard is
        //                    allowed to use, in every build and every run.
        //   lbPadAllowed  -- the XInput read; a pad is a device, not a window, so a harness run
        //                    keeps it in the background.
        const bool lbForeground  = IsGameWindowForeground();
        const bool lbPadAllowed  = IsPadReadAllowed();
        XInputState lXState;
        std::memset(&lXState, 0, sizeof(lXState));
        bool lbXPad = false;
        if (lbPadAllowed)
        {
            if (XInputGetStateFn lpfGetState = ResolveXInputGetState())
                lbXPad = (lpfGetState(0, &lXState) == 0);   // ERROR_SUCCESS
        }

        // ---- [input] focus witness -------------------------------------------------------
        // DIAG. NOT IN THE X360 BINARY. One line per CHANGE of the focus gate, capped, naming
        // how many BOUND keys were physically down at the instant it changed. It is the only way
        // to say, from a run's own log, "the window lost the focus here and the N keys that were
        // down stopped counting" -- the measurement the lane brief asks for, and the thing that
        // separates "the gate held" from "nobody typed". A count only: no key identity is logged.
        // Bounded at 64 changes so a user alt-tabbing for an hour cannot flood the log.
        {
            static s32 siLastFocus   = -1;
            static u32 suFocusPrints = 0;
            // ⚠️ HOISTED OUT OF THE STREAM EXPRESSION BELOW, deliberately. HostKeyboardSuppressed()
            // emits its own one-shot WriteToLog on its FIRST call, and calling it mid-`<<` chain
            // spliced that whole paragraph into the middle of this line -- a witness a check
            // cannot parse. Evaluate it first; the one-shot then lands on its own line.
            const bool lbKbdSuppressed = HostKeyboardSuppressed();
            const s32 liFocus = lbForeground ? 1 : 0;
            if (liFocus != siLastFocus && suFocusPrints < 64u && CgsDev::Log::gpDebugPrint != 0)
            {
                ++suFocusPrints;
                s32 liKeysDown = 0;
                for (u32 luBind = 0; luBind < KU_NUM_BINDINGS; ++luBind)
                    for (const int* lpiKey = KA_BINDINGS[luBind].paiVKeys; *lpiKey != 0; ++lpiKey)
                        if ((GetAsyncKeyState(*lpiKey) & 0x8000) != 0)
                            ++liKeysDown;
                *CgsDev::Log::gpDebugPrint
                    << "[input] focus=" << liFocus
                    << " kbd=" << liKeysDown
                    << " (bound keys physically down; consumed only when focus=1)"
                    << " padgate=" << (lbPadAllowed ? 1 : 0)
                    << " kbdsuppressed=" << (lbKbdSuppressed ? 1 : 0) << "\n";
            }
            siLastFocus = liFocus;
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
        // Per action: the value is the MAX over every host control mapped to it (a key or pad
        // button has no travel, so it is full scale; a trigger and a stick direction carry
        // their normalised curve), and the status is the console muStatus contract (bit0 held,
        // bit1 pressed-this-frame, bit2 released-this-frame) with "held" being InputPads::
        // Update's own `mfValue > 0.1` ACTION test (0x820F78E4), not the device's 0.2.
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
        //
        // ⭐⭐ THE ACCUMULATION IS PER ACTION, NOT PER BINDING ROW -- that is the shape change.
        // It used to walk KA_BINDINGS and write one action per row, which forced each action to
        // have at most one host control and kept the edge state (sabWasDown) per ROW. The
        // console has neither restriction: InputPads::Update @0x828F8690 MAXes every mapped
        // control into maActionInfo[id].mfValue and only THEN, in a second pass over the
        // actions, derives muStatus from the accumulated value against a per-ACTION previous
        // state. Reproduced here: three devices contribute into one value array, then one
        // status pass. That is what lets the dpad feed 37 AND 41 AND 58 at once.
        f32 lafActionValue[E_GAMEINPUTACTIONS_COUNT];
        for (u32 luAction = 0; luAction < E_GAMEINPUTACTIONS_COUNT; ++luAction)
            lafActionValue[luAction] = 0.0f;

        // Which source raised each action, for the [input-src] diagnostic below only.
        bool labFromKey[E_GAMEINPUTACTIONS_COUNT]     = {};
        bool labFromHarness[E_GAMEINPUTACTIONS_COUNT] = {};
        bool labFromPad[E_GAMEINPUTACTIONS_COUNT]     = {};

        // ---- device 1+2: the host keyboard and the harness's named-event channel ----------
        // ⓘ Order and short-circuiting are UNCHANGED from the per-row loop this replaces:
        // ConsumeHarnessAction is still not called when a key for the same action is already
        // down, which matters because the menu channels are AUTO-RESET and a second call would
        // consume the request a second time. The pad is read after, and never gated a consume.
        for (u32 luBind = 0; luBind < KU_NUM_BINDINGS; ++luBind)
        {
            const PcActionBinding& lrBinding = KA_BINDINGS[luBind];
            const s32 liAction = lrBinding.iActionId;

            const bool lbFromKey = lbForeground && AnyKeyDown(lrBinding.paiVKeys);
            const bool lbFromHarness = !lbFromKey && ConsumeHarnessAction(liAction);
            if (lbFromKey || lbFromHarness)
            {
                if (lafActionValue[liAction] < 1.0f)
                    lafActionValue[liAction] = 1.0f;      // a key has no travel
                labFromKey[liAction]     = labFromKey[liAction]     || lbFromKey;
                labFromHarness[liAction] = labFromHarness[liAction] || lbFromHarness;
            }
        }

        // ---- device 3: the XInput pad, through the CONSOLE's own mapping table -------------
        // The two nested loops ARE InputPads::Update's mapping walk (@0x828F88C8..0x828F898C):
        // for each raw control, for each of its four action slots, MAX the control's value into
        // that action. -1 slots are skipped exactly as the `cmpwi r10, -1 / beq` arms do.
        if (lbXPad)
        {
            f32 lafRawControls[KU_NUM_PAD_CONTROLS];
            FillPadRawControls(lXState.Gamepad, lafRawControls);
            for (u32 luControl = 0; luControl < KU_NUM_PAD_CONTROLS; ++luControl)
            {
                const f32 lfRaw = lafRawControls[luControl];
                for (u32 luSlot = 0; luSlot < KU_MAPPING_SLOTS; ++luSlot)
                {
                    const s32 liAction = KA_DEFAULT_GAME_INPUT_MAPPING[luControl][luSlot];
                    // FLAG PC-platform leaf: the `< 0` half is the console's own `cmpwi -1`
                    // skip; the upper half is a transcription guard, not console behaviour --
                    // the console indexes maActionInfo[] unchecked, and the largest id in the
                    // table is 60 (== E_GAMEINPUTACTIONS_SET_PLAYERSTATS_MAX, in range), so it
                    // can only fire if someone mistypes a row. DELETE-WHEN the table is
                    // generated from the image rather than transcribed.
                    if (liAction < 0 || liAction >= E_GAMEINPUTACTIONS_COUNT)
                        continue;
                    if (lfRaw > lafActionValue[liAction])
                        lafActionValue[liAction] = lfRaw;
                    if (lfRaw > KF_ACTION_DOWN_THRESHOLD)
                        labFromPad[liAction] = true;
                }
            }
        }

        // ---- the status pass (InputPads::Update @0x828F89B4, per ACTION) -------------------
        static bool sabActionWasDown[E_GAMEINPUTACTIONS_COUNT] = {};
        for (u32 luAction = 0; luAction < E_GAMEINPUTACTIONS_COUNT; ++luAction)
        {
            const f32  lfValue = lafActionValue[luAction];
            const bool lbDown  = (lfValue > KF_ACTION_DOWN_THRESHOLD);

            // ---- [input-src] WHO PRESSED THE BUTTON THAT PAUSED THE WORLD ------------------
            // DIAG. NOT IN THE X360 BINARY. Always on for the two ids that stop the simulation --
            // 45 GUI_START (-> InGame::PauseGame(true,true) -> CrashNavDriverDetails) and 46
            // GUI_BACK (-> PauseGame(true,false) -> CrashNavMapMain). Both post
            // GuiEventActivateCrashNav(false), which becomes game event 93 -> RequestPause(4) ->
            // mbSimPaused, and a paused world FREEZES THE CAR.
            // ⭐ WHY IT IS WORTH A PERMANENT LINE. Measured 2026-09-03: unattended harness runs
            // open one of those screens BY THEMSELVES -- once in scratch/flow_run/stA_right (early,
            // which voided the whole run: 145 bit-identical [motion] samples that three waves read
            // as a physics/teleport failure) and three times in scratch/flow_run/hw1 (late, after
            // the car had already driven 403 m). The harness never touched either channel in
            // either run. This line is the discriminator the logs did not have: a press has exactly
            // three possible sources here and it names which one it was.
            // These two ids fire at most a handful of times in a run, so there is no budget and no
            // env gate -- an opt-in diagnostic is exactly what was missing when those runs were read.
            // DELETE-WHEN the spontaneous press is explained.
            if (lbDown && !sabActionWasDown[luAction]
                && (luAction == static_cast<u32>(E_GAMEINPUTACTIONS_GUI_START)
                    || luAction == static_cast<u32>(E_GAMEINPUTACTIONS_GUI_BACK))
                && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[input-src] action " << static_cast<s32>(luAction)
                    << " PRESSED -- key " << (labFromKey[luAction] ? 1 : 0)
                    << " padbtn " << (labFromPad[luAction] ? 1 : 0)
                    << " harness " << (labFromHarness[luAction] ? 1 : 0)
                    << " (foreground " << (lbForeground ? 1 : 0)
                    << " padgate " << (lbPadAllowed ? 1 : 0)
                    << " xpad " << (lbXPad ? 1 : 0)
                    << " padButtons 0x" << static_cast<s32>(lXState.Gamepad.wButtons)
                    << ") -- this press PAUSES THE SIMULATION\n";
            }
            // ---- end [input-src] -----------------------------------------------------------

            u32 luStatus = 0;
            if (lbDown)
                luStatus |= 1u;                                  // held
            if (lbDown && !sabActionWasDown[luAction])
                luStatus |= 2u;                                  // pressed edge
            if (!lbDown && sabActionWasDown[luAction])
                luStatus |= 4u;                                  // released edge
            sabActionWasDown[luAction] = lbDown;

            InputIO::ActionInfo& lrAction = lrPad.maActionInfo[luAction];
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
