// ===========================================================================
// BrnWorldModule_wG_Bridges_01.cpp -- four WorldModule per-frame bridge seams:
// each takes one module's output-buffer queue or interface and merges or latches
// it into the next module's input buffer.
//
// Source getter first, then destination getter, then the merge -- both getters are
// lock tripwires, so the order is observable. Null tripwires appear only where the
// console body has one; two of the four have no compare at all.
// FLAG: the console brackets two of these in a CPU perf monitor taken out of the
// world-module context, which arrives here as an untyped pointer; the monitors are
// not modelled, the standing disposition of every landed sibling in Bridges/.
// ===========================================================================

#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h" // BridgeRaceCarModuleToTrafficModule_PrePhysics
#include "GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h"        // BridgeSceneQueryResultsToTriggerModule_PrePhysics
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"       // BridgeRaceCarEntityInfoToOutput_PrePhysics
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToScene.h"        // BridgeTriggerModuleToSceneModule_PostScene

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                  // SceneManagerIO::InputBuffer_Query / OutputBuffer
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"    // InEventLineTestFine
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/BrnWorldModuleIO.h"                                      // BrnWorldIO::UpdateOutputBuffer

#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                    // VariableEventQueue / Event

namespace WorldModule
{

// ---------------------------------------------------------------------------
// WorldModule::BridgeRaceCarModuleToTrafficModule_PrePhysics
//
// One latch: read the race car's pre-physics player-reset interface out of its own
// output buffer (read-lock getter) and hand it to the traffic module's pre-physics
// input buffer (write-lock setter, which copies it into its member seat).
// No null tripwires: the console body has no compare and no assert, unlike its
// three siblings.
// ---------------------------------------------------------------------------
void BridgeRaceCarModuleToTrafficModule_PrePhysics(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutputBuffer_PrePhysics)
{
    (void)lpWorldModule;   // the world-module context: copied out of its register and never read

    // Source getter (read-lock), then destination setter (write-lock) -- the console's order.
    lpTrafficInputBuffer_PrePhysics->SetPlayerResetInterface(
        lpRaceCarOutputBuffer_PrePhysics->GetPlayerResetInterface());
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeSceneQueryResultsToTriggerModule_PrePhysics
//
// The trigger module's half of the scene-query results fan-out: append the scene
// manager's results ring into the trigger module's pre-physics scene-result queue.
// Both are the same variable-event-queue instantiation, so the merge is the queue's
// own Append. No null tripwires -- this body has no compare at all.
// It is the inbound half of the round trip BridgeTriggerModuleToSceneModule_PostScene
// below opens.
// ---------------------------------------------------------------------------
void BridgeSceneQueryResultsToTriggerModule_PrePhysics(
    void* lpWorldModule,
    BrnWorld::TriggerEntityModuleIO::InputBuffer_PrePhysics* lpTriggerInputBuffer_PrePhysics,
    const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneQueryOutput)
{
    (void)lpWorldModule;

    const CgsSceneManager::SceneManagerIO::OutputBuffer::SceneQueryResultsQueue* lpResults =
        lpSceneQueryOutput->GetSceneQueryResultsQueue();

    lpTriggerInputBuffer_PrePhysics->GetSceneResultQueue()->Append(*lpResults);
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeRaceCarEntityInfoToOutput_PrePhysics
//
// The race car's pre-physics game-event flush: append the race-car module's own
// game-event queue into the world update-output buffer's game-event queue, which
// the game-state module drains.
// Both asserts are the console's and both are NON-gating -- it fires and falls
// through into the transfer. The strings are verbatim ("lpOutputBuffer" carries no
// "!= NULL" tail; not a typo).
// The source queue DERIVES from the shared variable-event-queue instantiation while
// the destination is a direct alias of it, so the source is bound to the base
// reference explicitly rather than deduced through the derived type.
// ---------------------------------------------------------------------------
void BridgeRaceCarEntityInfoToOutput_PrePhysics(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutput_PrePhysics)
{
    (void)lpWorldModule;   // read only for the perf-monitor handle, which is not modelled

    CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer");
    CGS_ASSERT(lpRaceCarOutput_PrePhysics != 0, "lpRaceCarOutputBuffer_PrePhysics");

    // Source getter (read-lock) first, destination getter (write-lock) second -- the
    // console's order, and both are lock tripwires.
    const CgsModule::VariableEventQueue<1536, 16>& lrSourceEvents =
        *lpRaceCarOutput_PrePhysics->GetGameEventQueue();

    lpOutputBuffer->GetGameEventQueue()->Append(lrSourceEvents);
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeTriggerModuleToSceneModule_PostScene
//
// The trigger module's post-scene query staging. The trigger module posts its
// line queries into a VARIABLE event queue (heterogeneous, records tagged with a
// type id); the scene manager wants them in its TYPED fine-line-test queue. So
// this bridge cannot be a queue Append -- it walks the source record by record
// and re-adds each one.
//
// The loop guard is the record POINTER, not the type id: a type id of 0 still
// iterates, a null record ends the walk. Both asserts, and the "Invalid event type."
// arm, are non-gating -- the walk continues either way.
//
// The record is reinterpreted rather than cast through a hierarchy: the variable
// queue hands back a pointer into its own packed byte buffer and the scene-manager
// query element is an unrelated type -- the sanctioned external-byte-stream case.
// The literal 5 is the console's compare immediate, left a literal on purpose: it is
// the record id the trigger producer stamps, and the scene manager's query-result
// enumeration's 5 is a COUNT, so naming it after that would be invention.
// ---------------------------------------------------------------------------
void BridgeTriggerModuleToSceneModule_PostScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneQueryInput,
    const BrnWorld::TriggerEntityModuleIO::OutputBuffer_PostScene* lpTriggerOutputBuffer_PostScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneQueryInput != 0, "lpSceneModuleInputBuffer != NULL");
    CGS_ASSERT(lpTriggerOutputBuffer_PostScene != 0, "lpTriggerOutput_PostScene != NULL");

    const BrnWorld::TriggerEntityModuleIO::SceneFineQueryQueue* lpTriggerQueries =
        lpTriggerOutputBuffer_PostScene->GetSceneFineQueryQueue();

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;

    s32 liType = lpTriggerQueries->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != 0)
    {
        if (liType == 5)   // the trigger module's fine line-test record id
        {
            const CgsSceneManager::SceneManagerIO::InEventLineTestFine* lpQuery =
                reinterpret_cast<const CgsSceneManager::SceneManagerIO::InEventLineTestFine*>(lpEvent);

            lpSceneQueryInput->GetFineLineTestQueue()->AddEvent(*lpQuery);
        }
        else
        {
            // Non-gating on the console: it fires and then falls into the next step.
            CGS_ASSERT(false, "Invalid event type.");
        }

        liType = lpTriggerQueries->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}

}
