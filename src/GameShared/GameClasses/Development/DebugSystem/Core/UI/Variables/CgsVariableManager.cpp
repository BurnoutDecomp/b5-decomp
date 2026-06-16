#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"             // GetUI().GetMenuManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"           // Menu::AddMenuItem
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"  // Variable::Prepare
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsMenuItemVariable.h"  // MenuItemVariable::Prepare
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                 // gpDebugPrint, gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT

// CgsDev::DebugUI::VariableManager::RegisterVariable - the shared core every typed RegisterVariable
// overload (and DebugComponent, as a friend) funnels into. X360 0x82829A80: resolve the menu path,
// pull a Variable + a MenuItemVariable from their pools, hang the row on the menu, fill the variable
// with the value Variant + name, then bind the row to it. Out-of-pool failures emit the filter-gated
// debug spew and bail.
//
// SetRange/SetStep/SetMetadata/FindVariable + the typed RegisterVariable overloads + the attribute
// setters are the metadata follow-on.

namespace CgsDev
{
    namespace DebugUI
    {
        void VariableManager::RegisterVariable(const Variant& lrVariant, const char* lpcPath, const char* lpcName)
        {
            Menu* lpMenu = GetUI().GetMenuManager().CreateMenuPath(lpcPath, nullptr);
            if (!lpMenu)
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "We've run out of debug Menu Memory.\n";
                return;
            }

            Variable* lpVariable = mVariablePool.Allocate();
            if (!lpVariable)
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "We've run out of debug variable memory.\n";
                return;
            }

            MenuItemVariable* lpMenuItem = mMenuItemPool.Allocate();
            CGS_ASSERT(lpMenuItem, "lpMenuItem");

            lpMenu->AddMenuItem(lpMenuItem);
            lpVariable->Prepare(lrVariant, lpcName);
            lpMenuItem->Prepare(lpVariable);
        }
    }
}
