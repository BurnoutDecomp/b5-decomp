// Translation-unit embed check for CgsAttribSys::AttribSysMemoryManager.
// Forces the owning header to compile standalone and exercises the bodied
// accessor so its signature stays wired to its home.
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"

namespace
{
void EmbedCheck()
{
    CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
        CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator();
    (void)lpAllocator;
}
}
