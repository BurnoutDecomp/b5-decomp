#pragma once

#include "GameShared/GameClasses/System/Resource/CgsResourceBasePool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceImportHashTable.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/Memory/CgsGatherStream.h"
#include "GameShared/GameClasses/Memory/CgsScatterStream.h"
#include "rw/rwcore_structs.h"

namespace CgsResource
{
    // DWARF CgsResourceScratchPool.h:36; ARTIST AddEntry 828EDE80 and
    // GetEntry 828D7E50. The three locations are native pointers, including
    // the destination that the earlier header represented as a 32-bit word.
    struct ScratchEntry
    {
        s32 miEntryId;
        void* mpSrcLocation;
        void* mpTempLocation;
        void* mpDestLocation;
        u32 muSize;
    };

    // Stages live resources through packed scratch memory in bounded updates.
    // Original behavior comes from ARTIST; record widths follow the native ABI.
    class ScratchPool : public BasePool
    {
    public:
        static const u32 KU_SCRATCH_MEMORY_ALIGNMENT = 128;
        enum EPrepareStage { E_PREPARESTAGE_START = 0, E_PREPARESTAGE_DONE = 1 };
        enum EReleaseStage { E_RELEASESTAGE_START = 0, E_RELEASESTAGE_DONE = 1 };
        enum EUpdateStage
        {
            E_UPDATESTAGE_IDLE = 0,
            E_UPDATESTAGE_GATHERING = 1,
            E_UPDATESTAGE_SCATTERING = 2
        };

        // ARTIST ScratchPool::InitPool 828F6B60 reads all five RW resource
        // lanes through the existing SmallResource conversion helpers.
        struct InitOptions
        {
            u32 muMaxEntries;
            void* mpOverhead;
            u32 muOverheadMemorySize;
            rw::ResourceDescriptor mDescriptor;
            rw::Resource mResource;
            s32 miBankId;
        };

        static u32 GetOverheadMemoryRequired(const InitOptions* lpOptions);
        void Construct();
        bool Prepare();
        void InitPool(const InitOptions* lpOptions);
        void Clear();
        ScratchEntry* GetEntry(u32 luIndex);
        void* AddEntry(Entry* lpEntry, s32 liEntryId, s32 liMemType, void* lpDestLocation);
        void BeginDistribution(s32 liMemType);
        bool UpdateGather();
        bool UpdateScatter();
        void SortIdList();
        static s32 SortIdsQSortCallback(const void* lpLeft, const void* lpRight);

        s32 GetBankId() { return miBankId; }
        u32 GetNumEntries() const { return muNumEntries; }
        const ID* GetIdList() const { return mpEntryIds; }
        void* GetTempAddress(ID lId) { return mImportHashTable.GetTempAddress(lId); }
        void* GetDestAddress(ID lId) { return mImportHashTable.GetDestAddress(lId); }

    private:
        void BuildGatherDistributionList();
        void BuildScatterDistributionList();

        CgsMemory::LinearMalloc mAllocator;
        CgsMemory::LinearMalloc maResourceAllocators[KI_NUM_TYPES];
        EPrepareStage mePrepareStage;
        EReleaseStage meReleaseStage;
        EUpdateStage meUpdateStage;
        s32 miBankId;
        ScratchEntry* mpEntries;
        ID* mpEntryIds;
        u32 muNumEntries;
        u32 muMaxEntries;
        s32 miCurrentMemType;
        CgsMemory::GatherStream mGatherStream;
        CgsMemory::ScatterStream mScatterStream;
        CgsMemory::DistributionStreamEntry* mpDistributionEntries;
        ImportHashTable mImportHashTable;
    };
}
