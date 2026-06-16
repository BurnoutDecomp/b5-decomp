#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"

// CgsDev::DebugUI::MenuManager - owns the debug menu tree: pools the Menu nodes + their MenuWindow
// frames, keeps the root (mpMainMenu), and opens/closes menus into on-screen windows. Recovered
// from the DecFIGS DWARF (Development/DebugSystem/Core/UI/Menu/CgsMenuManager.h). Menu / MenuWindow
// / Window are pooled/returned by pointer and forward-declared; the deeper navigation surface and
// the method bodies follow with the menu-tree reconstruction. This header gives DebugUI a complete
// by-value MenuManager member to lay out.

namespace CgsDev
{
    struct DebugManagerConstructParameters;

    namespace DebugUI
    {
        struct Menu;
        struct MenuWindow;
        struct Window;

        struct MenuManager : public Internal::DebugInternal
        {
        private:
            Internal::DebugStaticPool<Menu>       mMenuPool;
            Internal::DebugStaticPool<MenuWindow> mWindowPool;
            Menu*                                 mpMainMenu;

        public:
            void    Construct(const DebugManagerConstructParameters* lpParameters);
            void    Destruct();
            Window* Open(Menu* lpMenu);
            Window* Open(const char* lpcTitle, Menu* lpMenu);
            void    Close(Menu* lpMenu);
            void    ShowMainMenu();

            // Walk a '/'- or '\'-separated path, finding-or-creating each menu segment, and return
            // the leaf menu (the variable/function managers register their items into it). A null
            // path or parent resolves to mpMainMenu. X360 CreateMenuPath 0x82829650.
            Menu* CreateMenuPath(const char* lpcPath, Menu* lpParent);

        private:
            Menu* FindSubMenu(const char* lpcName, Menu* lpParent);  // X360 0x82819D20
            Menu* CreateMenu(const char* lpcName, Menu* lpParent);
        };
    }
}
