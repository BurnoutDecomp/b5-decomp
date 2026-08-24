#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

// CgsDev::Internal::DebugInternal accessor bodies (X360 GetUI @ 0x82815F08).
//
// DebugInternal is an empty mixin (see header); its accessors expose the process-wide debug
// singletons - the debug manager, the debug UI, and the debug resource allocator - to every
// debug class that derives from it. On the X360 each accessor reads straight through
// DebugManager::mpInstance (GetUI is `lwz mpInstance; lwz r3, 0x140(r11)` - the singleton's mpUI
// member), so no separate wiring step exists; the same raw reads are modelled here by name
// (DebugInternal is a friend of DebugManager).

namespace CgsDev
{
    namespace Internal
    {
        CgsDev::DebugManager& DebugInternal::GetDebugManager()
        {
            return *DebugManager::mpInstance;
        }

        // X360 @0x82815F08: mpInstance->mpUI (+0x140).
        CgsDev::DebugUI::DebugUI& DebugInternal::GetUI()
        {
            return *DebugManager::mpInstance->mpUI;
        }

        // mpInstance->mpAllocator (+0x8170) - the allocator DebugManager::Construct latched.
        rw::IResourceAllocator* DebugInternal::GetAllocator()
        {
            return DebugManager::mpInstance->mpAllocator;
        }
    }
}
