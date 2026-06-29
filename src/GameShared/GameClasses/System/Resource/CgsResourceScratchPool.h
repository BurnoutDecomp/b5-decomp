#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"        // LinearMalloc (master + per-memtype)
#include "GameShared/GameClasses/Memory/CgsGatherStream.h"        // GatherStream  (scattered -> packed)
#include "GameShared/GameClasses/Memory/CgsScatterStream.h"       // ScatterStream (packed -> scattered)
#include "GameShared/GameClasses/Memory/CgsDistributionStream.h"  // DistributionStreamEntry

// CgsResource::ScratchPool - the staging buffer the resource defragmenter relocates live
// resources through. During a defrag pass a Pool: (1) AddEntry's the live resources that
// must move into the scratch pool's per-memtype LinearMalloc buffers; (2) BeginDistribution
// + UpdateGather copy them out of the live heap into that packed scratch space (GatherStream);
// then (3) UpdateScatter copies them back to their new homes (ScatterStream). It is pure
// defrag infrastructure - PoolModule embeds one by value and InitPool's it at construction,
// but nothing touches it until a pool actually defragments.
//
// SOURCES (X360 ARTIST, CgsResourceScratchPool.{h,cpp}): Clear 0x828EDE08, GetEntry
// 0x828D7E50, AddEntry 0x828EDE80, GetOverheadMemoryRequired 0x828E2BB8, BeginDistribution
// 0x828E3398, UpdateGather 0x828E3568, UpdateScatter 0x828D8DE0, BuildGatherDistributionList
// 0x828D8EA8, BuildScatterDistributionList 0x828D9330. Construct + InitPool have no standalone
// export - they are inlined into PoolModule::Construct 0x828FC0B8 (the 4x LinearMalloc::Construct
// + 2x stream Construct + 10MB budgets + field zeroing visible there). Layout order is faithful
// to the X360; widths follow x64 (LinearMalloc/streams are larger than the X360's 28/36 bytes),
// so the PC compiler lays the class out - offsets are NOT byte-matched.
//
// DEFER STATUS: this lands the type + the fully-visible bodies (Construct/Clear/GetEntry +
// GetOverheadMemoryRequired). GetOverheadMemoryRequired (0x828E2BB8) is reconstructed bar one
// un-grounded term: its gather/scatter distribution-buffer contribution comes from sub_82BBCF10
// (external/unknown, not a recovered TU), carried as a flagged-0 placeholder. The rest of the
// defrag-streaming surface (InitPool's scratch-memory carving, AddEntry, BeginDistribution/
// UpdateGather/UpdateScatter/Build*DistributionList) is DEFERRED with the resource-defrag
// subsystem (they pair with the PoolModule defrag-state machine), and are inert marked stubs -
// nothing drives them until a pool defragments.
namespace CgsResource
{
    class ScratchPool
    {
    public:
        static const s32 KI_NUM_MEMTYPES = 3;   // main / graphics-system / graphics-local

        enum EStage   // mStage @ +120 (a1[30])
        {
            E_STAGE_IDLE    = 0,
            E_STAGE_GATHER  = 1,
            E_STAGE_SCATTER = 2,
        };

        // A staged resource awaiting relocation (20 bytes on X360 / 5 dwords).
        struct Entry
        {
            u32 muId;             // [0x00] resource id / type tag
            u8* mpResourceAddr;   // [0x04] the resource's real (scattered) address
            u8* mpScratchAddr;    // [0x08] its packed slot inside the scratch buffer
            u32 muField12;        // [0x0C]
            u32 muSize;           // [0x10] byte size (16-aligned)
        };

        // The sizing/init/defrag surface (see DEFER STATUS).
        static u32 GetOverheadMemoryRequired(const void* lpInitOptions);
        void Construct();
        void InitPool(const void* lpInitOptions);
        void Clear();
        Entry* GetEntry(u32 luIndex);
        void* AddEntry(const void* lpDesc, u32 luId, s32 liMemType, u32 luUserData);
        s32  BeginDistribution(s32 liMemType);
        bool UpdateGather();
        bool UpdateScatter();
        void BuildGatherDistributionList();
        void BuildScatterDistributionList();

    private:
        // ---- Layout (faithful order; x64 widths; compiler-laid-out) -------------------
        CgsMemory::LinearMalloc mMasterAllocator;                       // +0x00   master scratch backing
        CgsMemory::LinearMalloc maMemTypeAllocators[KI_NUM_MEMTYPES];   // +0x1C  per-memtype packed buffers
        EStage       mStage;                           // +0x78 (a1[30]) defrag stage
        u32          mField31;                         // +0x7C (a1[31])
        Entry*       mpEntries;                        // +0x80 (a1[32]) staged-entry array
        void*        mpIdList;                         // +0x84 (a1[33]) parallel 8-byte id list
        u32          muNumEntries;                     // +0x88 (a1[34])
        u32          muMaxEntries;                     // +0x8C (a1[35])
        s32          miMemType;                        // +0x90 (a1[36]) current distribution memtype
        CgsMemory::GatherStream  mGatherStream;        // +0x100 (a1[64]) scattered -> packed
        CgsMemory::ScatterStream mScatterStream;       // +0x180 (a1[96]) packed -> scattered
        CgsMemory::DistributionStreamEntry* mpDistributionEntries; // +0x200 (a1[128]) built distribution list
        u32          muIdMapCount;                     // +0x20C (a1[131]) Clear()'s 8-byte-stride table count
        u32          muIdMapClearValue;                // +0x210 (a1[132])
        void*        mpIdMap;                          // +0x218 (a1[134])
    };
}
