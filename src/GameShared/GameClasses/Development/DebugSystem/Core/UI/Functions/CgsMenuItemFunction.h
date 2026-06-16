#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.h"

// CgsDev::DebugUI::MenuItemFunction - the menu row that invokes a registered Function when selected.
// Recovered from the DecFIGS DWARF (Development/DebugSystem/Core/UI/Functions/CgsMenuItemFunction.h).
// FunctionManager pools these and pairs each with the Function it calls.

namespace CgsDev
{
    struct Debug2DImmediateRender;

    namespace DebugUI
    {
        struct Function;

        struct MenuItemFunction : public MenuItem
        {
            MenuItemFunction();

            void Prepare(Function* lpFunction);

            virtual void Update(f32 lfTimeStep, InputEvent leEvent);
            virtual void Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, bool lbSelected, f32 lfAlpha);
            virtual void ComputeSize();
            virtual bool IsUseful() const;
            virtual void GetDisplayName(char* lpcBuffer, s32 liBufferLen) const;
            virtual void GetItemString(char* lpcBuffer, s32 liBufferLen) const;

            Function* GetFunction();

        protected:
            Function* mpFunction;
        };
    }
}
