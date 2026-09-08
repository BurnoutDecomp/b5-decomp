#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsCustomWindow.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

#include <cstring>

// CgsDev::DebugUI::CustomWindow + CustomWindowMenuItem. Reconstructed from ARTIST
// (Update 0x82830FD0, Register 0x82829E48, GetMenuPath 0x8281A0F8 and the menu-item
// functions at 0x828169A8/0x82829ED8/0x82829F10/0x82829F78).

namespace CgsDev
{
    namespace DebugUI
    {
        // --- CustomWindowMenuItem ---
        void CustomWindowMenuItem::Prepare(Window* lpWindow)
        {
            MenuItem::Prepare();
            mpWindow = lpWindow;
        }

        void CustomWindowMenuItem::Update(f32 /*lfTimeStep*/, InputEvent leEvent)
        {
            if (leEvent == E_INPUTEVENT_SELECT)
                OpenAsWindow();
        }

        void CustomWindowMenuItem::Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY,
                                          bool lbSelected, f32 lfItemWidth)
        {
            RenderMenuItemText(lpRender, mpWindow->GetCaption(), lfX, lfY, mfWidth, mfHeight,
                               lbSelected, lfItemWidth);
        }

        void CustomWindowMenuItem::ComputeSize()
        {
            ComputeSizeFromText(mpWindow->GetCaption());
        }

        bool CustomWindowMenuItem::IsUseful() const { return true; }

        void CustomWindowMenuItem::GetDisplayName(char* lpcBuffer, s32 liBufferLen) const
        {
            if (liBufferLen <= 0)
                return;
            std::strncpy(lpcBuffer, mpWindow->GetCaption(), static_cast<size_t>(liBufferLen - 1));
            lpcBuffer[liBufferLen - 1] = '\0';
        }

        Window* CustomWindowMenuItem::OpenAsWindow()
        {
            DebugUI& lrUI = GetUI();
            if (!lrUI.IsWindowAdded(mpWindow))
                lrUI.AddWindow(mpWindow);
            lrUI.SetActiveWindow(mpWindow);
            return mpWindow;
        }

        // --- CustomWindow ---
        void CustomWindow::Prepare(f32 lfWidth, f32 lfHeight, const char* lpcCaption,
                                   const char* lpcMenuPath, s32 lxFlags)
        {
            Window::Prepare(lfWidth, lfHeight, lpcCaption, lxFlags);
            mMenuItem.Prepare(this);
            if (lpcMenuPath)
                Register(lpcMenuPath);
        }

        void CustomWindow::Update(f32 /*lfTimeStep*/, InputEvent leEvent)
        {
            if (leEvent != E_INPUTEVENT_BACK && leEvent != E_INPUTEVENT_CLOSE)
                return;

            if (IsPinned())
            {
                if (!IsModal())
                    GetUI().SetActiveWindow(GetUI().GetNextActiveWindow(this));
                return;
            }

            GetUI().RemoveWindow(this);
            if (leEvent == E_INPUTEVENT_BACK)
            {
                Menu* lpMenu = GetUI().GetMenuManager().FindMenu(&mMenuItem);
                if (lpMenu)
                    GetUI().GetMenuManager().Open(lpMenu);
            }
        }

        void CustomWindow::Register(const char* lpcMenuPath)
        {
            MenuManager& lrManager = GetUI().GetMenuManager();
            if (lrManager.FindMenu(&mMenuItem))
                return;

            Menu* lpMenu = lrManager.CreateMenuPath(lpcMenuPath, nullptr);
            if (lpMenu)
                lpMenu->AddMenuItem(&mMenuItem);
        }

        void CustomWindow::Unregister()
        {
            GetUI().GetMenuManager().RemoveMenuItem(&mMenuItem);
        }

        void CustomWindow::GetMenuPath(char* lpcBuffer, s32 liBufferLen)
        {
            Menu* lpMenu = GetUI().GetMenuManager().FindMenu(&mMenuItem);
            if (lpMenu)
                lpMenu->GetPath(lpcBuffer, liBufferLen);
            else if (liBufferLen > 0)
                lpcBuffer[0] = '\0';

            if (mpcCaption && mpcCaption[0])
            {
                GetUI().SafeStringCat(lpcBuffer, "/", liBufferLen);
                GetUI().SafeStringCat(lpcBuffer, mpcCaption, liBufferLen);
            }
        }
    }
}
