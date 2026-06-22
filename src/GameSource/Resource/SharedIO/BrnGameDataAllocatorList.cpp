#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "ppmalloc/EAGeneralAllocator.h"             // EA::Allocator::GeneralAllocator::BlockInfo
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags

// BrnResource::GameDataIO::AllocatorList -- see the header. The per-type accessors are faithful
// reconstructions of the X360 ARTIST bodies (each: assert the bank id is in range, has an allocator
// slot, and is the requested type; then return the slot's entry from the matching pointer array).
// The X360 emits the bank id into the assert text via a StrStream; the static messages here are the
// PC-side equivalent. (returning a pointer to a still-forward-declared allocator type is fine -- no
// dereference happens here.)

namespace BrnResource
{
    namespace GameDataIO
    {
        // [inferred] The X360 AllocatorList::Construct is inlined into GameDataModule::CreateAllocators
        // (no standalone body in the export). Its job is to start every bank "unassigned" (map = -1, so
        // the Get* asserts fire if a bank is used before CreateAllocators registers it) and null every
        // allocator/resource pointer. CreateAllocators then fills the slots as it builds each allocator.
        void AllocatorList::Construct()
        {
            for (s32 li = 0; li < KI_NUM_BANKS; ++li)
            {
                maiAllocatorMap[li]  = -1;
                maeAllocatorType[li] = CgsMemory::MemoryMap::E_ALLOCATORTYPE_RAW;
            }
            for (s32 li = 0; li < 2; ++li)
            {
                mapRawResources[li]           = 0;
                mapRawResourceDescriptors[li] = 0;
            }
            for (s32 li = 0; li < 7; ++li)
                mapLinearAllocators[li] = 0;
            for (s32 li = 0; li < 8; ++li)
                mapHeapAllocators[li] = 0;
            for (s32 li = 0; li < 5; ++li)
            {
                mapRWLinearAllocators[li]  = 0;
                mapRWGeneralAllocators[li] = 0;
            }
            mpAudioStreamAllocator = 0;
        }

        s32 AllocatorList::GetAllocatorSlot(s32 liBankId, CgsMemory::MemoryMap::EAllocatorType leExpectedType) const
        {
            CGS_ASSERT(liBankId >= 0 && liBankId <= KI_MAX_BANK_ID, "Bank id is out of range");
            CGS_ASSERT(maiAllocatorMap[liBankId] >= 0, "Bank is not an allocator");
            CGS_ASSERT(maeAllocatorType[liBankId] == leExpectedType, "Bank is not the requested allocator type");
            return maiAllocatorMap[liBankId];
        }

        rw::Resource* AllocatorList::GetRawResource(s32 liBankId) const
        {
            return mapRawResources[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_RAW)];
        }

        rw::ResourceDescriptor* AllocatorList::GetRawResourceDescriptor(s32 liBankId) const
        {
            return mapRawResourceDescriptors[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_RAW)];
        }

        CgsMemory::LinearMalloc* AllocatorList::GetLinearAllocator(s32 liBankId) const
        {
            return mapLinearAllocators[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_LINEAR)];
        }

        CgsMemory::HeapMalloc* AllocatorList::GetHeapAllocator(s32 liBankId) const
        {
            return mapHeapAllocators[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_HEAP)];
        }

        rw::LinearResourceAllocator* AllocatorList::GetRWLinearResourceAllocator(s32 liBankId) const
        {
            return mapRWLinearAllocators[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_RWLINEAR)];
        }

        rw::core::GeneralResourceAllocator* AllocatorList::GetRWGeneralResourceAllocator(s32 liBankId) const
        {
            return mapRWGeneralAllocators[GetAllocatorSlot(liBankId, CgsMemory::MemoryMap::E_ALLOCATORTYPE_RWHEAP)];
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
