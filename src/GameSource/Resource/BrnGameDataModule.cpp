#include "GameSource/Resource/BrnGameDataModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Memory/CgsMemoryModule.h"   // MemoryModule::InitOptions
#include "GameSource/Resource/BrnResourceAllocator.h"        // Allocators::mpInternalDebugAllocator
#include "rw/rwcore_structs.h"                                // rw::IResourceAllocator / Resource / ResourceDescriptor
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // [5b trace] (Construct runs at runtime, gpDebugPrint ready)

// BrnResource::GameDataModule - see the header. This pass reconstructs the rw-INDEPENDENT
// lifecycle spine (Prepare's base -> ResourceModule::Prepare core). The rw-allocator-gated
// bring-up (CreateBanks/CreatePools/CreateAllocators), the DLC/AttribSys/HUD/popup prepare
// stages, Construct, Update, Destruct and the ProcessXxxRequest handlers are DEFERRED.
namespace BrnResource
{
    // "New module": mark before any Prepare. NO logging here -- BrnGameModule embeds GameDataModule by
    // value, so this ctor runs at STATIC INIT (before gpDebugPrint is initialized); logging here would
    // deref an uninitialized gpDebugPrint and crash before the window opens.
    GameDataModule::GameDataModule()
    {
        mbIsNewModule = true;
    }

    // @ 0x82673F38 - resumable bring-up: base module, then the ResourceModule (memory/pool/
    // bundle/filesystem), then carve banks/pools/allocators. Re-enters at the current stage if
    // a sub-step is not yet ready. (The X360 then runs DLC stages 16-18+ and the AttribSys/HUD/
    // popup prepares - DEFERRED here; the machine completes after the allocator stage.)
    bool GameDataModule::Prepare(void* lpInputBuffer, void* lpOutputBuffer)
    {
        mbIsNewModule = true;   // [reliable] see ResourceModule::Prepare -- set before base Prepare
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
        case E_PREPARE_BASE:
            mePrepareStage = E_PREPARE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            // (X360 also Registers two debug components here - deferred)
            // fall through
        case E_PREPARE_RESOURCE:
            mePrepareStage = E_PREPARE_RESOURCE;
            if (!mResourceModule.Prepare())
                return false;
            mReceiverQueue.Clear();
            // fall through
        case E_PREPARE_BANKS:
            mePrepareStage = E_PREPARE_BANKS;
            if (!CreateBanks(lpInputBuffer, lpOutputBuffer))
                return false;
            // fall through
        case E_PREPARE_POOLS:
            mePrepareStage = E_PREPARE_POOLS;
            if (!CreatePools(lpInputBuffer, lpOutputBuffer))
                return false;
            // fall through
        case E_PREPARE_ALLOCATORS:
            mePrepareStage = E_PREPARE_ALLOCATORS;
            if (!CreateAllocators(lpInputBuffer, lpOutputBuffer))
                return false;
            // (X360: DLC stages 16-18+ and the AttribSys/HUD/popup prepares - deferred)
            // fall through
        case E_PREPARE_DONE:
            meReleaseStage = E_PREPARE_START;
            mePrepareStage = E_PREPARE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // ---- DEFERRED (rw allocator middleware + game-data subsystems) --------------------
    // CreateBanks (0x8266DA28) / CreatePools (0x8266DB88) / CreateAllocators (0x8266DD00) carve the
    // resource banks/pools/allocators out of the GameData heap through the rw::IResourceAllocator
    // (the rwcore allocator layer). Inert stubs that report success so the Prepare stage machine
    // advances; reconstruct with that allocator layer. Construct (0x82671B90) builds the embedded
    // subsystems; Update pumps the ResourceModule and services the game-data query handlers
    // (ProcessGetVehicle/Wheel/Traffic/WorldUnit/PVS/... - 30+ handlers); Destruct (0x82664508)
    // tears it down. All inert until the loading-machine case 8 drives this module.
    bool GameDataModule::CreateBanks(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return true; }
    bool GameDataModule::CreatePools(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return true; }
    bool GameDataModule::CreateAllocators(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return true; }

    void GameDataModule::Construct(const void* /*lpInitOptions*/)
    {
        mbIsNewModule = true;   // X360 *(this+4)=1 (new module type; base Prepare skips the old IO path)

        // [5b] Bring up the resource memory system. This is a minimal stand-in for the X360
        // ConstructResourceModule (which builds a 1216-byte ResourceModule::InitOptions): build just the
        // MemoryModule InitOptions, carve the root memory region from the debug allocator, and construct
        // the ResourceModule -> MemoryModule carves its bank/block tables. (Pool/Bundle/FileSystem
        // Construct + the full multi-pool heaps land in later 5b passes.)
        rw::IResourceAllocator* lpAllocator =
            static_cast<rw::IResourceAllocator*>(Allocators::mpInternalDebugAllocator);
        if (lpAllocator == 0)
            return;

        static CgsMemory::MemoryModule::InitOptions s_InitOptions;   // outlives Construct (module keeps the region)
        s_InitOptions.muMaxBanks  = 128;
        s_InitOptions.muHighestId = 1024;
        s_InitOptions.muMaxBlocks = 30000;

        // Root region for memory type 0 (main): 4 MB carved in 64 KB blocks.
        rw::ResourceDescriptor lRegionDesc;
        for (u32 li = 0; li < 4; ++li)
        {
            lRegionDesc.m_baseResourceDescriptors[li].m_size      = 0;
            lRegionDesc.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        lRegionDesc.m_baseResourceDescriptors[0].m_size      = 0x00400000;  // 4 MB
        lRegionDesc.m_baseResourceDescriptors[0].m_alignment = 16;
        rw::Resource lRegion = lpAllocator->DoAllocate(lRegionDesc, "GameDataRoot");

        s_InitOptions.maResourceSets[0].mpDataStart = lRegion.m_baseResources[0];
        s_InitOptions.maResourceSets[0].muDataSize  = 0x00400000;
        s_InitOptions.maResourceSets[0].muNumBlocks = 64;       // 64 x 64KB = 4MB
        // memory types 1-4 left zeroed (skipped by CreateRootBank)

        *CgsDev::Log::gpDebugPrint << "[5b] GameDataModule::Construct: region="
                                  << (s32)(u32)(size_t)lRegion.m_baseResources[0]
                                  << " -> ResourceModule::Construct\n";
        mResourceModule.Construct(&s_InitOptions, lpAllocator);
        *CgsDev::Log::gpDebugPrint << "[5b] GameDataModule::Construct: ResourceModule constructed ok\n";
    }
    bool GameDataModule::Release() { return true; }
    void GameDataModule::Destruct() {}
    bool GameDataModule::Update(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return false; }
}
