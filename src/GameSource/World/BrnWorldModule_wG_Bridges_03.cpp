// ===========================================================================
// BrnWorldModule_wG_Bridges_03.cpp -- four WorldModule per-frame bridge seams: two
// that stage a module's own query queues into the scene manager's query input buffer,
// one that fans the scene manager's results back out to the traffic module's two
// pre-physics input buffers, and one that posts the traffic module's removed-car list
// to the GUI as a single update-output event.
//
// Source getter first, then destination getter, then the merge -- both getters are
// lock tripwires, so the order is observable. Null tripwires appear only where the
// console body has one.
// FLAG: the console brackets these in CPU perf monitors taken out of the world-module
// context, which arrives here as an untyped pointer; the monitors are not modelled,
// the standing disposition of every landed sibling in Bridges/.
// ===========================================================================

#include "GameSource/World/Bridges/WorldBridgeEntityModulesToScene.h"   // the two ...ToSceneModule_PostScene decls
#include "GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h"   // BridgeSceneQueryResultsToTrafficModule_PrePhysics
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"  // BridgeTrafficCarEntityInfoToOutput_PrePhysics

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                        // SceneManagerIO::InputBuffer_Query / OutputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"  // RaceCarEntityModuleIO::OutputBuffer_PostScene
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"  // BrnTrafficIO::OutputBuffer_PostScene / InputBuffer_Pre|PostPhysics

#include "GameSource/World/BrnWorldModuleIO.h"                                            // BrnWorldIO::UpdateOutputBuffer
#include "GameShared/GameClasses/Containers/CgsArray.h"                                   // Array<short,25> (the GUI record's byte image)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                          // VariableEventQueue

namespace WorldModule
{

// ---------------------------------------------------------------------------
// WorldModule::BridgeRaceCarModuleToSceneModule_PostScene
//
// Two merges, in the console's order: the race car's coarse query queue into the
// scene query input buffer's coarse queue, then its fine line-test queue into the
// matching typed fine queue. Each leg is source getter (read-lock) then destination
// getter (write-lock) then the queue's own Append -- the coarse pair are the same
// variable-event-queue instantiation, the fine pair the same typed event queue.
//
// Both null tripwires are the console's and both are NON-gating: it fires and falls
// through into the transfer.
//
// The coarse source DERIVES from the shared variable-event-queue instantiation
// (InCoarseQueryQueue<16384> adds only enqueue helpers), so it is bound to the base
// reference explicitly rather than deduced through the derived type -- the same
// binding the landed BridgeRaceCarEntityInfoToOutput_PrePhysics needs.
// ---------------------------------------------------------------------------
void BridgeRaceCarModuleToSceneModule_PostScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneQueryInput,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene* lpRaceCarOutputBuffer_PostScene)
{
    (void)lpWorldModule;   // the world-module context: copied out of its register and never read

    CGS_ASSERT(lpSceneQueryInput != 0, "lpSceneModuleInputBuffer != NULL");                    // :123
    CGS_ASSERT(lpRaceCarOutputBuffer_PostScene != 0, "lpRaceCarOutputBuffer_PostScene != NULL"); // :124

    const CgsModule::VariableEventQueue<16384, 16>& lrCoarseSource =
        *lpRaceCarOutputBuffer_PostScene->GetSceneCoarseQueryQueue();
    lpSceneQueryInput->GetCoarseQueryQueue()->Append(lrCoarseSource);

    lpSceneQueryInput->GetFineLineTestQueue()->Append(
        *lpRaceCarOutputBuffer_PostScene->GetSceneFineLineTestQueue());
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeTrafficModuleToSceneModule_PostScene
//
// The traffic module's half of the same staging, coarse queue only: its post-scene
// output buffer carries no fine line-test queue, so there is one merge and one
// tripwire. The console asserts the DESTINATION only -- there is no compare on the
// traffic output buffer at all, and the assert that is there is non-gating.
// ---------------------------------------------------------------------------
void BridgeTrafficModuleToSceneModule_PostScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneQueryInput,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutputBuffer_PostScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneQueryInput != 0, "lpSceneModuleInputBuffer != NULL");   // :144

    const CgsModule::VariableEventQueue<16384, 16>& lrCoarseSource =
        *lpTrafficOutputBuffer_PostScene->GetSceneCoarseQueryQueue();

    lpSceneQueryInput->GetCoarseQueryQueue()->Append(lrCoarseSource);
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeSceneQueryResultsToTrafficModule_PrePhysics
//
// The traffic module's half of the scene-query results fan-out, and the only one of
// the family with TWO destinations: the SAME results ring is appended onto the
// traffic module's pre-physics scene-result queue and onto its post-physics one.
// Both are the same variable-event-queue instantiation as the source, so each merge
// is the queue's own Append. The source getter runs again for the second leg -- it is
// a read-lock tripwire, so the repeat is observable and kept.
//
// Only the SOURCE has a null tripwire on the console (and it is non-gating); neither
// destination is compared.
//
// ⚠️ MEASURED, and left as it stands: the console reaches its second argument's
// scene-result queue at +166960 and its third argument's through the other buffer's
// accessor, i.e. the two traffic input buffers arrive in the opposite order to the
// spelling this declaration has carried since it was first declared. Because BOTH
// destinations receive an Append of the SAME source ring, the two orders are
// byte-identical in effect, so the parameter names are left alone rather than
// churning the call site in BrnWorldModule.cpp for a no-op.
// ---------------------------------------------------------------------------
void BridgeSceneQueryResultsToTrafficModule_PrePhysics(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInputBuffer_PostPhysics,
    BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
    const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneQueryOutput)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneQueryOutput != 0, "lpSceneModuleOutputBuffer != NULL");   // :119

    lpTrafficInputBuffer_PostPhysics->GetSceneResultQueue()->Append(
        *lpSceneQueryOutput->GetSceneQueryResultsQueue());

    lpTrafficInputBuffer_PrePhysics->GetSceneResultQueue()->Append(
        *lpSceneQueryOutput->GetSceneQueryResultsQueue());
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeTrafficCarEntityInfoToOutput_PrePhysics
//
// The only bridge in this TU that BUILDS a record instead of merging a queue: it
// walks the traffic module's remove-crashed-traffic request queue, pulls the 14-bit
// entity index out of each request's volume-instance handle, and posts the whole
// list to the GUI as one event on the world update-output buffer's GUI event queue.
//
// The whole body is gated on the pre-physics output buffer's showtime flag -- when it
// is clear nothing is read and nothing is posted (this IS a gate, unlike the asserts).
//
// Both null tripwires are the console's and both are NON-gating; the strings are
// verbatim ("lpOutputBuffer" carries no "!= NULL" tail, as on its pre-physics sibling).
//
// The record is an Array<short,25> by its own byte image: the console Clears the count
// word, Appends one short per request through Array<short,25>::Append, and passes 56 --
// sizeof(Array<short,25>) -- as the event size. The record id 209 is the console's
// immediate, kept a literal exactly as its 208 sibling
// (BridgeTrafficEntityInfoToOutput_PreScene) keeps its own.
//
// The loop re-reads the queue length each iteration, as the console does; the guard is
// a separate ">0" test before the first pass (a do/while walk under an if).
// ---------------------------------------------------------------------------
void BridgeTrafficCarEntityInfoToOutput_PrePhysics(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics* lpTrafficOutput_PrePhysics)
{
    (void)lpWorldModule;   // read only for the perf-monitor handle, which is not modelled

    CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer");                              // :355
    CGS_ASSERT(lpTrafficOutput_PrePhysics != 0, "lpTrafficOutput_PrePhysics");      // :356

    if (!lpTrafficOutput_PrePhysics->GetPlayingShowtime())
        return;

    const BrnPhysics::Vehicle::VehicleInputInterface::RemoveTrafficEventQueue* lpRemoveRequests =
        lpTrafficOutput_PrePhysics->GetVehicleInputInterface()->GetRemoveTrafficEvents();

    Array<short, 25> lCrashedCars;
    static_assert(sizeof(lCrashedCars) == 56,
                  "the GUI record's byte image must match the console's baked event size");
    lCrashedCars.Clear();

    if (lpRemoveRequests->GetLength() > 0)
    {
        s32 liIndex = 0;
        do
        {
            const short lsEntityIndex = static_cast<short>(
                lpRemoveRequests->GetEvent(liIndex).mVolumeInstanceID.GetEntityIDEntityIndex());
            lCrashedCars.Append(lsEntityIndex);
            ++liIndex;
        }
        while (liIndex < lpRemoveRequests->GetLength());
    }

    lpOutputBuffer->GetGuiEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lCrashedCars),
        209,
        static_cast<s32>(sizeof(lCrashedCars)));
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeRaceCarModuleToTrafficModule_PostScene
//
// One publish: read the race car's post-scene race-car-to-traffic interface out of its
// own output buffer (read-lock getter) and hand it to the traffic module's post-scene
// input buffer (write-lock setter, which Clear+Appends the two rival queues onto its
// member seat and copies the flag word and the showtime density scale).
//
// Both null tripwires are the console's and both are NON-gating; the strings are
// verbatim (neither carries a "!= NULL" tail).
// ---------------------------------------------------------------------------
void BridgeRaceCarModuleToTrafficModule_PostScene(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostScene* lpTrafficInputBuffer_PostScene,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene* lpRaceCarOutputBuffer_PostScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpTrafficInputBuffer_PostScene != 0, "lpTrafficInputBuffer_PostScene");   // :61
    CGS_ASSERT(lpRaceCarOutputBuffer_PostScene != 0, "lpRaceCarOutputBuffer_PostScene"); // :62

    // Source getter (read-lock) first, destination setter (write-lock) second -- the
    // console's order, and both are lock tripwires.
    lpTrafficInputBuffer_PostScene->SetRaceCarToTrafficInterface(
        lpRaceCarOutputBuffer_PostScene->GetRaceCarToTrafficInterface());
}

}   // namespace WorldModule
