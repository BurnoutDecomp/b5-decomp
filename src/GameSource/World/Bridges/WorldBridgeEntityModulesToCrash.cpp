#include "GameSource/World/Bridges/WorldBridgeEntityModulesToCrash.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT


// @ 0x827A5060 -- latch the race-car module's pre-scene active-car view (read-locked
// getter @0x8279D500) into the crash module's input buffer.
// Both null tripwires are NON-gating (the X360 falls through after firing); the
// X360 tail returns the forwarded call's result as a register artifact -- the
// logical return type is void.

namespace WorldModule
{
// @ 0x827A5060
void BridgeEntityModulesToCrashModule_PreScene(
    void* lpWorldModule,
    BrnWorld::CrashIO::InputBuffer_PreScene* lpCrashInputBuffer_PreScene,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpCrashInputBuffer_PreScene != 0, "lpCrashInputBuffer_PreScene");           // :37
    CGS_ASSERT(lpRaceCarOutputBuffer_PreScene != 0, "lpRaceCarOutputBuffer_PreScene");     // :38

    lpCrashInputBuffer_PreScene->SetActiveRaceCarInterface(
        lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface());
}
}
