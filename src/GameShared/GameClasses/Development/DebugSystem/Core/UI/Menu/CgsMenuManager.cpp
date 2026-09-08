#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"      // DebugManagerConstructParameters (pool sizes + allocator)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"      // Menu (Prepare/AddMenuItem/GetParent/GetCaption)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuWindow.h"

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
        // X360 CgsMenuManager.cpp:59. Build both pools and allocate the "Development" root menu.
        void MenuManager::Construct(const DebugManagerConstructParameters* lpParameters)
        {
            rw::IResourceAllocator* lpAllocator = lpParameters->mpRwAllocator;

            mMenuPool.Construct(lpParameters->miMenuPoolSize, lpAllocator);
            mWindowPool.Construct(lpParameters->miMenuWindowPoolSize, lpAllocator);
            mMenuPool.Clear();
            mWindowPool.Clear();

            mpMainMenu = CreateMenu("Development", nullptr);
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

        MenuWindow* MenuManager::FindMenuWindow(const Menu* lpMenu) const
        {
            for (s32 liIndex = 0; liIndex < mWindowPool.GetActiveCount(); ++liIndex)
            {
                MenuWindow* lpWindow = mWindowPool.GetActiveAt(liIndex);
                if (lpWindow->GetMenu() == lpMenu)
                    return lpWindow;
            }
            return nullptr;
        }

        Menu* MenuManager::FindMenu(const MenuItem* lpMenuItem) const
        {
            for (s32 liIndex = 0; liIndex < mMenuPool.GetActiveCount(); ++liIndex)
            {
                Menu* lpMenu = mMenuPool.GetActiveAt(liIndex);
                if (lpMenu->IsMenuItemAdded(lpMenuItem))
                    return lpMenu;
            }
            return nullptr;
        }

        MenuWindow* MenuManager::CreateMenuWindow(Menu* lpMenu)
        {
            EnsureFreeMenuWindow();
            MenuWindow* lpWindow = mWindowPool.Allocate();
            if (!lpWindow)
                return nullptr;

            lpWindow->Prepare(lpMenu);
            GetUI().AddWindow(lpWindow);
            GetUI().SetActiveWindow(lpWindow);
            return lpWindow;
        }

        Window* MenuManager::Open(Menu* lpMenu)
        {
            MenuWindow* lpWindow = FindMenuWindow(lpMenu);
            if (!lpWindow)
                return CreateMenuWindow(lpMenu);

            GetUI().SetActiveWindow(lpWindow);
            return lpWindow;
        }

        Window* MenuManager::Open(const char* lpcPath, Menu* lpParent)
        {
            Menu* lpMenu = GetMenuFromPath(lpcPath, lpParent);
            return lpMenu ? Open(lpMenu) : nullptr;
        }

        void MenuManager::Close(Menu* lpMenu)
        {
            MenuWindow* lpWindow = FindMenuWindow(lpMenu);
            if (!lpWindow)
                return;
            GetUI().RemoveWindow(lpWindow);
            mWindowPool.Free(lpWindow);
        }

        void MenuManager::ShowMainMenu()
        {
            Open(mpMainMenu);
        }

        void MenuManager::EnsureFreeMenuWindow()
        {
            if (mWindowPool.GetFreeCount() > 0)
                return;

            for (s32 liIndex = 0; liIndex < mWindowPool.GetActiveCount(); ++liIndex)
            {
                MenuWindow* lpWindow = mWindowPool.GetActiveAt(liIndex);
                if (!lpWindow->GetMenu()->IsMenuUseful())
                {
                    Close(lpWindow->GetMenu());
                    return;
                }
            }

            MenuWindow* lpWindow = mWindowPool.GetFirstActive();
            if (lpWindow)
                Close(lpWindow->GetMenu());
        }

        void MenuManager::AddMenuItem(const char* lpcPath, MenuItem* lpMenuItem)
        {
            Menu* lpMenu = CreateMenuPath(lpcPath, nullptr);
            if (lpMenu)
                lpMenu->AddMenuItem(lpMenuItem);
        }

        void MenuManager::DeleteMenu(Menu* lpMenu)
        {
            if (!lpMenu || lpMenu == mpMainMenu || !lpMenu->IsEmpty())
                return;

            Close(lpMenu);
            Menu* lpParent = lpMenu->GetParent();
            if (lpParent)
                lpParent->RemoveMenuItem(lpMenu);
            mMenuPool.Free(lpMenu);

            if (lpParent && lpParent != mpMainMenu && lpParent->IsEmpty())
                DeleteMenu(lpParent);
        }

        void MenuManager::RemoveMenuItem(MenuItem* lpMenuItem)
        {
            Menu* lpMenu = FindMenu(lpMenuItem);
            if (lpMenu)
                RemoveMenuItemFromMenu(lpMenu, lpMenuItem);
        }

        void MenuManager::RemoveMenuItemFromMenu(Menu* lpMenu, MenuItem* lpMenuItem)
        {
            if (!lpMenu || !lpMenu->IsMenuItemAdded(lpMenuItem))
                return;
            lpMenu->RemoveMenuItem(lpMenuItem);
            if (lpMenu != mpMainMenu && lpMenu->IsEmpty())
                DeleteMenu(lpMenu);
        }

        bool MenuManager::IsMenuItemAdded(MenuItem* lpMenuItem) const
        {
            return FindMenu(lpMenuItem) != nullptr;
        }

        Menu* MenuManager::GetMenuFromPath(const char* lpcPath, Menu* lpParent)
        {
            Menu* lpCurrentMenu = lpParent ? lpParent : mpMainMenu;
            if (!lpcPath)
                return lpCurrentMenu;

            const char* lpcSegment = lpcPath;
            while (*lpcSegment)
            {
                while (*lpcSegment == '/' || *lpcSegment == '\\')
                    ++lpcSegment;
                if (!*lpcSegment)
                    break;

                const char* lpcEnd = lpcSegment;
                while (*lpcEnd && *lpcEnd != '/' && *lpcEnd != '\\')
                    ++lpcEnd;

                const s32 liLength = static_cast<s32>(lpcEnd - lpcSegment);
                char acName[256];
                s32 liCopy = liLength < 255 ? liLength : 255;
                memcpy(acName, lpcSegment, static_cast<size_t>(liCopy));
                acName[liCopy] = '\0';
                lpCurrentMenu = FindSubMenu(acName, lpCurrentMenu);
                if (!lpCurrentMenu)
                    return nullptr;
                lpcSegment = lpcEnd;
            }
            return lpCurrentMenu;
        }

        void MenuManager::GetMenuItemPath(const MenuItem* lpMenuItem, char* lpcPathOut, s32 liSize)
        {
            Menu* lpMenu = FindMenu(lpMenuItem);
            if (!lpMenu)
            {
                lpcPathOut[0] = '\0';
                return;
            }

            lpMenu->GetPath(lpcPathOut, liSize);
            GetUI().SafeStringCat(lpcPathOut, "/", liSize);
            char acName[256];
            lpMenuItem->GetDisplayName(acName, sizeof(acName));
            GetUI().SafeStringCat(lpcPathOut, acName, liSize);
        }

        void MenuManager::ReplaceMenuItem(MenuItem* lpOld, MenuItem* lpNew)
        {
            Menu* lpMenu = FindMenu(lpOld);
            if (lpMenu)
                lpMenu->ReplaceMenuItem(lpOld, lpNew);
        }

        void MenuManager::MoveItemAfter(MenuItem* lpItemReference, MenuItem* lpItemToMove)
        {
            Menu* lpTarget = FindMenu(lpItemReference);
            Menu* lpSource = FindMenu(lpItemToMove);
            if (!lpTarget || !lpSource)
                return;
            if (lpTarget != lpSource)
                lpSource->RemoveMenuItem(lpItemToMove);
            lpTarget->AddMenuItemAfter(lpItemReference, lpItemToMove);
        }

        void MenuManager::SplitPath(const char* lpcMenuItemPath, char* lpcPath,
                                    char* lpcMenuItem, s32 liSize)
        {
            const char* lpcSplit = nullptr;
            for (const char* lpc = lpcMenuItemPath; *lpc; ++lpc)
                if (*lpc == '/' || *lpc == '\\')
                    lpcSplit = lpc;

            if (!lpcSplit)
            {
                lpcPath[0] = '\0';
                GetUI().SafeStringCopy(lpcMenuItem, lpcMenuItemPath, liSize);
                return;
            }

            const s32 liPathLength = static_cast<s32>(lpcSplit - lpcMenuItemPath);
            const s32 liCopy = liPathLength < liSize - 1 ? liPathLength : liSize - 1;
            memcpy(lpcPath, lpcMenuItemPath, static_cast<size_t>(liCopy));
            lpcPath[liCopy] = '\0';
            GetUI().SafeStringCopy(lpcMenuItem, lpcSplit + 1, liSize);
        }

        Window* MenuManager::OpenWindowFromPath(const char* lpcPath)
        {
            char acMenuPath[256];
            char acMenuItem[256];
            SplitPath(lpcPath, acMenuPath, acMenuItem, sizeof(acMenuPath));
            Menu* lpMenu = GetMenuFromPath(acMenuPath, nullptr);
            if (!lpMenu)
                return nullptr;
            MenuItem* lpMenuItem = lpMenu->FindMenuItemByName(acMenuItem);
            return lpMenuItem ? lpMenuItem->OpenAsWindow() : nullptr;
        }
    }
}
