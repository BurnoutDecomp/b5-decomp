// Tiny embed/compile check for the RealmcCore home: includes the header and
// touches each reconstructed entry point so the gate exercises the full TU.
#include "SDKs/Realmc/RealmcCore.h"

namespace
{
void RealmcCoreEmbedCheck()
{
    using namespace RealmcCore;

    void* p = allocator::allocate(8, 0);
    allocator::deallocate(p, 8);

    Message msg;
    IMessageProcessor* pProcessor = nullptr;
    if (pProcessor)
        msg.Apply(pProcessor);
}
} // namespace
