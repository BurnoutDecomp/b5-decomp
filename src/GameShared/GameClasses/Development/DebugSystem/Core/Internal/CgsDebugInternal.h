#pragma once

#include "types.hpp"

// CgsDev::Internal::DebugInternal - the lightweight shared base for the whole debug system
// (CgsDev::DebugComponent and the DebugUI::Window / MenuItem / MenuManager / FunctionManager /
// ScriptInterface / VariableManager families all derive from it). It holds NO data of its own;
// it simply exposes the debug singletons - the debug manager, the debug UI, and the debug
// resource allocator - to every derived class through protected accessors. Recovered from the
// DecFIGS DWARF (Development/DebugSystem/Core/Internal/CgsDebugInternal.h).

namespace rw { class IResourceAllocator; }

namespace CgsDev
{
    class DebugManager;

    namespace DebugUI
    {
        struct DebugUI;
    }

    namespace Internal
    {
        struct DebugInternal
        {
        protected:
            // Accessors to the process-wide debug singletons (wired up by the debug system at
            // construction; see CgsDebugInternal.cpp). GetUI is the X360 0x82815F08 accessor.
            CgsDev::DebugManager&     GetDebugManager();
            CgsDev::DebugUI::DebugUI& GetUI();
            rw::IResourceAllocator*   GetAllocator();
        };

        // Wire the process-wide debug singletons the accessors above hand out. Called once by
        // DebugManager::Construct, before any RegisterVariable/RegisterFunction (X360: the accessors
        // read these out of the debug-system singleton; modelled here as cached pointers).
        void SetDebugSingletons(CgsDev::DebugManager* lpManager, CgsDev::DebugUI::DebugUI* lpUI, rw::IResourceAllocator* lpAllocator);
    }
}
