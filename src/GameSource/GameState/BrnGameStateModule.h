#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                         // CgsID (CarSelect player-car-change grows)
#include "GameSource/BurnoutConstants.h"                            // EActiveRaceCarIndex
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"  // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::VariableEventQueue<N,16> (output GUI event queue)
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CgsDev::Assert Begin/Fire/EndAssert
#include "GameSource/GameState/ModeManager/BrnModeManager.h"        // BrnGameState::ModeManager (mModeManager, by value)

// Forward declarations for the BrnTrainingManager additive-grow accessor signatures below (pointer/
// reference only -- no layout needed here; the real types are included by the TrainingManager TU).
namespace BrnGameState { namespace GameStateModuleIO { class GameActionQueue; } }
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }
// The achievement manager the BurnoutSkillzManager grow returns (StuntModeScoring::AchievementManager
// is a typedef of this; pointer only here).
namespace BrnGameState { class AchievementManagerPS3; }
// For the DriveThruManager additive grow below (pointer-only).
namespace BrnProgression { class Profile; }
namespace InputBuffer    { class GameActionQueue; }
// For the DeveloperChallengeManager additive grow below (pointer-only).
namespace BrnResource    { struct VehicleList; }
// For the ResetPlayerDebugComponent additive grows below (pointer-only).
namespace BrnResource    { class  WheelList; }
namespace BrnTrigger     { struct TriggerData; }
namespace CgsWorld       { struct WorldMap2D; }

namespace BrnGameState
{
// Minimal slice (BrnPursuitMode.h GetName-only precedent): the full GameStateModule layout (~290KB,
// ~190 methods) is owned by the BrnGameStateModule.cpp TU. Only the members touched by the three
// reconstructed functions of this TU are declared here; exact member offsets are NOT modelled (the
// X360 attests mbIsUpdating at this+292289, mePlayerActiveRaceCarIndex at this+208304, mModeManager at
// the +0x1DB8 region) -- inter-member padding for the full class is out of scope. The base
// (CgsModule::ModuleSingleBuffered) is #included, not forked.
class GameStateModule : public CgsModule::ModuleSingleBuffered
{
public:
    // X360 @ 0x82311570 (BrnGameStateModule.h:949). Inline accessor for the player's active-race-car
    // slot index. Asserts the module is mid-update (mbIsUpdating) via CGS_ASSERT before handing back
    // the cached index.
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return mePlayerActiveRaceCarIndex;
    }

    // X360 @ 0x823116D0 (BrnGameStateModule.h:982). Out-of-line; defined in BrnGameStateModule.cpp.
    bool IsOnlineGameMode();

    // ADDITIVE GROW (declare-only) for the BrnGameState::DeveloperChallengeManager TU. CheckCarID
    // resolves the loaded vehicle list off the owning module (the X360 reads the VehicleList* at
    // this+284392). Body + the real member land with the GameStateModule TU. Declare-only.
    BrnResource::VehicleList* GetVehicleList();

    // ADDITIVE GROW (declare-only) for the BrnPaybackManager TU. X360 @ 0x823566F8. Hands back the
    // module's per-frame output GUI event queue (a CgsModule::VariableEventQueue<18432,16>) the
    // PaybackManager publishes its countdown / state-change HUD events onto via AddEvent(&ev,type,
    // size). Body + the real embedded-queue member land with the GameStateModule TU; declare-only
    // suffices for the per-TU `cl /c` gate.
    CgsModule::VariableEventQueue<18432, 16>* GetOutputGuiEventQueue();

    // ADDITIVE GROW (declare-only) for the BrnPaybackManager TU. X360 PaybackManager::
    // HandleHavingPayback resolves the active race car for a slot index from the module's active-car
    // table (this+7632) and treats a null entry OR a dead car-id (@+328 == -1) as "the car has left
    // the game". De-inlined to this named predicate (true == the slot still holds a live race car);
    // body + the real table walk land with the GameStateModule TU.
    bool IsActiveRaceCarStillPresent(::EActiveRaceCarIndex leActiveRaceCarIndex) const;

    // ADDITIVE GROW (declare-only) for the BrnMugshotManager TU. The X360 build inlines the access
    // to the owning module's embedded ModeManager (MugshotManager reads through *(this+0x1DB8) to
    // test the current mode's post-event state via ModeManager::IsInPostEvent). De-inlined to this
    // named accessor; body + the real embedded-ModeManager wiring land with the GameStateModule TU.
    // FLAG: declare-only additive grow on the minimal GameStateModule slice.
    ModeManager* GetModeManager();

    // ADDITIVE GROW (declare-only) for the AchievementManagerBase TU. FLAG: the cached
    // current-game-mode-type scalar AchievementManagerBase::OnTakedown (X360 0x8235AAE0)
    // reads through mpGameStateModule (raw read at this+7604, just below mModeManager) and
    // compares == E_MODE_ROAD_RAGE. Body lands with the GameStateModule TU; declare-only
    // suffices for the compile gate.
    GameStateModuleIO::EGameModeType GetCurrentGameModeType() const;

    // DWARF BrnGameStateModule.h:1300/651. X360 inlines it (sets mbToggleShowtimeBehaviour=true at
    // offset 284512); declared-only here, used by GameStateDebugComponent::ToggleShowtimeCallback.
    void ToggleShowtimeBehaviour();

    // ADDITIVE GROW (declare-only) for the BrnBurnoutSkillzManager TU. FLAG: the X360
    // BurnoutSkillzManager::Construct (0x82332688) reaches the embedded achievement manager
    // through the owning GameStateModule (the inlined `*(modeManager->mpGameStateModule) +
    // 181680` pointer adjust to the achievement-manager subobject). De-inlined to this named
    // accessor; body + the real embedded-AchievementManager wiring land with the GameStateModule
    // TU. Returns the StuntModeScoring::AchievementManager (== AchievementManagerPS3) the manager
    // caches as mpAchievementManager.
    AchievementManagerPS3* GetAchievementManager();

    // ADDITIVE GROW (declare-only) for the BrnTrainingManager TU.
    // X360 BrnGameState::GameStateModule::RequestPause -- TrainingManager::TriggerAnyFollowOnTrainingTips
    // (0x823889C8) calls it as RequestPause(this, 64, lpGameActionQueue, 0, 0) to pause the world while a
    // training voiceover plays. The leading s32 is the pause-reason bitflag (the X360 immediate 0x40 ==
    // "training" reason); the GameActionQueue* is the OutputBuffer's action queue the pause request is
    // broadcast through; the trailing two s32s are 0 at this call site. Body + the real pause-stack
    // wiring land with the GameStateModule TU. FLAG: declare-only additive grow on the minimal slice.
    void RequestPause(s32 liPauseReasonFlags,
                      GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                      s32 liArg3, s32 liArg4);

    // ADDITIVE GROW (declare-only) for the BrnTrainingManager TU. FLAG: the X360 reads an s32 at
    // this+232288 (0x38B60) in BOTH TrainingManager::RequestTraining (0x82365B20) and
    // TriggerAnyFollowOnTrainingTips (0x823889C8); in each it suppresses the training-driven pause /
    // request when nonzero (the X360 `if (*(mpGameStateModule+232288)) bail/skip-pause`). The exact
    // member name is unconfirmed in this bounded view -- semantics ("a training-pause is currently
    // suppressed/blocked") are inferred from the two call sites; rename when the GameStateModule TU is
    // fully reconstructed. Body + real member land with the GameStateModule TU.
    bool IsTrainingPauseSuppressed() const;

    // ADDITIVE GROW (declare-only) for the BrnTrainingManager TU.
    // X360 BrnGameState::GameStateModule::GetLastActiveRaceCarInterface -- the read-only snapshot of the
    // player's + rivals' active race cars the GameStateModule cached at the end of the last world update
    // (the X360 reaches it as the embedded interface at this+0x397E0). RequestTraining queries it for the
    // boost / player-car state behind the boost-training tip. Returns a pointer to the embedded
    // by-value interface; body + real member land with the GameStateModule TU. FLAG: declare-only grow.
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        GetLastActiveRaceCarInterface() const;

    // ADDITIVE GROW (declare-only) for the BrnTrainingManager TU. FLAG: the X360 reads an f32 at
    // this+42300 (0xA53C) in TrainingManager::RequestTraining / IsTipAllowedInGameMode and compares it
    // against fixed thresholds (5.0 == KF_MAX_TIMED_PLAYED_FOR_NOOB_TRAINING_TIPS, 30.0 for the
    // DISCOVERS_EVENT / boost gates). Semantically the elapsed time the player has spent in the current
    // timed game mode. Body + real member land with the GameStateModule TU.
    f32 GetTimePlayedInTimedMode() const;

    // ADDITIVE GROW (declare-only) for the BrnDriveThruManager TU (X360 0x82382460).
    // UnlockCarChallengeForCar calls it after flipping a newly-found event so the module can check
    // whether ALL events are now found (and fire the matching unlock/achievement). Declare-only.
    // NOTE: globally-qualified ::InputBuffer to avoid the BrnGameState::InputBuffer nested forward
    // decl (BrnScoringSystem.h) shadowing the global queue type when this header is parsed inside
    // namespace BrnGameState.
    void CheckForAllEventsBeingFound(BrnProgression::Profile* lpProfile,
                                     ::InputBuffer::GameActionQueue* lpQueue);

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnGameState::CarSelectManager (junkyard car-select) TUs.
    // The junkyard FSM routes the player-car snapshot / streaming / swap-broadcast / unpause hooks
    // through the owning GameStateModule. Signatures + semantics are X360-asm-attested; bodies land
    // with the GameStateModule TU. Declare-only suffices for the `cl /c` compile gate.
    // ------------------------------------------------------------------------

    // X360 read at GameStateModule+0x456D8 -- the active player car's CgsID, compared against the
    // desired/current car id to detect a swap completing.
    CgsID GetActivePlayerCarId() const;

    // X360 0x82382550. Kick the world to start streaming the chosen vehicle-selection car.
    void RequestStreamingForVehicleSelection(CgsID lCarId);

    // X360 0x8238FB40. Broadcast a special-event player-car change (the unlock-display / change-car
    // path uses it). lWheelOrZero is the wheel id (0 when unchanged); lbArg == the X360 trailing 1.
    void OnSpecialEventPlayerCarChange(CgsID lCarId, CgsID lWheelOrZero,
                                       ::InputBuffer::GameActionQueue* lpQueue, bool lbArg);

    // X360 0x82396B88. Broadcast a (non-special-event) player-car change on junkyard exit (offline).
    void OnPlayerCarChange(CgsID lCarId, CgsID lZero,
                           ::InputBuffer::GameActionQueue* lpQueue, bool lbArg);

    // X360 0x82363698. Mark car lCarId as already-shown in the unlock sequence (so it is not shown again).
    void SetCarUnlockAlreadyShown(CgsID lCarId);

    // X360 0x82382138. Request the world un-pause on junkyard exit.
    void RequestUnpause(bool lbArg, ::InputBuffer::GameActionQueue* lpQueue);

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnGameState::ResetPlayerDebugComponent TU. The
    // "Reset Player Car" debug menu reads the loaded vehicle/wheel resources, the active track's
    // trigger data and the district map off the owning GameStateModule, and publishes its
    // teleport / change-car requests onto the module's output GUI event queue. Signatures +
    // semantics are X360-asm-attested (offsets noted); bodies land with the GameStateModule TU.
    // Declare-only suffices for the per-TU `cl /c` gate.
    // ------------------------------------------------------------------------

    // X360 read at GameStateModule+0x456EC (284396). The loaded wheel-list resource (the runtime
    // aggregation of wheel records the change-wheel menu enumerates).
    BrnResource::WheelList* GetWheelList();

    // X360 read at GameStateModule+0xAB70 (43888). The currently-loaded track's trigger data (the
    // landmark / generic-region table the teleport-location menu is built from). The X360 reaches
    // it through the embedded resource pointer at this offset; de-inlined to a named accessor.
    BrnTrigger::TriggerData* GetTrackTriggerData();

    // X360 read at GameStateModule+0x3C090 (245904). The world district map (sampled to label each
    // teleport region with its county/district name).
    CgsWorld::WorldMap2D* GetDistrictMap();

    // The output GUI event queue the debug menu's teleport/change-car requests are published onto
    // (X360 reaches it at GameStateModule+0x3CA40 / 248384 -- the same per-frame output queue
    // GetOutputGuiEventQueue() above returns; exposed here for the debug component's AddEvent calls).

    // X360 read at GameStateModule+0x456E0 (284384). The player's currently-equipped wheel id; the
    // change-wheel menu pre-selects the option whose wheel-record id matches it. (GetActivePlayerCarId()
    // above exposes the sibling active-car id at +0x456D8.)
    CgsID GetActivePlayerWheelId() const;

    // X360 read at GameStateModule+0x1DB8 (7608) -- the leading flag of the embedded mModeManager.
    // The change-car debug action gates on it: it only publishes a change-car request when this is
    // false (the X360 `if ( !*(mpGameStateModule + 7608) ) { AddEvent(...); }`). Semantically "a mode
    // change / mode-data load is in progress", so a live car swap is deferred while it is true. FLAG:
    // the exact member name is unconfirmed (it is the first byte of the not-yet-named mModeManager
    // prefix); the predicate name reflects the asm-attested gate semantics. Body lands with the
    // GameStateModule TU.
    bool IsModeChangeInProgress() const;

private:
    // DWARF BrnGameStateModule.h:771. The by-value ModeManager that owns the current game mode.
    ModeManager         mModeManager;
    // DWARF BrnGameStateModule.h:794 (X360 this+208304).
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex;
    // DWARF BrnGameStateModule.h:882 (X360 this+292289) -- set true only while the module is updating.
    bool                mbIsUpdating;
};
}
