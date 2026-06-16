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

    private:
        DebugManager* mpDebugManager;
        bool          mbIsAutomaticClass;
    };
}
