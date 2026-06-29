#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // mReceiverQueue
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"    // Pool (128 embedded)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"    // Type (type table)
#include "GameShared/GameClasses/System/Resource/CgsResourceScratchPool.h" // ScratchPool (defrag staging, embedded)
#include "GameShared/GameClasses/System/Resource/CgsResourceHeap.h"     // AllocRequestAddressed/RelocateRequest/RelocateSource/LinearHeapNode (defrag scratch buffers)
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsAllocatePoolModuleState.h"     // mAllocateState (defrag-state cluster)
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsDeAllocatePoolModuleState.h"   // mDeAllocateState
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsIntelliFragPoolModuleState.h"  // mIntelliFragState / IntelliFragParams
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsLiveUpdatePoolModuleState.h"   // mLiveUpdateState
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsEmergencyFragPoolModuleState.h"// mEmergencyFragState / EmergencyFragParams
#include "rw/rwcore_structs.h"                                          // rw::Resource / ResourceDescriptor (InitOptions)

// Async pool-create dispatch (CreateResourceRequest -> MemoryModule -> CreateResourceResponse).
namespace CgsMemory { namespace MemoryIO { struct InputBuffer; struct CreateResourceResponse; } }
namespace CgsResource { namespace PoolIO { struct OutputBuffer; } }   // SendCreatePoolMemoryRequest target
namespace CgsResource { namespace Events { struct AcquireResourceRequest; } }   // DoAcquireResourceRequest
// Defrag distribution/relocation records (pointer members only; full layouts live in the deferred
// defrag subsystem -- forward-declared to avoid a transitive cascade for pointer-only storage).
namespace CgsResource { struct DistributionEntry; struct RelocationEntry; }

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
// Layout: faithful field order; x64 widths; the PC compiler lays the class out (we identify
// members by the X360 offsets but do NOT byte-match). Populated incrementally: this pass
// lands the members the pure-logic spine needs (type table, the 128 Pools, the two stage
// machines, the receiver queue) plus the embedded defrag infra (ScratchPool/Relocator);
// the defrag-state cluster, the EA Job, the RW mutexes, the embedded resource registry and
// the typed request queues are added with the passes that use them.
//
// DEFER STATUS: this home reconstructs the rw-allocator-INDEPENDENT spine - GetPoolIndex,
// FindResourceType, and the Prepare/Release/Destruct stage machines (they only iterate the
// 128 Pools + the base module + the receiver queue) - plus, in the CgsPoolModule.cpp ledger TU
// (the X360 source path for the same class), the per-frame defrag-state DRIVERS (UpdateAllocating /
// UpdateDeAllocating / UpdateIntelliFrag / UpdateLiveUpdate), the request handlers
// (ConvertPoolRequestOptions / DoAllocateResourceListRequest / DoDeletePoolRequest / DebugReport),
// and the embedded defrag-state cluster they poll. Still DEFERRED (rw-allocator middleware / defrag
// subsystem, trap-stubbed at link): Construct's rw-allocator back half (the 128 pools' backing memory),
// AllocateResourceList, the step machines' own Update/Begin/GenerateResponse bodies, and the Relocator -
// nothing drives them until the GameDataModule runs.
//
// NOTE: this header is the single canonical home for CgsResource::PoolModule. The defrag-state
// drivers + their working-set members were folded in here (additively) so the CgsPoolModule.cpp TU
// compiles against ONE class definition - the earlier dual-header ODR (a separate CgsPoolModule.h
// redefining the class) is resolved.
namespace CgsResource
{
    // The CreatePool request payload CreatePools publishes to the ResourceModule input (event id 0).
    // ProcessResourceRequests routes it to the pool input; PoolModule::ProcessInputBuffer dispatches it to
    // SendCreatePoolMemoryRequest. [x64 payload deviation: carries the resolved InitOptions + bank/sizing.
    // The X360 carries the raw 172B pool def and derives these via ConvertPoolRequestOptions /
    // GetRequiredResourceDescriptor, whose byte counts are x360-width-specific -- so the resolved form is
    // the faithful x64 choice; see the gamedatamodule-scoping notes.]
    struct CreatePoolRequestEvent
    {
        Pool::InitOptions mOptions;
        s32               miBankId;
        s32               miParentBankId;
        u32               mauRegion[3];
        u32               mauAlign[3];
    };

    class PoolModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        static const s32 KI_MAX_POOLS = 128;
        static const s32 KI_MAX_TYPES = 128;
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
        // [ARTIST 5-TYPE DRIFT: the X360 mDefragBufferDescriptor is a 5-type ResourceDescriptor (40B)
        // and mDefragBufferResource is 5 base ptrs; modelled here with the rw 4-type typedefs since the
        // defrag buffer is a deferred subsystem -- widen to 5-type when the defrag path is reconstructed.]
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

        // ---- lifecycle (Construct is rw-allocator-gated -> deferred) -------------------
        void Construct(const void* lpInitOptions, void* lpAllocator);
        bool Prepare();
        bool Release();
        void Destruct();

        // ---- pure-logic accessors -----------------------------------------------------
        s32         GetPoolIndex(s32 liPoolId);            // 0x828D80E8
        const Type* FindResourceType(u32 luTypeId);        // 0x828D8268

        // @ 0x82904B20 - stand up a pool from InitOptions: validate the id, assert no duplicate, find a
        // free slot (GetId()==-1), Pool::InitPool it. Returns the created pool (null if no free slot).
        // The dispatch (DoCreatePoolRequest) calls this after allocating the pool's backing memory.
        Pool* CreatePool(const Pool::InitOptions* lpOptions);

        // ---- async pool-create dispatch -----------------------------------------------
        // @ 0x828F3A50 - SendCreatePoolMemoryRequest: queue the pending pool options (FIFO) and emit a
        // CreateResource memory request (onto this module's PoolIO::OutputBuffer resource-request queue) so
        // the ResourceModule shuttle forwards it to the MemoryModule, which carves the pool's backing bank.
        // miBankId/region/align/parentBankId describe the bank. The output buffer must be write-locked by
        // the caller (the X360 holds it locked across PoolModule::Update's ProcessInputBuffer).
        void SendCreatePoolMemoryRequest(const Pool::InitOptions& lrOptions, s32 liBankId, s32 liParentBankId,
                                         const u32 lauRegion[3], const u32 lauAlign[3],
                                         PoolIO::OutputBuffer* lpPoolOutput);

        // @ 0x82905000 - DoCreatePoolRequest: pop the matching pending options (FIFO), adopt the memory
        // the MemoryModule allocated (the response's rw::Resource data pointers), and CreatePool. Reached
        // via the receiver queue (event id 10) from ProcessReceiverQueue.
        Pool* DoCreatePoolRequest(const CgsMemory::MemoryIO::CreateResourceResponse* lpResponse);

        // Per-response part of the ResourceModule memory-response shuttle: forward a CreateResource memory
        // response onto its originating receiver queue (the request's reply target) as a CreatePool event
        // (id 10), so ProcessReceiverQueue dispatches it to DoCreatePoolRequest. [the full multi-submodule
        // ProcessMemoryResponses shuttle is still deferred -- this is its pool-create slice.]
        void ProcessMemoryResponse(const CgsMemory::MemoryIO::CreateResourceResponse* lpResponse);

        // Look up a created pool by id (null if absent). Lets the CreatePools driver fetch the pool the
        // receiver-queue path stood up (for cross-pool dependency wiring).
        Pool* GetPool(s32 liPoolId);

        // @ 0x828FCD48 - acquire a single resource by id: GetPoolIndex(poolId) -> Pool::FindResource(
        // resourceId, checkRefCount, statusMask=2) -> reply with an AcquireResourceResponse carrying the
        // resolved handle (or null if absent) on the pool output queue (tag 6). Dispatched by
        // ProcessInputBuffer for resource request id 4.
        void DoAcquireResourceRequest(const Events::AcquireResourceRequest* lpRequest, PoolIO::OutputBuffer* lpOutput);

        // ---- dispatch (deferred) ------------------------------------------------------
        bool Update(void* lpInputBuffer, void* lpOutputBuffer);
        void ProcessReceiverQueue(void* lpOutputBuffer);
        void ProcessInputBuffer(void* lpInputBuffer, void* lpOutputBuffer);

        // ---- request -> pool-options conversion (CgsPoolModule.cpp:1027) --------------
        // @ 0x828E2ED0 -- copy a CreatePoolRequest into a Pool::InitOptions (id/name/heap sizing).
        void ConvertPoolRequestOptions(const void* lpRequest, void* lpOutOptions);

        // @ 0x828F3CD0 -- DebugReport: visit every live pool (id != -1 && valid).
        void DebugReport(FPoolReportCallback lpfnCallback, void* lpUserData);

        // ---- per-frame defrag-state drivers (CgsPoolModule.cpp) -----------------------
        // Polled once per frame from the dispatch path; each drives its embedded step machine and, on
        // completion, posts the resource-list response (event id 17) / escalates to the next defrag pass.
        void UpdateAllocating(void* lpOutputBuffer);     // @ 0x82904860
        void UpdateDeAllocating(void* lpOutputBuffer);   // @ 0x828F38E8
        void UpdateIntelliFrag(void* lpOutputBuffer);    // @ 0x829013F8
        void UpdateLiveUpdate(void* lpOutputBuffer);     // @ 0x82906E70

        // ---- request handlers ----------------------------------------------------------
        // @ 0x828EC590 -- forward an AllocateResourceListRequest to AllocateResourceList.
        void DoAllocateResourceListRequest(const void* lpRequest);
        // @ 0x828D81D0 -- DeletePool response handler (asserts on failure result).
        void DoDeletePoolRequest(const void* lpResponse);

        // @ 0x82900690 -- AllocateResourceList (own TU): walk the bundle entries, batch the per-pool
        // allocations, drive the defrag state machine. First arg is an 8-byte resource ID (asm ld 0x10).
        bool AllocateResourceList(u64 luId, s32 liEventId, const void* lpEntries, s32 liNumEntries,
                                  bool* lpbOut, void* lpHandles, bool lbA, bool lbB);

    private:
        // ---- Layout (faithful order; x64 widths; compiler-laid-out; incremental) ------
        s32               mNumTypes;                 // +0x228 (a1[138]) registered type count
        ResourceTypeEntry maTypes[KI_MAX_TYPES];     // +0x22C type registry (12B X360 entries)
        EPoolPrepareStage mePoolPrepareStage;        // +0x1A2C (a1[1675])
        EPoolReleaseStage mePoolReleaseStage;        // +0x1A30 (a1[1676])
        Pool              maPools[KI_MAX_POOLS];      // +0x1A38 the 128 resource pools (464B X360 stride)
        CgsModule::EventReceiverQueue<16384, 16> mReceiverQueue; // +0x158C4 (a1[22033]) create/delete-pool events (DWARF CgsPoolModule.h:217)
        ScratchPool       mScratchPool;              // +0x198C0 (a1+104576) defrag staging (Construct'd; InitPool deferred)
        s32               mProcessState;             // +0x19B30 defrag/alloc state machine (0..6)

        // CgsResource::Events::CreatePoolRequest_128 (X360 a1+66104): the FIFO of pool options awaiting
        // their backing-memory allocation. SendCreatePoolMemoryRequest pushes; DoCreatePoolRequest pops
        // when the matching memory response arrives. Modelled as a 128-slot ring of Pool::InitOptions
        // (the X360 stores the raw 172B pool request; we carry the resolved InitOptions). [marked]
        static const s32 KI_MAX_PENDING_CREATE = 128;
        Pool::InitOptions maPendingCreate[KI_MAX_PENDING_CREATE];
        s32               miPendingHead = 0;   // FIFO empty until CreatePool path drives it (not ctor-set)
        s32               miPendingTail = 0;
        s32               miPendingCount = 0;

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

        // [DWARF :216/:217] CgsMemory::Relocator (~1088B) + RelocationParams -- not reconstructed as a
        // complete type, so the embedded region is explicit padding (EA-Job / RW-mutex relocator infra
        // is a deferred subsystem; Construct'd inline, addressed-only by the EmergencyFrag escalation).
        u8 mPadRelocator[1152];

        // [DWARF :219] mPendingAllocationRequests (FifoQueue<AllocateResourceListRequest,4>) -- not
        // reconstructed as a complete type, explicit padding.
        u8 mPadPendingAllocationRequests[64];
    };
}
