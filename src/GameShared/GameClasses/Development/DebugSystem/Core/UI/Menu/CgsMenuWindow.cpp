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

        // --- display virtuals: menu-render follow-on (stubbed for link; dead in the bounded build) ---
        void MenuWindow::Update(f32, InputEvent) {}
        void MenuWindow::Render(Debug2DImmediateRender*) {}
        void MenuWindow::GetMenuPath(char* lpcBuffer, s32 liBufferLen)
        {
            if (lpcBuffer && liBufferLen > 0)
                lpcBuffer[0] = '\0';
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
