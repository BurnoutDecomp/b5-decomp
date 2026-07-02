#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsMenuItemVariable.h"

#include <stdio.h>   // snprintf
#include <string.h>  // strncpy

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"  // Variable::Get*/edit

// CgsDev::DebugUI::MenuItemVariable - the manager-path bodies (Prepare + the variable accessor) and
// the variable-row render/update/size virtuals that draw + edit the row's bound Variable. The row
// displays "<name>: <value>" and edits the Variable on the left/right/select input events.

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

        // X360 0x8281A088: dispatch the input event to the bound Variable's edit ops (tail-called).
        void MenuItemVariable::Update(f32 /*lfTimeStep*/, InputEvent leEvent)
        {
            switch (leEvent)
            {
                case E_INPUTEVENT_SELECT:      mpVariable->Select();    break;
                case E_INPUTEVENT_CURSORLEFT:  mpVariable->Decrement(); break;
                case E_INPUTEVENT_CURSORRIGHT: mpVariable->Increment(); break;
                default:                                                break;
            }
        }

        // X360 0x8282EB10: draw "<name>: <value>" via the shared MenuItem text renderer.
        void MenuItemVariable::Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, bool lbSelected, f32 lfAlpha)
        {
            char lacValue[100];
            mpVariable->GetValueAsString(lacValue, 100);

            char lacText[256];
            snprintf(lacText, sizeof(lacText), "%s: %s", mpVariable->GetName(), lacValue);

            RenderMenuItemText(lpRender, lacText, lfX, lfY, GetWidth(), GetHeight(), lbSelected, lfAlpha);
        }

        // X360 0x8282EBA8: size the row from its "<name>: <value>" label.
        void MenuItemVariable::ComputeSize()
        {
            char lacValue[100];
            mpVariable->GetValueAsString(lacValue, 100);

            char lacText[256];
            snprintf(lacText, sizeof(lacText), "%s: %s", mpVariable->GetName(), lacValue);

            ComputeSizeFromText(lacText);
        }

        bool MenuItemVariable::IsUseful() const { return true; }

        // X360 0x8281A0C0: a variable row is visible unless its E_TYPE_VISIBLE metadata says otherwise.
        bool MenuItemVariable::IsVisible() const
        {
            return mpVariable->IsVisible();
        }

        // X360 0x8282EC38: copy "<name>: <value>" into the caller buffer (truncating).
        void MenuItemVariable::GetDisplayName(char* lpcBuffer, s32 liBufferLen) const
        {
            char lacValue[100];
            mpVariable->GetValueAsString(lacValue, 100);

            char lacText[256];
            snprintf(lacText, sizeof(lacText), "%s: %s", mpVariable->GetName(), lacValue);

            if (liBufferLen > 1)
            {
                strncpy(lpcBuffer, lacText, liBufferLen - 1);
                lpcBuffer[liBufferLen - 1] = '\0';
            }
            else
            {
                lpcBuffer[0] = '\0';
            }
        }

        // X360 0x82816590: copy the bound variable's name into the caller buffer (truncating).
        void MenuItemVariable::GetItemString(char* lpcBuffer, s32 liBufferLen) const
        {
            const char* lpcName = mpVariable->GetName();
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
