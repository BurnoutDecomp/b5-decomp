#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"

// WorldModule::BridgeRaceCarModuleToTrafficModule_PreScene -- reconstructed from
// BURNOUT_X360_ARTIST.XEX @0x827A50E0 (DWARF home WorldBridgeEntityModulesToEntityModules.cpp;
// its sibling TU cannot be mounted yet -- three of the OTHER bridges' accessors are still
// declaration-only -- so this bridge lands in its own mountable TU per the committed
// WorldBridgeRaceCarToWorldModule.cpp precedent; fold back when the sibling TU mounts).
//
// Landed wave T1 round-4 consolidation (2026-08-21): this was THE activeHulls==0 blocker --
// the ONE producer (xrefs_to on all three setters) that primes the traffic module's
// pre-scene / post-scene / post-physics input buffers with the race-car pre-scene output, so
// PreSceneUpdate's IsPlayerCarActive() test and UpdateRaceCarHulls' player lookup finally
// read a buffer something fills. Sole caller: WorldModule::EntityModulePreSceneUpdate
// @0x827BD1F0 (the BrnWorldModule.cpp spine already calls it under the three-buffer write
// lock). The four null tripwires are NON-gating (the X360 falls through after firing); the
// X360 tail returns the last setter's r3 as a register artifact -- logical return type void.

namespace WorldModule
{

void BridgeRaceCarModuleToTrafficModule_PreScene(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PreScene* lpTrafficInputBuffer_PreScene,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostScene* lpTrafficInputBuffer_PostScene,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInputBuffer_PostPhysics,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpTrafficInputBuffer_PreScene != 0, "lpTrafficInputBuffer_PreScene");       // :33
    CGS_ASSERT(lpTrafficInputBuffer_PostScene != 0, "lpTrafficInputBuffer_PostScene");     // :34
    CGS_ASSERT(lpTrafficInputBuffer_PostPhysics != 0, "lpTrafficInputBuffer_PostPhysics"); // :35
    CGS_ASSERT(lpRaceCarOutputBuffer_PreScene != 0, "lpRaceCarOutputBuffer_PreScene");     // :36

    // 0x827A5194..0x827A51CC: the same const getter (@0x8279D500) feeds all three setters.
    lpTrafficInputBuffer_PreScene->SetActiveRaceCarOutputInterface(
        lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface());
    lpTrafficInputBuffer_PostScene->SetActiveRaceCarOutputInterface(
        lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface());
    lpTrafficInputBuffer_PostPhysics->SetActiveRaceCarOutputInterface(
        lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface());

    // 0x827A51D0..0x827A51E0: the global interface rides only on the pre-scene buffer
    // (getter @0x8279D5A8, setter @0x8279FCA0).
    lpTrafficInputBuffer_PreScene->SetGlobalRaceCarOutputInterface(
        lpRaceCarOutputBuffer_PreScene->GetGlobalRaceCarOutputInterface());
}

}
