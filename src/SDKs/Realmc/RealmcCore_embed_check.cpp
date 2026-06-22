// Tiny embed/compile check for the RealmcCore home: includes the header and
// touches each reconstructed entry point so the gate exercises the full TU.
#include "SDKs/Realmc/RealmcCore.h"

namespace
{
void RealmcCoreEmbedCheck()
{
    using namespace RealmcCore;

    void* p = allocator::allocate(8, 0);
    (void)p;
    allocator::deallocate();

    Message msg;
    IRealmcMessageTarget* pTarget = nullptr;
    (void)Message::Apply(&msg, pTarget);
}
} // namespace
