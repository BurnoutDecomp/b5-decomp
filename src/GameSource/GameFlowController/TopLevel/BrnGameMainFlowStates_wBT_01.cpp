// ============================================================================
// b5-decomp/src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates_wBT_01.cpp
//
// Partfile 01 of BrnGameMainFlowStates.cpp:
//
//   LoadingScriptedState::LoadControllerModule
//     The boot loading screen's controller stage: give the input module its own general
//     resource allocator and run the input module's Prepare with it.
// ============================================================================

#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"               // CgsMemory::HeapMalloc::Malloc
#include "SharedClasses/Trigger/BrnRoamingLocation.h"                  // BrnTriggerData needs complete record types
#include "SharedClasses/Trigger/BrnSpawnLocation.h"
#include "GameSource/Game/BrnGameModule.hpp"                           // BrnGame::GetMainGameModule / GetInputModule
#include "GameSource/Resource/BrnGameDataModuleIO.h"                   // GameDataIO::OutputBuffer::GetAllocatorList
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"     // AllocatorList::GetHeapAllocator
#include "rw/rwcore_general_alloc.h"                                   // rw::core::GeneralResourceAllocator

namespace
{
// The input module's allocator: 8 KB of main memory, 16-aligned, carved from GameData heap 0x26.
const uint32_t KU_INPUT_MODULE_MEMORY_SIZE      = 0x2000;
const uint32_t KU_INPUT_MODULE_MEMORY_ALIGNMENT = 0x10;
const s32      KI_INPUT_MODULE_HEAP_BANK        = 0x26;

// The canonical descriptor includes the real native allocator-object reserve.
}

// Console body, in order:
//   * assert the out-pointer;
//   * first call only (no allocator yet): lDescriptor = { lane 0: 0x2000 bytes, 16-aligned;
//     lanes 1..4 empty }, lExtended = GetResourceDescriptor(lDescriptor), assert lanes 1..4 of the
//     extended descriptor are empty ("none-main memory"), carve lExtended's lane 0 from heap 0x26,
//     and build the allocator over that block and lDescriptor (the console's
//     GeneralResourceAllocator::Initialize: construct the allocator, or null when the carve
//     failed);
//   * return the input module's Prepare(allocator).
bool LoadingScriptedState::LoadControllerModule(BrnResource::GameDataIO::InputBuffer* /*lpGDMInput*/,
                                                const BrnResource::GameDataIO::OutputBuffer* lpGDMOutput,
                                                rw::core::GeneralResourceAllocator** lppInputModuleAllocator)
{
    CGS_ASSERT(lppInputModuleAllocator, "lppInputModuleAllocator");

    if (*lppInputModuleAllocator == 0)
    {
        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = KU_INPUT_MODULE_MEMORY_SIZE;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = KU_INPUT_MODULE_MEMORY_ALIGNMENT;

        rw::ResourceDescriptor lExtended;
        rw::Resource           lResource;
        rw::core::GeneralResourceAllocator::GetResourceDescriptor(&lExtended, &lDescriptor);

        for (s32 liIndex = 1; liIndex < static_cast<s32>(rw::KU_RESOURCE_LANE_COUNT); ++liIndex)
        {
            CGS_ASSERT(lExtended.m_baseResourceDescriptors[liIndex].m_size == 0,
                       "Extended descriptor is asking for none-main memory\n");
        }

        CgsMemory::HeapMalloc* lpGUIAllocator =
            lpGDMOutput->GetAllocatorList()->GetHeapAllocator(KI_INPUT_MODULE_HEAP_BANK);
        lResource.m_baseResources[0] =
            lpGUIAllocator->Malloc(static_cast<s32>(lExtended.m_baseResourceDescriptors[0].m_size),
                                   static_cast<s32>(lExtended.m_baseResourceDescriptors[0].m_alignment));

        // ARTIST 0x823C6A78 calls the canonical placement API, including its null path.
        *lppInputModuleAllocator = rw::core::GeneralResourceAllocator::Initialize(lResource, lDescriptor);
    }

    return BrnGame::GetMainGameModule()->GetInputModule().Prepare(*lppInputModuleAllocator);
}
