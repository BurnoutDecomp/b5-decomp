#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuWindow.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"

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

        // MenuManager pairs the pooled window with the Menu it displays; the window inherits the
        // menu's caption so the Window caption/border layout reads the menu name.
        void MenuWindow::Prepare(Menu* lpMenu)
        {
            mpMenu = lpMenu;
            if (lpMenu)
                SetCaption(lpMenu->GetCaption());
        }

        Menu* MenuWindow::GetMenu() const { return mpMenu; }

        // --- display virtuals: menu-render follow-on (stubbed for link; dead in the bounded build) ---
        void MenuWindow::Update(f32, InputEvent) {}
        void MenuWindow::Render(Debug2DImmediateRender*) {}
        void MenuWindow::GetMenuPath(char* lpcBuffer, s32 liBufferLen)
        {
            if (lpcBuffer && liBufferLen > 0)
                lpcBuffer[0] = '\0';
        }
        void MenuWindow::GetSelectedItemString(char* lpcBuffer, s32 liBufferLen) const
        {
            if (lpcBuffer && liBufferLen > 0)
                lpcBuffer[0] = '\0';
        }
    }
}
