#include "GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h"

#include <cstring>   // std::memset
#include <cstdlib>   // std::getenv (the harness focus-gate bypass)

// ============================================================================
// FLAG PC-platform leaf (whole file): the host pad source standing in for the
// unreconstructed console pad fill (CgsInput::InputPads::Update + the binding
// tables). See the header for the contract. Win32/XInput imports are declared
// locally (no <Windows.h> -- its NOUSER/NOGDI lean-defines conflict with the
// game TUs; same pattern as the BrnGuiModule.cpp bring-up helpers this replaces).
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

    // XINPUT_GAMEPAD wButtons bits.
    const unsigned short KU_XPAD_DPAD_UP    = 0x0001;
    const unsigned short KU_XPAD_DPAD_DOWN  = 0x0002;
    const unsigned short KU_XPAD_DPAD_LEFT  = 0x0004;
    const unsigned short KU_XPAD_DPAD_RIGHT = 0x0008;
    const unsigned short KU_XPAD_START      = 0x0010;
    const unsigned short KU_XPAD_A          = 0x1000;
    const unsigned short KU_XPAD_B          = 0x2000;

    // FOCUS GATE: GetAsyncKeyState reads the GLOBAL key state, so without a foreground
    // check the boot flow reacts to keys typed into ANY app (verified: terminal Enters
    // accepted the title menu). Only read the keyboard while this process is foreground.
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

    // ---- host -> GUI-action binding (the PC stand-in for the console binding tables) ----
    // Action ids are the pad ActionInfo slots the controller bridge scans / repeats
    // (the real X360 tables at 0x820352F0 / 0x82035330); BootLegal consumes 45 accept,
    // 49 stop, and the repeat-table ids 41 menu-next / 42 menu-prev.
    struct PcActionBinding
    {
        s32            iActionId;
        const int*     paiVKeys;     // virtual keys mapped to this action (0-terminated)
        unsigned short uXPadButtons; // XINPUT wButtons mask mapped to this action
    };
    const int KAI_KEYS_ACCEPT[] = { 0x0D /*VK_RETURN*/, 0x20 /*VK_SPACE*/, 0 };
    const int KAI_KEYS_STOP[]   = { 0x1B /*VK_ESCAPE*/, 0 };
    const int KAI_KEYS_NEXT[]   = { 0x28 /*VK_DOWN*/, 0x27 /*VK_RIGHT*/, 0 };
    const int KAI_KEYS_PREV[]   = { 0x26 /*VK_UP*/, 0x25 /*VK_LEFT*/, 0 };
    const PcActionBinding KA_BINDINGS[] =
    {
        { 45, KAI_KEYS_ACCEPT, static_cast<unsigned short>(KU_XPAD_A | KU_XPAD_START) },
        { 49, KAI_KEYS_STOP,   KU_XPAD_B },
        { 41, KAI_KEYS_NEXT,   static_cast<unsigned short>(KU_XPAD_DPAD_DOWN | KU_XPAD_DPAD_RIGHT) },
        { 42, KAI_KEYS_PREV,   static_cast<unsigned short>(KU_XPAD_DPAD_UP | KU_XPAD_DPAD_LEFT) },
    };
    const u32 KU_NUM_BINDINGS = sizeof(KA_BINDINGS) / sizeof(KA_BINDINGS[0]);

    // FLAG PC-platform leaf: the unattended boot harness signals one auto-reset event
    // per host action. Unlike synthetic desktop keystrokes these survive a locked or
    // disconnected desktop and are consumed by exactly one input update. The channel is
    // enabled only with the harness's BRN_INPUT_ALLOW_BACKGROUND environment marker.
    bool ConsumeHarnessAction(s32 liActionId)
    {
        static const bool s_bHarnessEnabled =
            (std::getenv("BRN_INPUT_ALLOW_BACKGROUND") != nullptr);
        if (!s_bHarnessEnabled)
            return false;

        const char* lpcEventName = 0;
        switch (liActionId)
        {
        case 45: lpcEventName = "Local\\BurnoutPC_Input_Accept"; break;
        case 49: lpcEventName = "Local\\BurnoutPC_Input_Stop";   break;
        case 41: lpcEventName = "Local\\BurnoutPC_Input_Next";   break;
        case 42: lpcEventName = "Local\\BurnoutPC_Input_Prev";   break;
        default: return false;
        }

        // SYNCHRONIZE == 0x00100000; WAIT_OBJECT_0 == 0. The harness creates
        // auto-reset events before launching the game, so a successful zero-time wait
        // both observes and consumes one requested action.
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

        // Host device reads.
        const bool lbForeground = IsProcessForeground();
        XInputState lXState;
        std::memset(&lXState, 0, sizeof(lXState));
        bool lbXPad = false;
        if (XInputGetStateFn lpfGetState = ResolveXInputGetState())
            lbXPad = (lpfGetState(0, &lXState) == 0);   // ERROR_SUCCESS

        // Analogue sticks (XInput thumbs normalised to [-1,1]; keyboard leaves them 0).
        const f32 KF_THUMB_SCALE = 1.0f / 32768.0f;
        lrPad.mfStickLX = lbXPad ? lXState.Gamepad.sThumbLX * KF_THUMB_SCALE : 0.0f;
        lrPad.mfStickLY = lbXPad ? lXState.Gamepad.sThumbLY * KF_THUMB_SCALE : 0.0f;
        lrPad.mfStickRX = lbXPad ? lXState.Gamepad.sThumbRX * KF_THUMB_SCALE : 0.0f;
        lrPad.mfStickRY = lbXPad ? lXState.Gamepad.sThumbRY * KF_THUMB_SCALE : 0.0f;
        lrPad.mfAxis10  = lrPad.mfStickRX;
        lrPad.mfAxis14  = lrPad.mfStickRY;

        // Per-action edge tracking -> the console muStatus contract
        // (bit0 held, bit1 pressed-this-frame, bit2 released-this-frame).
        static bool sabWasDown[KU_NUM_BINDINGS] = {};
        for (u32 luBind = 0; luBind < KU_NUM_BINDINGS; ++luBind)
        {
            const PcActionBinding& lrBinding = KA_BINDINGS[luBind];
            bool lbDown = false;
            if (lbForeground)
            {
                for (const int* lpiKey = lrBinding.paiVKeys; *lpiKey != 0 && !lbDown; ++lpiKey)
                    lbDown = (GetAsyncKeyState(*lpiKey) & 0x8000) != 0;
            }
            if (!lbDown)
                lbDown = ConsumeHarnessAction(lrBinding.iActionId);
            if (!lbDown && lbXPad)
                lbDown = (lXState.Gamepad.wButtons & lrBinding.uXPadButtons) != 0;

            u32 luStatus = 0;
            if (lbDown)
                luStatus |= 1u;                          // held
            if (lbDown && !sabWasDown[luBind])
                luStatus |= 2u;                          // pressed edge
            if (!lbDown && sabWasDown[luBind])
                luStatus |= 4u;                          // released edge
            sabWasDown[luBind] = lbDown;

            InputIO::ActionInfo& lrAction = lrPad.maActionInfo[lrBinding.iActionId];
            lrAction.mfValue  = lbDown ? 1.0f : 0.0f;
            lrAction.muStatus = luStatus;
        }

        // Connection/state tail: the pad is present and assigned to player 0.
        lrPad.muConnectionWord  = 0;   // usable (GetPadInfoForPlayer0's gate)
        lrPad.meControllerState = 1;   // connected (!=0 publishes the active-user index; !=2 keeps both stick axis events)
        lrPad.mbDisconnected    = 0;
    }
}
