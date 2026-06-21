#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

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
    namespace DebugUI { struct DebugUI; }
    class DebugRender;   // Get2dRender returns a reference (the buffered debug renderer)

    struct DebugInterface
    {
        DebugInterface()
            : mpDebugManager(nullptr)
            , mbIsAutomaticClass(false)
        {
        }

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

        DebugManager&     GetDebugManager() { return *mpDebugManager; }
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

    private:
        DebugManager* mpDebugManager;
        bool          mbIsAutomaticClass;
    };
}
