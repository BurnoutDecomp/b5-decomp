// Tiny embed/ODR + linkage check for this group's homes:
//   - CgsSound::MemBase                                     (CgsMemBase.h)
//   - CgsSound::Logic::StateManager::RegisteredContent      (CgsStateManager.h, canonical)
//   - ObjectPool<RegisteredContent, 4, int>                 (instantiation TU)
// Exercises each owned surface from a TU other than the home/.cpp so the layouts
// and signatures compile and link.
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"  // canonical StateManager (RegisteredContent host view retired 2026-08-25, wave 4)
#include "GameShared/GameClasses/Sound/CgsMemBase.h"

namespace CgsSound
{
namespace Logic
{
// Declared in ObjectPool_StateManagerRegisteredContent_4.cpp.
void ObjectPool_StateManagerRegisteredContent_4_AnchorDtor(
    CgsContainers::ObjectPool<StateManager::RegisteredContent, 4, int>* lpPool);
}
}

static void RegisteredContent_EmbedCheck()
{
    // MemBase: construct + virtual-destroy through a base pointer.
    CgsSound::MemBase* lpMem = new CgsSound::MemBase();
    delete lpMem;

    // Pool of RegisteredContent slots: allocate a slot, query occupancy.
    CgsContainers::ObjectPool<CgsSound::Logic::StateManager::RegisteredContent, 4, int> lPool;
    const int liSlot = lPool.AllocateObject();
    (void)lPool.IsObjectAllocated(liSlot);

    // Anchor the pool destructor (per-element RegisteredContent teardown).
    CgsSound::Logic::ObjectPool_StateManagerRegisteredContent_4_AnchorDtor(&lPool);
}

void RegisteredContent_EmbedCheckEntry()
{
    RegisteredContent_EmbedCheck();
}
