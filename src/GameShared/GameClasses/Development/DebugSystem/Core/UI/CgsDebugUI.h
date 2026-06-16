#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunctionManager.h"

// CgsDev::DebugUI::DebugUI - the singleton hub of the whole debug menu: the window stack, the
// shared palette/metrics/controller, and the three managers (menu / variable / function) every
// DebugComponent talks to via GetUI(). Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Core/UI/CgsDebugUI.h).
//
// INCREMENTAL LAYOUT: the seven heavy by-value members the DWARF lists between/around the managers
// - Palette mPalette, Metrics mMetrics, DebugController mController, Console mConsole, ErrorWindow
// mErrorWindow, ScriptInterface mScriptInterface, CommandWindow mCommandWindow - are their own
// debug-UI subsystems and are NOT modelled yet; only the members the current consumers need (the
// window list, the cascade/visibility scalars, the 2D renderer pointer, and the three managers)
// are declared. The full method surface is declared so callers compile against the real interface;
// the deferred-member accessors (GetPalette/GetMetrics/GetConsole/...) are declared but defined in
// the DebugUI TU once those members are reconstructed. Exact member offsets are therefore not
// modelled (semantic parity: DebugComponent reaches the managers through GetXManager(), not a raw
// offset). DebugComponent's MakeFullPath uses SafeStringCat; the Register* paths use GetVariableManager
// / GetFunctionManager.

namespace CgsDev
{
    class DebugManager;
    class Debug2DImmediateRender;
    struct DebugManagerConstructParameters;

    namespace DebugUI
    {
        struct Window;
        struct Palette;
        struct Metrics;
        struct DebugController;
        struct Console;
        struct ErrorWindow;
        struct ScriptInterface;
        struct CommandWindow;
        struct DebugManagerPad;

        enum DockEdge
        {
            E_DOCKEDGE_TOP = 0,
            E_DOCKEDGE_BOTTOM = 1,
            E_DOCKEDGE_LEFT = 2,
            E_DOCKEDGE_RIGHT = 3,
        };

        struct DebugUI
        {
        private:
            Window*                          mpActiveWindow;
            Internal::DebugLinkedList<Window> mWindowList;
            f32                              mfCascadeX;
            f32                              mfCascadeY;
            bool                             mbVisible;
            bool                             mbRunAutoExec;
            Debug2DImmediateRender*          mp2dRender;
            // -- deferred heavy members (see header note): Palette, Metrics, DebugController here --
            MenuManager                      mMenuManager;
            VariableManager                  mVariableManager;
            FunctionManager                  mFunctionManager;
            // -- deferred heavy members (see header note): Console, ErrorWindow, ScriptInterface,
            //    CommandWindow follow --

        public:
            void Construct(const DebugManagerConstructParameters* lpParameters);
            void Update(f32 lfTimeStep);
            void Destruct();

            void SetGamePad(DebugManagerPad* lpPad);
            void Render();

            void          AddWindow(Window* lpWindow);
            void          RemoveWindow(Window* lpWindow);
            bool          IsWindowAdded(Window* lpWindow);
            void          SetActiveWindow(Window* lpWindow);
            const Window* GetActiveWindow() const;
            void          GetCascadePosition(const Window* lpWindow, f32& lrfX, f32& lrfY);
            void          DockWindow(Window* lpWindow, DockEdge leEdge);

            const Palette&   GetPalette() const;
            const Metrics&   GetMetrics() const;
            MenuManager&     GetMenuManager();
            VariableManager& GetVariableManager();
            FunctionManager& GetFunctionManager();
            Console&         GetConsole();
            ScriptInterface& GetScriptInterface();
            CommandWindow&   GetCommandWindow();
            DebugController&  GetController();
            bool             IsVisible();

            void ShowErrorMessage(const char* lpcMessage);
            void SafeStringCopy(char* lpcBuffer, const char* lpcSource, s32 liBufferLen);
            void SafeStringCat(char* lpcBuffer, const char* lpcSource, s32 liBufferLen);
            void SetMetrics(const Metrics& lrMetrics);
            void SetPalette(const Palette& lrPalette);

            Debug2DImmediateRender* const Get2DRenderer() const;
            void                          Set2DRenderer(Debug2DImmediateRender* lpRender);

        private:
            void    UpdateCascadePosition(bool lbReset);
            bool    HasModalWindow() const;
            Window* GetNextWindow(Window* lpWindow);
            Window* GetPreviousWindow(Window* lpWindow);
            Window* GetNextActiveWindow(Window* lpWindow);
            Window* GetPreviousActiveWindow(Window* lpWindow);
            DebugManager& GetDebugManager() const;
        };
    }
}
