#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [diag] CgsDev::Log::gpDebugPrint

// ============================================================================
// GameSource/World/Bridges/WorldBridgeRaceCarToWorldModule.cpp
//
// ⭐⭐ FILE SPLIT, 2026-08-01 (car-select hand-off wave). This function's body already
// existed, verbatim as below, in WorldBridgeEntityModulesToEntityModules.cpp -- and that TU
// IS NOT MOUNTED (its other two bridges each need an IO accessor that is still
// declaration-only: TriggerEntityModuleIO::InputBuffer_PreScene::GetInputInterface,
// BrnTrafficIO::OutputBuffer_PostScene::GetTrafficToRaceCarInterface_PostScene and
// BrnTrafficIO::OutputBuffer_PreScene::GetTriggerManagementInputInterface -- MEASURED, +3
// unresolved for the whole TU). So the copy that actually LINKED was the inert one-shot log
// in WorldLinkStubs.cpp, whose own comment said "real body @0x827A52B0 in its own home TU
// (not mounted: IO accessor closure)".
//
// ⛔ THAT INERT STUB WAS THE ONLY PRODUCER OF WorldModule::meLocalPlayerActiveRaceCarIndex.
// With it in the link the index stayed at Construct's -1 for the whole session, so:
//   * WorldModule::HandleGameActions case 7 (the junkyard drive-thru's "put the player car
//     under AI control" action, posted by CarSelectManager::ReallyEnterJunkyardAtStartOfGame)
//     asserted "Unable to set the player car under AI control, as we don't know who they are
//     yet" (BrnWorldModule.cpp:1327) and then had to BAIL to avoid the console's own
//     `maeCarControls[-1]` out-of-bounds write, so the player car's control mode was never
//     set at all;
//   * every other consumer of the player's active-race-car slot in WorldModule saw -1.
// The data was there the whole time: RaceCarEntityModule::UpdateOutputInterfaces publishes
// `playerIdx 1` into the race-car module's own output interface every frame (its [uoi] diag
// prints it) -- nothing was carrying it across the module boundary.
//
// This TU exists so the ONE bridge that is fully closed can be mounted without waiting on the
// three IO accessors its two file-mates need. Same pattern, same reason, as
// GameSource/Director/BrnDirectorICEWrapperPrepare.cpp.
// DELETE-WHEN: the three accessors above are bodied and
// WorldBridgeEntityModulesToEntityModules.cpp can be mounted whole -- then fold this back.
// ============================================================================

namespace WorldModule
{

namespace
{
    // WorldModule (the X360 r3 context) member offsets this bridge writes through.
    // FLAG: the WorldModule class layout is not reconstructed *in this TU's include set*
    // (BrnWorldModule.h is not reachable from the Bridges layer), so the two members are
    // accessed at their X360 byte offsets. Both are cross-checked against the committed
    // BrnWorldModule.h member comments: meLocalPlayerActiveRaceCarIndex @+6167272 (:307) and
    // maeCarControls @+6167280 (:309).
    const u32 KU_WORLD_MODULE_PLAYER_ACTIVE_RACE_CAR_INDEX_OFFSET = 6167272; // 0x5E1AE8 -- EActiveRaceCarIndex
    const u32 KU_WORLD_MODULE_RACE_CAR_TYPE_ARRAY_OFFSET          = 6167280; // 0x5E1AF0 -- s32[8] per active-car type
    // FLAG: the per-car "type" enum's home is not reconstructed; the X360 stores the
    // literal 2 for a rival car (DWARF names the gate IsRaceCarRival()).
    const s32 KI_WORLD_MODULE_RACE_CAR_TYPE_RIVAL = 2;
}

// @ 0x827A52B0 -- WorldBridgeEntityModulesToEntityModules.cpp:88. Latch the race-car
// module's pre-scene active-race-car output interface into the world-entity module's
// pre-scene input buffer, then publish the player index + per-car "is rival" markers
// into the WorldModule (the X360 r3 context) member state.
//
// The interior range asserts the pseudocode inlines ("mePlayerActiveRaceCarIndex <
// E_ACTIVE_RACE_CAR_INDEX_COUNT" :967, "Player car index hasn't been set" :980,
// "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0"/"< E_ACTIVE_RACE_CAR_INDEX_COUNT"
// :854/:855/:876/:877 from BrnRaceCarEntityModuleOutputInterface.h) live inside the
// named getters IsPlayerCarActive()/IsRaceCarActive()/IsRaceCarRival(); the loop-guard
// assert ("leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT", BurnoutConstants.h:39) lives in
// EActiveRaceCarIndex's committed range-guarded operator++. They are NOT re-emitted here.
//
// The X360 tail returns the last EndAssert artifact in r3; the logical return type is void.
void BridgeRaceCarModuleToWorldModule_PreScene(
    void* lpWorldModule,
    BrnWorld::WorldEntityIO::InputBuffer_PreScene* lpWorldInputBuffer_PreScene,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene)
{
    CGS_ASSERT(lpWorldInputBuffer_PreScene != 0, "lpWorldInputBuffer_PreScene");           // :94
    CGS_ASSERT(lpRaceCarOutputBuffer_PreScene != 0, "lpRaceCarOutputBuffer_PreScene");     // :95

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpRaceCarEntityOutputInterface =
        lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface();
    CGS_ASSERT(lpRaceCarEntityOutputInterface != 0, "lpRaceCarEntityOutputInterface");     // :100

    // The committed world-entity input buffer names the race-car interface by its real type
    // (InputBuffer_PreScene::ActiveRaceCarInterface ==
    // RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface), so the setter block-copies
    // the same 10480-byte payload the X360 re-fetches.
    lpWorldInputBuffer_PreScene->SetActiveRaceCarInterface(
        *lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface());

    // ---- publish the player active-race-car index into the WorldModule -------------------
    u8* lpWorldModuleBytes = static_cast<u8*>(lpWorldModule);
    if (lpRaceCarEntityOutputInterface->IsPlayerCarActive())
    {
        *reinterpret_cast<EActiveRaceCarIndex*>(
            lpWorldModuleBytes + KU_WORLD_MODULE_PLAYER_ACTIVE_RACE_CAR_INDEX_OFFSET) =
            lpRaceCarEntityOutputInterface->GetPlayerActiveRaceCarIndex();

        // [diag, one-shot -- NOT console code] this bridge spent the whole project inert (see
        // the banner); the line proves the index really crosses the module boundary rather
        // than the mount merely linking. Same shape/gate as the sibling bring-up diags.
        // Remove with the FLAG above.
        {
            static bool sbReported = false;
            if (!sbReported && (CgsDev::Message::gxMessageFilterFlags & 1) &&
                CgsDev::Log::gpDebugPrint != 0)
            {
                sbReported = true;
                *CgsDev::Log::gpDebugPrint
                    << "[bridge] BridgeRaceCarModuleToWorldModule_PreScene: player active"
                       " race-car index published = "
                    << static_cast<s32>(lpRaceCarEntityOutputInterface->GetPlayerActiveRaceCarIndex())
                    << "\n";
            }
        }
    }
    else
    {
        *reinterpret_cast<EActiveRaceCarIndex*>(
            lpWorldModuleBytes + KU_WORLD_MODULE_PLAYER_ACTIVE_RACE_CAR_INDEX_OFFSET) =
            E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }

    // ---- mark each active rival car in the WorldModule per-car type array ----------------
    s32* lpRaceCarType = reinterpret_cast<s32*>(
        lpWorldModuleBytes + KU_WORLD_MODULE_RACE_CAR_TYPE_ARRAY_OFFSET);
    for (EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leActiveRaceCarIndex++)
    {
        if (lpRaceCarEntityOutputInterface->IsRaceCarActive(leActiveRaceCarIndex))
        {
            if (lpRaceCarEntityOutputInterface->IsRaceCarRival(leActiveRaceCarIndex))
                lpRaceCarType[leActiveRaceCarIndex] = KI_WORLD_MODULE_RACE_CAR_TYPE_RIVAL;
        }
    }
}

}
