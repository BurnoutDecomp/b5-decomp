// =================================================================================================
// BrnPhysicsModule_wG_GameActionsPostScene.cpp
//
// PhysicsModule::HandleGameActionsPostScene -- the post-scene game-action dispatch, called every
// frame by PostSceneUpdate over the same GameStateModuleIO::GameActionQueue the pre-scene
// HandleGameActions drains. Four arms (ids 23 / 34 / 97 / 99) preceded by one unconditional
// DeformationManager::ProcessDebugResetDeformationModels sweep. Only arm 97 (the body-shop
// drive-thru repair) is live; the other three and the pre-loop sweep call methods with no body in
// the tree and each logs a named one-shot deferral instead.
// =================================================================================================

#include "GameSource/Physics/BrnPhysicsModule.h"
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"   // the 97 arm's consumer
#include "GameSource/GameState/BrnGameStateSharedIO.h"                     // GameActionQueue
#include "GameSource/GameState/BrnGameActions.h"                           // EGameActionType wire ids
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint (deferral witnesses)

namespace BrnPhysics
{
    namespace
    {
        // The body-shop action's 144-byte payload carries the player's packed entity-id WORD at
        // +0x80 (written there by BrnDriveThruManager.cpp, PostShopAction).
        const u32 KU_EV_BODY_SHOP_ENTITY_ID = 128;

        // One line per distinct deferral, ever. DELETE-WHEN: the three VehicleManager mode/junkyard
        // methods and the debug reset sweep are reconstructed.
        void ReportDeferral(bool& lrbLogged, const char* lpcWhat)
        {
            if (lrbLogged || CgsDev::Log::gpDebugPrint == 0)
                return;
            lrbLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[postscene-action] DEFERRED: " << lpcWhat << " has no body in the tree [FLAG]\n";
        }
    }

    void PhysicsModule::HandleGameActionsPostScene(
            const BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue,
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        CGS_ASSERT(lpGameActionQueue, "lpGameActionQueue");

        // The first event and its action id are fetched BEFORE the debug sweep runs; the id is
        // only rewritten by GetNextEvent at the bottom of the loop.
        const CgsModule::Event* lpEventData = 0;
        s32 liEventSize = 0;
        s32 liAction = lpGameActionQueue->GetFirstEvent(&lpEventData, &liEventSize);

        // Unconditional, and it runs even when the queue is empty. Deferred: the debug-only
        // "reset every live model" sweep is declared in BrnDeformationManager.h, defined nowhere.
        {
            static bool sbLogged = false;
            ReportDeferral(sbLogged, "DeformationManager::ProcessDebugResetDeformationModels");
        }

        while (lpEventData)
        {
            switch (liAction)
            {
                // 23 -- the post-scene half of the mode PREPARE. Its callee
                // VehicleManager::OnPrepareGameMode is NOT VehicleManager::OnGameModePrepare
                // (landed, and reached from the pre-scene dispatch's own case 23).
                case BrnGameState::GameStateModuleIO::E_ACTION_PREPARE_FOR_MODE:
                {
                    static bool sbLogged = false;
                    ReportDeferral(sbLogged, "VehicleManager::OnPrepareGameMode");
                    break;
                }

                // 34 -- the post-scene half of mode START.
                case BrnGameState::GameStateModuleIO::E_ACTION_START_PLAYING_MODE:
                {
                    static bool sbLogged = false;
                    ReportDeferral(sbLogged, "VehicleManager::OnStartGameMode");
                    break;
                }

                // 97 -- the body-shop repair. The reset is bracketed by two part-index
                // verification sweeps: verify, reset, verify.
                case BrnGameState::GameStateModuleIO::E_ACTION_BODY_SHOP_DRIVE_THRU:
                {
                    const u8* const lpu8Payload = reinterpret_cast<const u8*>(lpEventData);
                    const u32 luEntityIdWord =                                  // FLAG: serialized event payload, not a typed record
                        *reinterpret_cast<const u32*>(lpu8Payload + KU_EV_BODY_SHOP_ENTITY_ID);
                    const EntityId lEntityId = { luEntityIdWord };

                    mDeformationManager.VerifyPartIndices();
                    mDeformationManager.ProcessResetDeformationModelEvent(
                        lpSimModuleInputBuffer, lpSceneInterface, lEntityId);
                    mDeformationManager.VerifyPartIndices();
                    break;
                }

                // 99 -- the junkyard drive-thru.
                case BrnGameState::GameStateModuleIO::E_ACTION_DRIVE_THRU_JUNK_YARD:
                {
                    static bool sbLogged = false;
                    ReportDeferral(sbLogged, "VehicleManager::OnJunkYardDriveThru");
                    break;
                }

                default:
                    break;
            }

            // The console reuses one stack slot for the out pointer here; a distinct local reads
            // the same because the current event is held in a register across the call.
            const CgsModule::Event* lpNextEvent = 0;
            liAction = lpGameActionQueue->GetNextEvent(lpEventData, &lpNextEvent, &liEventSize);
            lpEventData = lpNextEvent;
        }
    }
}
