#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"
#include "GameSource/World/BrnWorldModule.h"   // BrnWorld::WorldModule -- the members, BY NAME
#include "GameShared/GameClasses/Core/CgsAssert.h"

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
//
// ⛔⛔ SECOND BUG, FIXED 2026-08-11 (junkyard-entry wave). Mounting this TU was necessary but
// NOT sufficient: the body wrote its two WorldModule outputs through the X360 BYTE OFFSETS
//     +6167272 (meLocalPlayerActiveRaceCarIndex) and +6167280 (maeCarControls)
// applied to the x64 PC object. MEASURED with a compile-time offsetof probe on this build:
//     PC offsetof(WorldModule, meLocalPlayerActiveRaceCarIndex) == 6234776  (X360 6167272)
//     PC offsetof(WorldModule, maeCarControls)                  == 6234784  (X360 6167280)
//     PC sizeof  (WorldModule)                                  == 6243504
// -- WorldModule embeds the whole sub-module fleet BY VALUE (RaceCar/Traffic/World/Prop/
// Trigger/Physics/EnvironmentManager/Scene/AI/Crash), every one an independently reconstructed
// x64 layout, so the console offsets drift by 67,504 bytes here. Consequences:
//   * the real meLocalPlayerActiveRaceCarIndex was NEVER written -- it stayed at Prepare's -1
//     for the whole session, so WorldModule::HandleGameActions case 7 (the junkyard drive-thru
//     "put the player car under AI control" action posted by
//     CarSelectManager::ReallyEnterJunkyardAtStartOfGame) asserted
//     "Unable to set the player car under AI control, as we don't know who they are yet"
//     (BrnWorldModule.cpp:1327) and bailed -- the junkyard entry never handed the car over;
//   * and the 4 + 32 bytes it did write landed 67,504 bytes short, INSIDE the embedded
//     sub-module fleet -- a silent live corruption every pre-scene frame.
// The old one-shot "[bridge] ... player active race-car index published = 0" diag hid this:
// it printed the SOURCE interface value, never the WorldModule member, so it read as proof the
// hand-off worked. It is removed with the offsets.
//
// Both outputs are now written BY NAME. The DWARF settles the model: BrnWorldModule.h:473
// declares this as a WorldModule METHOD
//     void BridgeRaceCarModuleToWorldModule_PreScene(InputBuffer_PreScene*,
//                                                    const OutputBuffer_PreScene*);
// -- `this` == the X360 r3 -- so it is reconstructed as a member here. (The rest of the
// WorldBridge* family stays namespace functions with an explicit `void* lpWorldModule`; this
// is the only one of them that ever dereferences it. A global `namespace WorldModule` cannot
// be pulled into BrnWorldModule.h either: BrnGameModule.hpp does `using BrnWorld::WorldModule`.)
// ============================================================================

namespace BrnWorld
{

namespace
{
    // The per-car control mode the X360 stores for a rival: literal 2, into the same
    // maeCarControls[8] (DWARF BrnWorldModule.h:309) that HandleGameActions cases 7/23/34
    // drive -- case 23 writes exactly this 2 on its own rival arm. (DWARF names the gate
    // IsRaceCarRival().)
    const s32 KI_WORLD_MODULE_CAR_CONTROL_RIVAL = 2;
}

// @ 0x827A52B0 -- WorldBridgeEntityModulesToEntityModules.cpp:88. Latch the race-car
// module's pre-scene active-race-car output interface into the world-entity module's
// pre-scene input buffer, then publish the player index + per-car "is rival" markers
// into this WorldModule's own member state.
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
void WorldModule::BridgeRaceCarModuleToWorldModule_PreScene(
    WorldEntityIO::InputBuffer_PreScene* lpWorldInputBuffer_PreScene,
    const RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene)
{
    CGS_ASSERT(lpWorldInputBuffer_PreScene != 0, "lpWorldInputBuffer_PreScene");           // :94
    CGS_ASSERT(lpRaceCarOutputBuffer_PreScene != 0, "lpRaceCarOutputBuffer_PreScene");     // :95

    const RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpRaceCarEntityOutputInterface =
        lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface();
    CGS_ASSERT(lpRaceCarEntityOutputInterface != 0, "lpRaceCarEntityOutputInterface");     // :100

    // The committed world-entity input buffer names the race-car interface by its real type
    // (InputBuffer_PreScene::ActiveRaceCarInterface ==
    // RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface), so the setter block-copies
    // the same 10480-byte payload the X360 re-fetches.
    lpWorldInputBuffer_PreScene->SetActiveRaceCarInterface(
        *lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface());

    // ---- publish the player active-race-car index into the WorldModule -------------------
    // The X360 gate is `(mePlayerActiveRaceCarIndex != -1) && mbIsPlayerCarActive`, which is
    // exactly what the committed IsPlayerCarActive() returns; its interior "Player car index
    // hasn't been set" assert (:980) lives inside that getter pair, not here.
    if (lpRaceCarEntityOutputInterface->IsPlayerCarActive())
    {
        meLocalPlayerActiveRaceCarIndex =
            lpRaceCarEntityOutputInterface->GetPlayerActiveRaceCarIndex();
    }
    else
    {
        meLocalPlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }

    // ---- mark each active rival car in the WorldModule per-car control array --------------
    for (EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leActiveRaceCarIndex++)
    {
        if (lpRaceCarEntityOutputInterface->IsRaceCarActive(leActiveRaceCarIndex))
        {
            if (lpRaceCarEntityOutputInterface->IsRaceCarRival(leActiveRaceCarIndex))
                maeCarControls[leActiveRaceCarIndex] = KI_WORLD_MODULE_CAR_CONTROL_RIVAL;
        }
    }
}

}   // namespace BrnWorld
