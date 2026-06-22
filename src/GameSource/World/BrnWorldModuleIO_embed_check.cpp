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

}   // namespace BrnWorldIO
