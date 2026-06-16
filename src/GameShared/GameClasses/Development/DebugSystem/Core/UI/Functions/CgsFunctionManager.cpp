#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunctionManager.h"

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
    }
}
