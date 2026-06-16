#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunctionManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"           // DebugManagerConstructParameters (pool sizes + allocator)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"             // GetUI().GetMenuManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"           // Menu::AddMenuItem
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsMenuItemFunction.h"  // MenuItemFunction::Prepare
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT

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

        // --- function-edit setters: FindFunction + edit is the function-edit follow-on. Stubbed so
        // the subsystem links; dead in the loading build (no function is unregistered/renamed during
        // loading). ---
        void FunctionManager::UnregisterFunction(Function::DebugCallbackFunction, void*) {}
        void FunctionManager::SetFunctionName(Function::DebugCallbackFunction, void*, const char*) {}
    }
}
