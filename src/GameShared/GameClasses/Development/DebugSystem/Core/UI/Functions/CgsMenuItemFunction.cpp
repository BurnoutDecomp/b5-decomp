#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsMenuItemFunction.h"

#include <string.h>  // strncpy

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
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

        // X360 0x82832240: a function row is useful unless its callback is
        // DebugComponent::DebugUISectionCallback (the auto-generated section-header row). That
        // callback is a *private static* of DebugComponent (CgsDebugComponent.h) with no public
        // accessor and MenuItemFunction is not a friend, so this comparison cannot be expressed
        // without a cross-TU change to that header. Left as the committed stub pending that edit.
        bool MenuItemFunction::IsUseful() const { return true; }

        void MenuItemFunction::GetDisplayName(char* lpcBuffer, s32 liBufferLen) const { if (liBufferLen > 0) lpcBuffer[0] = '\0'; }

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
