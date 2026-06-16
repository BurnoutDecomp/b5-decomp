#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsMenuItemFunction.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

// CgsDev::DebugUI::MenuItemFunction - the manager-path bodies (Prepare + the function accessor). The
// render/update/size virtuals are the render follow-on; declared in the header, not defined here.

namespace CgsDev
{
    namespace DebugUI
    {
        MenuItemFunction::MenuItemFunction()
            : mpFunction(nullptr)
        {
        }

        // X360 0x82822E58: bind the row to its Function.
        void MenuItemFunction::Prepare(Function* lpFunction)
        {
            CGS_ASSERT(lpFunction, "lpFunction");
            mpFunction = lpFunction;
        }

        Function* MenuItemFunction::GetFunction()
        {
            return mpFunction;
        }
    }
}
