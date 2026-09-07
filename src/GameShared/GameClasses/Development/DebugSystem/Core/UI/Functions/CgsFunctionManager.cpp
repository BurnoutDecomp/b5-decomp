#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunctionManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"           // DebugManagerConstructParameters (pool sizes + allocator)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"             // GetUI().GetMenuManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"           // Menu::AddMenuItem
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsMenuItemFunction.h"  // MenuItemFunction::Prepare
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"
#include <string.h>

// CgsDev::DebugUI::FunctionManager::RegisterFunction - X360 0x8282E7E0: resolve the menu path, pull a
// Function + a MenuItemFunction from their pools, hang the row on the menu, fill the function with
// (callback, userData, name), then bind the row to it.
//
// UnregisterFunction/SetFunctionName/CallFunction/FindFunction are the function-edit follow-on.

namespace CgsDev
{
    namespace DebugUI
    {
        // X360 CgsFunctionManager.cpp:61. Size the function + menu-item pools from the construct
        // parameters (1:1 - each registered function gets one menu row), then Clear each to fill the
        // free lists.
        void FunctionManager::Construct(const DebugManagerConstructParameters* lpParameters)
        {
            rw::IResourceAllocator* lpAllocator = lpParameters->mpRwAllocator;

            mFunctionPool.Construct(lpParameters->miFunctionPoolSize, lpAllocator);
            mMenuItemPool.Construct(lpParameters->miFunctionPoolSize, lpAllocator);

            mFunctionPool.Clear();
            mMenuItemPool.Clear();
        }

        // X360 CgsFunctionManager.cpp:83 is empty (the debug allocator owns the pool backing).
        void FunctionManager::Destruct() {}

        void FunctionManager::RegisterFunction(Function::DebugCallbackFunction lpfCallback, void* lpUserData, const char* lpcPath, const char* lpcName)
        {
            Menu* lpMenu = GetUI().GetMenuManager().CreateMenuPath(lpcPath, nullptr);
            if (!lpMenu)
                return;

            Function* lpFunction = mFunctionPool.Allocate();
            if (!lpFunction)
                return;

            MenuItemFunction* lpMenuItem = mMenuItemPool.Allocate();
            CGS_ASSERT(lpMenuItem, "lpMenuItem");

            lpMenu->AddMenuItem(lpMenuItem);
            lpFunction->Prepare(lpfCallback, lpUserData, lpcName);
            lpMenuItem->Prepare(lpFunction);
        }

        Function* FunctionManager::FindFunction(Function::DebugCallbackFunction lpfCallback, void* lpUserData)
        {
            for (s32 i = 0; i < mFunctionPool.GetActiveCount(); ++i)
            {
                Function* f = mFunctionPool.GetActiveAt(i);
                if (f->GetFunction() == lpfCallback && f->GetParameter() == lpUserData)
                    return f;
            }
            return nullptr;
        }

        MenuItemFunction* FunctionManager::FindMenuItem(Function* lpFunction)
        {
            for (s32 liIndex = 0; liIndex < mMenuItemPool.GetActiveCount(); ++liIndex)
            {
                MenuItemFunction* lpItem = mMenuItemPool.GetActiveAt(liIndex);
                if (lpItem->GetFunction() == lpFunction)
                    return lpItem;
            }
            return nullptr;
        }

        Function* FunctionManager::FindFunctionFromPath(const char* lpcPath)
        {
            if (!lpcPath) return nullptr;
            if (*lpcPath == '/') ++lpcPath;
            char path[512];
            for (s32 i = 0; i < mMenuItemPool.GetActiveCount(); ++i)
            {
                MenuItemFunction* item = mMenuItemPool.GetActiveAt(i);
                Menu* menu = GetUI().GetMenuManager().FindMenu(item);
                if (!menu) continue;
                menu->GetPath(path, sizeof(path));
                GetUI().SafeStringCat(path, "/", sizeof(path));
                GetUI().SafeStringCat(path, item->GetFunction()->GetName(), sizeof(path));
                const char* compare = path[0] == '/' ? path + 1 : path;
                if (_stricmp(compare, lpcPath) == 0)
                    return item->GetFunction();
            }
            return nullptr;
        }

        void FunctionManager::CallFunction(const char* lpcPath)
        {
            Function* f = FindFunctionFromPath(lpcPath);
            CGS_ASSERT(f, "lpFunctionToCall");
            if (f) f->Select();
        }

        void FunctionManager::UnregisterFunction(Function::DebugCallbackFunction lpfCallback, void* lpUserData)
        {
            Function* f = FindFunction(lpfCallback, lpUserData);
            if (!f) return;
            for (s32 i = 0; i < mMenuItemPool.GetActiveCount(); ++i)
            {
                MenuItemFunction* item = mMenuItemPool.GetActiveAt(i);
                if (item->GetFunction() == f)
                {
                    GetUI().GetMenuManager().RemoveMenuItem(item);
                    mMenuItemPool.Free(item);
                    break;
                }
            }
            f->Release();
            mFunctionPool.Free(f);
        }

        void FunctionManager::SetFunctionName(Function::DebugCallbackFunction lpfCallback,
                                              void* lpUserData, const char* lpcName)
        {
            Function* f = FindFunction(lpfCallback, lpUserData);
            if (f) f->mpcName = lpcName;
        }
    }
}
