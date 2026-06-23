#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // mReceiverQueue
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"    // Pool (128 embedded)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"    // Type (type table)
#include "GameShared/GameClasses/System/Resource/CgsResourceScratchPool.h" // ScratchPool (defrag staging, embedded)
#include "rw/rwcore_structs.h"                                          // rw::Resource / ResourceDescriptor (InitOptions)

// Async pool-create dispatch (CreateResourceRequest -> MemoryModule -> CreateResourceResponse).
namespace CgsMemory { namespace MemoryIO { struct InputBuffer; struct CreateResourceResponse; } }
namespace CgsResource { namespace PoolIO { struct OutputBuffer; } }   // SendCreatePoolMemoryRequest target

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
// DEFER STATUS: this pass reconstructs the rw-allocator-INDEPENDENT spine - GetPoolIndex,
// FindResourceType, and the Prepare/Release/Destruct stage machines (they only iterate the
// 128 Pools + the base module + the receiver queue). Construct (allocates the 128 pools'
// memory through the rw::IResourceAllocator), the dispatch spine (Update / ProcessInputBuffer
// / ProcessReceiverQueue), all DoXxxRequest handlers, and the UpdateXxx + defrag-state
// machine are DEFERRED (rw-allocator middleware / defrag subsystem) as inert marked stubs -
// nothing drives them until the GameDataModule runs.
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

        // PoolModule's own prepare/release stage machine (distinct from the base module's).
        enum EPoolPrepareStage { E_POOLPREPARE_START = 0, E_POOLPREPARE_BASE = 1, E_POOLPREPARE_POOLS = 2, E_POOLPREPARE_DONE = 3 };
        enum EPoolReleaseStage { E_POOLRELEASE_START = 0, E_POOLRELEASE_POOLS = 1, E_POOLRELEASE_BASE = 2, E_POOLRELEASE_DONE = 3 };

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

        // "New module": skip ModuleSingleBuffered's old DataStructure IO path (X360 *(this+4)=1).
        PoolModule() { mbIsNewModule = true; miPendingHead = 0; miPendingTail = 0; miPendingCount = 0; }

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

        // ---- dispatch (deferred) ------------------------------------------------------
        bool Update(void* lpInputBuffer, void* lpOutputBuffer);
        void ProcessReceiverQueue(void* lpOutputBuffer);
        void ProcessInputBuffer(void* lpInputBuffer, void* lpOutputBuffer);

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
        s32               miPendingHead;
        s32               miPendingTail;
        s32               miPendingCount;
        // (embedded ScratchPool / Relocator / defrag-state cluster / EA Job / RW mutexes /
        //  resource registry / typed request queues are added with the Construct + dispatch
        //  + defrag passes that use them.)
    };
}
