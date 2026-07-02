#include "GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT


// @ 0x827ABAC8 -- append the scene manager's query-results ring (read-locked getter
// @0x823B1ED0) into the race-car module's pre-physics scene-result queue
// (write-locked getter @0x8279DB98) through the real instantiated
// VariableEventQueue<32768,16>::Append symbol.
// Both null tripwires are NON-gating (the X360 falls through after firing); the
// X360 tail returns the forwarded call's result as a register artifact -- the
// logical return type is void.

namespace WorldModule
{
// @ 0x827ABAC8
void BridgeSceneQueryResultsToRaceCarModule_PrePhysics(
    void* lpWorldModule,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
    const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneModuleOutputBuffer)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpRaceCarInputBuffer_PrePhysics != 0, "lpRaceCarInputBuffer_PrePhysics != NULL");   // :98
    CGS_ASSERT(lpSceneModuleOutputBuffer != 0, "lpSceneModuleOutputBuffer != NULL");               // :99

    lpRaceCarInputBuffer_PrePhysics->GetSceneResultQueue()->Append(
        *lpSceneModuleOutputBuffer->GetSceneQueryResultsQueue());
}
}
