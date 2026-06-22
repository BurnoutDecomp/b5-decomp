// Tiny embed/compile check for the RealmcCore::ObjectManager home: includes the
// header and touches each reconstructed entry point so the gate exercises the TU.
#include "SDKs/Realmc/RealmcObjectManager.h"

namespace
{
void RealmcObjectManagerEmbedCheck()
{
    using namespace RealmcCore;

    sizeof(ObjectManager);
    sizeof(IRealmcObjectAllocatorBackend);
    sizeof(IRealmcManagedObject);

    void* p = ObjectManager::Initialize();
    (void)p;
    void* q = ObjectManager::Finalize();
    (void)q;
}
} // namespace
