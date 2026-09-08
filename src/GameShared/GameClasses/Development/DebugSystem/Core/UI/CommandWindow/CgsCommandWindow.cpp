#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CommandWindow/CgsCommandWindow.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Console/CgsConsole.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/ScriptInterface/CgsScriptInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

#include <cstring>

// The interactive console input window. ARTIST behavior is recovered from ProcessInput
// 0x82833908, Render 0x8282A150, ToggleShow 0x828308B0, Update 0x82833DA8,
// UpdatePosition 0x8282F240 and the focus thunks at 0x8282A3E0/0x8282A3F8.
namespace CgsDev
{
    namespace DebugUI
    {
        const f32 CommandWindow::KF_CURSOR_BLINK_TIME = 0.2f;

        CommandWindow::CommandWindow()
            : Window()
            , mfBlinkTime(0.0f)
            , mbCursorBlink(false)
        {
            macInput[0] = '\0';
        }

        void CommandWindow::Construct()
        {
            mfBlinkTime = 0.0f;
            mbCursorBlink = false;
            macInput[0] = '\0';
        }

        void CommandWindow::Render(Debug2DImmediateRender* lpRender)
        {
            Window::Render(lpRender);

            char lacDisplay[192];
            std::strncpy(lacDisplay, macInput, sizeof(lacDisplay) - 1);
            lacDisplay[sizeof(lacDisplay) - 1] = '\0';
            if (mbCursorBlink && std::strlen(lacDisplay) + 1 < KI_MAX_INPUT)
                std::strcat(lacDisplay, "_");

            const Metrics& lrMetrics = GetUI().GetMetrics();
            lpRender->DrawText(lacDisplay,
                               GetX() + lrMetrics.mfScreenBorderLeft,
                               GetY() + lrMetrics.mfWindowBorderSize,
                               lrMetrics.mfTextSize,
                               GetUI().GetPalette().mColourText);
        }

        void CommandWindow::Update(f32 lfTimeStep, InputEvent /*leEvent*/)
        {
            ProcessInput();
            mfBlinkTime += lfTimeStep;
            if (mfBlinkTime > KF_CURSOR_BLINK_TIME)
            {
                mfBlinkTime = 0.0f;
                mbCursorBlink = !mbCursorBlink;
            }
            UpdatePosition();
        }

        void CommandWindow::ToggleShow()
        {
            DebugUI& lrUI = GetUI();
            Console& lrConsole = lrUI.GetConsole();
            if (!lrConsole.IsEnabled())
                return;

            if (lrUI.GetController().IsKeyboardPresent())
            {
                if (lrUI.IsWindowAdded(this))
                {
                    if (lrUI.GetActiveWindow() == this)
                    {
                        lrConsole.ToggleShow();
                        lrUI.RemoveWindow(this);
                    }
                    else
                    {
                        lrUI.SetActiveWindow(this);
                    }
                    return;
                }

                const Metrics& lrMetrics = lrUI.GetMetrics();
                Window::Prepare(lrMetrics.mfScreenWidth,
                                lrMetrics.mfTextSize + lrMetrics.mfWindowBorderSize * 2.0f,
                                nullptr,
                                KX_FLAGNOCAPTION | KX_FLAGNOCASCADE | KX_FLAGNOCLAMPTOSCREEN);
                UpdatePosition();
                lrUI.AddWindow(this);
            }
            lrConsole.ToggleShow();
        }

        bool CommandWindow::IsVisible()
        {
            return GetUI().GetConsole().IsVisible();
        }

        void CommandWindow::OnGetFocus()
        {
            GetUI().GetController().LockKeyboard();
        }

        void CommandWindow::OnLostFocus()
        {
            GetUI().GetController().UnlockKeyboard();
        }

        void CommandWindow::Clear()
        {
            macInput[0] = '\0';
        }

        void CommandWindow::ProcessInput()
        {
            DebugUI& lrUI = GetUI();
            const char lcKey = lrUI.GetController().GetKeyPress();
            switch (lcKey)
            {
            case 0:
                return;
            case 8:
            {
                const size_t luLength = std::strlen(macInput);
                if (luLength)
                    macInput[luLength - 1] = '\0';
                return;
            }
            case 9:
                if (lrUI.GetController().IsShiftPressed())
                {
                    char lacItem[256];
                    GetCurrentItemString(lacItem, sizeof(lacItem));
                    if (std::strlen(macInput) + std::strlen(lacItem) < KI_MAX_INPUT - 1)
                        std::strcat(macInput, lacItem);
                }
                return;
            case 10:
            case 13:
                lrUI.GetScriptInterface().Execute(macInput);
                Clear();
                return;
            case 96:
                Clear();
                ToggleShow();
                return;
            default:
                if (lcKey >= 32 && lcKey < 126)
                {
                    const size_t luLength = std::strlen(macInput);
                    if (luLength < KI_MAX_INPUT - 1)
                    {
                        macInput[luLength] = lcKey;
                        macInput[luLength + 1] = '\0';
                    }
                }
                return;
            }
        }

        void CommandWindow::UpdatePosition()
        {
            const Metrics& lrMetrics = GetUI().GetMetrics();
            SetSize(GetWidth(), lrMetrics.mfTextSize + lrMetrics.mfWindowBorderSize * 2.0f);

            Console& lrConsole = GetUI().GetConsole();
            SetPosition(0.0f, lrConsole.GetY() + lrConsole.GetHeight() + lrMetrics.mfWindowBorderSize);
            ClampToScreen();
        }

        void CommandWindow::GetCurrentItemString(char* lpcBuffer, s32 liBufferLen)
        {
            lpcBuffer[0] = '\0';
            DebugUI& lrUI = GetUI();
            Window* lpWindow = lrUI.GetPreviousActiveWindow(&lrUI.GetConsole());
            if (lpWindow == &lrUI.GetConsole() || lpWindow == this)
                lpWindow = lrUI.GetPreviousActiveWindow(this);
            if (!lpWindow || lpWindow == &lrUI.GetConsole() || lpWindow == this)
                return;

            char lacPath[256];
            char lacItem[256];
            lpWindow->GetMenuPath(lacPath, sizeof(lacPath));
            lpWindow->GetSelectedItemString(lacItem, sizeof(lacItem));

            const size_t luPathLength = std::strlen(lacPath);
            if (!luPathLength || lacPath[luPathLength - 1] != '/')
                GetUI().SafeStringCat(lacPath, "/", sizeof(lacPath));

            if (std::strchr(lacPath, ' ') || std::strchr(lacItem, ' '))
                CgsCore::SPrintf(lpcBuffer, liBufferLen, "\"%s%s\"", lacPath, lacItem);
            else
                CgsCore::SPrintf(lpcBuffer, liBufferLen, "%s%s", lacPath, lacItem);
        }

        void CommandWindow::Register(const char* /*lpcName*/) {}
        void CommandWindow::Unregister() {}
    }
}
