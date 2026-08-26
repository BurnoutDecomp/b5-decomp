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

// =================================================================================================
// @ 0x827AD688 -- WorldModule::BridgeRaceCarModuleToAIModule_PostScene   (resetpump wave)
//
// The console body is four instructions of work behind two non-gating null tripwires:
//   0x827AD6EC  v5 = lpRaceCarOutputBuffer_PostScene->GetAIModuleRequestInterface()  (sub_8279DA48,
//                    the READ-locked const twin of the write accessor @0x822B5608)
//   0x827AD6FC  lpAIInputBuffer->AppendAIModuleRequestInterface(v5)                  (@0x827AC960)
// and AppendAIModuleRequestInterface is `Clear(); Append(src);` on the 128-deep
// ResetOnTrackRequest queue at the AI input buffer's +0xFBB0.
//
// ⭐ THIS IS THE HOP THAT CARRIES A CRASHED CAR'S RESET REQUEST OUT OF THE RACE-CAR MODULE.
// RCEM::SendResetOnTrackRequests fills the source queue in the post-scene pass three call
// sites earlier in WorldModule::Update (BrnWorldModule.cpp:2665 vs :2697), so the whole
// request/answer round trip closes inside ONE frame.
//
// ⚠️ The X360 assert strings are baked with WorldBridgeEntityModulesToAI.cpp:60/61, which is
// this file -- that is what identifies 0x827AD688 as belonging here rather than to the
// prop/AI bridge TU that carries its PrePhysics sibling.
// =================================================================================================
void BridgeRaceCarModuleToAIModule_PostScene(
    void* lpWorldModule,
    BrnAI::AIModuleIO::InputBuffer* lpAIInputBuffer,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene* lpRaceCarOutputBuffer_PostScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpAIInputBuffer != 0, "lpAIInputBuffer");                                    // :60
    CGS_ASSERT(lpRaceCarOutputBuffer_PostScene != 0, "lpRaceCarOutputBuffer_PostScene");    // :61

    if (lpAIInputBuffer == 0 || lpRaceCarOutputBuffer_PostScene == 0)
    {
        return;
    }

    lpAIInputBuffer->AppendAIModuleRequestInterface(
        lpRaceCarOutputBuffer_PostScene->GetAIModuleRequestInterface());
}
}
