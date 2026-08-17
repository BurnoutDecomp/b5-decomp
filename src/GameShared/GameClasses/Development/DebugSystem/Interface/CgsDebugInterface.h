#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunction.h"        // DebugUI::Function::DebugCallbackFunction (RegisterFunction)

// CgsDev::DebugInterface - the lightweight handle the engine passes around to talk to the debug
// system: it wraps a DebugManager pointer (plus an "is automatic" flag marking stack-scoped uses)
// and exposes GetDebugManager / GetUI plus a register/console/render mirror of the DebugComponent
// API. Recovered from the DecFIGS DWARF (Development/DebugSystem/Interface/CgsDebugInterface.h).
//
// INCREMENTAL: only the wrapper itself + the two accessors the component registration path uses
// (GetDebugManager / GetUI) are defined here; the broad register-mirror surface (RegisterVariable /
// RegisterFunction / ConsolePrint / GetRender / ExecuteScript / ...) and the automatic-release dtor
// semantics are the DebugInterface-reconstruction follow-on. DebugComponent::Register constructs one
// of these around the thread-safe-acquired manager and releases the lock explicitly.

namespace CgsDev
{
    namespace DebugUI { struct DebugUI; struct StringList; }
    class DebugRender;   // Get2dRender returns a reference (the buffered debug renderer)

    struct DebugInterface
    {
        // X360 DebugInterface() @ 0x821F1F20 - the AUTOMATIC (stack-scoped) handle: it sets the
        // is-automatic flag, asserts the DebugManager singleton exists, ENTERS the per-manager
        // debug critical section, and grabs the singleton into mpDebugManager. The matching
        // release (when mbIsAutomaticClass) leaves the section. Bodied in CgsDebugInterface.cpp.
        DebugInterface();

        explicit DebugInterface(DebugManager* lpDebugManager)
            : mpDebugManager(lpDebugManager)
            , mbIsAutomaticClass(true)
        {
        }

        ~DebugInterface()
        {
            // Automatic-release semantics (when mbIsAutomaticClass) are deferred; the current
            // caller (DebugComponent::Register) releases the lock explicitly.
        }

        // X360 GetDebugManager @ 0x823A61B0 - assert the manager pointer is set, then return it.
        // Bodied in CgsDebugInterface.cpp (the X360 asserts mpDebugManager, CgsDebugInterface.h:163).
        DebugManager&     GetDebugManager();
        DebugUI::DebugUI& GetUI()           { return mpDebugManager->GetUI(); }

        // Show/hide the on-screen debug console. FLAG: these DebugInterface members
        // are inferred from the ICEWrapper call site (no second in-tree reference),
        // homed in CgsDebugInterface.cpp (DebugInterface::EnableConsole /
        // DisableConsole); the in-game ICE editor toggles the console through these
        // (BrnDirector::ICEWrapper::EditorOn/EditorOff). DECLARATION-ONLY: the bodies
        // belong to the DebugInterface TU and the /c gate does not link.
        void EnableConsole();
        void DisableConsole();

        // The buffered 2D debug renderer this interface draws through (X360-attested,
        // DecFIGS DWARF CgsDebugInterface.h:24 `DebugRender& Get2dRender()`). The debug
        // overlay code (ICERender::RenderPoly / ScrPrintfArg) queues 2D box/text prims
        // here. DECLARATION-ONLY: the body (forwards the manager's buffered renderer)
        // lives in the DebugInterface TU and is the render-mirror follow-on.
        DebugRender& Get2dRender();

        // The buffered 3D (WORLD-space) debug renderer this interface draws through
        // (X360-attested: ICEWidgetTargetBox::Render @0x8252D2B8 calls GetRender then
        // DebugRender::DrawBox to draw a world-space target box). Mirrors Get2dRender
        // but hands back the 3D queue's renderer. DECLARATION-ONLY: the body (forwards
        // the manager's buffered 3D renderer) lives in the DebugInterface TU and is the
        // render-mirror follow-on.
        // FLAG (header grow): GetRender() added here for ICEWidgetTargetBox::Render; the
        //       symbol is a disasm-attested `bl` target in that Render (NOT in
        //       progress/tu_index.json) and has no DWARF here, so the signature is
        //       asm-derived (returns the same DebugRender& as the 2D getter, the 3D
        //       queue being the renderer's second event queue).
        DebugRender& GetRender();

        // ADDITIVE GROW (WorldEntityModule::Construct @0x82302398): the debug-variable
        // register mirror (X360 0x8282E400 bool / 0x8282E3B8 s32 / RegisterVariable f32)
        // with the range/step tuners (0x8282F910 / 0x8282F9B8). Declarations only --
        // bodies belong to the DebugSystem TU (per-TU compile gate).
        // ⭐ ADDED 2026-08-17 (boot audit F-P1-14). The argument order is pinned by two
        // call sites in BrnGameModule::Construct @0x823CAF28/48, which load r4 = the
        // callback, r5 = the context (the game module), r6 = the path ("Debug/Sim"),
        // r7 = the name ("Step" / "Play") -- and it matches, argument for argument, the
        // FunctionManager::RegisterFunction that is already bodied behind it
        // (CgsFunctionManager.cpp, X360 0x8282E7E0). A thin forwarder, exactly like GetUI().
        // Out-of-line: the body needs CgsDebugUI.h, whose GetNextWindow() collides with the
        // Windows.h macro of that name, so it must not be pulled in through this header.
        void RegisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback,
                              void* lpUserData, const char* lpcPath, const char* lpcName);

        void RegisterVariable(bool* lpbVariable, const char* lpcPath, const char* lpcName);
        void RegisterVariable(s32* lpiVariable, const char* lpcPath, const char* lpcName);
        void RegisterVariable(f32* lpfVariable, const char* lpcPath, const char* lpcName);
        void SetRange(s32* lpiVariable, s32 liMin, s32 liMax);
        void SetStep(s32* lpiVariable, s32 liStep);

        // ADDITIVE (attested by BrnWorld::ShadowMap::Construct @0x827B43E8, which tunes its
        // f32 debug variables and sets the shadow-map-type option list; declaration shapes =
        // the DecFIGS DWARF CgsDebugInterface.h:63/:72/:101 overloads). Declarations only --
        // bodies belong to the DebugSystem TU (per-TU compile gate).
        void SetRange(f32* lpfVariable, f32 lfMin, f32 lfMax);
        void SetStep(f32* lpfVariable, f32 lfStep);
        void SetOptions(s32* lpiVariable, const DebugUI::StringList* lpOptions);

    private:
        DebugManager* mpDebugManager;
        bool          mbIsAutomaticClass;
    };
}
