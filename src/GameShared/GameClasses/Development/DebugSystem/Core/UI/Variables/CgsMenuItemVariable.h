#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.h"

// CgsDev::DebugUI::MenuItemVariable - the menu row that renders + edits a Variable. Recovered from
// the DecFIGS DWARF (Development/DebugSystem/Core/UI/Variables/CgsMenuItemVariable.h). VariableManager
// pools these and pairs each with the Variable it edits; SetCustomMenuItem swaps in a subclass
// (bar / graph widgets) for richer display.

namespace CgsDev
{
    class Debug2DImmediateRender;

    namespace DebugUI
    {
        struct Variable;

        struct MenuItemVariable : public MenuItem
        {
            MenuItemVariable();

            virtual void Prepare(Variable* lpVariable);

            virtual void Update(f32 lfTimeStep, InputEvent leEvent);
            virtual void Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, bool lbSelected, f32 lfAlpha);
            virtual void ComputeSize();
            virtual bool IsUseful() const;
            virtual bool IsVisible() const;
            virtual void GetDisplayName(char* lpcBuffer, s32 liBufferLen) const;
            virtual void GetItemString(char* lpcBuffer, s32 liBufferLen) const;

            Variable* GetVariable();

        protected:
            Variable* mpVariable;
        };
    }
}
