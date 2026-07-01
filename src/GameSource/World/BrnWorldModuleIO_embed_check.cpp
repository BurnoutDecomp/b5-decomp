// Compile-only embedder check for BrnWorldIO::UpdateInputBuffer. Not part of the shipping
// build: it embeds the buffer by value (the way the World module owns one), forcing the full
// frozen member layout to instantiate, re-pins the X360-proven leading-array byte offsets with
// static_asserts, and exercises a representative sample of the 32 out-of-line accessors by name
// (one read-lock getter, one write-lock getter, the queue Append mergers, the index setters,
// and the interface Set writers) -- the same calls the producing/consuming modules make.
#include "GameSource/World/BrnWorldModuleIO.h"

namespace BrnWorldIO
{

// Stand-in for the owning World module: embeds the buffer by value (forces the full frozen
// layout to instantiate; the X360-proven leading-array byte offsets are pinned inside the
// class by UpdateInputBuffer::_AssertLayout(), compiled with the .cpp TU).
struct UpdateInputBufferEmbedderCheck
{
    UpdateInputBuffer mUpdateInputBuffer;
};

void EmbedderCheck_Exercise(UpdateInputBufferEmbedderCheck& lrEmbedder,
                            const VehicleInputInterface* lpVehicleInput,
                            const VehicleDriverInputInterface* lpVehicleDriverInput,
                            const GameActionQueue* lpGameActionQueue,
                            const TimerStatusInterface* lpTimer,
                            const TakedownEventQueue* lpTakedownQueue,
                            const TriggerQueryInputInterface* lpTriggerQuery,
                            const TrafficNetworkInputInterface* lpTrafficNet,
                            const CrashNetworkInputInterface* lpCrashNet,
                            const RaceCarRaceDistanceInterface* lpRaceDistance,
                            const ScoringInterface* lpScoring,
                            const OnlineScoringInterface* lpOnlineScoring,
                            const ReplayStatusInterface* lpReplayStatus,
                            const PlayerVehicleControls* lpControls,
                            BrnNetwork::EPaybackType lePaybackType,
                            EActiveRaceCarIndex leActiveRaceCar)
{
    UpdateInputBuffer& lrBuffer = lrEmbedder.mUpdateInputBuffer;

    // store-only per-race-car setters
    lrBuffer.SetRaceCarColourIndex(leActiveRaceCar, 3);
    lrBuffer.SetRaceCarPaintFinishIndex(leActiveRaceCar, 4);
    lrBuffer.SetLostContact(leActiveRaceCar);
    lrBuffer.SetRegainedContact(leActiveRaceCar);
    lrBuffer.SetCarSelectStatus(leActiveRaceCar, true);

    // vehicle interfaces (read-getter + append)
    (void)lrBuffer.GetVehicleInputInterface();
    lrBuffer.AppendVehicleInputInterface(lpVehicleInput);
    (void)lrBuffer.GetVehicleDriverInputInterface();
    lrBuffer.AppendVehicleDriverInputInterface(lpVehicleDriverInput);

    // queues
    (void)lrBuffer.GetGameActionQueue();
    lrBuffer.AppendGameActionQueue(lpGameActionQueue);
    lrBuffer.AppendTakedownEventQueue(lpTakedownQueue);
    lrBuffer.AppendTriggerQueryInputInterface(lpTriggerQuery);

    // status / network / scoring / replay interfaces
    lrBuffer.SetTimerStatusInterface(lpTimer);
    (void)lrBuffer.GetTrafficNetworkInterface();
    lrBuffer.SetTrafficNetworkInterface(lpTrafficNet);
    (void)lrBuffer.GetCrashNetworkInterface();
    lrBuffer.SetCrashNetworkInterface(lpCrashNet);
    lrBuffer.SetRaceCarRaceDistanceInterface(lpRaceDistance);
    (void)lrBuffer.GetScoringInterface();
    lrBuffer.SetScoringInterface(lpScoring);
    lrBuffer.SetOnlineScoringInterface(lpOnlineScoring);
    (void)lrBuffer.GetReplayStatusInterface();
    lrBuffer.SetReplayStatusInterface(lpReplayStatus);

    // debug controller (both overloads)
    (void)static_cast<const UpdateInputBuffer&>(lrBuffer).GetDebugController();
    (void)lrBuffer.GetDebugController();

    // controller-active flag (both directions)
    (void)static_cast<const UpdateInputBuffer&>(lrBuffer).GetControllerActive();
    lrBuffer.SetControllerActive(true);

    // world-entity request interface (both overloads)
    (void)lrBuffer.GetWorldEntityRequestInterface();
    (void)static_cast<const UpdateInputBuffer&>(lrBuffer).GetWorldEntityRequestInterface();

    // player controls + active payback
    lrBuffer.SetPlayerVehicleControls(lpControls);
    lrBuffer.SetActivePaybackType(lePaybackType);
    lrBuffer.SetActivePaybackAggressor(leActiveRaceCar);
}

// ----------------------------------------------------------------------------
// UpdateOutputBuffer embedder check (additive): embeds the OUTPUT buffer by value
// (forces the full frozen member layout to instantiate) and exercises the 46
// out-of-line accessors by name -- the same calls the bridging module makes.
// ----------------------------------------------------------------------------
struct UpdateOutputBufferEmbedderCheck
{
    UpdateOutputBuffer mUpdateOutputBuffer;
};

void EmbedderCheck_ExerciseOutput(UpdateOutputBufferEmbedderCheck& lrEmbedder,
                                  const UpdateOutputBuffer::OutTriangleCacheInterface* lpTriangleCache,
                                  const UpdateOutputBuffer::WorldResourceRequestInterface* lpResourceRequest,
                                  const UpdateOutputBuffer::DirectorVehicleInputInterface* lpDirectorVehicleInput,
                                  const UpdateOutputBuffer::RCEntityGlobalOutputInterface* lpRaceCarGlobal,
                                  const UpdateOutputBuffer::TriggerEntityModuleOutputInterface* lpTriggerEntity,
                                  const UpdateOutputBuffer::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCar,
                                  const UpdateOutputBuffer::AICarOutputInterface* lpAICarOutput,
                                  const PlayerVehicleControls* lpControls,
                                  const UpdateOutputBuffer::RouteResponseQueue* lpRouteResponses,
                                  const UpdateOutputBuffer::TrafficNetworkOutputInterface* lpTrafficNetwork,
                                  const UpdateOutputBuffer::TrafficSoundOutputInterface* lpTrafficSound,
                                  const UpdateOutputBuffer::TrafficDirectorOutputInterface* lpTrafficDirector,
                                  const UpdateOutputBuffer::CrashNetworkOutputInterface* lpCrashNetwork,
                                  const UpdateOutputBuffer::GameEventQueue* lpGameEvents,
                                  const UpdateOutputBuffer::DeformationOutputInterface* lpDeformation,
                                  const UpdateOutputBuffer::TrafficTypeResponseQueue* lpTrafficTypeResponses,
                                  const UpdateOutputBuffer::StatusInterface* lpWorldEntityStatus,
                                  const UpdateOutputBuffer::SoundWorldLoadInterface* lpSoundWorldLoads,
                                  const UpdateOutputBuffer::ReplayRequestInterface* lpReplayRequests,
                                  const UpdateOutputBuffer::PropVFXLocatorQueue* lpPropVFXLocators,
                                  const UpdateOutputBuffer::PropBecamePhysicalEventQueue* lpPropBecamePhysical,
                                  const UpdateOutputBuffer::PropUpdateNotificationQueue* lpPropUpdateNotifications)
{
    UpdateOutputBuffer&       lrBuffer      = lrEmbedder.mUpdateOutputBuffer;
    const UpdateOutputBuffer& lrConstBuffer = lrEmbedder.mUpdateOutputBuffer;

    // player indices
    (void)lrConstBuffer.GetPlayerGlobalRaceCarIndex();

    // triangle cache (lock-free mutator)
    lrBuffer.AppendTriangleCacheInterface(lpTriangleCache);

    // resource / attrib-sys request interfaces
    (void)lrConstBuffer.GetResourceRequestResourceInterface();
    (void)lrBuffer.GetResourceRequestResourceInterface();
    lrBuffer.AppendResourceRequestInterface(lpResourceRequest);
    (void)lrBuffer.GetAttribSysVaultRequestInterface();

    // vehicle output interfaces
    (void)lrConstBuffer.GetVehicleOutputInterface();
    (void)lrBuffer.GetVehicleOutputInterface();
    (void)lrConstBuffer.GetVehicleManagerOutputInterface();

    // director vehicle input
    (void)lrConstBuffer.GetDirectorVehicleInputInterface();
    lrBuffer.SetDirectorVehicleInputInterface(lpDirectorVehicleInput);

    // race-car entity / trigger interfaces
    lrBuffer.SetRaceCarGlobalOutputInterface(lpRaceCarGlobal);
    lrBuffer.SetTriggerEntityOutputInterface(lpTriggerEntity);
    lrBuffer.SetActiveRaceCarOutputInterface(lpActiveRaceCar);
    lrBuffer.SetReplayActiveRaceCarOutputInterface(lpActiveRaceCar);

    // contact spy (both overloads)
    (void)lrConstBuffer.GetContactSpyInterface();
    (void)lrBuffer.GetContactSpyInterface();

    // AI car output
    (void)lrConstBuffer.GetAICarOutputInterface();
    lrBuffer.SetAICarOutputInterface(lpAICarOutput);

    // player vehicle controls
    (void)lrConstBuffer.GetPlayerVehicleControls();
    lrBuffer.SetPlayerVehicleControls(lpControls);

    // route responses
    lrBuffer.AppendRouteResponseQueue(lpRouteResponses);

    // traffic network / sound / director
    (void)lrConstBuffer.GetTrafficNetworkOutputInterface();
    lrBuffer.SetTrafficNetworkOutputInterface(lpTrafficNetwork);
    (void)lrConstBuffer.GetTrafficSoundOutputInterface();
    lrBuffer.SetTrafficSoundOutputInterface(lpTrafficSound);
    (void)lrConstBuffer.GetTrafficDirectorOutputInterface();
    lrBuffer.SetTrafficDirectorOutputInterface(lpTrafficDirector);

    // crash network
    (void)lrConstBuffer.GetCrashNetworkOutputInterface();
    lrBuffer.SetCrashNetworkOutputInterface(lpCrashNetwork);

    // game event queue
    (void)lrConstBuffer.GetGameEventQueue();
    (void)lrBuffer.GetGameEventQueue();
    lrBuffer.AppendGameEventQueue(lpGameEvents);

    // deformation
    (void)lrConstBuffer.GetDeformationOutputInterface();
    lrBuffer.SetDeformationOutputInterface(lpDeformation);

    // traffic type responses
    lrBuffer.AppendTrafficTypeResponseQueue(lpTrafficTypeResponses);

    // effects environment (both overloads)
    (void)lrConstBuffer.GetEffectsEnvironmentInterface();
    (void)lrBuffer.GetEffectsEnvironmentInterface();

    // world entity status
    (void)lrConstBuffer.GetWorldEntityStatusInterface();
    lrBuffer.SetWorldEntityStatusInterface(lpWorldEntityStatus);

    // sound world load
    lrBuffer.AppendSoundWorldLoadInterface(lpSoundWorldLoads);

    // replay requests
    (void)lrConstBuffer.GetReplayRequestInterface();
    lrBuffer.AppendReplayRequestInterface(lpReplayRequests);

    // prop VFX locators + prop notifications
    lrBuffer.SetPropVFXLocatorQueue(lpPropVFXLocators);
    lrBuffer.AppendPropBecamePhysicalEventQueue(lpPropBecamePhysical);
    lrBuffer.AppendPropUpdateNotificationQueue(lpPropUpdateNotifications);
}

}   // namespace BrnWorldIO
