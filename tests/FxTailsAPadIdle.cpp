// FX-TAILS-A item 1 (crash parity 2026-09-24): the pad record's idle byte, run on the PRODUCTION
// CgsInput::InputPadsPC::UpdatePlayer0 -- the revision's whole CgsInputPadsPC.cpp is included below as
// fxtailsa_pads_pc.inc (run_fxtailsa_pad_idle.py writes it), with the system XInput DLL replaced by a
// scripted pad: LoadLibraryA / GetProcAddress are renamed onto the two stand-ins in this file BEFORE the
// include, so ResolveXInputGetState binds FakeXInputGetState. The keyboard is never read (the harness
// marker suppresses it) and the harness channels are opened under a private slot suffix, so no live game
// on this box can feel the test and no real pad can reach it.
//
// What the console computes (InputPads::Update @0x828F8690, export hole, tools/re/ppcdis.py):
//   per pad r7 = 1 (`li r26,1` @0x828F8780, `mr r7,r26` @0x828F89B8); per action the value is compared with
//   `lfs f0, 0x78E4(r17)` (0x820F78E4 = 0x3DCCCCCD = 0.1f) ; `fcmpu value, f0` ; `blt` -> not held. HELD
//   (0x828F89E8): down byte 1, r7 = 0, status |= 1, |= 2 unless the previous frame's byte was set. NOT HELD
//   (0x828F8A34): down byte 0, status &= ~1, previous byte set -> r7 = 0 and status |= 4 (released). After
//   the 112 actions `stb r7, 0x3A0(r25)` @0x828F8CB0 -- PadOutputInformation::mbPadIdle. The per-frame
//   IOBuffer is re-Constructed (OutputBuffer::Construct @0x828F85E0 zeroes the pairs), so the status word
//   holds only this frame's bits.
// So +0x3A0 is 1 exactly when no action is held (>= 0.1) and none was released this frame.
// The pad values below come from the leaf's own curves: trigger byte 45 -> 0.0955 (below 0.1, not held),
// byte 46 -> 0.10049 (held); thumb 5000 -> inside the 0.2 deadzone (0.0), thumb 9000 -> 0.1067 (held,
// through mapping row 19 -> 44 GUI_RIGHT). No pad input produces exactly 0.1f, so the `>=` edge is pinned
// structurally by the runner.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#pragma comment(lib, "user32.lib")

// ---- the scripted XInput DLL -------------------------------------------------------------------
static int gFakeModuleTag = 0;
static HMODULE FxaLoadLibraryA(LPCSTR) { return reinterpret_cast<HMODULE>(&gFakeModuleTag); }
static FARPROC FxaGetProcAddress(HMODULE, LPCSTR lpcName);
#define LoadLibraryA   FxaLoadLibraryA
#define GetProcAddress FxaGetProcAddress

// The revision's CgsInputPadsPC.cpp, verbatim.
#include "fxtailsa_pads_pc.inc"

#undef LoadLibraryA
#undef GetProcAddress

// ---- link stand-ins for what the leaf references but the test never exercises -------------------
namespace CgsDev
{
    bool IsDebugKeyboardCapturedPC() { return false; }
    namespace Assert                                  // CgsStrStream.cpp's format asserts
    {
        int   BeginAssert() { return 0; }
        int   FireAssert(const char*, const char*, int) { return 0; }
        void* EndAssert() { return nullptr; }
    }
    namespace Log
    {
        DebugPrint* gpDebugPrint = nullptr;          // every [DIAG] line stays silent
        void WriteToLog(const char*) {}
    }
}
namespace renderengine { HWND__* hWnd = nullptr; }
namespace CgsInput
{
    EBindResult InputPads::BindPlayerToPort(s32, s32) { return E_BINDRESULTOK; }
    bool DeviceX360Pad::BindToPort(u32, s32) { return true; }
}

static XInputGamepad gPad;
static unsigned long gPacket = 0;

static unsigned long __stdcall FakeXInputGetState(unsigned long luUser, XInputState* lpState)
{
    if (luUser != 0)
        return 1167;                                  // ERROR_DEVICE_NOT_CONNECTED
    lpState->dwPacketNumber = ++gPacket;
    lpState->Gamepad = gPad;
    return 0;                                         // ERROR_SUCCESS: a pad on user 0
}

static FARPROC FxaGetProcAddress(HMODULE, LPCSTR lpcName)
{
    if (std::strcmp(lpcName, "XInputGetState") == 0)
        return reinterpret_cast<FARPROC>(&FakeXInputGetState);
    return nullptr;
}

// ---- the checks ------------------------------------------------------------------------------------
static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
        ++gFailures;
    std::printf("  %s %s\n", lbPass ? "ok  " : "FAIL", lpcName);
}

alignas(16) static unsigned char gOutputStorage[sizeof(CgsInput::InputIO::OutputBuffer)];

static CgsInput::InputIO::OutputBuffer* Output()
{
    return reinterpret_cast<CgsInput::InputIO::OutputBuffer*>(gOutputStorage);
}
static const CgsInput::InputIO::PadOutputInformation& Pad() { return Output()->maPadOutputInformation[0]; }
static u32 Idle()               { return Pad().mbDisconnected; }
static u32 Status(u32 luAction) { return Pad().maActionInfo[luAction].muStatus; }
static f32 Value(u32 luAction)  { return Pad().maActionInfo[luAction].mfValue; }

// One input update with the pad in the given state.
static void Frame(unsigned short luButtons, unsigned char lucRightTrigger, short liThumbLX)
{
    std::memset(&gPad, 0, sizeof(gPad));
    gPad.wButtons      = luButtons;
    gPad.bRightTrigger = lucRightTrigger;
    gPad.sThumbLX      = liThumbLX;
    CgsInput::InputPadsPC::UpdatePlayer0(Output());
}

int main()
{
    // The leaf latches these on its first call. A private slot suffix keeps OpenEventA away from every
    // live harness's "Local\BurnoutPC_Input_*" channels (an AUTO-RESET menu tap would otherwise be
    // consumed here); BRN_INPUT_ALLOW_BACKGROUND opens the pad gate and suppresses the host keyboard.
    char lacSlot[16];
    std::snprintf(lacSlot, sizeof(lacSlot), "97%u", static_cast<unsigned>(GetCurrentProcessId() % 10000u));
    _putenv_s("BRN_HARNESS_SLOT", lacSlot);
    _putenv_s("BRN_INPUT_ALLOW_BACKGROUND", "1");
    _putenv_s("BRN_INPUT_KEEP_KEYBOARD", "");
    _putenv_s("BRN_INPUT_MAP_DUMP", "");

    const u32 KU_ACCELERATE = 0, KU_BOOST = 3, KU_GUI_RIGHT = 44, KU_GUI_SELECT = 49;
    const unsigned short KU_BUTTON_A = 0x1000;

    std::printf("pad at rest, then the right trigger (row 15 -> 0 ACCELERATE):\n");
    Frame(0, 0, 0);
    Check(Idle() == 1, "rest: +0x3A0 = 1 (no action held, none released)");
    Check(Status(KU_ACCELERATE) == 0, "rest: ACCELERATE status 0");
    Frame(0, 255, 0);
    Check(Value(KU_ACCELERATE) == 1.0f, "trigger 255: ACCELERATE value 1.0");
    Check(Status(KU_ACCELERATE) == 3u, "trigger 255: status held|pressed (3)");
    Check(Idle() == 0, "trigger 255: +0x3A0 = 0 (held)");
    Frame(0, 255, 0);
    Check(Status(KU_ACCELERATE) == 1u, "trigger held: status held (1)");
    Check(Idle() == 0, "trigger held: +0x3A0 = 0");
    Frame(0, 0, 0);
    Check(Status(KU_ACCELERATE) == 4u, "trigger released: status released (4)");
    Check(Idle() == 0, "release frame: +0x3A0 = 0 (released this frame counts as input)");
    Frame(0, 0, 0);
    Check(Status(KU_ACCELERATE) == 0, "after the release: status 0");
    Check(Idle() == 1, "after the release: +0x3A0 = 1");

    std::printf("the 0.1 action threshold (0x820F78E4) through the trigger curve:\n");
    Frame(0, 45, 0);
    Check(Value(KU_ACCELERATE) > 0.0f && Value(KU_ACCELERATE) < 0.1f, "trigger 45: value in (0, 0.1)");
    Check(Status(KU_ACCELERATE) == 0, "trigger 45: not held (status 0)");
    Check(Idle() == 1, "trigger 45: +0x3A0 = 1 (a value below 0.1 is not input)");
    Frame(0, 46, 0);
    Check(Value(KU_ACCELERATE) >= 0.1f, "trigger 46: value >= 0.1");
    Check(Status(KU_ACCELERATE) == 3u, "trigger 46: held|pressed (3)");
    Check(Idle() == 0, "trigger 46: +0x3A0 = 0");
    Frame(0, 0, 0);
    Check(Idle() == 0, "trigger 46 released: +0x3A0 = 0");
    Frame(0, 0, 0);
    Check(Idle() == 1, "rest again: +0x3A0 = 1");

    std::printf("a button tap (row 8 -> 3 BOOST + 49 GUI_SELECT):\n");
    Frame(KU_BUTTON_A, 0, 0);
    Check(Status(KU_BOOST) == 3u && Status(KU_GUI_SELECT) == 3u, "A down: BOOST and GUI_SELECT held|pressed");
    Check(Idle() == 0, "A down: +0x3A0 = 0");
    Frame(0, 0, 0);
    Check(Status(KU_BOOST) == 4u && Status(KU_GUI_SELECT) == 4u, "A up: both released (4)");
    Check(Idle() == 0, "A up: +0x3A0 = 0");
    Frame(0, 0, 0);
    Check(Idle() == 1, "after A: +0x3A0 = 1");

    std::printf("the left stick (row 19 -> 44 GUI_RIGHT; the axis itself is not an action):\n");
    Frame(0, 0, 32767);
    Check(Pad().mfStickLX == 1.0f, "full right: mfStickLX 1.0");
    Check(Status(KU_GUI_RIGHT) == 3u, "full right: GUI_RIGHT held|pressed");
    Check(Idle() == 0, "full right: +0x3A0 = 0");
    Frame(0, 0, 5000);
    Check(Idle() == 0, "back inside the deadzone: +0x3A0 = 0 (GUI_RIGHT released this frame)");
    Frame(0, 0, 5000);
    Check(Pad().mfStickLX == 0.0f, "resting inside the deadzone: mfStickLX 0");
    Check(Idle() == 1, "resting inside the deadzone: +0x3A0 = 1");
    Frame(0, 0, 9000);
    Check(Value(KU_GUI_RIGHT) >= 0.1f && Value(KU_GUI_RIGHT) < 0.11f, "thumb 9000: GUI_RIGHT value 0.1067");
    Check(Idle() == 0, "thumb 9000: +0x3A0 = 0 (a barely deflected stick is input)");

    std::printf("FxTailsAPadIdle: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
