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
        struct MenuItem;
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
            Window* Open(const char* lpcPath, Menu* lpParent);
            void    Close(Menu* lpMenu);
            void    ShowMainMenu();
            MenuWindow* FindMenuWindow(const Menu* lpMenu) const;
            Menu* FindMenu(const MenuItem* lpMenuItem) const;
            void AddMenuItem(const char* lpcPath, MenuItem* lpMenuItem);
            void RemoveMenuItem(MenuItem* lpMenuItem);
            void RemoveMenuItemFromMenu(Menu* lpMenu, MenuItem* lpMenuItem);
            bool IsMenuItemAdded(MenuItem* lpMenuItem) const;
            Menu* CreateMenuPath(const char* lpcPath, Menu* lpParent);
            Menu* GetMenuFromPath(const char* lpcPath, Menu* lpParent);
            void GetMenuItemPath(const MenuItem* lpMenuItem, char* lpcPathOut, s32 liSize);
            void ReplaceMenuItem(MenuItem* lpOld, MenuItem* lpNew);
            void MoveItemAfter(MenuItem* lpItemReference, MenuItem* lpItemToMove);
            Window* OpenWindowFromPath(const char* lpcPath);

        private:
            void DeleteMenu(Menu* lpMenu);
            Menu* CreateMenu(const char* lpcName, Menu* lpParent);
            MenuWindow* CreateMenuWindow(Menu* lpMenu);
            Menu* FindSubMenu(const char* lpcName, Menu* lpParent);
            void EnsureFreeMenuWindow();
            void SplitPath(const char* lpcMenuItemPath, char* lpcPath, char* lpcMenuItem, s32 liSize);
        };
    }
}
