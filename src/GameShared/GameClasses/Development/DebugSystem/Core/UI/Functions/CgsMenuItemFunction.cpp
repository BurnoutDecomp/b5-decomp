#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsMenuItemFunction.h"

#include <string.h>  // strncpy

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunction.h"  // Function::Get*

// CgsDev::DebugUI::MenuItemFunction - the manager-path bodies (Prepare + the function accessor) and
// the function-row render/update/size virtuals that drive the row's bound Function.

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

        // X360 0x82816310: on a select event, invoke the bound Function (inlined Function::Select()).
        void MenuItemFunction::Update(f32 /*lfTimeStep*/, InputEvent leEvent)
        {
            if (leEvent == E_INPUTEVENT_SELECT)
            {
                Function::DebugCallbackFunction lpfCallback = mpFunction->GetFunction();
                if (lpfCallback)
                    lpfCallback(mpFunction->GetParameter());
            }
        }

        // X360 0x8282E8B8: draw the row's label via the shared MenuItem text renderer.
        void MenuItemFunction::Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, bool lbSelected, f32 lfAlpha)
        {
            RenderMenuItemText(lpRender, mpFunction->GetName(), lfX, lfY, GetWidth(), GetHeight(), lbSelected, lfAlpha);
        }

        // X360 0x8282E8F0: size the row from its label (inlined MenuItem::ComputeSizeFromText).
        void MenuItemFunction::ComputeSize()
        {
            ComputeSizeFromText(mpFunction->GetName());
        }

        // X360 0x82832240: section-header callbacks identify hierarchy rows, not invokable
        // actions, and are the only registered functions filtered out here.
        bool MenuItemFunction::IsUseful() const
        {
            return mpFunction->GetFunction() != &CgsDev::DebugComponent::DebugUISectionCallback;
        }

        void MenuItemFunction::GetDisplayName(char* lpcBuffer, s32 liBufferLen) const
        {
            const char* lpcName = mpFunction->GetName();
            if (lpcName && liBufferLen > 1)
            {
                strncpy(lpcBuffer, lpcName, liBufferLen - 1);
                lpcBuffer[liBufferLen - 1] = '\0';
            }
            else if (liBufferLen > 0)
                lpcBuffer[0] = '\0';
        }

        // X360 0x828163A8: copy the bound function's name into the caller buffer (truncating).
        void MenuItemFunction::GetItemString(char* lpcBuffer, s32 liBufferLen) const
        {
            const char* lpcName = mpFunction->GetName();
            if (lpcName && liBufferLen > 1)
            {
                strncpy(lpcBuffer, lpcName, liBufferLen - 1);
                lpcBuffer[liBufferLen - 1] = '\0';
            }
            else
            {
                lpcBuffer[0] = '\0';
            }
        }
    }
}
