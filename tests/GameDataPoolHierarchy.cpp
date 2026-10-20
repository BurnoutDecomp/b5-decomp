// Production GameData producers, sizing helpers, typed map and root-allocation statements.
// The resource transport only captures requests and delivers replies when the test asks.
// The debug allocator only captures root requests and supplies aligned address tokens;
// no game subsystem, native GUI phase, heap allocation or MemoryModule allocation is simulated.
#include <Windows.h>
#undef DrawText
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <vector>
#include "GameSource/Resource/BrnMemoryMapData.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceModule.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h"
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "eathread/eathread_semaphore.h"

static int siChecks, siFailures;
struct AssertRecord { std::string message, file; s32 line; };
static std::vector<AssertRecord> saAsserts;
static void Check(bool condition, const char* label)
{
    ++siChecks;
    if (!condition) { ++siFailures; std::printf("FAIL %s\n", label); }
}
namespace EA { namespace Thread {
    ThreadTime GetThreadTime() { std::abort(); }
    int Semaphore::Wait(const ThreadTime&) { std::abort(); }
} }
RwMemoryFunctions rwMemoryFunctions = {};
namespace CgsDev {
namespace Log {
    static DebugPrint sPrint;
    StrStreamBase* gpDebugPrint = &sPrint;
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace Assert {
    // Only the final display/log sink is substituted. The real assert front end,
    // MessageBufferPC, mutex, StrStream and canonical Append provider are linked.
    void Manager::HandleAssert(const char* message, const char* file, s32 line)
    { saAsserts.push_back({message, file, line}); }
    void Manager::HandleAssertCapturedPC(const char* message, const char* file, s32 line,
        const StackUnpick&, bool)
    { saAsserts.push_back({message, file, line}); }
}
}

namespace BrnResource
{
    struct RootAllocation { rw::ResourceDescriptor descriptor; std::string name; void* address; };
    struct RootAllocatorDouble
    {
        std::vector<RootAllocation> requests;
        rw::Resource DoAllocate(const rw::ResourceDescriptor& descriptor, const char* name)
        {
            const uintptr_t addresses[] = {0x10000000u, 0x20000000u};
            Check(requests.size() < 2, "root owner makes exactly two allocator requests");
            rw::Resource result;
            if (requests.size() < 2)
                result.m_baseResources[0] = reinterpret_cast<void*>(addresses[requests.size()]);
            requests.push_back({descriptor, name, result.m_baseResources[0]});
            return result;
        }
    };
    struct RootSnapshot
    {
        CgsMemory::MemoryModule::InitOptions options;
        std::vector<RootAllocation> allocations;
    };
    RootSnapshot CaptureRootRequests(RootAllocatorDouble*);
    void InitialiseMapForFixture();

    struct ResourceTransportDouble
    {
        std::vector<CgsMemory::MemoryIO::CreateBankRequest> banks;
        std::vector<CgsResource::Events::CreatePoolRequest> pools;
        size_t bankDelivered = 0, poolDelivered = 0;
        s32 updates = 0;
        bool Update(void* input, void*)
        {
            ++updates;
            auto* buffer = static_cast<CgsResource::ResourceIO::InputBuffer*>(input);
            buffer->LockForRead();
            const auto* queue = static_cast<const CgsResource::ResourceIO::InputBuffer*>(buffer)->GetResourceQueue();
            const CgsModule::Event* event = nullptr;
            s32 bytes = 0;
            s32 tag = queue->GetFirstEvent(&event, &bytes);
            while (event)
            {
                if (tag == 9)
                {
                    Check(bytes == sizeof(CgsMemory::MemoryIO::CreateBankRequest), "bank transport receives the canonical request size");
                    banks.push_back(*reinterpret_cast<const CgsMemory::MemoryIO::CreateBankRequest*>(event));
                }
                else if (tag == 0)
                {
                    Check(bytes == sizeof(CgsResource::Events::CreatePoolRequest), "pool transport receives one raw request contract");
                    pools.push_back(*reinterpret_cast<const CgsResource::Events::CreatePoolRequest*>(event));
                }
                else Check(false, "unexpected resource request in isolated preparation transport");
                const auto* previous = event;
                tag = queue->GetNextEvent(previous, &event, &bytes);
            }
            buffer->UnlockForRead();
            // Readiness never comes from this return value. Only explicitly
            // delivered canonical replies can advance the production counter wait.
            return false;
        }
        void DeliverBanks(size_t count)
        {
            while (count-- && bankDelivered < banks.size())
            {
                const auto& request = banks[bankDelivered++];
                CgsMemory::MemoryIO::CreateBankResponse reply;
                reply.Construct(request.GetUser(), request.GetEventId());
                reply.SetResult(CgsMemory::MemoryIO::E_RESULT_OK);
                request.GetUser()->AddEvent(&reply, 9, sizeof(reply));
            }
        }
        void DeliverPools(size_t count)
        {
            while (count-- && poolDelivered < pools.size())
            {
                const auto& request = pools[poolDelivered++];
                CgsResource::Events::CreatePoolResponse reply;
                static_cast<CgsResource::Events::PoolEvent&>(reply) = request;
                request.mpUser->AddEvent(reinterpret_cast<const CgsModule::Event*>(&reply), 0, sizeof(reply));
            }
        }
    };
    struct GameDataPoolFixture
    {
        const CgsMemory::MemoryMap* mpMemoryMap = &MemoryMapImageInitializer::GetMemoryMap();
        CgsModule::EventReceiverQueue<32768,16> mReceiverQueue;
        s32 miNumBankCreationRequests = 0, miNumPoolCreationRequests = 0;
        ResourceTransportDouble mResourceModule;
        GameDataPoolFixture() { mReceiverQueue.Construct(); }
        bool CreateBanks(void*, void*);
        bool CreatePools(void*, void*);
        const CgsMemory::MemoryBank::Params& Bank(s32 id) const
        {
            for (const auto& request : mResourceModule.banks)
                if (request.GetParams()->mnBankId == id) return *request.GetParams();
            Check(false, "required original parent-bank request was not emitted");
            std::abort();
        }
        const CgsResource::Events::CreatePoolRequest& Pool(s32 id) const
        {
            for (const auto& request : mResourceModule.pools)
                if (request.miPoolId == id) return request;
            Check(false, "required raw pool request was not emitted");
            std::abort();
        }
    };
}
#include "GameDataPoolProduction.inc"
namespace CgsDev { namespace Assert { Manager gAssertManager; } }

static void TestAppendProvider()
{
    using namespace CgsResource;
    ResourceIO::InputBuffer input;
    ResourceIO::ResourceRequestQueue<16384> source;
    CgsModule::EventReceiverQueue<1024,16> replies;
    input.Construct(); source.Construct(); replies.Construct();
    Events::CreatePoolRequest request = *BrnResource::MemoryMapImageInitializer::GetMemoryMap().GetPool(10)->GetRequest();
    request.mpUser = &replies; request.miEventId = 77;
    source.AddEvent(&request, 0);
    input.AppendResourceQueue(&source); // deliberately unlocked: the original asserts and continues.
    Check(saAsserts.size() == 1 && saAsserts[0].line == 82
        && saAsserts[0].message == "Not locked for writing\n"
        && saAsserts[0].file.find("CgsResourceModuleIO.h") != std::string::npos,
        "real Append provider retains its original lock diagnostic and file line");
    input.LockForRead();
    const auto* queue = static_cast<const ResourceIO::InputBuffer&>(input).GetResourceQueue();
    const CgsModule::Event* event = nullptr; s32 bytes = 0;
    const s32 tag = queue->GetFirstEvent(&event, &bytes);
    const auto* copied = reinterpret_cast<const Events::CreatePoolRequest*>(event);
    Check(tag == 0 && bytes == sizeof(request) && copied->mpUser == &replies
        && copied->miEventId == 77 && copied->miBankId == 37 && copied->miParentBankId == 5,
        "unlocked Append still copies the complete native request after its assertion");
    input.UnlockForRead();
}

static void TestHierarchy()
{
    using namespace BrnResource;
    RootAllocatorDouble allocator;
    const RootSnapshot root = CaptureRootRequests(&allocator);
    Check(root.allocations.size() == 2 && root.allocations[0].name == "GameDataRoot0"
        && root.allocations[1].name == "GameDataRoot1", "actual root allocation owner and names remain intact");
    Check(root.options.maResourceSets[0].muDataSize == 0x0A000000u
        && root.options.maResourceSets[0].muNumBlocks == 2560
        && root.options.maResourceSets[2].muDataSize == 0x0E000000u
        && root.options.maResourceSets[2].muNumBlocks == 3584,
        "production root requests retain the original host 160/224 MiB budgets and 64 KiB granules");
    bool rootLanes = true;
    for (s32 lane : {1,3,4,5}) rootLanes = rootLanes
        && !root.options.maResourceSets[lane].mpDataStart
        && root.options.maResourceSets[lane].muDataSize == 0
        && root.options.maResourceSets[lane].muNumBlocks == 0;
    Check(rootLanes && root.options.maResourceSets[0].mpDataStart == root.allocations[0].address
        && root.options.maResourceSets[2].mpDataStart == root.allocations[1].address,
        "real root publication binds main to RW0 and graphics to RW2 only");
    bool allocations = true;
    for (const auto& request : root.allocations)
    {
        allocations = allocations && request.descriptor.m_baseResourceDescriptors[0].m_alignment == 65536;
        for (s32 lane = 1; lane < 5; ++lane)
            allocations = allocations && request.descriptor.m_baseResourceDescriptors[lane].m_size == 0;
    }
    Check(allocations && root.allocations[0].descriptor.m_baseResourceDescriptors[0].m_size == 0x0A000000u
        && root.allocations[1].descriptor.m_baseResourceDescriptors[0].m_size == 0x0E000000u,
        "allocator double captures unchanged real requests without reserving large test heaps");

    auto fixture = std::make_unique<GameDataPoolFixture>();
    const auto& map = *fixture->mpMemoryMap;
    Check(map.GetNumBanks() == 15 && map.GetNumPools() == 27, "typed original map exposes 15 non-root banks and 27 pools");
    Check(!fixture->CreateBanks(nullptr,nullptr) && fixture->miNumBankCreationRequests == 15
        && fixture->mResourceModule.banks.size() == 15 && fixture->mReceiverQueue.GetLength() == 0,
        "production CreateBanks emits once and waits without a reply");
    bool held = true;
    for (int i = 0; i < 7; ++i) held = !fixture->CreateBanks(nullptr,nullptr) && held;
    Check(held && fixture->mResourceModule.banks.size() == 15 && fixture->miNumBankCreationRequests == 15,
        "repeated empty transport pumps never fabricate bank readiness or duplicate requests");
    fixture->mResourceModule.DeliverBanks(14);
    Check(!fixture->CreateBanks(nullptr,nullptr) && fixture->mReceiverQueue.GetLength() == 14,
        "one missing real bank reply keeps preparation pending");
    fixture->mResourceModule.DeliverBanks(1);
    Check(fixture->CreateBanks(nullptr,nullptr) && fixture->miNumBankCreationRequests == 0
        && fixture->mReceiverQueue.GetLength() == 0, "last bank reply releases the original count wait and clears its owner queue");

    u64 mainReserved = 0, graphicsReserved = 0;
    bool parents = true, granules = true, otherLanes = true, identities = true;
    for (s32 i = 0; i < map.GetNumBanks(); ++i)
    {
        const auto& raw = *map.GetBank(i)->GetParams();
        const auto& bank = fixture->Bank(raw.mnBankId);
        const u32 ownBlock = raw.mauBankSize[0] / raw.mauBankBlocks[0];
        const u32 parentBlock = raw.mnParentBankId == 0 ? 65536u :
            fixture->Bank(raw.mnParentBankId).mauBankSize[0] / fixture->Bank(raw.mnParentBankId).mauBankBlocks[0];
        parents = parents && bank.mnParentBankId == raw.mnParentBankId;
        identities = identities && std::strcmp(bank.macName,raw.macName) == 0
            && bank.mbIsLeaf == raw.mbIsLeaf && bank.mbAllowFragmentation == raw.mbAllowFragmentation
            && fixture->mResourceModule.banks[i].GetEventId() == i
            && fixture->mResourceModule.banks[i].GetUser() == &fixture->mReceiverQueue;
        granules = granules && bank.mauBankSize[0] >= raw.mauBankSize[0]
            && bank.mauBankSize[0] % ownBlock == 0 && bank.mauBankSize[0] % parentBlock == 0
            && bank.mauBankSize[0] / bank.mauBankBlocks[0] == ownBlock;
        for (s32 lane = 1; lane < 5; ++lane) otherLanes = otherLanes
            && bank.mauBankSize[lane] == raw.mauBankSize[lane] && bank.mauBankBlocks[lane] == raw.mauBankBlocks[lane];
        if (bank.mnParentBankId == 0) { mainReserved += bank.mauBankSize[0]; graphicsReserved += bank.mauBankSize[2]; }
    }
    Check(parents && identities, "real bank producer preserves original parent/name/flags and per-request reply identity");
    Check(granules && otherLanes, "actual host bank projection preserves each own granule, parent divisibility and other lanes");
    Check(mainReserved <= root.options.maResourceSets[0].muDataSize
        && graphicsReserved <= root.options.maResourceSets[2].muDataSize, "projected parent-bank reservations fit unchanged real root budgets");
    const auto& physics = fixture->Bank(50);
    Check(physics.mnParentBankId == 2 && physics.mauBankSize[0] == 1572864
        && physics.mauBankBlocks[0] == 16 && physics.mauBankSize[0] % 131072u == 0,
        "eight widened physics pools grow bank50 through original bank2 across 96/128 KiB granules");
    Check(fixture->Bank(5).mauBankSize[0] == 9371648 && fixture->Bank(6).mauBankSize[0] == 13893632,
        "GUI and retained WorldMapData capacities are actual source-derived host reservations");

    Check(!fixture->CreatePools(nullptr,nullptr) && fixture->miNumPoolCreationRequests == 27
        && fixture->mResourceModule.pools.size() == 27, "production CreatePools emits 27 canonical raw requests and waits");
    held = true;
    for (int i = 0; i < 7; ++i) held = !fixture->CreatePools(nullptr,nullptr) && held;
    Check(held && fixture->mResourceModule.pools.size() == 27, "missing pool replies never produce readiness after repeated pumps");
    const auto& language = fixture->Pool(11); const auto& satnav = fixture->Pool(12);
    Check(language.miBankId == 37 && language.miParentBankId == 5
        && satnav.miBankId == 39 && satnav.miParentBankId == 5,
        "actual pool11/12 producer bindings retain banks37/39 under original GUI bank5");
    bool poolRows = true;
    for (s32 i = 0; i < map.GetNumPools(); ++i)
    {
        const auto& raw = *map.GetPool(i)->GetRequest(); const auto& request = fixture->mResourceModule.pools[i];
        poolRows = poolRows && request.miPoolId == raw.miPoolId && request.miBankId == raw.miBankId
            && request.miParentBankId == raw.miParentBankId && request.miEventId == i
            && request.mpUser == &fixture->mReceiverQueue && std::strcmp(request.mpcName,raw.mpcName) == 0
            && request.muMaxResources == raw.muMaxResources && request.muMaxImports == raw.muMaxImports
            && request.miNumDependencies == raw.miNumDependencies;
        for (s32 d = 0; d < request.miNumDependencies; ++d) poolRows = poolRows && request.maiDependencyIds[d] == raw.maiDependencyIds[d];
        const u32 expectedMain = raw.mDescriptor.m_baseResourceDescriptors[0].m_size * (raw.miPoolId == 5 ? 2u : 1u);
        poolRows = poolRows && request.mDescriptor.m_baseResourceDescriptors[0].m_size == expectedMain
            && request.mDescriptor.m_baseResourceDescriptors[1].m_size == raw.mDescriptor.m_baseResourceDescriptors[1].m_size
            && request.mbAllowDefragmentation == (raw.mbAllowDefragmentation || raw.miPoolId == 3 || raw.miPoolId == 4 || raw.miPoolId == 15);
    }
    Check(poolRows, "real pool producer retains row geometry/dependencies and only the original defrag and retained host heap adjustments");
    fixture->mResourceModule.DeliverPools(26);
    Check(!fixture->CreatePools(nullptr,nullptr) && fixture->mReceiverQueue.GetLength() == 26,
        "one missing actual pool response leaves the original owner pending");
    fixture->mResourceModule.DeliverPools(1);
    Check(fixture->CreatePools(nullptr,nullptr) && fixture->miNumPoolCreationRequests == 0
        && fixture->mReceiverQueue.GetLength() == 0, "final pool response completes and resets the real counter/queue");
    Check(saAsserts.size() == 1, "hierarchy preparation adds no assertions beyond the deliberate append-lock case");
}

int main()
{
    CgsDev::Assert::Construct();
    BrnResource::InitialiseMapForFixture();
    TestAppendProvider();
    TestHierarchy();
    std::printf("GameDataPoolHierarchy: %d checks, %d failures\n", siChecks, siFailures);
    return siFailures ? 1 : 0;
}
