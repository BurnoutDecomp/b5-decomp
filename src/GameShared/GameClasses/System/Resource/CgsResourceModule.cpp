#include "GameShared/GameClasses/System/Resource/CgsResourceModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"  // PoolIO::Input/OutputBuffer (shuttle)
#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h" // ResourceIO::InputBuffer (module input)
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"         // MemoryIO buffers + MemoryResponse
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // receiver-queue forward target
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h" // ResolveResourceType (bundle FixUp resolver)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"     // Events::PoolEvent (pool response routing)
#include <cstring>                                    // memset (InitOptions zero-init)

// CgsResource::ResourceModule - see the header. This pass reconstructs the lifecycle
// orchestration spine (Prepare / Release). Construct + Update + the request shuttles are
// DEFERRED (rw allocator middleware) as inert marked stubs.
namespace CgsResource
{
    // Minimal placeholder debug component (deferred).
    void DebugComponent::Construct() {}
    void DebugComponent::Register() {}

    // ResourceModule::InitOptions ctor - zero-init. The X360 ConstructResourceModule constructs this
    // then memset(0)s the whole 1216B block before filling fields, so a zero-init is the faithful net
    // state. (memset over the already-constructed members matches that ctor-then-memset sequence.)
    ResourceModule::InitOptions::InitOptions()
    {
        memset(this, 0, sizeof(*this));
    }

    // @ 0x828F4140 - resumable bring-up: base module, then Memory -> FileSystem -> Bundle ->
    // Pool, then register the debug component. Re-enters at the current stage if a sub-module
    // is not yet ready.
    bool ResourceModule::Prepare()
    {
        // [reliable] The X360 sets *(this+4)=1 (mbIsNewModule) in Construct; that Construct chain isn't
        // wired for the embedded static yet, and the header ctor doesn't take effect on it, so set it
        // here (before the base Prepare) so ModuleSingleBuffered skips the old DataStructure IO path
        // (and its virtual CreateInputDataStructure). Move to Construct once the bring-up chain lands.
        mbIsNewModule = true;
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
        case E_PREPARE_BASE:
            mePrepareStage = E_PREPARE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            // fall through
        case E_PREPARE_MEMORY:
            mePrepareStage = E_PREPARE_MEMORY;
            if (!mMemoryModule.Prepare())
                return false;
            // fall through
        case E_PREPARE_FILESYSTEM:
            mePrepareStage = E_PREPARE_FILESYSTEM;
            if (!mFileSystem.Prepare())
                return false;
            // fall through
        case E_PREPARE_BUNDLE:
            mePrepareStage = E_PREPARE_BUNDLE;
            if (!mBundleLoaderModule.Prepare())
                return false;
            // fall through
        case E_PREPARE_POOL:
            mePrepareStage = E_PREPARE_POOL;
            if (!mPoolModule.Prepare())
                return false;
            mDebugComponent.Register();
            // fall through
        case E_PREPARE_DONE:
            meReleaseStage = E_RELEASE_START;
            mePrepareStage = E_PREPARE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // @ 0x82906570 - resumable tear-down: Pool -> Bundle -> FileSystem -> Memory, then the
    // base module (reverse of Prepare).
    bool ResourceModule::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASE_START:
        case E_RELEASE_POOL:
            meReleaseStage = E_RELEASE_POOL;
            if (!mPoolModule.Release())
                return false;
            // fall through
        case E_RELEASE_BUNDLE:
            meReleaseStage = E_RELEASE_BUNDLE;
            if (!mBundleLoaderModule.Release())
                return false;
            // fall through
        case E_RELEASE_FILESYSTEM:
            meReleaseStage = E_RELEASE_FILESYSTEM;
            if (!mFileSystem.Release())
                return false;
            // fall through
        case E_RELEASE_MEMORY:
            meReleaseStage = E_RELEASE_MEMORY;
            if (!mMemoryModule.Release())
                return false;
            // fall through
        case E_RELEASE_BASE:
            meReleaseStage = E_RELEASE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Release())
                return false;
            // fall through
        case E_RELEASE_DONE:
            mePrepareStage = E_PREPARE_START;
            meReleaseStage = E_RELEASE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // @ 0x829055B0 - construct the sub-modules over the resource heaps. [5b in progress] lpInitOptions
    // is now the real CgsResource::ResourceModule::InitOptions composite (the GameDataModule builds it);
    // each sub-module gets its own option block. Brings up the MemoryModule (carves the bank/block tables
    // from the resource allocator, from mMemoryInitOptions) and the PoolModule structural front half
    // (base + 128 Pool::Construct + stages); the PoolModule allocator-gated back half (consumes
    // mPoolInitOptions), BundleLoaderModule/FileSystem Construct + the FileSystem GeneralResourceAllocator
    // remain deferred (next 5b passes). lpAllocator is the root rw::IResourceAllocator.
    void ResourceModule::Construct(const void* lpInitOptions, void* lpAllocator)
    {
        if (lpInitOptions == 0 || lpAllocator == 0)
            return;
        const InitOptions* lpOptions = static_cast<const InitOptions*>(lpInitOptions);
        rw::IResourceAllocator* lpRwAllocator = static_cast<rw::IResourceAllocator*>(lpAllocator);

        mMemoryModule.Construct(
            const_cast<CgsMemory::MemoryModule::InitOptions*>(&lpOptions->mMemoryInitOptions),
            lpRwAllocator);
        // PoolModule front half (rw-allocator-independent: base + 128 Pool::Construct + stage init).
        // It receives its own mPoolInitOptions; the front half ignores it (the type-registry /
        // ScratchPool / region allocations that consume it are deferred to the back half).
        mPoolModule.Construct(&lpOptions->mPoolInitOptions, lpAllocator);
    }
    // Update (0x82907948) pumps each sub-module + shuttles requests; Destruct (0x828EC6B0) tears them
    // down. Deferred.
    void ResourceModule::Destruct() {}

    // @ 0x82907268 - route inbound resource requests to the sub-module inputs. Pool-create slice: a
    // CreatePool request (id 0) is forwarded to the pool input queue. [The bundle/file/memory request
    // routes (ids 2/3/9..0xD) are deferred -- pool bring-up only sends CreatePool. X360 forwards size 172;
    // we use the recorded event size so the x64-width CreatePoolRequestEvent copies whole.]
    void ResourceModule::ProcessResourceRequests(ResourceIO::InputBuffer* lpResIn, PoolIO::InputBuffer* lpPoolIn)
    {
        const ResourceIO::ResourceRequestQueue<16384>* lpQ =
            static_cast<const ResourceIO::InputBuffer&>(*lpResIn).GetResourceQueue();
        PoolIO::InputBuffer::PoolInputQueue* lpPoolQ = lpPoolIn->GetPoolInputQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpQ->GetFirstEvent(&lpEvent, &liSize);
        while (liId != -1 && lpEvent != 0)
        {
            if (liId == 0)        // CreatePool -> pool input
                lpPoolQ->AddEvent(lpEvent, 0, liSize);
            else if (liId == 2)   // LoadBundle -> bundle loader's load-request intake
                mBundleLoaderModule.EnqueueLoadRequest(*reinterpret_cast<const Events::LoadBundleRequest*>(lpEvent));
            else if (liId == 3)   // UnloadBundle -> bundle loader's unload-request intake
                mBundleLoaderModule.EnqueueUnloadRequest(*reinterpret_cast<const Events::UnloadBundleRequest*>(lpEvent));
            else if (liId == 4)   // AcquireResource -> pool input (X360 routes id 4 -> pool tag 4)
                lpPoolQ->AddEvent(lpEvent, 4, liSize);
            const CgsModule::Event* lpNext = 0;
            liId = lpQ->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // Drain the pool module's output queue (DoAcquireResourceRequest etc. responses) and forward each to
    // its requester's receiver queue (response->mpUser). The pool-response slice of ProcessResourceResponses.
    void ResourceModule::ProcessPoolOutputResponses(PoolIO::OutputBuffer* lpPoolOut)
    {
        const PoolIO::OutputBuffer::PoolOutputQueue* lpQ =
            static_cast<const PoolIO::OutputBuffer&>(*lpPoolOut).GetPoolOutputQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpQ->GetFirstEvent(&lpEvent, &liSize);
        while (liId != -1 && lpEvent != 0)
        {
            CgsModule::BaseEventReceiverQueue* lpUser =
                reinterpret_cast<const Events::PoolEvent*>(lpEvent)->mpUser;
            if (lpUser != 0)
                lpUser->AddEvent(lpEvent, liId, liSize);
            const CgsModule::Event* lpNext = 0;
            liId = lpQ->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // @ 0x82907948 - the per-frame streaming shuttle (pool-create slice): route inbound resource requests
    // to the pool, run the pool module (CreatePool -> memory requests; receiver -> DoCreatePoolRequest),
    // forward pool memory requests to the MemoryModule, run it (allocate the banks), then route the memory
    // responses back to the pool's receiver queue. Multi-frame: a CreatePool sent in frame N has its bank
    // allocated + response queued the same frame, but the receiver is drained at the START of the pool
    // update, so DoCreatePoolRequest (the actual CreatePool) runs frame N+1. The scratch sub-module IO
    // buffers are function-static (the X360 creates them on an IOBufferStack each frame). [The bundle-loader
    // + file-system sub-shuttles + ProcessResourceResponses are deferred -- not exercised by pool creation.]
    bool ResourceModule::Update(void* lpInputBuffer, void* /*lpOutputBuffer*/)
    {
        ResourceIO::InputBuffer* lpResIn = static_cast<ResourceIO::InputBuffer*>(lpInputBuffer);
        if (lpResIn == 0)
            return false;

        static PoolIO::InputBuffer               s_poolIn;
        static PoolIO::OutputBuffer              s_poolOut;
        static CgsMemory::MemoryIO::InputBuffer  s_memIn;
        static CgsMemory::MemoryIO::OutputBuffer s_memOut;
        static bool s_ctor = false;
        if (!s_ctor) { s_poolIn.Construct(); s_poolOut.Construct(); s_memIn.Construct(); s_memOut.Construct(); s_ctor = true; }

        // clear the scratch sub-module queues for this frame
        s_poolIn.LockForWrite();  s_poolIn.GetPoolInputQueue()->Clear();            s_poolIn.UnlockForWrite();
        s_poolOut.LockForWrite(); s_poolOut.GetPoolResourceRequestQueue()->Clear(); s_poolOut.GetPoolOutputQueue()->Clear(); s_poolOut.UnlockForWrite();
        s_memIn.LockForWrite();   s_memIn.GetMemoryRequestQueue()->Clear();         s_memIn.UnlockForWrite();
        s_memOut.LockForWrite();  s_memOut.GetMemoryResponseQueue()->Clear();       s_memOut.UnlockForWrite();

        // [1] inbound resource requests -> pool input, then consume the resource input.
        lpResIn->LockForRead(); s_poolIn.LockForWrite();
        ProcessResourceRequests(lpResIn, &s_poolIn);
        s_poolIn.UnlockForWrite(); lpResIn->UnlockForRead();
        lpResIn->LockForWrite(); lpResIn->GetResourceQueue()->Clear(); lpResIn->UnlockForWrite();

        // [2] pool module: CreatePool requests -> memory requests; AcquireResource -> pool output;
        //     drain receiver -> DoCreatePoolRequest.
        mPoolModule.Update(&s_poolIn, &s_poolOut);

        // [2b] route pool output responses (AcquireResource etc.) back to their requesters.
        s_poolOut.LockForRead();
        ProcessPoolOutputResponses(&s_poolOut);
        s_poolOut.UnlockForRead();

        // [3] pool memory requests -> MemoryModule input.
        s_poolOut.LockForRead(); s_memIn.LockForWrite();
        ProcessPoolResourceRequests(&s_memIn, &s_poolOut);
        s_memIn.UnlockForWrite(); s_poolOut.UnlockForRead();

        // [4] MemoryModule: allocate the requested banks.
        mMemoryModule.Update(0, 0, &s_memIn, &s_memOut);

        // [5] memory responses -> their receiver queues (drained next frame by the pool module).
        s_memOut.LockForRead();
        ProcessMemoryResponses(&s_memOut);
        s_memOut.UnlockForRead();

        // [6] bundle loader: drain the LoadBundleRequests routed in [1] and load each into its (already
        // created) pool via the PC synchronous BundleLoader, replying with a LoadBundleResponse. [The X360
        // async streaming FSM + FileSystem are the deferred remainder; see BundleLoaderModule.]
        mBundleLoaderModule.ProcessLoadRequests(&mPoolModule, ResolveResourceType);
        mBundleLoaderModule.ProcessUnloadRequests(&mPoolModule);

        return false;
    }

    // @ 0x829019F0 - forward the pool module's emitted resource-memory requests into the MemoryModule
    // input queue. Each pool output event tagged 10 (CreatePool) / 13 (DeletePool) becomes a memory
    // request (queue tag 1). [X360 hardcodes the per-type size 92/16; we use the recorded event size so
    // the x64-width CreateResourceRequest copies whole.]
    void ResourceModule::ProcessPoolResourceRequests(CgsMemory::MemoryIO::InputBuffer* lpMemInput,
                                                     PoolIO::OutputBuffer* lpPoolOutput)
    {
        const PoolIO::OutputBuffer::PoolResourceRequestQueue* lpQ =
            static_cast<const PoolIO::OutputBuffer&>(*lpPoolOutput).GetPoolResourceRequestQueue();
        CgsMemory::MemoryIO::InputBuffer::MemoryRequestQueue* lpMemQ = lpMemInput->GetMemoryRequestQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpQ->GetFirstEvent(&lpEvent, &liSize);
        while (liId != -1 && lpEvent != 0)
        {
            if (liId == 10 || liId == 13)   // CreatePool / DeletePool memory requests
                lpMemQ->AddEvent(lpEvent, 1, liSize);
            else
                CGS_ASSERT(false, "Unexpected request from bundle loader\n");

            const CgsModule::Event* lpNext = 0;
            liId = lpQ->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // @ 0x828EC6F0 - route each MemoryModule response to its originating receiver queue (the request's
    // reply target, response->GetUser()) tagged for that response type. The X360 indexes a static table
    // dword_820F71D0[meEventType]; the pool-create flow carries only CREATE_RESOURCE(6) -> CreatePool(10).
    void ResourceModule::ProcessMemoryResponses(CgsMemory::MemoryIO::OutputBuffer* lpMemOutput)
    {
        const CgsMemory::MemoryIO::OutputBuffer::MemoryResponseQueue* lpQ =
            static_cast<const CgsMemory::MemoryIO::OutputBuffer&>(*lpMemOutput).GetMemoryResponseQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpQ->GetFirstEvent(&lpEvent, &liSize);
        while (liId != -1 && lpEvent != 0)
        {
            const CgsMemory::MemoryIO::MemoryResponse* lpResp =
                static_cast<const CgsMemory::MemoryIO::MemoryResponse*>(lpEvent);
            CgsModule::BaseEventReceiverQueue* lpReceiver = lpResp->GetUser();
            if (lpReceiver != 0)
            {
                const s32 liReceiverTag =
                    (lpResp->GetEventType() == CgsMemory::MemoryIO::E_EVENT_TYPE_CREATE_RESOURCE) ? 10 : 0;
                lpReceiver->AddEvent(lpEvent, liReceiverTag, liSize);
            }
            const CgsModule::Event* lpNext = 0;
            liId = lpQ->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // CgsResource:: (unnamed by IDA) @ X360 0x828F2890 -- fixed-slot free-list pop.
    // Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. Pops the head entry of a
    // small u8-indexed free list and returns the pointer to its stride-12 record; returns
    // null when the list is empty. Called by ResourceModule's file-system request adders
    // (AddOpenFileRequest / AddCloseFileRequest / AddOpenReadStreamRequest /
    // AddCloseReadStreamRequest) to reserve a request record slot.
    //
    // FLAG (confidence low): the owning class/member names are not recovered (IDA left the
    // symbol unnamed). The pool shape is fully attested by the asm; only the naming/home is
    // provisional. Modelled as a free helper over an opaque 4-field POD pool so the byte
    // behaviour is exact.
    //
    // Pool layout (X360 byte offsets, base is u8*):
    //   +0x00  u8*  mpRecords      // stride-12 record array base
    //   +0x04  u8*  mpFreeIndices  // free-index ring (u8 entries)
    //   +0x08  u8   muUsedCount    // slots in use
    //   +0x09  u8   muFreeCount    // slots free (loop count)
    namespace Detail
    {
        struct RequestSlotPool
        {
            u8* mpRecords;       // +0x00
            u8* mpFreeIndices;   // +0x04
            u8  muUsedCount;     // +0x08
            u8  muFreeCount;     // +0x09
        };

        // X360 0x828F2890: pop the head free index, compact the ring, bump the used count,
        // and return &mpRecords[12 * index]; null when the pool is empty.
        u8* AllocRequestSlot(RequestSlotPool* lpPool)
        {
            s8 lcFree = static_cast<s8>(lpPool->muFreeCount);
            s8 lcIndex;
            if (lcFree != 0)
            {
                u8* lpFree = lpPool->mpFreeIndices;
                lcIndex = static_cast<s8>(lpFree[0]);
                lpFree[0] = lpFree[lpPool->muFreeCount - 1];   // pull tail into head
                ++lpPool->muUsedCount;
                lpPool->muFreeCount = static_cast<u8>(lpPool->muFreeCount - 1);
            }
            else
            {
                lcIndex = -1;
            }

            if (lcIndex == -1)
                return nullptr;
            return lpPool->mpRecords + 12 * lcIndex;
        }
    }
}
