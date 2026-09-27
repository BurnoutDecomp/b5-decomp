#pragma once

// ============================================================================
// GameSource/Game/BrnHarnessWinTeleport.h
//
// [HARNESS -- NOT A CONSOLE FUNCTION] The guaranteed-win teleport, BRN_WIN_TELEPORT.
// PERMANENT harness capability, inert unless the variable is set.
//
// WHAT IT IS FOR. Races, Burning Routes and Marked Man are won by reaching the event's finish
// landmark (after any checkpoints). A keyboard harness cannot drive a 3 km route, so a test that
// needs the WIN path had only BRN_DEBUG_FINISH_POS, which forces a finish position and skips
// finish detection. This capability does the opposite: it moves the player's car INTO each
// landmark box through the game's own place-on-track chain, and the game's own finish chain does
// the rest --
//     TriggerQueryManager::PostWorldUpdateLandmarksBringUp  (the car's segment meets the box)
//       -> ModeManager::RaceCarTriggersLandmark -> HasRaceCarHitValidCheckpoint
//       -> ModeManager::RaceCarFinishes -> ScoringSystem::RegisterFinishForCar
//       -> mbFinishCurrentModeNextUpdate -> ModeManager::FinishCurrentMode -> OUTRO -> RESULTS
// Nothing here writes a score, a position, a checkpoint bit, a transform or a velocity.
//
// THE TWO HALVES AND THE MAILBOX.
//   PRODUCER  PreWorldUpdate below, called once per pre-world tick from ModeManager::PreWorldUpdate
//             (GameState side). It decides WHERE the car goes and posts a Request.
//   CONSUMER  PlaceOnTrackManager::ArmWinTeleportBringUp (World side, beside the BRN_CAR_TELEPORT
//             trigger). When a newer sequence number appears it makes exactly one call,
//             ActiveRaceCar::RequestPlaceOnTrack(position, direction, speed).
//   The mailbox is one Request guarded by a sequence number; the consumer only ever acts on the
//   newest one.
//
// ENVIRONMENT (read once; flow_run.ps1 -WinTeleport/-WinTeleportGap/-WinTeleportSpeed/
// -WinTeleportLead sets them and clears them on every other run):
//   BRN_WIN_TELEPORT=<seconds>       IN_PROGRESS time (sim seconds) before the first hop. Set but
//                                    empty or not a number >= 0 is REFUSED with a log line.
//   BRN_WIN_TELEPORT_GAP=<frames>    pre-world ticks between hops (default 90).
//   BRN_WIN_TELEPORT_SPEED=<m/s>     forward speed handed to RequestPlaceOnTrack (default 0).
//   BRN_WIN_TELEPORT_LEAD=<metres>   offline race only: stage the car this far outside the finish
//                                    box before the finish hop (default 30, 0 disables). See the
//                                    .cpp banner for why a race needs it and the others do not.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"               // Vector3
#include "GameSource/BurnoutConstants.h"  // EGlobalRaceCarIndex

namespace BrnGameState { class ModeManager; }
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

namespace BrnGame
{
namespace HarnessWinTeleport
{
    // One placement request. muSeq == 0 means nothing has ever been posted.
    struct Request
    {
        Vector3 mPosition;    // world point handed to RequestPlaceOnTrack (the line test runs through it)
        Vector3 mDirection;   // unit, horizontal; the car's facing and its velocity direction
        f32     mfSpeed;      // >= 0 (the place-on-track chain asserts it)
        u32     muSeq;
    };

    // True when BRN_WIN_TELEPORT is set and valid. Parses (and logs "armed" or the refusal) on
    // the first call, which must come from a tick, never from a static initialiser.
    bool IsEnabled();

    // PRODUCER. Called from ModeManager::PreWorldUpdate while a game mode exists. The two region
    // indices are ModeManager-private and arrive by value: the player's next checkpoint's
    // landmark region (-1 when none remains) and the one before it (-1 for the first).
    void PreWorldUpdate(const BrnGameState::ModeManager& lrModeManager,
                        EGlobalRaceCarIndex lePlayerGlobalRaceCarIndex,
                        s32 liNextRegionIndex,
                        s32 liPreviousRegionIndex,
                        f32 lfSimTimeStep,
                        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput);

    // CONSUMER. True, with *lpRequest filled, when a request newer than luLastSeq is waiting.
    bool PeekRequest(u32 luLastSeq, Request* lpRequest);
}
}
