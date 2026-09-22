// ============================================================================
// b5-decomp/src/GameSource/Game/BrnGameModule_wN1_01.cpp
//
// Network wave N1, partfile 01 of BrnGameModule.cpp: the network bridges that compile against
// BrnGameModule.hpp as it stands (the module member is still the placeholder class; see
// BrnGameModule_wN1_02.cpp for the two update legs that need the real module).
//
//   BrnGame::BrnGameModule::BridgeWorldToNetwork
//     Original home GameBridgeWorldToX.cpp (its assert cites that file). Called once per
//     sub-step by DoUpdate_NetworkPostSim inside the post-sim input's write lock, and only
//     when the update set does not carry 0x20.
//
// The other network bridges of this wave (BridgeNetworkToGameState, BridgeNetworkToWorld,
// TranslateNetworkEventsToWorld) are declared in BrnGameModule.hpp and are NOT bodied here:
// each needs a declaration in a header this wave does not own. The wave report lists the
// exact requests.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                          // CgsModule::Event, VariableEventQueue<14000,16>::AddEvent
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                              // CgsSceneManager::EntityId (crasher owner / index)
#include "GameSource/Network/BrnNetworkModuleIO.h"                                        // PostSimulationInputBuffer
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                               // TelemetryData, ETelemetryHook
#include "GameSource/World/BrnWorldModuleIO.h"                                            // BrnWorldIO::UpdateOutputBuffer
#include "GameSource/World/BrnEntityTypes.h"                                              // BrnWorld::EEntityTypeID
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h" // BrnWorld::PlayerVehicleControls

#include <cstring>   // std::memcpy (the 60-byte player-controls copy)

namespace BrnGame
{
    // =========================================================================================
    // BridgeWorldToNetwork
    //
    // Console body, in order:
    //   1. copy the world output's 60-byte PlayerVehicleControls into the post-sim input's;
    //   2. AppendVehicleOutputInterface / SetActiveRaceCarInterface / SetTrafficOutputInterface /
    //      SetCrashOutputInterface from the matching world-output interfaces;
    //   3. walk the vehicle manager's race-car crash queue for the first PRIMARY crash whose
    //      victim is the player's active race car. None -> done. Otherwise classify the crasher
    //      by its entity owner and post two network in-events onto the post-sim network queue:
    //        type 54, 4 bytes  -- the crasher's active race-car index (or INVALID);
    //        type 16, 20 bytes -- a TelemetryData { crash hook, "x.z" of the player position }.
    //
    // FLAG: the two in-event records are posted as their payloads. The console types are
    // NetworkInLocalPlayerCrashesEvent (one EActiveRaceCarIndex) and NetworkInTelemetryEvent
    // (one TelemetryData); both derive from the empty NetworkEvent<N> spine, so the payload IS
    // the record, byte for byte. Neither type has been added to BrnNetworkInEventTypeDefs.h
    // yet; the two ids are the immediates the console passes to AddEvent.
    //
    // FLAG: the player controls are copied with memcpy, as the console does, because the world
    // output hands them out as BrnWorldIO::PlayerVehicleControls (a 60-byte slice in
    // BrnWorldModuleIO.h) while the post-sim input holds the real BrnWorld::PlayerVehicleControls.
    // The static_asserts below keep the two spans equal until the slice is retyped.
    // =========================================================================================
    void BrnGameModule::BridgeWorldToNetwork(BrnNetwork::BrnNetworkModuleIO::PostSimulationInputBuffer* lpNetworkInput,
                                             const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput)
    {
        // The console's network in-event ids for the two records posted below.
        static const s32 KI_NETWORK_IN_EVENT_TELEMETRY             = 16;
        static const s32 KI_NETWORK_IN_EVENT_LOCAL_PLAYER_CRASHES  = 54;

        static_assert(sizeof(BrnWorld::PlayerVehicleControls) == 60, "PlayerVehicleControls is the console's 60-byte record");
        static_assert(sizeof(BrnWorldIO::PlayerVehicleControls) == sizeof(BrnWorld::PlayerVehicleControls),
                      "the world output's PlayerVehicleControls slice spans the real record");

        std::memcpy(lpNetworkInput->GetPlayerVehicleControls(),
                    lpWorldOutput->GetPlayerVehicleControls(),
                    sizeof(BrnWorld::PlayerVehicleControls));

        lpNetworkInput->AppendVehicleOutputInterface(lpWorldOutput->GetVehicleOutputInterface());
        lpNetworkInput->SetActiveRaceCarInterface(lpWorldOutput->GetActiveRaceCarOutputInterface());
        lpNetworkInput->SetTrafficOutputInterface(lpWorldOutput->GetTrafficNetworkOutputInterface());
        lpNetworkInput->SetCrashOutputInterface(lpWorldOutput->GetCrashNetworkOutputInterface());

        const BrnWorldIO::UpdateOutputBuffer::VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpCrashQueue =
            lpWorldOutput->GetVehicleManagerOutputInterface()->GetRaceCarCrashEventQueue();

        for (s32 liCrash = 0; liCrash < lpCrashQueue->GetLength(); ++liCrash)
        {
            const BrnPhysics::Vehicle::RaceCarCrashEvent& lrCrash = lpCrashQueue->GetEvent(liCrash);
            const EActiveRaceCarIndex leVictimIndex =
                static_cast<EActiveRaceCarIndex>(lrCrash.mRaceCarVolumeInstanceID.GetEntityIDEntityIndex());

            if (leVictimIndex != lpWorldOutput->GetPlayerActiveRaceCarIndex() || !lrCrash.mbIsPrimaryCrash)
            {
                continue;
            }

            const CgsSceneManager::EntityId lCrasherId(lrCrash.mCrasherEntityID.muValue);

            EActiveRaceCarIndex                         leCrasherIndex;
            BrnNetwork::BrnNetworkModuleIO::TelemetryData lTelemetry;

            switch (lCrasherId.GetOwner())
            {
                case BrnWorld::E_ENTITYTYPE_WORLD:
                case BrnWorld::E_ENTITYTYPE_WORLD_GRAPHICS:
                    lTelemetry.Construct(BrnNetwork::E_TELEMETRY_CRASHED_INTO_WORLD);
                    leCrasherIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                    break;

                case BrnWorld::E_ENTITYTYPE_RACECAR:
                {
                    const EActiveRaceCarIndex leRaceCarIndex =
                        static_cast<EActiveRaceCarIndex>(lCrasherId.GetEntityIndex());
                    if (leRaceCarIndex == leVictimIndex)
                    {
                        leCrasherIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                        lTelemetry.Construct(BrnNetwork::E_TELEMETRY_CRASHED_STARTED_SHOWTIME);
                    }
                    else
                    {
                        leCrasherIndex = leRaceCarIndex;
                        lTelemetry.Construct(BrnNetwork::E_TELEMETRY_CRASHED_INTO_RACE_CAR);
                    }
                    break;
                }

                case BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE:
                    lTelemetry.Construct(BrnNetwork::E_TELEMETRY_CRASHED_INTO_TRAFFIC);
                    leCrasherIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                    break;

                default:
                    // The console leaves the telemetry record unconstructed on this arm.
                    leCrasherIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                    CGS_ASSERT(false, "Unknown crash type");
                    break;
            }

            lTelemetry.AddParameter(lpWorldOutput->GetActiveRaceCarOutputInterface()->GetPlayerPosition());

            lpNetworkInput->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&leCrasherIndex),
                KI_NETWORK_IN_EVENT_LOCAL_PLAYER_CRASHES, static_cast<s32>(sizeof(leCrasherIndex)));
            lpNetworkInput->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lTelemetry),
                KI_NETWORK_IN_EVENT_TELEMETRY, static_cast<s32>(sizeof(lTelemetry)));
            return;
        }
    }
}
