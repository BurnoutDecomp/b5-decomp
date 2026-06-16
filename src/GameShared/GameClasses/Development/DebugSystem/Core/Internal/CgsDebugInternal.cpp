#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

// CgsDev::Internal::DebugInternal accessor bodies (X360 GetUI @ 0x82815F08).
//
// DebugInternal is an empty mixin (see header); its accessors expose the process-wide debug
// singletons - the debug manager, the debug UI, and the debug resource allocator - to every
// debug class that derives from it. The singletons are wired up by the debug system at
// construction (CgsDebugManager's TU). The X360 GetUI read the UI pointer out of the debug-
// system singleton (a raw singleton+0x50 fetch); it is modelled here as the cached UI singleton
// so the access is by name rather than by offset.

namespace
{
    CgsDev::DebugManager*     gpDebugManager   = nullptr;
    CgsDev::DebugUI::DebugUI* gpDebugUI        = nullptr;
    rw::IResourceAllocator*   gpDebugAllocator = nullptr;
}

namespace CgsDev
{
    namespace Internal
    {
        CgsDev::DebugManager& DebugInternal::GetDebugManager()
        {
            return *gpDebugManager;
        }

        CgsDev::DebugUI::DebugUI& DebugInternal::GetUI()
        {
            return *gpDebugUI;
        }

        rw::IResourceAllocator* DebugInternal::GetAllocator()
        {
            return gpDebugAllocator;
        }
    }
}
