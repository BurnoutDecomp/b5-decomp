// Production bodies are extracted without changing their statements. The protocol owner below
// substitutes only module scheduling/storage; real typed queues, transport and handlers are retained.
// Layout mode instead calls the real native Pool/Heap/LinearMalloc provider. Original InitPool's
// unresolved bank/name lifecycle is deliberately outside the protocol fixture boundary.
#include <Windows.h>
#undef DrawText
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "eathread/eathread_semaphore.h"

// Unused provider dependencies of the complete assertion/linear allocator TUs. The tested worker
// path uses production condition variables and capture; reaching these unrelated paths is a failure.
namespace EA { namespace Thread {
    ThreadTime GetThreadTime() { std::abort(); }
    int Semaphore::Wait(const ThreadTime&) { std::abort(); }
} }
RwMemoryFunctions rwMemoryFunctions = {};

static int siChecks, siFailures;
static DWORD suOwnerThread;
struct AssertRecord
{
    std::string mMessage;
    int miLine;
    DWORD muThread;
    bool mbCaptured;
    bool mbStack;
    const char* mpcInput;
};
static std::mutex sRecordsMutex;
static std::vector<AssertRecord> saRecords;
static void Check(bool lbCondition, const char* lpcName)
{
    ++siChecks;
    if (!lbCondition) { ++siFailures; std::printf("FAIL %s\n", lpcName); }
}
static std::vector<AssertRecord> Records()
{
    std::lock_guard<std::mutex> lLock(sRecordsMutex);
    return saRecords;
}
static void ClearRecords()
{
    std::lock_guard<std::mutex> lLock(sRecordsMutex);
    saRecords.clear();
}
namespace CgsDev
{
namespace Log
{
    static DebugPrint sPrint;
    StrStreamBase* gpDebugPrint = &sPrint;
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace Assert
{
    // Only the final UI/log sink is isolated. Begin/Fire/End, deferred queue/capture, owner dispatch,
    // Win32 critical section and stack capture are the actual production translation units.
    void Manager::HandleAssert(const char* lpcText, const char*, s32 liLine)
    {
        std::lock_guard<std::mutex> lLock(sRecordsMutex);
        saRecords.push_back({lpcText, liLine, GetCurrentThreadId(), false, false, lpcText});
    }
    void Manager::HandleAssertCapturedPC(const char* lpcText, const char*, s32 liLine,
                                         const StackUnpick& lrStack, bool)
    {
        std::lock_guard<std::mutex> lLock(sRecordsMutex);
        saRecords.push_back({lpcText, liLine, GetCurrentThreadId(), true,
            lrStack.miNumStackAddresses > 0, lpcText});
    }
}
}

#if !POOL_LAYOUT
namespace CgsResource
{
    static Pool::InitOptions sCapturedOptions;
    static char sacCapturedName[32];
    // Explicit boundary: inspect caller routing without reading the unproven bank-id slot or
    // executing the absent original InitPool. Native InitPool is exercised separately in layout mode.
    void Pool::InitPool(const InitOptions* lpOptions)
    {
        miId = lpOptions->miId;
        mbIsValid = true;
        std::strcpy(sacCapturedName, lpOptions->mpcName);
        sCapturedOptions.miId = lpOptions->miId;
        sCapturedOptions.mResource = lpOptions->mResource;
        sCapturedOptions.mDescriptor = lpOptions->mDescriptor;
        sCapturedOptions.miNumDependencies = lpOptions->miNumDependencies;
        for (s32 li = 0; li < lpOptions->miNumDependencies; ++li)
            sCapturedOptions.mapDependencies[li] = lpOptions->mapDependencies[li];
    }
    struct PoolProtocolFixture
    {
        static const s32 KI_MAX_POOLS = PoolModule::KI_MAX_POOLS;
        Pool maPools[KI_MAX_POOLS];
        FifoQueue<Events::CreatePoolRequest, 128> mPendingCreateRequests;
        CgsModule::EventReceiverQueue<16384, 16> mReceiverQueue;
        FifoQueue<Events::AllocateResourceListRequest, 4> mPendingAllocationRequests;
        PoolProtocolFixture()
        {
            for (auto& lrPool : maPools) lrPool.miId = -1;
            mPendingCreateRequests.Construct(); mReceiverQueue.Construct();
            mPendingAllocationRequests.Construct();
        }
        s32 GetPoolIndex(s32);
        static void ConvertPoolRequestOptions(const Events::CreatePoolRequest*, Pool::InitOptions*);
        static void GetRequiredResourceDescriptor(const Events::CreatePoolRequest*, rw::ResourceDescriptor*);
        void CreatePool(const Pool::InitOptions*);
        void SendCreatePoolMemoryRequest(const Events::CreatePoolRequest*, PoolIO::OutputBuffer*);
        void DoCreatePoolRequest(const CgsMemory::MemoryIO::CreateResourceResponse*, PoolIO::OutputBuffer*);
        void ProcessInputBuffer(const PoolIO::InputBuffer*, PoolIO::OutputBuffer*);
        void ProcessReceiverQueue(PoolIO::OutputBuffer*);
        void DoDeletePoolRequest(const CgsMemory::MemoryIO::DestroyBankResponse*, PoolIO::OutputBuffer*);
        void DoUnacquireResourceRequest(const Events::UnacquireResourceRequest*, PoolIO::OutputBuffer*);
        // Unexercised dispatcher callees fail loudly if a fixture unexpectedly reaches them.
        void SendDeletePoolMemoryRequest(const Events::DeletePoolRequest*, PoolIO::OutputBuffer*) { Check(false,"unexpected delete request"); }
        void DoAcquireResourceRequest(const Events::AcquireResourceRequest*, PoolIO::OutputBuffer*) { Check(false,"unexpected acquire"); }
        void DoAcquireResourceListRequest(const Events::AcquireResourceListRequest*, PoolIO::OutputBuffer*) { Check(false,"unexpected acquire list"); }
        void DoInvalidatePoolRequest(const Events::InvalidatePoolRequest*, PoolIO::OutputBuffer*) { Check(false,"unexpected invalidate"); }
        void DoValidatePoolRequest(const Events::ValidatePoolRequest*, PoolIO::OutputBuffer*) { Check(false,"unexpected validate"); }
        void DoFixUpAndResolveResourceListRequest(const Events::FixUpAndResolveResourceListRequest*, PoolIO::OutputBuffer*) { Check(false,"unexpected fixup"); }
        void DoUnloadResourceListRequest(const Events::UnloadResourceListRequest*, PoolIO::OutputBuffer*) { Check(false,"unexpected unload"); }
    };
    static const char* const KPC_POOLMODULE_SRC =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsPoolModule.cpp";
}
#endif
#include "PoolCreateProduction.inc"
namespace CgsDev { namespace Assert { Manager gAssertManager; } }

static void TestDeferredAssertions()
{
    using namespace CgsDev;
    using namespace CgsDev::Assert;
    ClearRecords();
    char* lpcOriginal = gpcMessageBuffer;
    const s32 liOriginalSize = giMessageBufferSize;
    char lacShared[KI_MESSAGEBUFFERSIZE] = {};
    gpcMessageBuffer = lacShared; giMessageBufferSize = 128;
    BeginAssert();
    MessageBufferPC lSynchronous;
    Check(lSynchronous.GetBuffer() == lacShared && lSynchronous.GetSize() == 128,
        "synchronous provider retains the configured original shared pointer and capacity");
    StrStream lStream(lSynchronous.GetBuffer(), lSynchronous.GetSize());
    lStream << "synchronous original message";
    FireAssert(lSynchronous.GetBuffer(), "fixture", 1);
    EndAssert();
    auto laRecords = Records();
    Check(laRecords.size() == 1 && laRecords[0].mpcInput == lacShared && !laRecords[0].mbCaptured,
        "synchronous FireAssert receives the actual shared buffer");
    ClearRecords();
    std::strcpy(lacShared, "canonical shared buffer stays unchanged");
    const int KI_WORKERS = 8;
    std::atomic<int> liFormatted{0}, liDone{0};
    std::atomic<bool> lbRelease{false};
    bool labPrivate[KI_WORKERS] = {};
    char* lapBuffers[KI_WORKERS] = {};
    AttachFrameOwnerPC(&siChecks, nullptr);
    std::vector<std::thread> laWorkers;
    for (int li = 0; li < KI_WORKERS; ++li)
        laWorkers.emplace_back([&, li]()
        {
            BeginAssert();
            MessageBufferPC lMessage;
            lapBuffers[li] = lMessage.GetBuffer();
            labPrivate[li] = lMessage.GetBuffer() != lacShared && lMessage.GetSize() == KI_MESSAGEBUFFERSIZE;
            StrStream lWorkerStream(lMessage.GetBuffer(), lMessage.GetSize());
            lWorkerStream << "worker " << li << " original failure";
            MessageBufferPC lNestedMessage;
            labPrivate[li] = labPrivate[li] && lNestedMessage.GetBuffer() != lMessage.GetBuffer();
            StrStream lNestedStream(lNestedMessage.GetBuffer(), lNestedMessage.GetSize());
            lNestedStream << "separate nested invocation";
            ++liFormatted;
            while (!lbRelease.load(std::memory_order_acquire)) std::this_thread::yield();
            FireAssert(lMessage.GetBuffer(), "worker", 100 + li);
            EndAssert();
#if !POOL_LAYOUT
            // These are the actual modified production helper bodies and real deferred captures.
            CgsResource::FireMissingDependencyAssert(700 + li);
            CgsResource::FireInvalidPoolIdAssert(-100 - li);
            CgsResource::FireDuplicatePoolIdAssert(900 + li);
#endif
            ++liDone;
        });
    const auto lDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (liFormatted != KI_WORKERS && std::chrono::steady_clock::now() < lDeadline)
        std::this_thread::yield();
    Check(liFormatted == KI_WORKERS && liDone == 0,
        "all workers format concurrently before the real deferred queue captures any message");
    bool lbDistinct = true;
    for (int li = 0; li < KI_WORKERS; ++li)
    {
        lbDistinct = lbDistinct && labPrivate[li];
        for (int lj = 0; lj < li; ++lj) lbDistinct = lbDistinct && lapBuffers[li] != lapBuffers[lj];
    }
    Check(lbDistinct, "live deferred formatting buffers are per invocation and distinct across workers");
    lbRelease.store(true, std::memory_order_release);
    while (liDone != KI_WORKERS && std::chrono::steady_clock::now() < lDeadline)
    {
        DispatchPendingAssertsPC(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (liDone != KI_WORKERS) { std::fprintf(stderr,"worker handoff timed out\n"); std::abort(); }
    for (auto& lrWorker : laWorkers) lrWorker.join();
    DispatchPendingAssertsPC(false);
    DetachFrameOwnerPC(&siChecks);
    laRecords = Records();
    bool lbMessages = laRecords.size() == KI_WORKERS * (POOL_LAYOUT ? 1 : 4);
    for (int li = 0; li < KI_WORKERS; ++li)
    {
        const std::string lExpected = "worker " + std::to_string(li) + " original failure";
        int liMatches = 0, liDependencies = 0, liInvalid = 0, liDuplicates = 0;
        for (const auto& lrRecord : laRecords)
        {
            lbMessages = lbMessages && lrRecord.mbCaptured && lrRecord.mbStack && lrRecord.muThread == suOwnerThread;
            if (lrRecord.miLine == 100 + li && lrRecord.mMessage == lExpected) ++liMatches;
            if (lrRecord.miLine == 1281 && lrRecord.mMessage == "Failed to find dependency pool with id " + std::to_string(700 + li) + "\n") ++liDependencies;
            if (lrRecord.miLine == 916 && lrRecord.mMessage == "Pool id " + std::to_string(-100 - li) + " is invalid\n") ++liInvalid;
            if (lrRecord.miLine == 922 && lrRecord.mMessage == "Pool id " + std::to_string(900 + li) + "is already in use\n") ++liDuplicates;
        }
        lbMessages = lbMessages && liMatches == 1 && (POOL_LAYOUT || (liDependencies == 1 && liInvalid == 1 && liDuplicates == 1));
    }
    Check(lbMessages, "real owner dispatch receives every isolated message and captured worker stack once");
    Check(std::strcmp(lacShared,"canonical shared buffer stays unchanged") == 0,
        "deferred helpers never overwrite synchronous shared assertion text");
    gpcMessageBuffer = lpcOriginal; giMessageBufferSize = liOriginalSize;
}

#if !POOL_LAYOUT
static void TestProtocol()
{
    using namespace CgsResource;
    auto lpModule = std::make_unique<PoolProtocolFixture>();
    PoolIO::InputBuffer lInput; lInput.Construct();
    PoolIO::OutputBuffer lOutput; lOutput.Construct();
    Events::UnacquireResourceRequest lUnacquire{};
    lUnacquire.miEventId = 19; lUnacquire.miPoolId = 3;
    lUnacquire.mpUser = reinterpret_cast<CgsModule::BaseEventReceiverQueue*>(0x123456780000ull);
    lUnacquire.mResourceId.SetHash(0xFEDCBA9876543210ull);
    lInput.LockForWrite();
    for (s32 liTag : {10,-1,8}) lInput.GetPoolInputQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lUnacquire), liTag, sizeof(lUnacquire));
    lInput.UnlockForWrite(); lInput.LockForRead(); lOutput.LockForWrite();
    ClearRecords(); lpModule->ProcessInputBuffer(&lInput,&lOutput);
    auto laRecords = Records();
    Check(laRecords.size()==1 && laRecords[0].miLine==1155,
        "typed input skips tag10 and asserts once for non-null tag-1");
    lInput.UnlockForRead(); lOutput.UnlockForWrite(); lOutput.LockForRead();
    const CgsModule::Event* lpEvent = nullptr; s32 liBytes = 0;
    const s32 liTag = static_cast<const PoolIO::OutputBuffer&>(lOutput).GetPoolOutputQueue()->GetFirstEvent(&lpEvent,&liBytes);
    const auto* lpEcho = reinterpret_cast<const Events::UnacquireResourceResponse*>(lpEvent);
    Check(liTag==9 && liBytes==sizeof(*lpEcho) && lpEcho->miEventId==19 && lpEcho->mpUser==lUnacquire.mpUser
        && lpEcho->miPoolId==3 && lpEcho->mResourceId==lUnacquire.mResourceId,
        "input advances after tag-1 and real tag8 handler echoes the complete native tag9 record");
    lOutput.UnlockForRead(); lOutput.LockForWrite(); lOutput.GetPoolOutputQueue()->Clear();
    CgsMemory::MemoryIO::DestroyBankResponse lDelete;
    lDelete.Construct(nullptr,3); lDelete.SetResult(CgsMemory::MemoryIO::E_RESULT_BANK_OUT_OF_RANGE);
    lpModule->mReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDelete),-1,sizeof(lDelete));
    lpModule->mReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDelete),13,sizeof(lDelete));
    ClearRecords(); lpModule->ProcessReceiverQueue(&lOutput); laRecords=Records();
    Check(laRecords.size()==2 && laRecords[0].miLine==886 && laRecords[1].miLine==1309
        && lpModule->mReceiverQueue.GetCount()==0,
        "receiver asserts for tag-1, processes the next real Delete response, then clears");

    Events::CreatePoolRequest lRequest{};
    lRequest.miPoolId=7; std::strcpy(lRequest.mpcName,"first");
    lRequest.muMaxResources=1; lRequest.miBankId=42; lRequest.miParentBankId=2;
    lRequest.mDescriptor.m_baseResourceDescriptors[0].m_size=4096;
    lRequest.mDescriptor.m_baseResourceDescriptors[0].m_alignment=64;
    lRequest.mDescriptor.m_baseResourceDescriptors[1].m_size=8192;
    lRequest.mDescriptor.m_baseResourceDescriptors[1].m_alignment=256;
    lRequest.mauMaxResources[0]=3; lRequest.mauMaxResources[1]=5;
    rw::ResourceDescriptor lDescriptor;
    for (const auto& lrCase : std::vector<std::pair<u32,u32>>{{0,4416},{1,4580},{4,5072},{0x80000000u,4416},{0xFFFFFFFFu,4316}})
    {
        lRequest.muMaxResources=lrCase.first;
        PoolProtocolFixture::GetRequiredResourceDescriptor(&lRequest,&lDescriptor);
        Check(lDescriptor.m_baseResourceDescriptors[0].m_size==lrCase.second
            && lDescriptor.m_baseResourceDescriptors[0].m_alignment==64
            && lDescriptor.m_baseResourceDescriptors[2].m_size==8192
            && lDescriptor.m_baseResourceDescriptors[2].m_alignment==256,
            "descriptor retains signed spread/low-word arithmetic and RW0/2 heap geometry");
    }
    lRequest.muMaxResources=1;
    Pool::InitOptions lOptions{}; lOptions.miBankId=0x345678;
    PoolProtocolFixture::ConvertPoolRequestOptions(&lRequest,&lOptions);
    Check(lOptions.miBankId==0x345678 && lOptions.mpcName==lRequest.mpcName,
        "conversion borrows the inline name and leaves the unresolved bank-id slot unwritten");
    ClearRecords(); lpModule->SendCreatePoolMemoryRequest(&lRequest,&lOutput);
    lOutput.GetPoolResourceRequestQueue()->Clear();
    Events::CreatePoolRequest lSecond=lRequest;
    lSecond.miPoolId=9; lSecond.miNumDependencies=1; lSecond.maiDependencyIds[0]=7;
    std::strcpy(lSecond.mpcName,"second");
    lpModule->SendCreatePoolMemoryRequest(&lSecond,&lOutput);
    rw::Resource lMemory{};
    lMemory.m_baseResources[0]=reinterpret_cast<void*>(0x123456780000ull);
    lMemory.m_baseResources[2]=reinterpret_cast<void*>(0x123456790000ull);
    PoolProtocolFixture::GetRequiredResourceDescriptor(&lRequest,&lDescriptor);
    CgsMemory::MemoryIO::CreateResourceResponse lReply;
    lReply.Construct(nullptr,999); lReply.SetResult(CgsMemory::MemoryIO::E_RESULT_BANK_OUT_OF_RANGE);
    lReply.SetResource(&lMemory,&lDescriptor);
    lpModule->DoCreatePoolRequest(&lReply,&lOutput);
    Check(sCapturedOptions.miId==7 && std::strcmp(sacCapturedName,"first")==0
        && sCapturedOptions.mResource.m_baseResources[0]==lMemory.m_baseResources[0]
        && sCapturedOptions.mResource.m_baseResources[1]==lMemory.m_baseResources[2],
        "real DoCreate consumes FIFO identity and converts RW0/2 despite unrelated response id/result");
    lpModule->DoCreatePoolRequest(&lReply,&lOutput);
    Check(sCapturedOptions.miId==9 && sCapturedOptions.miNumDependencies==1
        && sCapturedOptions.mapDependencies[0]==&lpModule->maPools[0],
        "second raw request resolves the previously created dependency synchronously");
    lOutput.UnlockForWrite(); lOutput.LockForRead();
    const auto* lpQueue=static_cast<const PoolIO::OutputBuffer&>(lOutput).GetPoolOutputQueue();
    s32 liCompletion=lpQueue->GetFirstEvent(&lpEvent,&liBytes);
    bool lbCompletion=liCompletion==1 && liBytes==sizeof(Events::CreatePoolResponse)
        && reinterpret_cast<const Events::CreatePoolResponse*>(lpEvent)->miPoolId==7;
    liCompletion=lpQueue->GetNextEvent(lpEvent,&lpEvent,&liBytes);
    Check(lbCompletion && liCompletion==1 && reinterpret_cast<const Events::CreatePoolResponse*>(lpEvent)->miPoolId==9
        && Records().empty(), "completion echoes only each original FIFO PoolEvent on tag1");
    lOutput.UnlockForRead(); lOutput.LockForWrite();
    for (int li=0;li<129;++li)
    {
        lOutput.GetPoolResourceRequestQueue()->Clear();
        lpModule->SendCreatePoolMemoryRequest(&lRequest,&lOutput);
    }
    Check(lpModule->mPendingCreateRequests.GetLength()==128
        && lOutput.GetPoolResourceRequestQueue()->GetLength()==1 && Records().empty(),
        "raw full FIFO refusal is ignored while the 129th memory request is still emitted");
    lOutput.UnlockForWrite();
}
#else
static void TestLayout()
{
    using namespace CgsResource;
    for (u32 luMax : {1u,3u,4u,9u})
    {
        Pool::InitOptions lOptions{};
        lOptions.miId=3; lOptions.mpcName="native layout"; lOptions.muMaxResources=luMax;
        lOptions.miBankId=-1;
        lOptions.maHeapInfo[0]={7,4096,16};
        lOptions.maHeapInfo[1]={11,0,16}; // Empty heaps must not consume their node storage.
        lOptions.maHeapInfo[2]={5,4096,16};
        const size_t luRegion=4096+Pool::GetOverheadMemoryRequired(&lOptions);
        auto* lpMain=static_cast<u8*>(_aligned_malloc(luRegion+32,16));
        auto* lpOther=static_cast<u8*>(_aligned_malloc(4096,16));
        std::memset(lpMain,0,luRegion); std::memset(lpMain+luRegion,0xA5,32);
        lOptions.mResource.m_baseResources[0]=lpMain;
        lOptions.mResource.m_baseResources[2]=lpOther;
        lOptions.mDescriptor.m_baseResourceDescriptors[0].m_size=static_cast<u32>(luRegion);
        lOptions.mDescriptor.m_baseResourceDescriptors[2].m_size=4096;
        Pool lPool; lPool.Construct(); ClearRecords(); lPool.InitPool(&lOptions);
        const size_t luEntryOffset=(4096+sizeof(s16)*luMax+alignof(Entry)-1)&~(alignof(Entry)-1);
        Check(reinterpret_cast<u8*>(lPool.mpResourceEntries)==lpMain+luEntryOffset,
            "real Entry carve follows exactly N 16-bit batch indices at host alignment");
        Check(reinterpret_cast<uintptr_t>(lPool.mpResourceEntries)%alignof(Entry)==0
            && reinterpret_cast<uintptr_t>(lPool.mpHashEntries)%alignof(HashTable::HashEntry)==0
            && reinterpret_cast<uintptr_t>(lPool.maHeaps[0].mpNodes)%alignof(Heap::HeapEntryNode)==0,
            "real native entry/hash/node arrays meet their host alignments");
        const s16* lpSlots=lPool.CreateBatchEntrySlots(static_cast<s16>(luMax));
        bool lbSlots=lpSlots==lPool.mpnBatchIndices;
        for (u32 li=0;li<luMax;++li) lbSlots=lbSlots && lpSlots[li]==li;
        Check(lbSlots && lPool.muNumFreeResources==0,"real batch consumer writes one index per available resource");
        const auto* lpEnd=reinterpret_cast<const u8*>(lPool.maHeaps[2].mpNodes+5);
        bool lbGuard=true; for(int li=0;li<32;++li) lbGuard=lbGuard && lpMain[luRegion+li]==0xA5;
        Check(lpEnd==lpMain+luRegion && lbGuard && Records().empty(),
            "exact management extent includes only nonempty heap nodes and leaves the following guard intact");
        _aligned_free(lpOther); _aligned_free(lpMain);
    }
}
#endif

int main()
{
    suOwnerThread=GetCurrentThreadId();
    CgsDev::Assert::Construct();
#if POOL_LAYOUT
    TestLayout();
#else
    TestProtocol();
#endif
    TestDeferredAssertions();
    std::printf("PoolCreateProtocol: %d checks, %d failures; mode=%s\n",siChecks,siFailures,POOL_LAYOUT?"layout":"protocol");
    return siFailures ? 1 : 0;
}
