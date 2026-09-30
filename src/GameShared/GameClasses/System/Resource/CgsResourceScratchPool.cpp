#include "GameShared/GameClasses/System/Resource/CgsResourceScratchPool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <cstdlib>
#include <cstdint>

namespace CgsResource
{
    namespace
    {
        // ARTIST 828F6BE4..828F6C28: next power of two >= 3 * maxEntries.
        u32 ScratchHashLength(u32 luMaxEntries)
        {
            u32 luLength = 3u * luMaxEntries - 1u;
            luLength |= luLength >> 1;
            luLength |= luLength >> 2;
            luLength |= luLength >> 4;
            luLength |= luLength >> 8;
            luLength |= luLength >> 16;
            return luLength + 1u;
        }
    }

    // Inlined in PoolModule::Construct, ARTIST 828FC0B8.
    void ScratchPool::Construct()
    {
        mAllocator.Construct();
        for (s32 li = 0; li < KI_NUM_TYPES; ++li)
            maResourceAllocators[li].Construct();
        mGatherStream.Construct();
        mScatterStream.Construct();
        mGatherStream.SetBytesPerUpdate(10485760);
        mScatterStream.SetBytesPerUpdate(10485760);
        mePrepareStage = E_PREPARESTAGE_START;
        meReleaseStage = E_RELEASESTAGE_DONE;
        meUpdateStage = E_UPDATESTAGE_IDLE;
        miBankId = -1;
        mpEntries = nullptr;
        mpEntryIds = nullptr;
        muNumEntries = 0;
        muMaxEntries = 0;
        miCurrentMemType = 0;
        mpDistributionEntries = nullptr;
        mImportHashTable = ImportHashTable{};
    }

    // Inlined in ARTIST PoolModule::Construct before ScratchPool::InitPool.
    bool ScratchPool::Prepare()
    {
        if (mePrepareStage == E_PREPARESTAGE_START || mePrepareStage == E_PREPARESTAGE_DONE)
        {
            mePrepareStage = E_PREPARESTAGE_DONE;
            meReleaseStage = E_RELEASESTAGE_START;
            return true;
        }
        CGS_ASSERT(false, "Should never get here!\n");
        return false;
    }

    // ARTIST 828E2BB8: (20+16+8)*(N+1) + (8+8)*(hashLength+64)
    // plus the fixed 0x94-byte allowance returned by 82BBCF10. That helper
    // reads no input: it is not an unknown distribution-buffer size.
    // FLAG PC-platform leaf: use native sizeof for records containing pointers.
    u32 ScratchPool::GetOverheadMemoryRequired(const InitOptions* lpOptions)
    {
        return static_cast<u32>((sizeof(ScratchEntry) + sizeof(CgsMemory::DistributionStreamEntry) + sizeof(ID))
            * (lpOptions->muMaxEntries + 1u)
            + (sizeof(u64) + sizeof(ImportHashTableValue)) * (ScratchHashLength(lpOptions->muMaxEntries) + 64u)
            + 0x94u);
    }

    // ARTIST 828F6B60..828F6E20. Overhead and resource staging buffers have
    // independent owners; carve only the tables from the overhead allocator.
    void ScratchPool::InitPool(const InitOptions* lpOptions)
    {
        mAllocator.Create(lpOptions->mpOverhead, lpOptions->muOverheadMemorySize);
        mAllocator.SetAlignment(KU_SCRATCH_MEMORY_ALIGNMENT);
        mpEntries = static_cast<ScratchEntry*>(mAllocator.Malloc(sizeof(ScratchEntry) * lpOptions->muMaxEntries));
        mpDistributionEntries = static_cast<CgsMemory::DistributionStreamEntry*>(
            mAllocator.Malloc(sizeof(CgsMemory::DistributionStreamEntry) * lpOptions->muMaxEntries));
        mpEntryIds = static_cast<ID*>(mAllocator.Malloc(sizeof(ID) * (lpOptions->muMaxEntries + 1u)));
        const u32 luHashLength = ScratchHashLength(lpOptions->muMaxEntries);
        u64* lpKeys = static_cast<u64*>(mAllocator.Malloc(sizeof(u64) * luHashLength));
        ImportHashTableValue* lpValues = static_cast<ImportHashTableValue*>(
            mAllocator.Malloc(sizeof(ImportHashTableValue) * luHashLength));
        mImportHashTable.Initialize(lpKeys, lpValues, static_cast<s32>(luHashLength));
        CGS_ASSERT(mpEntries != nullptr && mpDistributionEntries != nullptr && mpEntryIds != nullptr,
                   "Failiure to allocate\n");
        muNumEntries = 0;
        muMaxEntries = lpOptions->muMaxEntries;
        miBankId = lpOptions->miBankId;

        SmallResource lResource;
        for (s32 li = 0; li < KI_NUM_TYPES; ++li)
            lResource.m_baseResources[li] = nullptr;
        SmallResourceDescriptor lDescriptor;
        lResource.CreateFromRWResource(lpOptions->mResource);
        lDescriptor.CreateFromRWDescriptor(lpOptions->mDescriptor);
        for (s32 li = 0; li < KI_NUM_TYPES; ++li)
        {
            const u32 luSize = lDescriptor.m_baseResourceDescriptors[li].m_size;
            if (luSize != 0)
            {
                CGS_ASSERT(lResource.m_baseResources[li] != nullptr, "Allocator has NULL pointer\n");
                maResourceAllocators[li].Create(lResource.m_baseResources[li], luSize);
            }
        }
    }

    // ARTIST 828EDE08: rewind each resource allocator and clear all 64-bit keys.
    void ScratchPool::Clear()
    {
        muNumEntries = 0;
        for (s32 li = 0; li < KI_NUM_TYPES; ++li)
            maResourceAllocators[li].FreeAll();
        mImportHashTable.Clear();
    }

    // ARTIST 828D7E50; native ScratchEntry widens its three pointers.
    ScratchEntry* ScratchPool::GetEntry(u32 luIndex)
    {
        CGS_ASSERT(luIndex < muNumEntries, "Entry out of range\n");
        return &mpEntries[luIndex];
    }

    // ARTIST 828EDE80: r4=resource entry, r5=pool slot, r6=memory type,
    // r7=destination pointer. Failed staging consumes no entry or hash slot.
    void* ScratchPool::AddEntry(Entry* lpEntry, s32 liEntryId, s32 liMemType, void* lpDestLocation)
    {
        if (muNumEntries >= muMaxEntries)
            return nullptr;
        mpEntryIds[muNumEntries] = lpEntry->mID;
        const rw::BaseResourceDescriptor& lrDescriptor = lpEntry->mResourceDescriptor.m_baseResourceDescriptors[liMemType];
        CgsMemory::LinearMalloc& lrAllocator = maResourceAllocators[liMemType];
        lrAllocator.SetAlignment(lrDescriptor.m_alignment);
        void* lpTemp = lrAllocator.Malloc(lrDescriptor.m_size);
        if (lpTemp == nullptr)
            return nullptr;
        ScratchEntry& lrScratch = mpEntries[muNumEntries];
        lrScratch.miEntryId = liEntryId;
        lrScratch.mpSrcLocation = lpEntry->mResource.m_baseResources[liMemType];
        lrScratch.mpTempLocation = lpTemp;
        lrScratch.mpDestLocation = lpDestLocation;
        lrScratch.muSize = lrDescriptor.m_size;
        mImportHashTable.AddEntry(lpEntry->mID, lpTemp, lpDestLocation);
        ++muNumEntries;
        return lpTemp;
    }

    // ARTIST 828E3398. Build the gather list first, then arm the stream.
    void ScratchPool::BeginDistribution(s32 liMemType)
    {
        CGS_ASSERT(meUpdateStage == E_UPDATESTAGE_IDLE, "Can not begin distribution during none-idle stage\n");
        miCurrentMemType = liMemType;
        BuildGatherDistributionList();
        CGS_ASSERT(mpDistributionEntries != nullptr, "Distribution entries are NULL\n");
        u8* lpBase = static_cast<u8*>(maResourceAllocators[miCurrentMemType].GetStartAddress());
        if (lpBase == nullptr)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Linear allocator is NULL for memtype " << miCurrentMemType << "\n";
            CGS_ASSERT(false, lacMessage);
        }
        mGatherStream.Execute(mpDistributionEntries, muNumEntries, lpBase);
        meUpdateStage = E_UPDATESTAGE_GATHERING;
    }

    // ARTIST 828E3568: scatter is armed only once the entire gather completes.
    bool ScratchPool::UpdateGather()
    {
        CGS_ASSERT(meUpdateStage == E_UPDATESTAGE_GATHERING, "Can not call UpdateGather unless in gather stage\n");
        if (!mGatherStream.Update())
            return false;
        BuildScatterDistributionList();
        mScatterStream.Execute(mpDistributionEntries, muNumEntries,
            static_cast<u8*>(maResourceAllocators[miCurrentMemType].GetStartAddress()));
        meUpdateStage = E_UPDATESTAGE_SCATTERING;
        return true;
    }

    // ARTIST 828D8DE0.
    bool ScratchPool::UpdateScatter()
    {
        CGS_ASSERT(meUpdateStage == E_UPDATESTAGE_SCATTERING, "Can not call UpdateScatter unless in scatter stage\n");
        if (!mScatterStream.Update())
            return false;
        meUpdateStage = E_UPDATESTAGE_IDLE;
        return true;
    }

    // ARTIST 828D8EA8. The list uses byte offsets into this memory type's
    // packed allocator; alignment gaps must not be collapsed between entries.
    void ScratchPool::BuildGatherDistributionList()
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(maResourceAllocators[miCurrentMemType].GetStartAddress());
        for (u32 lu = 0; lu < muNumEntries; ++lu)
        {
            const ScratchEntry& lrEntry = mpEntries[lu];
            CgsMemory::DistributionStreamEntry& lrDistribution = mpDistributionEntries[lu];
            lrDistribution.mpScatteredAddress = static_cast<u8*>(lrEntry.mpSrcLocation);
            lrDistribution.muLength = lrEntry.muSize;
            lrDistribution.muPackedOffset = static_cast<u32>(reinterpret_cast<uintptr_t>(lrEntry.mpTempLocation) - luBase);
            const uintptr_t lauValues[] = {
                reinterpret_cast<uintptr_t>(lrDistribution.mpScatteredAddress),
                lrDistribution.muPackedOffset, lrDistribution.muLength
            };
            const char* const lapNames[] = { "Source address for entry ", "Dest offset for entry ", "Size for entry " };
            for (u32 luField = 0; luField < 3; ++luField)
            {
                if ((lauValues[luField] & 15u) != 0)
                {
                    char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
                    lMessage << lapNames[luField] << lu << " is " << static_cast<u64>(lauValues[luField])
                             << " - not multiple of 16 bytes\n";
                    CGS_ASSERT(false, lacMessage);
                }
            }
        }
    }

    // ARTIST 828D9330: retain length/packed offset; only replace source by dest.
    void ScratchPool::BuildScatterDistributionList()
    {
        for (u32 lu = 0; lu < muNumEntries; ++lu)
            mpDistributionEntries[lu].mpScatteredAddress = static_cast<u8*>(mpEntries[lu].mpDestLocation);
    }

    // ARTIST 828D9378 compares complete unsigned 64-bit IDs, not their low words.
    s32 ScratchPool::SortIdsQSortCallback(const void* lpLeft, const void* lpRight)
    {
        const ID lLeft = *static_cast<const ID*>(lpLeft);
        const ID lRight = *static_cast<const ID*>(lpRight);
        return lLeft < lRight ? -1 : (lLeft > lRight ? 1 : 0);
    }

    void ScratchPool::SortIdList()
    {
        std::qsort(mpEntryIds, muNumEntries, sizeof(ID), &SortIdsQSortCallback);
        // Inlined in ARTIST Pool::AddResourcesToScratchPool 828F6748.
        mpEntryIds[muNumEntries].SetHash(~u64(0));
    }
}
