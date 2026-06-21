#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // mReceiverQueue
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"    // Pool (128 embedded)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"    // Type (type table)

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

        // ---- lifecycle (Construct is rw-allocator-gated -> deferred) -------------------
        void Construct(const void* lpInitOptions, void* lpAllocator);
        bool Prepare();
        bool Release();
        void Destruct();

        // ---- pure-logic accessors -----------------------------------------------------
        s32         GetPoolIndex(s32 liPoolId);            // 0x828D80E8
        const Type* FindResourceType(u32 luTypeId);        // 0x828D8268

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
        CgsModule::BaseEventReceiverQueue mReceiverQueue; // +0x158C4 (a1[22033]) create/delete-pool events
        s32               mProcessState;             // +0x19B30 defrag/alloc state machine (0..6)
        // (embedded ScratchPool / Relocator / defrag-state cluster / EA Job / RW mutexes /
        //  resource registry / typed request queues are added with the Construct + dispatch
        //  + defrag passes that use them.)
    };
}
