#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsDev::DebugInterface - the two functions the component-registration path drives:
// the automatic-acquire constructor and the manager accessor.

namespace CgsDev
{
    // Faithful port of X360 DebugInterface() @ 0x821F1F20:
    //   *(this + 4) = 1;                                  // mbIsAutomaticClass = true
    //   if (!mpInstance) { Begin/Fire/EndAssert("mpInstance", CgsDebugManager.h:343); }
    //   DebugCriticalSection::Enter(dword_83019264);      // enter the per-manager debug section
    //   *this = mpInstance;                               // mpDebugManager = the singleton
    //
    // DebugManager::ThreadSafeAquire() IS this assert-mpInstance + enter-section + return-singleton
    // step (X360 ThreadSafeAquire 0x821F1E50), so the acquiring ctor forwards to it; the matching
    // ~DebugInterface release (when mbIsAutomaticClass) leaves the section.
    DebugInterface::DebugInterface()
        : mpDebugManager(DebugManager::ThreadSafeAquire())
        , mbIsAutomaticClass(true)
    {
    }

    // Faithful port of X360 GetDebugManager @ 0x823A61B0:
    //   if (!*this) { Begin/Fire/EndAssert("mpDebugManager", CgsDebugInterface.h:163); }
    //   return *this;
    DebugManager& DebugInterface::GetDebugManager()
    {
        CGS_ASSERT(mpDebugManager, "mpDebugManager");
        return *mpDebugManager;
    }
}
