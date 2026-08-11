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

// ⛔ MOVED OUT 2026-08-01 (car-select hand-off wave): BridgeRaceCarModuleToWorldModule_PreScene
// @0x827A52B0 now lives in its own TU, GameSource/World/Bridges/WorldBridgeRaceCarToWorldModule.cpp,
// so it can be MOUNTED. This TU cannot be: its two remaining bridges reference three IO accessors
// that are still declaration-only (MEASURED +3 unresolved --
//   TriggerEntityModuleIO::InputBuffer_PreScene::GetInputInterface,
//   BrnTrafficIO::OutputBuffer_PostScene::GetTrafficToRaceCarInterface_PostScene,
//   BrnTrafficIO::OutputBuffer_PreScene::GetTriggerManagementInputInterface),
// and the moved bridge needs none of them. Fold it back here when they land -- as the
// WorldModule METHOD it now is, NOT as a namespace function.
//
// ⛔ ITS X360-OFFSET CONSTANTS ARE DELETED WITH IT (2026-08-11). They read
//   KU_WORLD_MODULE_PLAYER_ACTIVE_RACE_CAR_INDEX_OFFSET = 6167272 / ..._TYPE_ARRAY = 6167280,
// which are the X360 offsets of meLocalPlayerActiveRaceCarIndex / maeCarControls. On the x64
// PC layout those members sit at 6234776 / 6234784 (compile-time offsetof probe), so the
// constants were a live corruption, not a stopgap. Do not reintroduce them here.

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
