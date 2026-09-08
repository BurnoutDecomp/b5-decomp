#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuWindow.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <typeinfo>
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsMenuItemFunction.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunction.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

// CgsDev::DebugUI::MenuWindow - the on-screen window that renders an open Menu. The manager-path
// bodies (ctor + Prepare + the Menu accessor) are reconstructed from the DecFIGS DWARF
// (Development/DebugSystem/Core/UI/Menu/CgsMenuWindow.h) and the pool element-ctor + MenuManager
// pairing. The display/navigation virtuals (Update/Render and the path/selected-item overrides that
// draw the menu through Debug2DImmediateRender) are the menu-render follow-on; they are stubbed for
// the vtable link, consistent with the rest of the menu family (Menu/MenuItem rows), and are dead in
// the bounded build (no MenuWindow is ticked or drawn).

namespace CgsDev
{
    namespace DebugUI
    {
        // Pool element ctor: a freshly pooled window owns no menu until Prepare pairs it with one.
        MenuWindow::MenuWindow()
            : Window()
            , mpMenu(nullptr)
        {
        }

        // MenuManager pairs the pooled window with the Menu it displays. The window is sized 10x10
        // and takes the menu's caption; an empty caption suppresses the caption bar (X360 0x8282EAA8:
        // Window::Prepare(10, 10, macCaption, *macCaption ? 0 : KX_FLAGNOCAPTION) then store mpMenu).
        void MenuWindow::Prepare(Menu* lpMenu)
        {
            const char* lpcCaption = lpMenu->GetCaption();
            const s32   lxFlags    = (*lpcCaption != '\0') ? KX_FLAGNORMAL : KX_FLAGNOCAPTION;
            Window::Prepare(10.0f, 10.0f, lpcCaption, lxFlags);
            mpMenu = lpMenu;
        }

        Menu* MenuWindow::GetMenu() const { return mpMenu; }

        // X360 0x82830C68. Handle menu navigation, recompute the visible row layout, then update the
        // selected row last with the real event (all other rows receive NONE).
        void MenuWindow::Update(f32 lfTimeStep, InputEvent leEvent)
        {
            switch (leEvent)
            {
            case E_INPUTEVENT_BACK:
                if (IsPinned())
                {
                    if (!IsModal())
                    {
                        Window* lpNext = GetUI().GetNextActiveWindow(this);
                        if (lpNext)
                            GetUI().SetActiveWindow(lpNext);
                    }
                }
                else
                {
                    Menu* lpParent = mpMenu->GetParent();
                    GetUI().GetMenuManager().Close(mpMenu);
                    if (lpParent)
                        GetUI().GetMenuManager().Open(lpParent);
                }
                return;

            case E_INPUTEVENT_CLOSE:
                if (IsPinned())
                {
                    if (!IsModal())
                    {
                        Window* lpNext = GetUI().GetNextActiveWindow(this);
                        if (lpNext)
                            GetUI().SetActiveWindow(lpNext);
                    }
                }
                else
                {
                    GetUI().GetMenuManager().Close(mpMenu);
                    return;
                }
                break;

            case E_INPUTEVENT_CURSORUP:
                mpMenu->SelectPreviousMenuItem();
                break;
            case E_INPUTEVENT_CURSORDOWN:
                mpMenu->SelectNextMenuItem();
                break;
            default:
                break;
            }

            const Metrics& lrMetrics = GetMetrics();
            const f32 lfBorderWidth = lrMetrics.mfWindowBorderSize * 2.0f;
            f32 lfWidth = 0.0f;
            f32 lfHeight = 0.0f;
            bool lbHasItems = false;

            for (MenuItem* lpItem = mpMenu->mMenuItems.GetFirst(); lpItem;
                 lpItem = mpMenu->mMenuItems.GetNext(lpItem))
            {
                if (!lpItem->IsVisible())
                    continue;
                lbHasItems = true;
                lpItem->ComputeSize();
                lfHeight += lpItem->GetHeight();
                const f32 lfItemWidth = lpItem->GetWidth() + lfBorderWidth;
                if (lfWidth < lfItemWidth)
                    lfWidth = lfItemWidth;
            }

            if (!lbHasItems)
            {
                lfWidth = Get2DRenderer()->CalcTextWidth("(empty)", lrMetrics.mfTextSize) + lfBorderWidth;
                lfHeight = lrMetrics.mfTextSize;
            }

            if (GetCaption() && (GetFlags() & KX_FLAGNOCAPTION) == 0)
            {
                const f32 lfCaptionWidth = Get2DRenderer()->CalcTextWidth(GetCaption(), lrMetrics.mfTextSize)
                                         + lfBorderWidth;
                if (lfWidth < lfCaptionWidth)
                    lfWidth = lfCaptionWidth;
            }

            if (lfWidth < GetWidth() && GetWidth() - lfWidth < 8.0f)
                lfWidth = GetWidth();
            SetSize(lfWidth, lfHeight + lfBorderWidth);

            for (MenuItem* lpItem = mpMenu->mMenuItems.GetFirst(); lpItem;
                 lpItem = mpMenu->mMenuItems.GetNext(lpItem))
                if (lpItem != mpMenu->mpCurrentMenuItem)
                    lpItem->Update(lfTimeStep, E_INPUTEVENT_NONE);

            if (mpMenu->mpCurrentMenuItem)
                mpMenu->mpCurrentMenuItem->Update(lfTimeStep, leEvent);
        }

        // X360 0x828297F0. Window chrome first, followed by each visible menu row at the client inset.
        void MenuWindow::Render(Debug2DImmediateRender* lpRender)
        {
            Window::Render(lpRender);
            // FLAG PC-platform leaf: dump root labels once for the opt-in UI harness.
            static bool lbDumped = false;
            if (!lbDumped && std::getenv("BRN_DEBUG_UI_TRACE") && !mpMenu->GetParent())
            {
                lbDumped = true;
                for (MenuItem* lpItem = mpMenu->mMenuItems.GetFirst(); lpItem;
                     lpItem = mpMenu->mMenuItems.GetNext(lpItem))
                {
                    char lacName[256] = {};
                    char lacTrace[512];
                    lpItem->GetDisplayName(lacName, sizeof(lacName));
                    const char* lpcType = typeid(*lpItem).name();
                    if (MenuItemFunction* lpFunctionItem = dynamic_cast<MenuItemFunction*>(lpItem))
                    {
                        Function* lpFunction = lpFunctionItem->GetFunction();
                        if (!lpFunctionItem->IsUseful())
                            lpcType = typeid(*static_cast<CgsDev::DebugComponent*>(lpFunction->GetParameter())).name();
                    }
                    std::snprintf(lacTrace, sizeof(lacTrace), "[debug-menu-row] type=%s name=\"%s\"\n", lpcType, lacName);
                    CgsDev::Log::WriteToLog(lacTrace);
                }
            }

            const Metrics& lrMetrics = GetMetrics();
            const f32 lfBorder = lrMetrics.mfWindowBorderSize;
            const f32 lfX = GetX() + lfBorder;
            f32 lfY = GetY() + lfBorder;

            if (mpMenu->mMenuItems.IsEmpty())
            {
                lpRender->DrawText("(empty)", lfX, lfY, lrMetrics.mfTextSize,
                                   GetPalette().mColourText);
                return;
            }

            const f32 lfItemWidth = GetWidth() - lfBorder * 2.0f;
            for (MenuItem* lpItem = mpMenu->mMenuItems.GetFirst(); lpItem;
                 lpItem = mpMenu->mMenuItems.GetNext(lpItem))
            {
                if (!lpItem->IsVisible())
                    continue;
                lpItem->Render(lpRender, lfX, lfY, lpItem == mpMenu->mpCurrentMenuItem, lfItemWidth);
                lfY += lpItem->GetHeight();
            }
        }

        void MenuWindow::GetMenuPath(char* lpcBuffer, s32 liBufferLen)
        {
            mpMenu->GetPath(lpcBuffer, liBufferLen);
        }
        // X360 0x82816560: forward to the open menu's selected row. If the menu has a current item
        // it tail-calls that item's GetItemString (Menu::GetSelectedItemString does exactly this);
        // otherwise it writes an empty string. No buffer guard in the binary.
        void MenuWindow::GetSelectedItemString(char* lpcBuffer, s32 liBufferLen) const
        {
            mpMenu->GetSelectedItemString(lpcBuffer, liBufferLen);
        }
    }
}
