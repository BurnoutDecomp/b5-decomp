#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/TrainingManager/BrnTrainingManager.h
// ============================================================================
// Canonical home for BrnGameState::TrainingManager -- the "Easy Drive" training-tip
// manager. It owns the little FSM that decides whether a requested tutorial tip
// (BrnProgression::ETrainingType) may play in the current game mode, queues the GameActions
// that show the tip ticker / pause the world while a voiceover plays, and unpauses again
// once the voiceover finishes.
//
// LAYOUT is DWARF-authoritative (references/DecFIGS/dwarfdump/.../TrainingManager/
// BrnTrainingManager.h) and gated against the X360 binary's per-function store/load offsets,
// which pin every member to its exact byte slot:
//   +0x00  ETrainingState               meTrainingState        (read as *this in RequestTraining/OnVoiceoverFinished)
//   +0x04  BrnProgression::ETrainingType meCurrentTrainingType  (stored at *(this+4) in RequestTraining)
//   +0x08  BrnProgression::ProgressionManager* mpProgressionManager (read this[2] in DEBUG_ClearTrainingFlags; this+8 -> +0x170 Profile in RequestTraining)
//   +0x0C  GameStateModule*             mpGameStateModule      (read this+0xC in DoesTrainingPauseGame/RequestTraining)
//   +0x10  f32                          mfStateTime            (OnVoiceoverFinished: result[4] > 1.0)
//   +0x14  bool                         mbInPictureParadise    (OnTogglePictureParadise stores *(this+0x14); RequestTraining gates on *(this+0x14)==0)
//   +0x15  bool                         mbIsOnlinePossible     (IsTipAllowedInGameMode reads *(this+0x15) for the INTRO_TO_ONLINE tips)
//   +0x16  bool                         mbTipsEnabled          (OnEnableTrainingTips stores *(this+0x16))
//   +0x17  bool                         mbVoiceoverFinishedLastFrame (OnVoiceoverFinished stores *(this+0x17)=1)
//   +0x18  f32                          mfLastMessageFinishedTime  (RequestTraining noob-gate reads *(this+0x18))
//   +0x1C  f32                          mfLastBoostMessagePlayTime (RequestTraining boost-gate reads/writes *(this+0x1C))
//   +0x20  s32                          miNextAtomikaFreeburnVoIndex
//   +0x24  bool                         mbGotAirBefore         (RequestTraining TRY_A_FLAT_SPIN gate reads/writes *(this+0x24))
// No vptr (every store offset is a plain field; the first field is meTrainingState at this+0).
//
// SCOPE: this header + BrnTrainingManager.cpp own the manager's own state + its 9 reconstructed
// functions. Cross-TU reads (GameStateModule internal state, the embedded RaceCar output
// interface, BrnProgression::Profile training-flag accessors, ProgressionManager::GetProfile,
// GameStateModule::RequestPause / GetLastActiveRaceCarInterface) are expressed through NAMED
// accessors that are additively declared (declare-only) on their respective minimal-slice homes
// and FLAGGED there -- their bodies + real member layout land with those owning TUs.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "SharedClasses/Progression/BrnTrainingTypes.h"                  // BrnProgression::ETrainingType
#include "GameSource/GameState/BrnGameStateModule.h"                     // GameStateModule (IsOnlineGameMode / RequestPause / GetLastActiveRaceCarInterface / timed-mode accessors)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // GameStateModuleIO::GameActionQueue (the OutputBuffer's CgsModule::VariableEventQueue<13312,16>)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // BrnProgression::ProgressionManager (GetProfile)
#include "GameSource/GameState/Progression/BrnProfile.h"                 // BrnProgression::Profile (training-flag accessors)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface (IsPlayerCarActive / GetBoostOutputInfoN / GetPlayerActiveRaceCarIndex)

namespace BrnGameState
{
// DWARF BrnTrainingManager.h:36 -- the training FSM state.
enum ETrainingState
{
    E_TRAINING_STATE_INACTIVE          = 0,
    E_TRAINING_STATE_PENDING_MESSAGE   = 1,
    E_TRAINING_STATE_PLAYING_MESSAGE   = 2,
    E_TRAINING_STATE_WAITINGFORUNPAUSE = 3,
    E_TRAINING_STATE_WAIT_FOR_MESSAGE  = 4,
    E_TRAINING_STATE_COUNT             = 5
};

// DWARF BrnTrainingManager.h:61.
class TrainingManager
{
public:
    // ---- Lifecycle / wiring ---------------------------------------------------------------
    // ⭐ [tut-ticker] BODIED 2026-08-24 from the PS3 DecFIGS export (0x241DE0; the X360 inlines
    // it into GameStateModule::Construct's neighbourhood -- no X360 export exists). Seeds every
    // member: state INACTIVE, type -1, tips ENABLED, boost-clock -600, everything else 0/false.
    void Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                   GameStateModule* lpGameStateModule);

    // ⭐⭐ [tut-ticker] X360 0x823937D0 -- the training FSM tick. Sole console caller:
    // GameStateModule::PreWorldUpdate @0x823A57C8. Console signature (PS3 DWARF):
    //     Update(const GameStateModuleIO::PreWorldInputBuffer*, BaseGameActionQueue<13312>*,
    //            RCEntityActiveRaceCarOutputInterface*, f32, bool)
    // [FLAG PC signature deviation] the PreWorldInputBuffer parameter is DROPPED on PC: no PC
    // code creates that buffer, and the console's only use of it here is
    // GetControllerToGameStateInterface(..)->user index -> XUserGetSigninState, feeding
    // mbIsOnlinePossible. The PC body calls the stubbed XUserGetSigninState(0) (returns 0 ==
    // signed out) with the deviation named at the read.
    void Update(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                    lpcActiveRaceCarInterface,
                f32 lfGameTimestep, bool lbAllowTimedTips);

    // ---- The reconstructed function set ---------------------------------------------------

    // X360 0x82365B20. Asks for training tip leTrainingType to be played; runs the eligibility
    // gauntlet (already-active?, picture-paradise?, timed-played threshold, IsTipAllowedInGameMode,
    // per-type "already seen" / "boost full" / "got air before" gates) and, if it passes, latches
    // meCurrentTrainingType + meTrainingState. ⭐ [tut-ticker] BODIED 2026-08-24 -- the two deep
    // reads that used to park it are IDENTIFIED (gsm+42300/42304 == the ModeManager clocks,
    // profile+117948 == meCurrentCarType, iface+10328/10332/10336 == the interface's named
    // trio); see the body.
    void RequestTraining(BrnProgression::ETrainingType leTrainingType);

    // X360 0x82359010. Latches the "in Picture Paradise" flag (suppresses tips while paused there).
    void OnTogglePictureParadise(bool lbActive);

    // X360 0x82359020. The sound layer's "training voiceover finished" hook: if a message has been
    // playing > 1s and the FSM is in a message-playing/waiting-for-unpause state, mark the voiceover
    // finished so the next Update can advance the FSM.
    void OnVoiceoverFinished();

    // X360 0x82359018. Latches whether training tips are globally enabled (gui toggle).
    void OnEnableTrainingTips(bool lbActive);

    // X360 0x823888F0. If the current training would have paused the game, push the unpause GameAction
    // onto the output queue.
    void ForceUnpause(GameStateModuleIO::GameActionQueue* lpGameActionQueue);

    // X360 0x823889C8. When a training tip finishes, fire any follow-on tip it chains to and, if the
    // newly-latched tip pauses the game while the previous one did not, request the pause.
    void TriggerAnyFollowOnTrainingTips(BrnProgression::ETrainingType leFinishedTrainingType,
                                        GameStateModuleIO::GameActionQueue* lpGameActionQueue);

    // X360 0x82366050. DEBUG-only: clears the player's persisted training flags via the Profile.
    void DEBUG_ClearTrainingFlags();

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnDriveThruManager TU. DriveThruManager's shop flow
    // (ProcessDriveThru / Update -> the inlined "TryPlayTrainingTip" guard) reads the pending-tip
    // slot + the Profile + the since-last-tip delta before requesting a shop training tip. The X360
    // inlines these as raw offset reads; reconstructed as named accessors here. FLAG: the field /
    // accessor names are recovered from the asm offsets, not the exports. Bodies land with this TU.
    // ------------------------------------------------------------------------

    // True when a training tip is already pending (X360 reads *(this+0) == the FSM state slot).
    bool IsTipPending() const;

    // The player Profile (X360 forwards mpProgressionManager->GetProfile(); the *(this+8) the asm
    // reads). Used to skip a tip the player has already seen.
    BrnProgression::Profile* GetProfile();

    // Seconds since the last tip finished (X360 (Profile+108) - (this+24), compared against 5.0).
    f32 GetTimeSinceLastTip() const;

    // Latch a pending tip request (X360 sets *(this+0)=1, *(this+4)=type).
    void RequestTip(BrnProgression::ETrainingType leType);

    // [tut-ticker] X360-inlined read of mbInPictureParadise (+0x14): ModeManager::PreWorldUpdate
    // @0x823537B8 reads gsm+46660 (== TrainingManager+20) to freeze the free-burn clock while
    // Picture Paradise is up. De-inlined; no symbol exists in the image.
    bool IsInPictureParadise() const { return mbInPictureParadise; }

    // ⭐ [stuntrace wave D, D3] X360-INLINED read of meCurrentTrainingType (+0x04). No symbol
    // exists in the image -- every consumer renders as a bare `lbz`/`lwz` at the owner's offset.
    // THREE sites in this wave's chain read it through the GameStateModule, all as gsm+46644
    // (== mpTrainingManager + 0x04, since the console embeds the manager at gsm+46640):
    //   ShouldStartSnapRaceMode              @0x82363700  `if (!*(a1+46640) || *(a1+46644) >= 77)`
    //   CheckIfPlayerIsAtJunctionWithAnEvent @0x82390418  twice -- the mbCanEnterEvent gate and
    //                                                     the action's own mbCanEnterEvent field
    // and each compares it against 77 == BrnProgression::E_TRAINING_TYPE_NOT_TIMED_COUNT, i.e.
    // "a BLOCKING (non-timed) tip is latched right now, so do not offer / do not start an event".
    // De-inlined to this named accessor so no reconstructed body pokes the byte offset.
    BrnProgression::ETrainingType GetCurrentTrainingType() const { return meCurrentTrainingType; }

    // X360 0x823590A0. True when a tip of this type is allowed in the current game mode. PRE-EXISTING
    // (was private); surfaced public additively so the DriveThruManager TU's TryPlayTrainingTip guard
    // can call it cross-class. Access-only change -- no layout/behaviour effect.
    // ⭐ [tut-ticker] BODIED 2026-08-24 (see the body: mode-type switch + engine-running gates).
    bool IsTipAllowedInGameMode(BrnProgression::ETrainingType leTrainingType) const;

private:
    // X360 0x82365FC8. Pick the next unseen Atomika free-burn VO tip (ids 128+miNext..); called
    // by Update's idle arm once the player has free-burned long enough. [tut-ticker] bodied.
    void PlayNewAtomikaFreeburnVO();

    // X360 0x823593C0. True when this training type should pause the world (never in online modes;
    // otherwise a fixed set of "intro / mode-explanation" tip types pause, the rest do not).
    bool DoesTrainingPauseGame(BrnProgression::ETrainingType leTrainingType);

    // X360 0x82388940 (BrnTrainingManager.cpp:794). Queues the "show training ticker" GameAction
    // (type 148, payload = the 4-byte ETrainingType) for the currently-latched tip and marks it
    // already-seen on the Profile. ⭐ BODIED 2026-08-16 (tutorial-ticker leg) -- it was previously
    // declared-only, so TriggerAnyFollowOnTrainingTips referenced a symbol no TU defined.
    void SendTrainingTickerMessage(GameStateModuleIO::GameActionQueue* lpGameActionQueue);

    // (IsTipAllowedInGameMode moved to the public additive-grow block above so the DriveThruManager
    //  TU's TryPlayTrainingTip helper can call it; X360 0x823590A0 body unchanged.)

    // ---- Members (DWARF-authoritative order; offsets X360-gated, see file header) ----------
    ETrainingState                      meTrainingState;             // +0x00
    BrnProgression::ETrainingType       meCurrentTrainingType;       // +0x04
    BrnProgression::ProgressionManager* mpProgressionManager;        // +0x08
    GameStateModule*                    mpGameStateModule;           // +0x0C
    f32                                 mfStateTime;                 // +0x10
    bool                                mbInPictureParadise;         // +0x14
    bool                                mbIsOnlinePossible;          // +0x15
    bool                                mbTipsEnabled;               // +0x16
    bool                                mbVoiceoverFinishedLastFrame;// +0x17
    f32                                 mfLastMessageFinishedTime;   // +0x18
    f32                                 mfLastBoostMessagePlayTime;  // +0x1C
    s32                                 miNextAtomikaFreeburnVoIndex;// +0x20
    bool                                mbGotAirBefore;              // +0x24
};
}
