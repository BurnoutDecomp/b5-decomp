#include "GameShared/GameClasses/Development/DebugSystem/Controller/CgsDebugController.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

#if !defined(D_PLATFORM_X360)
#include <cstdlib>
#include <cstdio>
#include "GameShared/GameClasses/System/CgsHarnessSlot.h"
#include "GameShared/GameClasses/System/Input/PC/CgsDebugKeyboardPC.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"

// FLAG PC-platform leaf: host keyboard polling in place of the X360 keystroke API. The resulting
// DebugController state and its event translation remain the original platform-independent path.
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
extern "C" __declspec(dllimport) void* __stdcall GetForegroundWindow(void);
extern "C" __declspec(dllimport) unsigned long __stdcall GetWindowThreadProcessId(void* hWnd, unsigned long* lpdwProcessId);
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentProcessId(void);
extern "C" __declspec(dllimport) void* __stdcall OpenEventA(unsigned long, int, const char*);
extern "C" __declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void*, unsigned long);
struct HWND__;
namespace renderengine { extern HWND__* hWnd; }
#endif

namespace CgsDev
{
#if !defined(D_PLATFORM_X360)
    bool IsDebugKeyboardCapturedPC()
    {
        DebugManager* lpDebug = DebugManager::GetInstance();
        return lpDebug && lpDebug->GetUI().GetController().IsPCKeyboardCaptured();
    }
#endif
    namespace
    {
        const DebugUI::InputEvent KAE_BUTTON_MAP_TABLE[DebugController::KI_MAX_BUTTONS] =
        {
            DebugUI::E_INPUTEVENT_SELECT,
            DebugUI::E_INPUTEVENT_BACK,
            DebugUI::E_INPUTEVENT_TOGGLEPIN,
            DebugUI::E_INPUTEVENT_TOGGLECONSOLE,
            DebugUI::E_INPUTEVENT_NEXTWINDOW,
            DebugUI::E_INPUTEVENT_PREVWINDOW,
            DebugUI::E_INPUTEVENT_MAINMENU,
            DebugUI::E_INPUTEVENT_TOGGLEUI,
        };

        const u32 KA_BUTTON_MAP[DebugController::KI_MAX_BUTTONS] = { 8, 9, 10, 11, 12, 13, 4, 5 };
        const DebugUI::InputEvent KAE_AXIS_MAP_TABLE[4] =
        {
            DebugUI::E_INPUTEVENT_CURSORUP,
            DebugUI::E_INPUTEVENT_CURSORDOWN,
            DebugUI::E_INPUTEVENT_CURSORLEFT,
            DebugUI::E_INPUTEVENT_CURSORRIGHT,
        };
        const DebugUI::InputEvent KAE_AXIS_MAP_TABLE_CTRL[4] =
        {
            DebugUI::E_INPUTEVENT_DOCKTOP,
            DebugUI::E_INPUTEVENT_DOCKBOTTOM,
            DebugUI::E_INPUTEVENT_DOCKLEFT,
            DebugUI::E_INPUTEVENT_DOCKRIGHT,
        };

        const f32 KF_CONTROLLER_DEAD_ZONE = 0.001f;

#if !defined(D_PLATFORM_X360)
        bool IsProcessForeground()
        {
            void* lpForeground = GetForegroundWindow();
            if (!lpForeground)
                return false;
            if (renderengine::hWnd)
                return lpForeground == static_cast<void*>(renderengine::hWnd);
            unsigned long luPid = 0;
            GetWindowThreadProcessId(lpForeground, &luPid);
            return luPid == GetCurrentProcessId();
        }

        // FLAG PC-platform leaf: debug keystrokes use the same named-event harness as gameplay.
        // Manual-reset events support modifier chords/holds; auto-reset events supply taps.
        bool HarnessKeyDown(s32 liVirtualKey)
        {
            static void* sapEvents[256] = {};
            if (!sapEvents[liVirtualKey])
            {
                char lacName[96];
                std::snprintf(lacName, sizeof(lacName), "Local\\BurnoutPC_DebugKey_%02X%s",
                              liVirtualKey, CgsSystem::HarnessSlot::Suffix());
                sapEvents[liVirtualKey] = OpenEventA(0x00100000, 0, lacName);
            }
            return sapEvents[liVirtualKey] && WaitForSingleObject(sapEvents[liVirtualKey], 0) == 0;
        }
#endif
    }

    const f32 DebugController::KF_KEY_REPEAT_TIME = 1.0f;

    void DebugController::Construct(const DebugUI::Metrics* lpMetrics)
    {
        mpMetrics = lpMetrics;
        CGS_ASSERT(mpMetrics, "mpMetrics");
        mpDebugManagerPad = nullptr;
        ClearPad();
        ClearKeyboard();
        ClearEvent();
        mbKeyboardPresent = InitKeyboard();
        mbKeyboardLocked = false;
        mfKeyRepeatDelay = KF_KEY_REPEAT_TIME;
#if !defined(D_PLATFORM_X360)
        for (s32 liKey = 0; liKey < 256; ++liKey)
            mabPCKeysDown[liKey] = mabPCKeysPressed[liKey] = false;
        mbPCKeyboardCaptured = false;
#endif
    }

    void DebugController::Destruct() {}

    void DebugController::SetGamePad(DebugManagerPad* lpDebugManagerPad)
    {
        mpDebugManagerPad = lpDebugManagerPad;
    }

    void DebugController::Update(f32 lfTimeStep)
    {
        UpdatePad();
        UpdateKeyboard(lfTimeStep);
        if (!mbKeyboardLocked)
            CopyKeyboardToPad();
        UpdateEvent(lfTimeStep);
    }

    void DebugController::ClearPad()
    {
        mfX = mfY = mfX2 = mfY2 = 0.0f;
        for (s32 liIndex = 0; liIndex < KI_MAX_BUTTONS; ++liIndex)
            mabButtons[liIndex] = false;
    }

    void DebugController::UpdatePad()
    {
#if defined(D_PLATFORM_X360)
        if (!mpDebugManagerPad || !mpDebugManagerPad->IsConnected())
        {
            ClearPad();
            return;
        }

        mfX = mpDebugManagerPad->GetAxisValue(0);
        mfY = -mpDebugManagerPad->GetAxisValue(1);
        mfX2 = mpDebugManagerPad->GetAxisValue(2);
        mfY2 = -mpDebugManagerPad->GetAxisValue(3);
        for (s32 liIndex = 0; liIndex < KI_MAX_BUTTONS; ++liIndex)
            mabButtons[liIndex] = mpDebugManagerPad->IsButtonPressed(KA_BUTTON_MAP[liIndex]);

        if (mpDebugManagerPad->IsButtonPressed(0))
            mfY = -1.0f;
        else if (mpDebugManagerPad->IsButtonPressed(1))
            mfY = 1.0f;
        else if (mpDebugManagerPad->IsButtonPressed(2))
            mfX = -1.0f;
        else if (mpDebugManagerPad->IsButtonPressed(3))
            mfX = 1.0f;
#else
        // FLAG PC-platform leaf: the PC debug menu is keyboard-driven; the gameplay input module
        // publishes action state rather than an X360 DeviceX360Pad record to this subsystem.
        ClearPad();
#endif
    }

    f32 DebugController::TranslateAxis(s32 liAxisValue)
    {
        return liAxisValue < 0 ? -1.0f : (liAxisValue > 0 ? 1.0f : 0.0f);
    }

    bool DebugController::InitKeyboard()
    {
        return true;
    }

    void DebugController::ClearKeyboard()
    {
        meSpecialKeyPress = E_KEY_NONE;
        mcKeyPress = 0;
        mbShiftPressed = false;
        mbAltPressed = false;
        mbCtrlPressed = false;
    }

    void DebugController::UpdateKeyboard(f32 lfTimeStep)
    {
        ClearKeyboard();
        if (!mbKeyboardPresent)
            return;
#if defined(D_PLATFORM_X360)
        ReadKey(lfTimeStep);
#else
        // Snapshot high-bit state once. GetAsyncKeyState's low bit is shared with other
        // pollers, so it cannot reliably represent this controller's key-down edges.
        const bool lbHarness = std::getenv("BRN_INPUT_ALLOW_BACKGROUND") != nullptr;
        const bool lbPhysical = (!lbHarness || std::getenv("BRN_INPUT_KEEP_KEYBOARD"))
                                && IsProcessForeground();
        bool lbAnyKeyDown = false;
        for (s32 liKey = 0; liKey < 256; ++liKey)
        {
            const bool lbDown = (lbPhysical && (GetAsyncKeyState(liKey) & 0x8000) != 0)
                                || (lbHarness && HarnessKeyDown(liKey));
            mabPCKeysPressed[liKey] = lbDown && !mabPCKeysDown[liKey];
            mabPCKeysDown[liKey] = lbDown;
            if (liKey >= 8) lbAnyKeyDown = lbAnyKeyDown || lbDown;
        }

        mbShiftPressed = IsKeyDown(0x10);
        mbCtrlPressed = IsKeyDown(0x11);
        mbAltPressed = IsKeyDown(0x12);

        if (IsKeyDown(0x25)) meSpecialKeyPress = E_KEY_LEFT;
        else if (IsKeyDown(0x27)) meSpecialKeyPress = E_KEY_RIGHT;
        else if (IsKeyDown(0x26)) meSpecialKeyPress = E_KEY_UP;
        else if (IsKeyDown(0x28)) meSpecialKeyPress = E_KEY_DOWN;
        else
        {
            for (s32 liKey = 0; liKey < 12; ++liKey)
                if (WasKeyPressed(0x70 + liKey))
                {
                    meSpecialKeyPress = static_cast<SpecialKey>(E_KEY_F1 + liKey);
                    break;
                }
        }
        ReadKey(lfTimeStep);

        const bool lbVisible = DebugManager::GetInstance()->GetUI().IsVisible();
        const bool lbOpenMenu = mcKeyPress == ' ' && mbCtrlPressed;
        const bool lbOpenConsole = mcKeyPress == '`';
        mbPCKeyboardCaptured = lbVisible || lbOpenMenu || lbOpenConsole
                               || (mbPCKeyboardCaptured && lbAnyKeyDown);
        // FLAG PC-platform leaf: Ctrl+Space opens the menu without stealing Space/Enter/Esc
        // from the game's screens. Once open, the original menu bindings apply unchanged.
        if (!lbVisible)
        {
            if (!lbOpenMenu && !lbOpenConsole) mcKeyPress = 0;
            if (meSpecialKeyPress < E_KEY_F1) meSpecialKeyPress = E_KEY_NONE;
        }
#endif
    }

    void DebugController::ReadKey(f32 /*lfTimeStep*/)
    {
#if !defined(D_PLATFORM_X360)
        if (WasKeyPressed(0x08)) { mcKeyPress = 8; return; }
        if (WasKeyPressed(0x09)) { mcKeyPress = 9; return; }
        if (WasKeyPressed(0x0D)) { mcKeyPress = 10; return; }
        if (WasKeyPressed(0x1B)) { mcKeyPress = 27; return; }
        if (WasKeyPressed(0x20)) { mcKeyPress = 32; return; }
        if (WasKeyPressed(0xBF)) { mcKeyPress = mbShiftPressed ? '?' : '/'; return; }
        if (WasKeyPressed(0xC0)) { mcKeyPress = mbShiftPressed ? '~' : '`'; return; }
        if (WasKeyPressed(0xBD)) { mcKeyPress = mbShiftPressed ? '_' : '-'; return; }
        if (WasKeyPressed(0xBB)) { mcKeyPress = mbShiftPressed ? '+' : '='; return; }
        if (WasKeyPressed(0xBC)) { mcKeyPress = mbShiftPressed ? '<' : ','; return; }
        if (WasKeyPressed(0xBE)) { mcKeyPress = mbShiftPressed ? '>' : '.'; return; }
        if (WasKeyPressed(0xBA)) { mcKeyPress = mbShiftPressed ? ':' : ';'; return; }
        if (WasKeyPressed(0xDE)) { mcKeyPress = mbShiftPressed ? '"' : '\''; return; }
        if (WasKeyPressed(0xDB)) { mcKeyPress = mbShiftPressed ? '{' : '['; return; }
        if (WasKeyPressed(0xDD)) { mcKeyPress = mbShiftPressed ? '}' : ']'; return; }
        if (WasKeyPressed(0xDC)) { mcKeyPress = mbShiftPressed ? '|' : '\\'; return; }

        static const char KAC_SHIFT_DIGITS[10] = { ')', '!', '@', '#', '$', '%', '^', '&', '*', '(' };
        for (s32 liKey = '0'; liKey <= '9'; ++liKey)
            if (WasKeyPressed(liKey))
            {
                mcKeyPress = mbShiftPressed ? KAC_SHIFT_DIGITS[liKey - '0'] : static_cast<char>(liKey);
                return;
            }
        for (s32 liKey = 'A'; liKey <= 'Z'; ++liKey)
            if (WasKeyPressed(liKey))
            {
                mcKeyPress = static_cast<char>(mbShiftPressed ? liKey : liKey + ('a' - 'A'));
                return;
            }
#endif
    }

    char DebugController::SimpleLocaliseKey(char lcKey) { return lcKey; }

    void DebugController::CopyKeyboardToPad()
    {
        switch (mcKeyPress)
        {
        case 8:   mabButtons[1] = true; return;
        case 9:   mabButtons[mbShiftPressed ? 5 : 4] = true; return;
        case 10:  mabButtons[0] = true; return;
        case 27:  mabButtons[7] = true; return;
        case 32:  mabButtons[6] = true; return;
        case '/': mabButtons[2] = true; return;
        case '`': mabButtons[3] = true; return;
        default: break;
        }

        if (mbShiftPressed)
        {
            if (meSpecialKeyPress == E_KEY_LEFT) mfX2 = -1.0f;
            else if (meSpecialKeyPress == E_KEY_RIGHT) mfX2 = 1.0f;
            else if (meSpecialKeyPress == E_KEY_UP) mfY2 = -1.0f;
            else if (meSpecialKeyPress == E_KEY_DOWN) mfY2 = 1.0f;
        }
        else
        {
            if (meSpecialKeyPress == E_KEY_LEFT) mfX = -1.0f;
            else if (meSpecialKeyPress == E_KEY_RIGHT) mfX = 1.0f;
            else if (meSpecialKeyPress == E_KEY_UP) mfY = -1.0f;
            else if (meSpecialKeyPress == E_KEY_DOWN) mfY = 1.0f;
        }
    }

    DebugUI::InputEvent DebugController::GetAxisEvent() const
    {
        s32 liAxis;
        if ((mfX < 0.0f ? -mfX : mfX) > (mfY < 0.0f ? -mfY : mfY))
        {
            if (mfX < -KF_CONTROLLER_DEAD_ZONE) liAxis = 2;
            else if (mfX > KF_CONTROLLER_DEAD_ZONE) liAxis = 3;
            else return DebugUI::E_INPUTEVENT_NONE;
        }
        else
        {
            if (mfY < -KF_CONTROLLER_DEAD_ZONE) liAxis = 0;
            else if (mfY > KF_CONTROLLER_DEAD_ZONE) liAxis = 1;
            else return DebugUI::E_INPUTEVENT_NONE;
        }

        if (mbCtrlPressed)
            return KAE_AXIS_MAP_TABLE_CTRL[liAxis];
        if (!mbShiftPressed)
            return KAE_AXIS_MAP_TABLE[liAxis];
        return DebugUI::E_INPUTEVENT_NONE;
    }

    DebugUI::InputEvent DebugController::TranslateControllerInputEvent() const
    {
        const DebugUI::InputEvent leAxisEvent = GetAxisEvent();
        for (s32 liIndex = 0; liIndex < KI_MAX_BUTTONS; ++liIndex)
            if (mabButtons[liIndex])
                return KAE_BUTTON_MAP_TABLE[liIndex];
        return leAxisEvent;
    }

    DebugUI::InputEvent DebugController::GetInputEvent(f32 lfTimeStep)
    {
        DebugUI::InputEvent leInputEvent = TranslateControllerInputEvent();
        if (leInputEvent != meLastInputEvent)
        {
            meLastInputEvent = leInputEvent;
            mfEventRepeatDelay = mpMetrics->mfAutoRepeatDelay;
            mfEventRepeatDelayMax = mpMetrics->mfAutoRepeatRate;
        }
        else
        {
            mfEventRepeatDelay -= lfTimeStep;
            if (mfEventRepeatDelay > 0.0f)
                return DebugUI::E_INPUTEVENT_NONE;
            mfEventRepeatDelay = mfEventRepeatDelayMax;
            mfEventRepeatDelayMax -= mpMetrics->mfAutoRepeatAcceleration * lfTimeStep;
            if (mfEventRepeatDelayMax < 0.0f)
                mfEventRepeatDelayMax = 0.0f;
        }
        return leInputEvent;
    }

    void DebugController::ClearEvent()
    {
        meInputEvent = DebugUI::E_INPUTEVENT_NONE;
        meLastInputEvent = DebugUI::E_INPUTEVENT_NONE;
        mfEventRepeatDelay = 0.0f;
        mfEventRepeatDelayMax = 0.0f;
    }

    void DebugController::UpdateEvent(f32 lfTimeStep)
    {
        meInputEvent = GetInputEvent(lfTimeStep);
    }

    void DebugController::LockKeyboard()
    {
        CGS_ASSERT(!mbKeyboardLocked, "!mbKeyboardLocked");
        // ARTIST 0x82823C54-0x82823C70 clears the key, special-key and modifier
        // fields as focus transfers to the command window.  Besides preserving
        // the original focus contract, this prevents the grave accent which
        // opened the console from immediately being consumed again as the
        // command-window close key in the same DebugUI::Update.
        ClearKeyboard();
        mbKeyboardLocked = true;
    }

    void DebugController::UnlockKeyboard()
    {
        CGS_ASSERT(mbKeyboardLocked, "mbKeyboardLocked");
        mbKeyboardLocked = false;
        ClearKeyboard();
    }
}
