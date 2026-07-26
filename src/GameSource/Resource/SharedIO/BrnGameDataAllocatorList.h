#ifndef BRN_GAMEDATA_ALLOCATOR_LIST_H
#define BRN_GAMEDATA_ALLOCATOR_LIST_H

#include "types.hpp"
#include "GameShared/GameClasses/Memory/MemoryMap/CgsMemoryMap.h"   // CgsMemory::MemoryMap::EAllocatorType

// All non-enum members are pointers -> forward declarations (no layout needed here).
namespace CgsMemory { class HeapMalloc; class LinearMalloc; }
namespace rw
{
    struct Resource;
    struct ResourceDescriptor;
    struct LinearResourceAllocator;
    namespace core { struct GeneralResourceAllocator; }
}
// The PPMalloc per-block report record handed to the debug-stats walk callback
// (EA::Allocator::GeneralAllocator::BlockInfo) -- it is a nested type, so the full vendor header
// is needed to name it in the callback declaration below.
#include "ppmalloc/EAGeneralAllocator.h"   // EA::Allocator::GeneralAllocator::BlockInfo

// BrnResource::GameDataIO::AllocatorList -- the GameDataModule's allocator registry. It maps a bank
// id (0..0x42) to a typed allocator: a raw resource, a CgsMemory::LinearMalloc / HeapMalloc, or an
// rw resource allocator. The GameDataModule's CreateAllocators populates it; everything that loads
// from the game-data heaps resolves its allocator through here (e.g. the loading screen's
// LoadControllerModule does GetHeapAllocator(0x26) to carve the controller module's memory). Each
// Get* validates the bank, then indexes the matching pointer array by the bank's slot.
//
// Reconstructed from the DecFIGS DWARF (GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h);
// the array sizes + the bank-map indirection + each accessor's allocator-type check are verified
// against the X360 ARTIST asm: GetHeapAllocator 0x822773E0, GetLinearAllocator 0x824EA678,
// GetRawResource 0x8252ADF8, GetRawResourceDescriptor 0x8252AFD0, GetRWLinearResourceAllocator
// 0x822775B8, GetRWGeneralResourceAllocator 0x823F3F98. Each resolves
// `mapXxx[ maiAllocatorMap[bankId] ]` after asserting `maeAllocatorType[bankId] == <type>`. The
// array sizes (2/2/7/8/5/5) match the GameDataModule's maGenerated*Allocators members.
// [x64: pointers are 8 bytes, so byte offsets differ from the X360's 32-bit layout -- members are
// accessed by name, not by offset.]
namespace BrnResource
{
    class GameDataModule;   // CreateAllocators/DestroyAllocators populate the registry

    namespace GameDataIO
    {
        class AllocatorList
        {
            // The owning module's CreateAllocators (0x8266DD00) writes maiAllocatorMap /
            // maeAllocatorType / the per-type pointer arrays directly as it builds each
            // allocator (the X360 pokes them in-module; the DWARF lists no setters).
            friend class ::BrnResource::GameDataModule;

        public:
            // Valid bank ids are 0..KI_MAX_BANK_ID (X360 GetHeapAllocator asserts id > 0x42 is out of range).
            enum { KI_MAX_BANK_ID = 0x42, KI_NUM_BANKS = 0x43 };

            void Construct();

            rw::Resource*                       GetRawResource(s32 liBankId) const;
            rw::ResourceDescriptor*             GetRawResourceDescriptor(s32 liBankId) const;
            CgsMemory::LinearMalloc*            GetLinearAllocator(s32 liBankId) const;
            CgsMemory::HeapMalloc*              GetHeapAllocator(s32 liBankId) const;
            rw::LinearResourceAllocator*        GetRWLinearResourceAllocator(s32 liBankId) const;
            rw::core::GeneralResourceAllocator* GetRWGeneralResourceAllocator(s32 liBankId) const;
            CgsMemory::LinearMalloc*            GetAudioStreamAllocator() const { return mpAudioStreamAllocator; }

        private:
            // DWARF BrnGameDataAllocatorList.h:85 -- the running allocated/free byte totals the
            // GeneralAllocatorReportCB walk accumulates while DebugDumpStats reports an rw general heap.
            struct GenAllocUsage
            {
                u32 muAllocated;   // BrnGameDataAllocatorList.h:87
                u32 muFree;        // BrnGameDataAllocatorList.h:88
            };

            // DWARF BrnGameDataAllocatorList.h:103 -- the per-block callback PPMalloc's report walk
            // invokes once per block; tallies the block's data size into muAllocated or muFree by its
            // block type (X360 0x82662E90). Declared as a callback (no `this`); registered statically.
            static bool GeneralAllocatorReportCB(const EA::Allocator::GeneralAllocator::BlockInfo* lpBlockInfo,
                                                 void* lpContext);

            // Validate the bank id + its registered allocator type, returning the slot index into the
            // matching pointer array (X360: each Get* asserts id<=0x42, map[id]>=0, type[id]==expected).
            s32 GetAllocatorSlot(s32 liBankId, CgsMemory::MemoryMap::EAllocatorType leExpectedType) const;

            // Members in DWARF order; sizes verified against the X360 accessor offsets.
            s32                                  maiAllocatorMap[KI_NUM_BANKS];    // bank id -> slot index (-1 = none)
            CgsMemory::MemoryMap::EAllocatorType maeAllocatorType[KI_NUM_BANKS];  // bank id -> allocator type
            rw::Resource*                        mapRawResources[2];
            rw::ResourceDescriptor*              mapRawResourceDescriptors[2];
            CgsMemory::LinearMalloc*             mapLinearAllocators[7];
            CgsMemory::HeapMalloc*               mapHeapAllocators[8];
            rw::LinearResourceAllocator*         mapRWLinearAllocators[5];
            rw::core::GeneralResourceAllocator*  mapRWGeneralAllocators[5];
            CgsMemory::LinearMalloc*             mpAudioStreamAllocator;
        };
    }
}

#endif
