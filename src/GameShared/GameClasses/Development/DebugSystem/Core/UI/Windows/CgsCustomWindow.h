#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.h"

// CgsDev::DebugUI::CustomWindow - a debug Window that also exposes itself as a menu row, so the user
// can open it from the debug menu tree. It owns a CustomWindowMenuItem (a MenuItem whose OpenAsWindow
// returns the owning window). Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Core/UI/Windows/CgsCustomWindow.h). Bodies of the (declared-only) methods
// live in their own TUs; only the layout + the trivial ctors needed to construct derived windows
// (LogWindow) are defined here.

namespace CgsDev
{
    struct Debug2DImmediateRender;

    namespace DebugUI
    {
        // The menu row that opens its owning Window. mpWindow points back at the CustomWindow it fronts.
        struct CustomWindowMenuItem : public MenuItem
        {
            CustomWindowMenuItem() : mpWindow(0) {}

            void Prepare(Window* lpWindow);

            virtual void    Update(f32 lfTimeStep, InputEvent leEvent) ;
            virtual void    Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, bool lbSelected, f32 lfAlpha) ;
            virtual void    ComputeSize() ;
            virtual bool    IsUseful() const ;
            virtual void    GetDisplayName(char* lpcBuffer, s32 liBufferLen) const ;
            virtual Window* OpenAsWindow() ;

        protected:
            Window* mpWindow;   // the window this row opens
        };

        struct CustomWindow : public Window
        {
            CustomWindow() : mMenuItem() {}

            void Prepare(f32 lfWidth, f32 lfHeight, const char* lpcCaption, const char* lpcMenuPath, s32 lxFlags);

            virtual void Update(f32 lfTimeStep, InputEvent leEvent) ;

            void Register(const char* lpcMenuPath);
            void Unregister();

        protected:
            virtual void GetMenuPath(char* lpcBuffer, s32 liBufferLen) ;

            CustomWindowMenuItem mMenuItem;   // the menu row that opens this window
        };
    }
}
