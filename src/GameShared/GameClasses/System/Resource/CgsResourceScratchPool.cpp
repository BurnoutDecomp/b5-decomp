#include "GameShared/GameClasses/System/Resource/CgsResourceScratchPool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"  // PoolModule::InitOptions (the X360 a1)
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

    // @ 0x828E2BB8 - size the scratch pool's overhead block from the pool-manager's
    // InitOptions (PoolModule::InitOptions, the X360 r3). Three contributions are summed:
    //
    //   (1) the staged-entry table:  44 * (miMaxResourceToDefrag + 1)
    //       (ASM: r11 = *a1; r11 += 1; r30 = r11 * 0x2C).  0x2C/entry = the 20-byte Entry
    //       plus its parallel 8-byte id-map slot, rounded to the table stride.
    //
    //   (2) the id->index hash table:  16 * (NextPow2Minus1(3*N - 1) + 0x41)
    //       where N = miMaxResourceToDefrag. The ASM computes, on a 64-bit value, the classic
    //       round-(n)-up-to-(2^k - 1) bit cascade (n |= n>>1; >>2; >>4; >>8; >>16) over
    //       (3*N - 1), adds 0x41 (65) slots of headroom, then <<4 (x16 bytes per hash slot).
    //
    //   (3) the gather/scatter distribution-buffer term:  v5[0]
    //       produced by sub_82BBCF10(&v5, &options.mDefragBufferResource[..], &options
    //       .mDefragBufferDescriptor[1]) - a distribution-stream sizing helper that walks the
    //       defrag buffer's per-memtype descriptor/resource pair. sub_82BBCF10 is NOT a
    //       recovered TU (external/unknown) and its result is the only un-grounded term here,
    //       so it is carried as a clearly-flagged 0 placeholder rather than fabricated. It is
    //       the deferred defrag-buffer contribution; reconstruct it (and call it here) with the
    //       PoolModule defrag-state machine / DistributionStream sizing subsystem.
    u32 ScratchPool::GetOverheadMemoryRequired(const void* lpInitOptions)
    {
        const PoolModule::InitOptions* lpOptions =
            static_cast<const PoolModule::InitOptions*>(lpInitOptions);
        const u32 luMaxEntries = static_cast<u32>(lpOptions->miMaxResourceToDefrag);

        // (1) staged-entry table.
        const u32 luEntryTableBytes = 44u * (luMaxEntries + 1u);

        // (2) hash table: smallest (2^k - 1) >= (3*N - 1), + 65 slots, * 16 bytes/slot.
        u32 luHashSlots = 3u * luMaxEntries - 1u;
        luHashSlots |= luHashSlots >> 1;
        luHashSlots |= luHashSlots >> 2;
        luHashSlots |= luHashSlots >> 4;
        luHashSlots |= luHashSlots >> 8;
        luHashSlots |= luHashSlots >> 16;
        const u32 luHashTableBytes = 16u * (luHashSlots + 0x41u);

        // (3) defrag distribution-buffer term - un-recoverable (sub_82BBCF10); flagged-0.
        const u32 luDistributionBufferBytes = 0u;  // FLAGGED PLACEHOLDER: deferred (sub_82BBCF10)

        return luEntryTableBytes + luHashTableBytes + luDistributionBufferBytes;
    }
    void ScratchPool::InitPool(const void* /*lpInitOptions*/) {}
    void* ScratchPool::AddEntry(const void* /*lpDesc*/, u32 /*luId*/, s32 /*liMemType*/, u32 /*luUserData*/) { return 0; }
    s32  ScratchPool::BeginDistribution(s32 /*liMemType*/) { return 0; }
    bool ScratchPool::UpdateGather() { return true; }
    bool ScratchPool::UpdateScatter() { return true; }
    void ScratchPool::BuildGatherDistributionList() {}
    void ScratchPool::BuildScatterDistributionList() {}
}
