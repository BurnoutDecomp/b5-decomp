#pragma once

#include "types.hpp"

// CgsDev::Internal::DebugInternal - the lightweight shared base for the whole debug system
// (CgsDev::DebugComponent and the DebugUI::Window / MenuItem / MenuManager / FunctionManager /
// ScriptInterface / VariableManager families all derive from it). It holds NO data of its own;
// it simply exposes the debug singletons - the debug manager, the debug UI, and the debug
// resource allocator - to every derived class through protected accessors. Recovered from the
// DecFIGS DWARF (Development/DebugSystem/Core/Internal/CgsDebugInternal.h).

namespace rw { struct IResourceAllocator; }   // struct -- must match rwcore_structs.h's class-key (MSVC mangling)

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
            // Accessors to the process-wide debug singletons. Each reads straight through
            // DebugManager::mpInstance, exactly as the X360 does (GetUI @0x82815F08 is
            // `mpInstance->mpUI`, a raw singleton+0x140 load; the manager and allocator accessors
            // are the same idiom over mpInstance itself and mpInstance->mpAllocator). Bodies in
            // CgsDebugInternal.cpp (DebugInternal is a friend of DebugManager).
            // GetDebugManager and GetUI are static (DWARF h:63/:67 cannot show it; the callers do):
            // no caller sets r3 for them. ARTIST RaceCarEntityModuleDebugComponent::OnActivate
            // 0x822C2CAC calls GetUI straight after DebugComponent::SetStep, and GetUI 0x82815F08 never
            // reads r3; PS3 DecFIGS calls GetDebugManager from the static callbacks
            // ScriptInterface::ScriptCommand_ActivateComponent (0xB2C6D8) and
            // DebugComponent::DebugUISectionCallback (0xB2D8FC) and from GetUI (0xB178FC) with no
            // object. GetAllocator stays a member: PS3 VariableManager::Construct sets r3 = this before
            // each call (0xB26548, 0xB26878).
            static CgsDev::DebugManager&     GetDebugManager();
            static CgsDev::DebugUI::DebugUI& GetUI();
            rw::IResourceAllocator*   GetAllocator();
        };
    }
}
