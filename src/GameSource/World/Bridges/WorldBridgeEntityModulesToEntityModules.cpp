#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // TriggerManagementInputInterface (real aggregate)

// WorldModule entity-module -> entity-module bridges -- reconstructed from
// BURNOUT_X360_ARTIST.XEX @ 0x827A52B0 / 0x827A51F0 / 0x827AD788 (this TU's DWARF home is
// WorldBridgeEntityModulesToEntityModules.cpp; signatures/locals/callee names verbatim from
// the PS3 DecFIGS BrnWorldBridgesUnity.cpp dump). Modelled as namespace functions whose
// leading lpWorldModule arg is the X360 r3 (the WorldModule `this`), per the committed bridge
// precedent (WorldBridgeInputToEntityModules.cpp / WorldBridgeToEntityModules.cpp).

namespace WorldModule
{

namespace
{
    // WorldModule (the X360 r3 context) member offsets this bridge writes through.
    // FLAG: the WorldModule class layout is not reconstructed; the two members are
    // accessed at their X360 byte offsets pending the module's own home.
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

    // FLAG cross-home cast: both the race-car active output interface and the world-entity
    // input buffer's ActiveRaceCarInterfaceStorage model the SAME 10480-byte X360 payload;
    // the setter block-copies it (the X360 re-fetches GetActiveRaceCarOutputInterface).
    lpWorldInputBuffer_PreScene->SetActiveRaceCarInterface(
        *reinterpret_cast<const BrnWorld::WorldEntityIO::InputBuffer_PreScene::ActiveRaceCarInterfaceStorage*>(
            lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface()));

    // ---- publish the player active-race-car index into the WorldModule -------------------
    u8* lpWorldModuleBytes = static_cast<u8*>(lpWorldModule);
    if (lpRaceCarEntityOutputInterface->IsPlayerCarActive())
    {
        *reinterpret_cast<EActiveRaceCarIndex*>(
            lpWorldModuleBytes + KU_WORLD_MODULE_PLAYER_ACTIVE_RACE_CAR_INDEX_OFFSET) =
            lpRaceCarEntityOutputInterface->GetPlayerActiveRaceCarIndex();
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

// @ 0x827A51F0 -- WorldBridgeEntityModulesToEntityModules.cpp:69. Latch the traffic
// module's post-scene traffic->race-car interface into the race-car module's pre-physics
// input buffer. Both null tripwires are NON-gating (the X360 falls through after firing);
// the X360 tail returns the forwarded call's result as a register artifact -- the logical
// return type is void.
//
// FLAG cross-home cast: the race-car buffer's TrafficToRaceCarInterface_PostScene resolves to
// the class-level BrnTrafficIO::TrafficToRaceCarInterface_PostScene (BrnRaceCarEntityModuleIO.h
// :301), while the traffic getter returns the OutputBuffer_PostScene-nested struct of the same
// name; both model the SAME X360 payload (the setter block-copies it), so the getter result is
// reinterpret_cast to the setter's pointer type -- the documented cross-home adapter used by the
// sibling bridges.
void BridgeTrafficToRaceCar_PrePhysics(
    void* lpWorldModule,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutputBuffer_PostScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpRaceCarInputBuffer_PrePhysics != 0, "lpRaceCarInputBuffer_PrePhysics");   // :72
    CGS_ASSERT(lpTrafficOutputBuffer_PostScene != 0, "lpTrafficOutputBuffer_PostScene");   // :73

    lpRaceCarInputBuffer_PrePhysics->SetTrafficToRaceCarInterface_PostScene(
        reinterpret_cast<const BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::TrafficToRaceCarInterface_PostScene*>(
            lpTrafficOutputBuffer_PostScene->GetTrafficToRaceCarInterface_PostScene()));
}

// @ 0x827AD788 -- WorldBridgeEntityModulesToEntityModules.cpp:46. Merge the traffic
// module's pre-scene trigger-management output (add + remove trigger queues) into the
// trigger module's pre-scene input buffer. The X360 inlines the aggregate merge as a
// VariableEventQueue<131072,16>::Append (the add queue) followed by an
// InRemoveTriggerEvent-queue Append at +131088 (0x20010); both are reproduced by the
// committed TriggerManagementInputInterface::Append. The X360 tail returns the remove-queue
// Append result in r3; the logical return type is void.
void BridgeTrafficToTrigger_PreScene(
    void* lpWorldModule,
    BrnWorld::TriggerEntityModuleIO::InputBuffer_PreScene* lpTriggerInputBuffer_PreScene,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutputBuffer_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpTriggerInputBuffer_PreScene != 0, "lpTriggerInputBuffer_PreScene");       // :50
    CGS_ASSERT(lpTrafficOutputBuffer_PreScene != 0, "lpTrafficOutputBuffer_PreScene");     // :51

    // FLAG cross-home casts: both IO buffers expose their trigger-management member as opaque
    // storage; both ARE the trigger module's TriggerManagementInputInterface (whose Append
    // performs the X360's inlined add-queue + remove-queue whole-interface merge).
    BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface* lpTriggerManagementInput =
        reinterpret_cast<BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface*>(
            lpTriggerInputBuffer_PreScene->GetInputInterface());
    lpTriggerManagementInput->Append(
        *reinterpret_cast<const BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface*>(
            lpTrafficOutputBuffer_PreScene->GetTriggerManagementInputInterface()));
}

}
