#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // mReceiverQueue
#include "GameShared/GameClasses/Containers/CgsFifoQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // canonical raw request FIFO payloads
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"    // Pool (128 embedded)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"    // Type (type table)
#include "GameShared/GameClasses/System/Resource/CgsResourceScratchPool.h" // ScratchPool (defrag staging, embedded)
#include "GameShared/GameClasses/System/Resource/CgsResourceHeap.h"     // AllocRequestAddressed/RelocateRequest/RelocateSource/LinearHeapNode (defrag scratch buffers)
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsAllocatePoolModuleState.h"     // mAllocateState (defrag-state cluster)
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsDeAllocatePoolModuleState.h"   // mDeAllocateState
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsIntelliFragPoolModuleState.h"  // mIntelliFragState / IntelliFragParams
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsLiveUpdatePoolModuleState.h"   // mLiveUpdateState
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsEmergencyFragPoolModuleState.h"// mEmergencyFragState / EmergencyFragParams
#include "GameShared/Jobs/Relocator/CgsRelocator.h"                      // mRelocator / mRelocationParams
#include "rw/rwcore_structs.h"                                          // rw::Resource / ResourceDescriptor (InitOptions)

// Async pool-create dispatch (CreateResourceRequest -> MemoryModule -> CreateResourceResponse).
namespace CgsModule { struct IOBufferStack; }
namespace CgsMemory { namespace MemoryIO { struct InputBuffer; struct CreateResourceResponse; struct DestroyBankResponse; } }
namespace CgsResource { namespace PoolIO { struct InputBuffer; struct OutputBuffer; } }
namespace CgsResource { namespace Events { struct CreatePoolRequest; struct AcquireResourceRequest; struct AcquireResourceListRequest; } }
namespace CgsResource { namespace Events { struct CreatePoolResponse; struct UnacquireResourceRequest; struct InvalidatePoolRequest; struct ValidatePoolRequest; struct DeletePoolRequest; } }
// Defrag distribution/relocation records (pointer members only; full layouts live in the deferred
// defrag subsystem -- forward-declared to avoid a transitive cascade for pointer-only storage).
namespace CgsResource { typedef CgsMemory::RelocateOp RelocationEntry; }

// CgsResource::PoolModule - the resource-pool manager module (X360 CgsPoolModule.cpp). It
// owns the game's fixed bank of 128 resource Pools and a registry of resource Types, and
// services per-frame requests to create/destroy pools and allocate/acquire/unload/fix-up
// resources, plus the multi-stage pool defragmenter. It is a CgsModule::ModuleSingleBuffered
// (input request buffer -> output response buffer) and is embedded by value inside
// ResourceModule, which is itself embedded inside the GameDataModule.
//
// SOURCES (X360 ARTIST): ctor 0x827E07B8, Construct 0x828FC0B8, Prepare 0x828E2C60,
// Release 0x828E2D78, Destruct 0x828E2E90, Update 0x829076D8, ProcessReceiverQueue
// 0x82906FD0, GetPoolIndex 0x828D80E8, FindResourceType 0x828D8268, CreatePool 0x82904B20,
// + the DoXxxRequest handlers and the UpdateXxx defrag-state drivers.
//
// Native fields retain the original responsibilities and fixed capacities;
// pointer-bearing records use the host ABI. Scratch storage and state construction
// are restored. Full per-frame request dispatch and staged bundle loading remain
// separate integration work.
namespace CgsResource
{

    class PoolModule : public CgsModule::ModuleSingleBuffered
    {
        friend class DebugPoolTypeList; //ARTIST828E4334..4388 reads the registered type names

    public:
        static const s32 KI_MAX_POOLS = 128;
        static const s32 KI_MAX_TYPES = 512; // DecFIGS KI_MAX_RESOURCE_TYPES; ARTIST stages+6700.
        // Defrag-path capacities (DWARF CgsPoolModule.h:61-64) -- the IntelliFrag/EmergencyFrag passes
        // size their scratch arrays from these.
        static const s32 KI_MAX_ALLOCATION_REQUESTS   = 4096;    // :61
        static const u16 KU_MAX_POOL_ENTRIES          = 36863;   // :62
        static const u16 KU_MAX_LINEAR_HEAP_LENGTH    = 65535;   // :63
        static const u16 KU_MAX_DISTRIBUTION_COMMANDS = 65535;   // :64

        // PoolModule's own prepare/release stage machine (distinct from the base module's).
        enum EPoolPrepareStage { E_POOLPREPARE_START = 0, E_POOLPREPARE_BASE = 1, E_POOLPREPARE_POOLS = 2, E_POOLPREPARE_DONE = 3 };
        enum EPoolReleaseStage { E_POOLRELEASE_START = 0, E_POOLRELEASE_POOLS = 1, E_POOLRELEASE_BASE = 2, E_POOLRELEASE_DONE = 3 };

        // The per-frame defrag/dispatch state machine the UpdateXxx drivers set (== mProcessState, the
        // X360 +0x19B30 field). (DWARF CgsPoolModule.h:89.)
        enum EUpdateState
        {
            E_UPDATESTATE_IDLE              = 0,
            E_UPDATESTATE_ALLOCATING_LIST   = 1,
            E_UPDATESTATE_DEALLOCATING_LIST = 2,
            E_UPDATESTATE_INTELLIFRAG       = 3,
            E_UPDATESTATE_SIMPLEFRAG        = 4,
            E_UPDATESTATE_LIVEUPDATE        = 5,
            E_UPDATESTATE_EMERGENCYFRAG     = 6,
            E_UPDATESTATE_COUNT             = 7,
        };

        // FPoolReportCallback (matches Pool::FPoolReportCallback): the DebugReport visitor.
        typedef void (*FPoolReportCallback)(const PoolStats& lrStats, void* lpUserData);

        // A registered resource type: id (the lookup key, = the type's own id) -> handler.
        struct ResourceTypeEntry
        {
            u32         muTypeId;   // [+0x00] (X360 entry +556; = *(Type+8))
            const Type* mpType;     // [+0x04] (X360 entry +560)
            const char* mpcName;    // [+0x08] (X360 entry +564)
        };

        // CgsResource::PoolModule::InitOptions (DWARF CgsPoolModule.h:121) - the pool-manager bring-up
        // options PoolModule::Construct consumes (the X360 `a2`): the defrag/scratch-buffer sizing plus
        // the game-specific resource-type list (registered into maTypes after the built-in "IDList").
        // Both the original and native RW records contain five resource lanes;
        // native pointers widen while the byte counts retain their original units.
        struct InitOptions
        {
            // A game-specific resource type to register into maTypes (X360 a2 type list / count).
            struct GSResourceType
            {
                const Type* mpType;    // CgsPoolModule.h:127
                const char* mpcName;   // CgsPoolModule.h:128
            };

            s32                    miMaxResourceToDefrag;     // :131
            rw::ResourceDescriptor mDefragBufferDescriptor;   // :132 [ARTIST 5-type]
            rw::Resource           mDefragBufferResource;     // :133 [ARTIST 5-type]
            u32                    muDebugBufferSize;         // :134
            GSResourceType*        mpGameSpecificTypes;       // :135 the type list
            s32                    miNumGameSpecificTypes;    // :136
        };

        // X360 ctor @0x827E07B8 stores only vtable/RWMutexes + the 128-slot BaseResourceDescriptor
        // vector init; it does NOT set mbIsNewModule (Construct @0x828FC0B8 does) nor touch the pending
        // FIFO indices (CreatePool path establishes them; in-class-zeroed below). Trivial ctor.
        PoolModule() {}

        // ---- lifecycle ---------------------------------------------------------------
        virtual void Construct(const InitOptions* lpInitOptions, rw::IResourceAllocator* lpAllocator,
                               rw::IResourceAllocator* lpDebugAllocator);
        bool Prepare();
        bool Release();
        void Destruct();

        // ---- pure-logic accessors -----------------------------------------------------
        s32         GetPoolIndex(s32 liPoolId);            // 0x828D80E8
        const Type* FindResourceType(u32 luTypeId);        // 0x828D8268

        // ARTIST 0x82904B20 returns void: validate the id, assert on duplicates, initialize the first
        // free slot and keep its assertion/continuation behavior. Original startup is still unactivated.
        void CreatePool(const Pool::InitOptions* lpOptions);

        // ---- original asynchronous pool-create protocol ---------------------------------
        // ARTIST 0x828F3A50 pushes the raw request to the 128-entry FIFO before asserting on output,
        // derives a memory descriptor, then posts a CREATE_RESOURCE request on output tag 10.
        void SendCreatePoolMemoryRequest(const Events::CreatePoolRequest* lpRequest,
                                         PoolIO::OutputBuffer* lpPoolOutput);
        // ARTIST 0x82905000 pops the original raw request, adopts the response's RW resource/descriptor,
        // resolves dependencies, creates the pool, then echoes PoolEvent on output tag 1. Return is void.
        void DoCreatePoolRequest(const CgsMemory::MemoryIO::CreateResourceResponse* lpResponse,
                                 PoolIO::OutputBuffer* lpPoolOutput);
        // Look up a created pool by id (null if absent). Lets the CreatePools driver fetch the pool the
        // receiver-queue path stood up (for cross-pool dependency wiring).
        Pool* GetPool(s32 liPoolId);

        // Direct slot accessor over the fixed bank of 128 pools (X360 mpPoolModule + 0x1A38 + i*stride).
        // The live-update pass sweeps every slot: FindPoolToUpdate polls each pool for the list's
        // resources, and AllocateNewResources re-resolves every pool that depends on the updated one.
        Pool& GetPoolByIndex(s32 liIndex) { return maPools[liIndex]; }

        // @ 0x828FCD48 - acquire a single resource by id: GetPoolIndex(poolId) -> Pool::FindResource(
        // resourceId, checkRefCount, statusMask=2) -> reply with an AcquireResourceResponse carrying the
        // resolved handle (or null if absent) on the pool output queue (tag 6). Dispatched by
        // ProcessInputBuffer for resource request id 4.
        void DoAcquireResourceRequest(const Events::AcquireResourceRequest* lpRequest, PoolIO::OutputBuffer* lpOutput);

        // @ 0x828FCE40 - acquire every resource named by a ResourceIdList resource into the caller's
        // handle array and echo the request back with the filled count (pool output tag 7 -> receiver
        // id 5, per the console's translation table dword_820F7194[7] == 5). Dispatched by
        // ProcessInputBuffer for resource request id 5. This is the per-zone collision protocol
        // (WorldEntityModule::PrepareZoneCollision's "TRK_CLIL<n>" acquires).
        void DoAcquireResourceListRequest(const Events::AcquireResourceListRequest* lpRequest,
                                          PoolIO::OutputBuffer* lpOutput);

        // ---- current two-argument native adapter ---------------------------------------
        // The full ARTIST four-argument Update/Construct activation remains a separate boundary.
        enum EUpdateResult { E_RESULT_OK = 0, E_RESULT_REQUIRE_STALL = 1 };
        EUpdateResult Update(CgsModule::IOBufferStack* lpInputStack, CgsModule::IOBufferStack* lpOutputStack,
                             const PoolIO::InputBuffer* lpInput, PoolIO::OutputBuffer* lpOutput);

        // ---- original typed event dispatch ---------------------------------------------
        void ProcessReceiverQueue(PoolIO::OutputBuffer* lpOutputBuffer);
        void ProcessInputBuffer(const PoolIO::InputBuffer* lpInputBuffer, PoolIO::OutputBuffer* lpOutputBuffer);

        // ---- request -> pool-options conversion and descriptor -------------------------
        // ARTIST 0x828E2ED0 and 0x828EC458 are static two-pointer procedures (r3=request, r4=output).
        static void ConvertPoolRequestOptions(const Events::CreatePoolRequest* lpRequest,
                                              Pool::InitOptions* lpOutOptions);
        static void GetRequiredResourceDescriptor(const Events::CreatePoolRequest* lpRequest,
                                                  rw::ResourceDescriptor* lpOutDescriptor);

        // @ 0x828F3CD0 -- DebugReport: visit every live pool (id != -1 && valid).
        void DebugReport(FPoolReportCallback lpfnCallback, void* lpUserData);

        // ---- per-frame defrag-state drivers (CgsPoolModule.cpp) -----------------------
        // Polled once per frame from the dispatch path; each drives its embedded step machine and, on
        // completion, posts the resource-list response (event id 17) / escalates to the next defrag pass.
        void UpdateAllocating(void* lpOutputBuffer);     // @ 0x82904860
        void UpdateDeAllocating(void* lpOutputBuffer);   // @ 0x828F38E8
        void UpdateIntelliFrag(void* lpOutputBuffer);    // @ 0x829013F8
        void UpdateEmergencyFrag(void* lpOutputBuffer);  // @ 0x829015F0
        void UpdateLiveUpdate(void* lpOutputBuffer);     // @ 0x82906E70

        // ---- request handlers ----------------------------------------------------------
        // @ 0x828EC590 -- forward an AllocateResourceListRequest to AllocateResourceList.
        void DoAllocateResourceListRequest(const void* lpRequest);
        void DoFixUpAndResolveResourceListRequest(
            const Events::FixUpAndResolveResourceListRequest* lpRequest, PoolIO::OutputBuffer* lpOutput);
        void DoUnloadResourceListRequest(
            const Events::UnloadResourceListRequest* lpRequest, PoolIO::OutputBuffer* lpOutput);
        // ARTIST 0x828D81D0 receives both response and output; output is unused by its echo-free body.
        void DoDeletePoolRequest(const CgsMemory::MemoryIO::DestroyBankResponse* lpResponse,
                                 PoolIO::OutputBuffer* lpOutput);
        // ARTIST 0x828FD278: echo the PoolEvent + ID on output tag 9; no reference is released.
        void DoUnacquireResourceRequest(const Events::UnacquireResourceRequest* lpRequest,
                                        PoolIO::OutputBuffer* lpOutput);

        // CgsPoolModule.h (bodies in CgsPoolModule.cpp). Invalidate a
        // pool (reply with its memory + descriptor and whether it was free), validate it again
        // (only while the module is idle), and reset a pool then ask the memory module to
        // destroy its bank.
        void DoInvalidatePoolRequest(const Events::InvalidatePoolRequest* lpRequest, PoolIO::OutputBuffer* lpOutput);
        void DoValidatePoolRequest(const Events::ValidatePoolRequest* lpRequest, PoolIO::OutputBuffer* lpOutput);
        void SendDeletePoolMemoryRequest(const Events::DeletePoolRequest* lpRequest, PoolIO::OutputBuffer* lpOutput);

        // @ 0x828E2F88 -- select the pool and arm ordinary or live-update allocation.
        bool AllocateResourceList(u64 luId, s32 liPoolId, const void* lpEntries, s32 liNumEntries,
                                  bool* lpNeeds, void* lpResources, bool lbLiveUpdateReplace, bool lbAllowFailiure);

    private:
        static void AssertNativeTypeLayout()
        {
            CGS_STATIC_ASSERT(offsetof(PoolModule, mePoolPrepareStage) == 6700, "ARTIST type-table extent");
            CGS_STATIC_ASSERT(offsetof(PoolModule, maPools) == 6712, "ARTIST pool table");
            CGS_STATIC_ASSERT(sizeof(Pool) == 464, "ARTIST pool stride");
            CGS_STATIC_ASSERT(offsetof(PoolModule, mReceiverQueue) == 0x15844, "ARTIST pool receiver");
            CGS_STATIC_ASSERT(offsetof(PoolModule, mScratchPool) == 0x19880, "ARTIST scratch owner");
            CGS_STATIC_ASSERT(offsetof(PoolModule, mAllocListSet) == 0x19b00, "ARTIST allocation list");
            CGS_STATIC_ASSERT(offsetof(PoolModule, mProcessState) == 0x19b30, "ARTIST pool update state");
            CGS_STATIC_ASSERT(offsetof(PoolModule, mRelocator) == 0x19d00, "ARTIST relocator owner");
            CGS_STATIC_ASSERT(offsetof(PoolModule, mRelocationParams) == 0x1a100, "ARTIST relocation parameters");
            CGS_STATIC_ASSERT(offsetof(PoolModule, mPendingAllocationRequests) == 0x1a110, "ARTIST pending allocations");
            CGS_STATIC_ASSERT(sizeof(PoolModule) == 0x1a200, "ARTIST pool module span");
        }
        // ---- Layout (faithful order; x64 widths; compiler-laid-out; incremental) ------
        s32               mNumTypes;                 // +0x228 (a1[138]) registered type count
        ResourceTypeEntry maTypes[KI_MAX_TYPES];     // +0x22C type registry (12B X360 entries)
        EPoolPrepareStage mePoolPrepareStage;        // +0x1A2C (a1[1675])
        EPoolReleaseStage mePoolReleaseStage;        // +0x1A30 (a1[1676])
        Pool              maPools[KI_MAX_POOLS];      // +0x1A38 the 128 resource pools (464B X360 stride)
        // ARTIST CgsPoolModule.h:194 -- canonical raw CreatePoolRequest FIFO (128 entries),
        // constructed with the module and driven by the canonical request/response processors.
        FifoQueue<Events::CreatePoolRequest, 128> mPendingCreateRequests;
        CgsModule::EventReceiverQueue<16384, 16> mReceiverQueue; // +0x15844 (a1[22033]) create/delete-pool events (CgsPoolModule.h)
        ScratchPool       mScratchPool;              // +0x198C0 (a1+104576) defrag staging (Construct'd; InitPool deferred)
        AllocListSet      mAllocListSet;             // +0x19B00; DWARF CgsPoolModule.h:198
        s32               mProcessState;             // +0x19B30 defrag/alloc state machine (0..6)

        // ---- defrag / allocate state machine (DWARF CgsPoolModule.h:202-219) ----------------------
        // Added with the UpdateXxx defrag-state driver pass (CgsPoolModule.cpp). mProcessState above is
        // the X360 +0x19B30 dispatch state the drivers set (the EUpdateState values); the cluster below
        // is the working set the drivers poll/marshal. Semantic parity (named members), NOT byte-match.
        s32               miAllocateRequestEventId;   // [+0x19B34] (a1[26317]) in-flight request's event id, echoed into the response

        // The five step-machine sub-objects, embedded by value (each its own reconstructed type). The
        // drivers poll them through their public Update / Begin / GenerateResponse entry points only.
        AllocatePoolModuleState      mAllocateState;       // [+0x19B38] (DWARF :203)
        DeAllocatePoolModuleState    mDeAllocateState;     // :204
        IntelliFragPoolModuleState   mIntelliFragState;    // :205
        LiveUpdatePoolModuleState    mLiveUpdateState;     // :206
        EmergencyFragPoolModuleState mEmergencyFragState;  // :207

        // Six allocator-carved defrag scratch arrays (base ptrs; their target buffers come from the rw
        // allocator -- deferred). Named per the DWARF types (:209-214).
        AllocRequestAddressed* mpAddressedAllocRequests;   // :209
        RelocateRequest*       mpRelocateRequests;         // :210
        DistributionEntry*     mpDistributionEntries;      // :211
        RelocationEntry*       mpRelocationEntries;        // :212
        LinearHeapNode*        mpLinearHeapNodes;          // :213
        RelocateSource*        mpRelocateSources;          // :214

        // The block-copy engine the EmergencyFrag escalation drives, plus the parameter block it
        // hands it. Both are embedded by value, immediately after the defrag scratch arrays.
        CgsMemory::Relocator        mRelocator;          // :216
        CgsMemory::RelocationParams mRelocationParams;   // :217

        // ARTIST 829076D8 dequeues at most one request while idle.
        FifoQueue<Events::AllocateResourceListRequest, 4> mPendingAllocationRequests;
    };
}
