#include "GameSource/World/Bridges/WorldBridgeEntityModulesToAI.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT


// @ 0x827A4FA0 -- latch the race-car module's pre-scene AI view (read-locked getter
// @0x8279D6F8) into the AI module's input buffer.
// Both null tripwires are NON-gating (the X360 falls through after firing); the
// X360 tail returns the forwarded call's result as a register artifact -- the
// logical return type is void.

namespace WorldModule
{
// @ 0x827A4FA0
void BridgeRaceCarModuleToAIModule_PreScene(
    void* lpWorldModule,
    BrnAI::AIModuleIO::InputBuffer* lpAIInputBuffer,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpAIInputBuffer != 0, "lpAIInputBuffer");                                   // :40
    CGS_ASSERT(lpRaceCarOutputBuffer_PreScene != 0, "lpRaceCarOutputBuffer_PreScene");     // :41

    lpAIInputBuffer->SetRaceCarAIInterface(
        lpRaceCarOutputBuffer_PreScene->GetRaceCarAIInterface());
}
}
