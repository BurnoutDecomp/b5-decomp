#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsMenuItemVariable.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

// CgsDev::DebugUI::MenuItemVariable - the manager-path bodies (Prepare + the variable accessor).
// The render/update/size virtuals (they draw + edit the variable via Debug2DImmediateRender and the
// Variable's value formatting) are the render follow-on; declared in the header, not defined here.

namespace CgsDev
{
    namespace DebugUI
    {
        MenuItemVariable::MenuItemVariable()
            : mpVariable(nullptr)
        {
        }

        // X360 0x82822F68: bind the row to its Variable.
        void MenuItemVariable::Prepare(Variable* lpVariable)
        {
            CGS_ASSERT(lpVariable, "lpVariable");
            mpVariable = lpVariable;
        }

        Variable* MenuItemVariable::GetVariable()
        {
            return mpVariable;
        }
    }
}
