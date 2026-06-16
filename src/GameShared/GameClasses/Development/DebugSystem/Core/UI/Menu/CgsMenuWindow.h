#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"

// CgsDev::DebugUI::MenuWindow - the on-screen window that renders an open Menu. Recovered from the
// DecFIGS DWARF (Development/DebugSystem/Core/UI/Menu/CgsMenuWindow.h). MenuManager pools these and
// pairs each with the Menu it displays.

namespace CgsDev
{
    class Debug2DImmediateRender;

    namespace DebugUI
    {
        struct Menu;

        struct MenuWindow : public Window
        {
            MenuWindow();

            void Prepare(Menu* lpMenu);

            virtual void Update(f32 lfTimeStep, InputEvent leEvent);
            virtual void Render(Debug2DImmediateRender* lpRender);

            Menu* GetMenu() const;

        protected:
            virtual void GetMenuPath(char* lpcBuffer, s32 liBufferLen);
            virtual void GetSelectedItemString(char* lpcBuffer, s32 liBufferLen) const;

            Menu* mpMenu;
        };
    }
}
