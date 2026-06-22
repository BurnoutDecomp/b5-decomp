// ============================================================================
// b5-decomp/src/GameSource/GameState/TrainingManager/BrnTrainingManager.cpp
// ============================================================================
// Bodies for BrnGameState::TrainingManager -- the Easy Drive training-tip manager.
// Layout home + member docs live in BrnTrainingManager.h. Each function below is
// reconstructed store-for-store from the X360 pseudocode/asm (addresses noted per body);
// every member access is BY NAME against the DWARF-attested, X360-offset-gated layout.
//
// Cross-TU reads (GameStateModule internal state, the embedded RaceCar output interface,
// BrnProgression::Profile training-flag accessors, ProgressionManager::GetProfile,
// GameStateModule::RequestPause) are expressed through the NAMED accessors additively
// declared on those types' minimal-slice homes (each flagged there).
//
// TWO functions are intentionally NOT defined here (declared-only in the header) and are
// reported in still_unbodied -- see the FLAG block at the bottom of this file:
//   * RequestTraining        (0x82365B20)
//   * IsTipAllowedInGameMode (0x823590A0)
// Both read deep, un-homed GameStateModule / RaceCar-output-interface internal state at raw
// offsets with no recoverable member name; bodying them store-for-store would require
// fabricating member semantics, which the project rules forbid.

#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<13312,16>::AddEvent

namespace BrnGameState
{
namespace
{
    // The GameActionQueue concrete type (forward-declared in BrnGameStateModuleIO.h) is the
    // OutputBuffer's variable-size event queue. The X360 invokes
    // VariableEventQueue<13312,16>::AddEvent straight on the GameActionQueue pointer the manager
    // is handed; bridge the opaque queue type to the concrete VEQ (same pattern as MugshotManager).
    inline CgsModule::VariableEventQueue<13312, 16>* AsVeq(GameStateModuleIO::GameActionQueue* lpQueue)
    {
        return reinterpret_cast<CgsModule::VariableEventQueue<13312, 16>*>(lpQueue);
    }

    // ---- GameAction payload type ids the X360 passes to AddEvent (the liType immediates) ----
    // These select which GameAction the gui/sound layer runs when it drains the queue. The records
    // the TrainingManager pushes are all single-byte payloads (the X360 AddEvent liSize immediate
    // is 1) -- a flag/discriminator byte. The full GameAction payload structs live in BrnGameActions.h;
    // a 1-byte stack record matches the X360 image exactly for these queue writes.
    const s32 KI_GAME_ACTION_TRAINING_UNPAUSE = 151;  // ForceUnpause                       (0x97)
    const s32 KI_GAME_ACTION_SHOW_SATNAV      = 190;   // TriggerAnyFollowOnTrainingTips     (0xBE)
    const s32 KI_GAME_ACTION_TRAINING_PAUSE   = 150;   // TriggerAnyFollowOnTrainingTips     (0x96)

    // The X360 reason bitflag passed to GameStateModule::RequestPause for a training-driven pause.
    const s32 KI_PAUSE_REASON_TRAINING = 64;           // (0x40)

    // Single-byte GameAction record. The X360 builds a 1-byte stack buffer (the leading byte set to
    // 1 == "active/enable") and hands its address + size 1 to AddEvent.
    struct TrainingFlagGameAction
    {
        u8 mbFlag;   // +0x00 (set to 1 by the producer)
    };
}

// ---------------------------------------------------------------------------
// OnTogglePictureParadise -- X360 0x82359010.
// Latches the "in Picture Paradise" flag (the X360 stores the bool arg at this+0x14).
// ---------------------------------------------------------------------------
void TrainingManager::OnTogglePictureParadise(bool lbActive)
{
    mbInPictureParadise = lbActive;
}

// ---------------------------------------------------------------------------
// OnEnableTrainingTips -- X360 0x82359018.
// Latches the global "training tips enabled" flag (the X360 stores the bool arg at this+0x16).
// ---------------------------------------------------------------------------
void TrainingManager::OnEnableTrainingTips(bool lbActive)
{
    mbTipsEnabled = lbActive;
}

// ---------------------------------------------------------------------------
// OnVoiceoverFinished -- X360 0x82359020.
// The sound layer's "training voiceover finished" hook. Only acts if a message has been
// playing for more than 1 second (mfStateTime > 1.0) and the FSM is mid-message; then asserts
// the FSM is in a playing/waiting-for-unpause state and marks the voiceover finished so the
// next Update can advance the FSM.
// ---------------------------------------------------------------------------
void TrainingManager::OnVoiceoverFinished()
{
    if (mfStateTime > 1.0f)
    {
        if (meTrainingState != E_TRAINING_STATE_INACTIVE)
        {
            CGS_ASSERT(meTrainingState == E_TRAINING_STATE_PLAYING_MESSAGE ||
                       meTrainingState == E_TRAINING_STATE_WAITINGFORUNPAUSE,
                       "meTrainingState == E_TRAINING_STATE_PLAYING_MESSAGE || "
                       "meTrainingState == E_TRAINING_STATE_WAITINGFORUNPAUSE");
            mbVoiceoverFinishedLastFrame = true;
        }
    }
}

// ---------------------------------------------------------------------------
// DoesTrainingPauseGame -- X360 0x823593C0.
// Never pauses in an online game mode; otherwise a fixed allow-list of "intro / mode
// explanation" tip types pause the world, the rest do not. (The X360 lowers the allow-list to
// a jump table over leTrainingType; reproduced here as an explicit case set.)
// ---------------------------------------------------------------------------
bool TrainingManager::DoesTrainingPauseGame(BrnProgression::ETrainingType leTrainingType)
{
    if (mpGameStateModule->IsOnlineGameMode())
        return false;

    switch (leTrainingType)
    {
        case BrnProgression::E_TRAINING_TYPE_LEAVES_JUNKYARD:        // 0
        case BrnProgression::E_TRAINING_TYPE_MAP_APPEARS:           // 1
        case BrnProgression::E_TRAINING_TYPE_START_ENGINE:          // 2
        case BrnProgression::E_TRAINING_TYPE_DISCOVERS_EVENT:       // 8
        case BrnProgression::E_TRAINING_TYPE_POINT_TO_POINT_EVENT:  // 9
        case BrnProgression::E_TRAINING_TYPE_OTHER_DRIVER:          // 16
        case BrnProgression::E_TRAINING_TYPE_CORRECT_CAR_FOR_CHALLENGE: // 24
        case BrnProgression::E_TRAINING_TYPE_WRONG_CAR_FOR_CHALLENGE:   // 25
        case BrnProgression::E_TRAINING_TYPE_ROAD_RULES_ON:         // 26
        case BrnProgression::E_TRAINING_TYPE_ROAD_RULES_FORCE_ON:   // 27
        case BrnProgression::E_TRAINING_TYPE_CRASH_ROAD_RULES_ON:   // 28
        case BrnProgression::E_TRAINING_TYPE_MAP_INFORMATION:       // 38
        case BrnProgression::E_TRAINING_TYPE_TAKEDOWN:              // 40
        case BrnProgression::E_TRAINING_TYPE_INTRO_TO_ONLINE_1:     // 41
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// ForceUnpause -- X360 0x823888F0.
// If the current training type would have paused the game, push the "training unpause"
// GameAction onto the queue. (The X360 builds a 1-byte stack record -- left uninitialised in the
// pseudocode -- and AddEvents it with type 151 / size 1.)
// ---------------------------------------------------------------------------
void TrainingManager::ForceUnpause(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    if (DoesTrainingPauseGame(meCurrentTrainingType))
    {
        TrainingFlagGameAction lAction;
        AsVeq(lpGameActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAction),
            KI_GAME_ACTION_TRAINING_UNPAUSE, (s32)sizeof(TrainingFlagGameAction));
    }
}

// ---------------------------------------------------------------------------
// TriggerAnyFollowOnTrainingTips -- X360 0x823889C8.
// When a tip finishes, fire any follow-on tip it chains to:
//   * after LEAVES_JUNKYARD (0): request MAP_APPEARS (1), then push a "show sat-nav" GameAction.
//   * after MAP_APPEARS (1):     request START_ENGINE (2).
// Then, if a tip is now pending (meTrainingState == PENDING_MESSAGE), send its ticker message,
// advance the FSM to PLAYING_MESSAGE, and -- unless a pause is suppressed -- if the newly-latched
// tip pauses the game while the just-finished one did NOT, request the pause and push the
// "training pause" GameAction.
// ---------------------------------------------------------------------------
void TrainingManager::TriggerAnyFollowOnTrainingTips(
    BrnProgression::ETrainingType leFinishedTrainingType,
    GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    if (leFinishedTrainingType == BrnProgression::E_TRAINING_TYPE_LEAVES_JUNKYARD)
    {
        RequestTraining(BrnProgression::E_TRAINING_TYPE_MAP_APPEARS);

        TrainingFlagGameAction lShowSatNavAction;
        lShowSatNavAction.mbFlag = 1;
        AsVeq(lpGameActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lShowSatNavAction),
            KI_GAME_ACTION_SHOW_SATNAV, (s32)sizeof(TrainingFlagGameAction));
    }
    else if (leFinishedTrainingType == BrnProgression::E_TRAINING_TYPE_MAP_APPEARS)
    {
        RequestTraining(BrnProgression::E_TRAINING_TYPE_START_ENGINE);
    }

    if (meTrainingState == E_TRAINING_STATE_PENDING_MESSAGE)
    {
        SendTrainingTickerMessage(lpGameActionQueue);
        meTrainingState = E_TRAINING_STATE_PLAYING_MESSAGE;

        if (!mpGameStateModule->IsTrainingPauseSuppressed())
        {
            if (DoesTrainingPauseGame(meCurrentTrainingType) &&
                !DoesTrainingPauseGame(leFinishedTrainingType))
            {
                mpGameStateModule->RequestPause(KI_PAUSE_REASON_TRAINING, lpGameActionQueue, 0, 0);

                TrainingFlagGameAction lAction;
                AsVeq(lpGameActionQueue)->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lAction),
                    KI_GAME_ACTION_TRAINING_PAUSE, (s32)sizeof(TrainingFlagGameAction));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// DEBUG_ClearTrainingFlags -- X360 0x82366050.
// DEBUG-only: clear the player's persisted training flags via the Profile. The X360 reaches the
// Profile through the ProgressionManager back-pointer (asserting it is non-null) and zeroes the
// training-flag bitfield region; that region's exact layout belongs to the Profile TU, so this
// body calls the named Profile::ClearTrainingFlags accessor.
// ---------------------------------------------------------------------------
void TrainingManager::DEBUG_ClearTrainingFlags()
{
    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    CGS_ASSERT(lpProfile, "lpProfile");
    lpProfile->ClearTrainingFlags();
}

// ===========================================================================
// FLAG -- declared-only (NOT defined here); reported in still_unbodied.
// ===========================================================================
// RequestTraining (X360 0x82365B20) and IsTipAllowedInGameMode (X360 0x823590A0) read deep,
// un-homed internal state through mpGameStateModule and its embedded RCEntityActiveRaceCarOutput-
// Interface at raw byte offsets that have NO recoverable member name in the bounded slices
// available to this TU:
//   * RequestTraining: a u64 at GameStateModule+183744 (transition/loading handle), a u8 at
//     +245952, an s32 at +232288 (named IsTrainingPauseSuppressed only because two call sites pin
//     its use), a pointer at +7608 whose +40 word == 2 is tested, and an f32 at +42304 -- the
//     boost-training (type 50) eligibility branch hinges on all of these.
//   * IsTipAllowedInGameMode: two s32 sub-state fields inside the embedded RaceCar output interface
//     (interface+10328 != -1, interface+10332 == 2) gating the MAP_INFORMATION tip.
// Naming those would be fabricating member semantics, which the project rules forbid. Their bodies
// land once the GameStateModule and RCEntityActiveRaceCarOutputInterface TUs home the real members;
// the declarations in BrnTrainingManager.h are sufficient for callers (TriggerAnyFollowOnTrainingTips
// calls RequestTraining; the compile gate type-checks the call against the declaration).
}
