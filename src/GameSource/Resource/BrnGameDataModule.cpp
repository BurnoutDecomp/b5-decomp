#include "GameSource/Resource/BrnGameDataModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnResource::GameDataModule - see the header. This pass reconstructs the rw-INDEPENDENT
// lifecycle spine (Prepare's base -> ResourceModule::Prepare core). The rw-allocator-gated
// bring-up (CreateBanks/CreatePools/CreateAllocators), the DLC/AttribSys/HUD/popup prepare
// stages, Construct, Update, Destruct and the ProcessXxxRequest handlers are DEFERRED.
namespace BrnResource
{
    // @ 0x82673F38 - resumable bring-up: base module, then the ResourceModule (memory/pool/
    // bundle/filesystem), then carve banks/pools/allocators. Re-enters at the current stage if
    // a sub-step is not yet ready. (The X360 then runs DLC stages 16-18+ and the AttribSys/HUD/
    // popup prepares - DEFERRED here; the machine completes after the allocator stage.)
    bool GameDataModule::Prepare(void* lpInputBuffer, void* lpOutputBuffer)
    {
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

    void GameDataModule::Construct(const void* /*lpInitOptions*/) {}
    bool GameDataModule::Release() { return true; }
    void GameDataModule::Destruct() {}
    bool GameDataModule::Update(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return false; }
}
