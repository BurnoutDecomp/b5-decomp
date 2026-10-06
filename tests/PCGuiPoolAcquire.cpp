#include <cstdio>
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"
#include "GameShared/GameClasses/System/PC/CgsResourcePoolBindingsPC.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static int checks, failures, assertions;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; std::printf("ASSERT %s\n", message); return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }
namespace CgsResource {
// The module scheduler/lifecycle is the boundary. Pool lookup, native resource
// status/refcount filtering, request handling and response queue are production.
class PoolModule {
public:
    Pool* maPools = nullptr;
    s32 GetPoolIndex(s32 id) { return maPools && maPools->miId == id ? 0 : -1; }
    Pool* GetPool(s32);
    void DoAcquireResourceRequest(const Events::AcquireResourceRequest*, PoolIO::OutputBuffer*);
};
PoolIO::OutputBuffer::PoolOutputQueue* PoolIO::OutputBuffer::GetPoolOutputQueue() { return &mPoolOutputQueue; }
}
#include "pc_gui_pool_acquire.inc"

static void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", message); }
}
int main() {
    using namespace CgsResource;
    Pool guiPool = {}, corePool = {};
    PoolModule module;
    module.maPools = &corePool;
    corePool.miId = 12;
    guiPool.mbIsValid = true;
    Entry entries[1] = {};
    HashTable::HashEntry hashes[4] = {};
    u8 status[1] = {2};
    s16 refs[1] = {1};
    int texture = 1234;
    entries[0].mResource.m_baseResources[E_MEMTYPE_MAINMEMORY] = &texture;
    guiPool.mpResourceEntries = entries;
    guiPool.mpx8ResourceStatuses = status;
    guiPool.mpiResourceRefCounts = refs;
    guiPool.mHashTable.Initialize(hashes, 4);
    ID textureId;
    textureId.SetHash(0x12345678);
    guiPool.mHashTable.AddEntry(textureId, 0);
    PoolIO::OutputBuffer output;
    output.mPoolOutputQueue.Construct();
    Events::AcquireResourceRequest request = {};
    request.miPoolId = 9;
    request.miEventId = 595017;
    request.mResourceId = textureId;
    auto acquire = [&]() {
        output.mPoolOutputQueue.Clear();
        module.DoAcquireResourceRequest(&request, &output);
        const CgsModule::Event* event = nullptr;
        s32 size = 0;
        const s32 type = output.mPoolOutputQueue.GetFirstEvent(&event, &size);
        Check(type == 6 && size == sizeof(Events::AcquireResourceResponse), "native acquire response queued");
        const auto response = *reinterpret_cast<const Events::AcquireResourceResponse*>(event);
        Check(response.miEventId == 595017 && response.miPoolId == request.miPoolId && response.mResourceId == request.mResourceId,
              "request identity round trips");
        return response;
    };
    Check(acquire().mpResourceMemory == nullptr, "unloaded GUI bank misses");
    PCPoolBindings::Publish(9, &guiPool);
    auto response = acquire();
    Check(response.mpSourceEntry == &entries[0] && response.mpResourceMemory == &entries[0].mResource.m_baseResources[E_MEMTYPE_MAINMEMORY],
          "GameData receives GUI-owned texture handle");
    Check(response.mpResourceMemory && *static_cast<void**>(response.mpResourceMemory) == &texture,
          "handle resolves original loaded texture");
    status[0] = 0;
    Check(acquire().mpResourceMemory == nullptr, "unloaded entry cannot be acquired");
    status[0] = 2; refs[0] = 0;
    Check(acquire().mpResourceMemory == nullptr, "original refcount policy preserved");
    request.mbCheckRefCount = true;
    Check(acquire().mpSourceEntry == &entries[0], "original unreferenced-entry option preserved");
    request.mResourceId.SetHash(0x87654321);
    Check(acquire().mpResourceMemory == nullptr, "wrong resource does not return another bank member");
    request.mResourceId = textureId; request.miPoolId = 10;
    Check(acquire().mpResourceMemory == nullptr, "wrong pool does not search GUI storage");
    Check(module.GetPool(12) == &corePool, "ordinary module-owned pool remains accessible");
    PCPoolBindings::Publish(9, nullptr);
    request.miPoolId = 9;
    Check(acquire().mpResourceMemory == nullptr, "withdrawn host pool misses");
    Check(assertions == 0, "no assertions");
    std::printf("PCGuiPoolAcquire: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
