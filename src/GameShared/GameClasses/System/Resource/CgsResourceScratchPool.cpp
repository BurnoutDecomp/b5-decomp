#include "GameShared/GameClasses/System/Resource/CgsResourceScratchPool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsResource::ScratchPool - see the header. This pass lands the type + the fully-visible
// simple bodies (Construct/Clear/GetEntry). The sizing + defrag-streaming surface is
// DEFERRED with the resource-defrag subsystem (inert marked stubs).
namespace CgsResource
{
    // Inlined into PoolModule::Construct 0x828FC0B8: construct the master + 3 per-memtype
    // bump allocators and the two distribution streams, give each stream a 10 MB/Update
    // budget, and clear the cursor/entry fields. (The scratch backing memory is adopted
    // later by InitPool, once the rw allocator has handed it the overhead block.)
    void ScratchPool::Construct()
    {
        mMasterAllocator.Construct();
        for (s32 li = 0; li < KI_NUM_MEMTYPES; ++li)
            maMemTypeAllocators[li].Construct();

        mGatherStream.Construct();
        mScatterStream.Construct();
        mGatherStream.SetBytesPerUpdate(10485760);   // X360: *(gather+0) = 10 MB
        mScatterStream.SetBytesPerUpdate(10485760);  // X360: *(scatter+0) = 10 MB

        mStage                = E_STAGE_IDLE;
        mField31              = 0;
        mpEntries             = 0;
        mpIdList              = 0;
        muNumEntries          = 0;
        muMaxEntries          = 0;
        miMemType             = 0;
        mpDistributionEntries = 0;
        muIdMapCount          = 0;
        muIdMapClearValue     = 0;
        mpIdMap               = 0;
    }

    // @ 0x828EDE08 - reset for reuse: drop the staged entries, rewind each per-memtype
    // allocator, and clear the id map. (The master backing region is left intact.)
    void ScratchPool::Clear()
    {
        muNumEntries = 0;
        for (s32 li = 0; li < KI_NUM_MEMTYPES; ++li)
            maMemTypeAllocators[li].FreeAll();

        // X360: for (i < muIdMapCount) *(u32*)(mpIdMap + 8*i) = muIdMapClearValue;
        if (mpIdMap != 0)
        {
            u8* lpMap = static_cast<u8*>(mpIdMap);
            for (u32 lu = 0; lu < muIdMapCount; ++lu)
                *reinterpret_cast<u32*>(lpMap + 8u * lu) = muIdMapClearValue;
        }
    }

    // @ 0x828D7E50 - bounds-checked access into the staged-entry array.
    ScratchPool::Entry* ScratchPool::GetEntry(u32 luIndex)
    {
        CGS_ASSERT(luIndex < muNumEntries, "Entry out of range");
        return &mpEntries[luIndex];
    }

    // ---- DEFERRED with the resource-defrag subsystem ---------------------------------
    // GetOverheadMemoryRequired (0x828E2BB8) sizes the scratch overhead from the pool's
    // InitOptions (hash table + entry array + id map); InitPool carves that block (it has
    // no standalone X360 export - it is inlined into PoolModule::Construct, and its true
    // scratch-memory partitioning isn't independently recoverable). AddEntry (0x828EDE80)
    // and the BeginDistribution/UpdateGather/UpdateScatter/Build*DistributionList bodies
    // (0x828E3398/0x828E3568/0x828D8DE0/0x828D8EA8/0x828D9330) drive the GatherStream/
    // ScatterStream during a defrag pass. All inert until a pool defragments; reconstruct
    // them with the PoolModule defrag-state machine. The distribution-stream data movers
    // they call (Gather/Scatter/DistributionStream) are already fully reconstructed.
    u32  ScratchPool::GetOverheadMemoryRequired(const void* /*lpInitOptions*/) { return 0; }
    void ScratchPool::InitPool(const void* /*lpInitOptions*/) {}
    void* ScratchPool::AddEntry(const void* /*lpDesc*/, u32 /*luId*/, s32 /*liMemType*/, u32 /*luUserData*/) { return 0; }
    s32  ScratchPool::BeginDistribution(s32 /*liMemType*/) { return 0; }
    bool ScratchPool::UpdateGather() { return true; }
    bool ScratchPool::UpdateScatter() { return true; }
    void ScratchPool::BuildGatherDistributionList() {}
    void ScratchPool::BuildScatterDistributionList() {}
}
