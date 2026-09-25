#pragma once

// =================================================================================================
// GameSource/World/AI/BrnAIHarnessPad.h -- [PC HARNESS, NOT X360] BRN_AI_PAD_PLAYER, the "AI PAD" seat.
//
// NOTHING IN THIS FILE EXISTS ON THE CONSOLE. It is the AI-side half of a test-harness seat in which the
// game's OWN AI driver computes the player car's controls while the car stays PAD-driven to the rest of
// the game: maeCarControls[player] stays 1 (E_CAR_CONTROL_ENTITY_MODULE), WorldModule::HarnessApplyAIPad
// writes the AI's decision into the race-car module's pre-scene PlayerVehicleControls, and from there the
// console's own ProcessPlayerVehicleInput @0x822FFE30 builds the PLAYER record the pre-physics bridge
// forwards. Rivals keep targeting the player (AIAggression::FindTarget @0x82793C60 is untouched),
// takedowns stay player-credited, race logic still sees the player.
//
// WHY THE AI SIDE NEEDS ANY OF THIS. The console's AI already emits a record for the player's slot every
// frame (ProcessAIVehicleInputs @0x82795E10), but while a human drives, StoreDrivenCarData @0x827957F0
// (0x82795CA4..0x82795CD4) sets the player's AICar::mbIsDrivenByPlayer, and with that byte set
// AIDriver::GetTargetPosition @0x8277CBF8 (`lbz 0x154A ; beq`, 0x8277CC20..0x8277CC28) aims one metre
// STRAIGHT AHEAD -- the record carries no steering decision. The console's own AI seat (control word 2)
// clears the byte, but that same byte is what AIAggression::FindTarget refuses a player car on
// (0x82793CA0..0x82793CB4), so the seat can never be a rival target.
//
// THE ONLY AI-SIDE EFFECTS, each gated on gHarnessAIPad.mbArmed (false unless BRN_AI_PAD_PLAYER is set):
//   1. AIModule::UpdateCars / AIModule::UpdateDrivers present the player's AICar with
//      mbIsDrivenByPlayer = 0 -- the value StoreDrivenCarData writes for the console seat -- only for the
//      duration of the player's OWN AICar::Update @0x82798F68 / AIDriver::Update @0x8279AEB0, and put the
//      frame's 1 back straight after. Every rival read of the byte in the frame (UpdatePlayerTimers,
//      IsPlayerProtected, AIAggression::Update -> FindTarget / UpdateAggressionPassive, BuzzBy::Update)
//      still reads 1.
//   2. pursuit only: AIModule::HarnessAIPadPursuit picks the target, routes the player's AICar to it with
//      the console's RACE-style standard route request, and -- inside the console's own slam window --
//      gives the player's AIDriver the target as its aggression victim and the steering fan's Slam bias
//      (AIDriver::SetDrivingFanBiases, the bias the console picks for ATTACK_SLAM), so the console's own
//      IncludeSmashIntoPlayer @0x82791230 steers the car into it. AIModule::UpdateResetOnTrackManager
//      reports a successful reset of the target (HarnessAIPadNoteReset) so the pursuit re-targets.
// The whole trace, with every address: scratch/CRASHPARITY_0922/fixes/FX-AIPAD.md.
// DELETE-WHEN a real pad can drive the harness.
// =================================================================================================

#include "types.hpp"

namespace BrnAI
{
    struct AICar;

    // The BRN_AI_PAD_PLAYER objective.
    enum EHarnessAIPadMode
    {
        E_HARNESS_AI_PAD_OFF     = 0,
        E_HARNESS_AI_PAD_CRUISE  = 1,   // the console seat's own driving, armed whenever the pad has the car
        E_HARNESS_AI_PAD_RACE    = 2,   // the same, armed only while the player's AICar is in a game mode
        E_HARNESS_AI_PAD_PURSUIT = 3,   // route to the nearest attached rival and ram it
    };

    struct HarnessAIPad
    {
        // ---- written by WorldModule::HarnessArmAIPadPlayer before every AIModule::Update ----------
        EHarnessAIPadMode meMode;             // the objective; OFF when the variable is unset
        bool              mbArmed;            // THIS frame the AI computes the player's seat

        // ---- pursuit state, owned by AIModule::HarnessAIPadPursuit --------------------------------
        s32  miTarget;                        // the target's EGlobalRaceCarIndex; -1 == none
        bool mbRamming;                       // the target is inside the console's slam window
        bool mbTargetReset;                   // the target was reset on track since the last pursuit frame
        f32  mfTargetSeparation;              // metres, ground plane (for the log)
        f32  mfTargetAheadness;               // metres along the player's facing (for the log)
        bool mbRouteOwned;                    // the player AICar's route style/destination are the harness's
        s32  miSavedRouteFindingStyle;        // the console's own style, restored when the pursuit lets go
        u16  muSavedDestinationSection;       // ... and destination
        u16  muOwnedDestinationSection;       // the destination the harness last wrote
        s32  miPlans;                         // log counters
        s32  miTargetChanges;
        s32  miRamEntries;
    };

    // Defined in BrnAIModule_Drive.cpp. Its initialiser is OFF / unarmed / no target: without
    // BRN_AI_PAD_PLAYER nothing ever writes it and every gate below reads false.
    extern HarnessAIPad gHarnessAIPad;

    // Effect 1: take the player seat's mbIsDrivenByPlayer for one call. Returns true when it cleared the
    // byte (the pad is armed, the car is the player's and a human is marked as driving it); pass that
    // result to the End call, which puts the byte back.
    bool HarnessAIPadBeginPlayerSeat(AICar* lpCar);
    void HarnessAIPadEndPlayerSeat(AICar* lpCar, bool lbTaken);

    // pursuit only: a ResetOnTrack result reported SUCCESS for this global race car (the result loop of
    // AIModule::UpdateResetOnTrackManager, ARTIST 0x8279AC40..0x8279AE90). Marks the target as reset.
    void HarnessAIPadNoteReset(s32 liGlobalRaceCarIndex);
}
