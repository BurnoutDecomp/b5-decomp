#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"      // DebugManagerConstructParameters (pool sizes + allocator)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"      // Menu (Prepare/AddMenuItem/GetParent/GetCaption)

#include <string.h>  // _stricmp

// CgsDev::DebugUI::MenuManager - the menu-path builder the variable/function managers call before
// registering an item. CreateMenuPath walks a '/'- or '\'-separated path and find-or-creates each
// segment; FindSubMenu matches by (parent, caption) over the pooled menus; CreateMenu allocates +
// prepares a new node and links it under its parent. Grounded in the X360 (CreateMenuPath 0x82829650,
// FindSubMenu 0x82819D20); CreateMenu is reconstructed from that call context + FindSubMenu's lookup
// (a created menu must be in the pool's active list with the right parent+caption to be found next
// time, and linked under its parent to display).
//
// The remaining MenuManager surface (Construct/Destruct/Open/Close/ShowMainMenu) is the
// window/lifecycle follow-on.

namespace CgsDev
{
    namespace DebugUI
    {
        // X360 CgsMenuManager.cpp:59 (bounded). Size the menu pool from the construct parameters and
        // Clear it to fill the free list. The root menu (mpMainMenu) is NOT allocated here - the X360
        // Construct shows no pool Allocate; it is created in the deferred menu-tree bring-up. Until then
        // mpMainMenu stays null, which CreateMenuPath tolerates (a null root resolves new segments as
        // top-level menus with a null parent - no dereference of the root).
        //
        // mWindowPool serves the menu-DISPLAY path (Open/CreateMenuWindow), which the bounded build
        // defers: MenuWindow has no reconstructed body yet (CgsMenuWindow.cpp), and the perfmon HUD
        // opens no menu window. Constructing it here would placement-construct MenuWindow() and pull in
        // the deferred Window/MenuWindow ctors + vtables. It is left zero-initialised (the global UI
        // instance zero-inits it to a valid empty pool) until that path is reconstructed.
        void MenuManager::Construct(const DebugManagerConstructParameters* lpParameters)
        {
            rw::IResourceAllocator* lpAllocator = lpParameters->mpRwAllocator;

            mMenuPool.Construct(lpParameters->miMenuPoolSize, lpAllocator);
            mMenuPool.Clear();

            mpMainMenu = nullptr;
        }

        // X360 CgsMenuManager.cpp:86 is empty (the debug allocator owns the pool backing).
        void MenuManager::Destruct() {}

        Menu* MenuManager::CreateMenuPath(const char* lpcPath, Menu* lpParent)
        {
            if (!lpcPath)
                return mpMainMenu;

            Menu* lpCurrentMenu = lpParent ? lpParent : mpMainMenu;

            const char* lpcSegment = lpcPath;
            while (*lpcSegment)
            {
                // Advance to the next separator (or the end of the string).
                const char* lpcEnd = lpcSegment;
                while (*lpcEnd && *lpcEnd != '\\' && *lpcEnd != '/')
                    ++lpcEnd;

                const s32 liLength = static_cast<s32>(lpcEnd - lpcSegment);
                if (liLength > 0)
                {
                    char acName[256];
                    s32  liIndex = 0;
                    for (; liIndex < liLength && liIndex < 255; ++liIndex)
                        acName[liIndex] = lpcSegment[liIndex];
                    acName[liIndex] = '\0';

                    Menu* lpSubMenu = FindSubMenu(acName, lpCurrentMenu);
                    if (lpSubMenu)
                    {
                        lpCurrentMenu = lpSubMenu;
                    }
                    else
                    {
                        lpCurrentMenu = CreateMenu(acName, lpCurrentMenu);
                        if (!lpCurrentMenu)
                            return nullptr;
                    }
                }

                if (*lpcEnd)
                    ++lpcEnd;
                lpcSegment = lpcEnd;
            }

            return lpCurrentMenu;
        }

        // X360 0x82819D20: scan the pooled menus for one whose parent + caption match.
        Menu* MenuManager::FindSubMenu(const char* lpcName, Menu* lpParent)
        {
            const s32 liCount = mMenuPool.GetActiveCount();
            for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
            {
                Menu* lpMenu = mMenuPool.GetActiveAt(liIndex);
                if (lpMenu->GetParent() == lpParent && _stricmp(lpMenu->GetCaption(), lpcName) == 0)
                    return lpMenu;
            }
            return nullptr;
        }

        Menu* MenuManager::CreateMenu(const char* lpcName, Menu* lpParent)
        {
            Menu* lpMenu = mMenuPool.Allocate();
            if (!lpMenu)
                return nullptr;

            lpMenu->Prepare(lpcName, lpParent);
            if (lpParent)
                lpParent->AddMenuItem(lpMenu);

            return lpMenu;
        }
    }
}
