#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "ppmalloc/EAGeneralAllocator.h"             // EA::Allocator::GeneralAllocator::BlockInfo
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags

// BrnResource::GameDataIO::AllocatorList -- see the header. The per-type accessors are faithful
// reconstructions of the X360 ARTIST bodies (each: assert the bank id is in range, has an allocator
// slot, and is the requested type; then return the slot's entry from the matching pointer array).
// ARTIST streams the bank id and the requested allocator type into its diagnostics.

namespace BrnResource
{
    namespace GameDataIO
    {
        static const char* const KPC_ALLOCATOR_LIST_ASSERT_FILE =
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\resource\\SharedIO/BrnGameDataAllocatorList.h";

        // ARTIST82671D30..82671D58: the inlined owner initialization clears
        // only the 67 bank-map words and the audio-stream pointer. Allocator
        // types and pointer tables retain their incoming state until creation.
        void AllocatorList::Construct()
        {
            CGS_STATIC_ASSERT((sizeof(AllocatorList) == 0x298), "ARTIST allocator registry extent");
            CGS_STATIC_ASSERT((offsetof(AllocatorList, maeAllocatorType) == 0x10C), "ARTIST allocator types");
            CGS_STATIC_ASSERT((offsetof(AllocatorList, mapRawResources) == 0x218), "ARTIST raw allocator slots");
            CGS_STATIC_ASSERT((offsetof(AllocatorList, mapRawResourceDescriptors) == 0x220), "ARTIST raw descriptors");
            CGS_STATIC_ASSERT((offsetof(AllocatorList, mapLinearAllocators) == 0x228), "ARTIST linear allocator slots");
            CGS_STATIC_ASSERT((offsetof(AllocatorList, mapHeapAllocators) == 0x248), "ARTIST heap allocator slots");
            CGS_STATIC_ASSERT((offsetof(AllocatorList, mapRWLinearAllocators) == 0x26C), "ARTIST RW linear slots");
            CGS_STATIC_ASSERT((offsetof(AllocatorList, mapRWGeneralAllocators) == 0x280), "ARTIST RW general slots");
            CGS_STATIC_ASSERT((offsetof(AllocatorList, mpAudioStreamAllocator) == 0x294), "ARTIST audio allocator slot");
            for (s32 li = 0; li < KI_NUM_BANKS; ++li)
            {
                maiAllocatorMap[li]  = -1;
            }
            mpAudioStreamAllocator = 0;
        }

        __forceinline s32 AllocatorList::GetAllocatorSlot(s32 liBankId,
            CgsMemory::MemoryMap::EAllocatorType leExpectedType, const char* lpcTypeMessage, int liFirstAssertLine) const
        {
            CGS_ASSERT_STREAM_AT(liBankId >= 0 && liBankId < KI_NUM_BANKS, "Bank id is out of range\n",
                KPC_ALLOCATOR_LIST_ASSERT_FILE, liFirstAssertLine);
            CGS_ASSERT_STREAM_AT(maiAllocatorMap[liBankId] >= 0,
                "Bank " << static_cast<u32>(liBankId) << " is not an allocator\n",
                KPC_ALLOCATOR_LIST_ASSERT_FILE, liFirstAssertLine + 1);
            CGS_ASSERT_STREAM_AT(maeAllocatorType[liBankId] == leExpectedType,
                "Bank " << static_cast<u32>(liBankId) << lpcTypeMessage,
                KPC_ALLOCATOR_LIST_ASSERT_FILE, liFirstAssertLine + 2);
            return maiAllocatorMap[liBankId];
        }

        rw::Resource* AllocatorList::GetRawResource(s32 liBankId) const
        {
            return mapRawResources[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_RAW,
                " is not a raw resource\n", 114)];
        }

        rw::ResourceDescriptor* AllocatorList::GetRawResourceDescriptor(s32 liBankId) const
        {
            return mapRawResourceDescriptors[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_RAW,
                " is not a raw resource\n", 123)];
        }

        CgsMemory::LinearMalloc* AllocatorList::GetLinearAllocator(s32 liBankId) const
        {
            return mapLinearAllocators[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_LINEAR,
                " is not a linear allocator\n", 132)];
        }

        CgsMemory::HeapMalloc* AllocatorList::GetHeapAllocator(s32 liBankId) const
        {
            return mapHeapAllocators[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_HEAP,
                " is not a heap allocator\n", 141)];
        }

        rw::LinearResourceAllocator* AllocatorList::GetRWLinearResourceAllocator(s32 liBankId) const
        {
            return mapRWLinearAllocators[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_RWLINEAR,
                " is not a rw linear resource allocator\n", 150)];
        }

        rw::core::GeneralResourceAllocator* AllocatorList::GetRWGeneralResourceAllocator(s32 liBankId) const
        {
            return mapRWGeneralAllocators[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_RWHEAP,
                " is not a rw general resource allocator\n", 159)];
        }

        // @ 0x82662E90 -- PPMalloc invokes this once per block while DebugDumpStats walks an rw general
        // heap; it tallies the block's usable size into the GenAllocUsage accumulator by block type.
        // X360: tests mBlockType (BlockInfo+0x14) against the PPMalloc free(4)/allocated(2) bits and adds
        // mnDataSize (BlockInfo+0x10) into muFree / muAllocated respectively; any other block type is
        // logged as "Unknown block found of size N" when the message filter's bit 0 is set. Always
        // returns true (continue the walk). lpContext is the GenAllocUsage being accumulated.
        bool AllocatorList::GeneralAllocatorReportCB(const EA::Allocator::GeneralAllocator::BlockInfo* lpBlockInfo,
                                                     void* lpContext)
        {
            // PPMalloc block-type bits (EAGeneralAllocator): allocated = 2, free = 4.
            const char KC_BLOCK_TYPE_ALLOCATED = 2;
            const char KC_BLOCK_TYPE_FREE      = 4;

            GenAllocUsage* lpUsage = static_cast<GenAllocUsage*>(lpContext);

            if (lpBlockInfo->mBlockType & KC_BLOCK_TYPE_FREE)
            {
                lpUsage->muFree += lpBlockInfo->mnDataSize;
            }
            else if (lpBlockInfo->mBlockType & KC_BLOCK_TYPE_ALLOCATED)
            {
                lpUsage->muAllocated += lpBlockInfo->mnDataSize;
            }
            else if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << "Unknown block found of size " << lpBlockInfo->mnDataSize << "\n";
            }
            return true;
        }
    }
}
