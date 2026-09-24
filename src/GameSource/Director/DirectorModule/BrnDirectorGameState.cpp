// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorGameState.cpp
//
// Compilation home for the BrnDirector::GameState slice this TU owns:
//   - GameState::Clear @0x82218930  (reset the director's per-event game-state snapshot)
//
// Clear is run by BrnDirector::MainDirector::Construct (and re-run by the replay director in
// FillInSharedInfo / PreSceneQueryUpdate) to seed a fresh event snapshot. It is NOT a whole-
// struct memset -- it stamps the three space transforms to identity, zeroes the per-frame
// vector, and seeds each scalar/flag to its specific reset value (the X360 store offsets are
// cross-checked against the reconstructed layout in BrnDirectorGameState.h). The float
// constants are flt_82001C98 = 1.0f and flt_82001CC0 = 0.0f.
//
// This TU also owns the DataJournal<EEventState,2>::GetCurrent() (non-const) instance the X360
// emits out-of-line @0x821FBB80 (linker-attributed to BrnDirector::GameState): it asserts the
// journal is non-empty (miSize > 0, BrnDirectorDataJournal.h) and returns &maEntries[miWriteIndex].
// The body is the generic inline in BrnDirectorDataJournal.h; forced out-of-line via the explicit
// member instantiation at the foot of this file.
// ============================================================================

#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// BrnDirector::GameState::Clear @0x82218930  (store-for-store off the asm)
// ----------------------------------------------------------------------------
void GameState::Clear()
{
    // The three director-space transforms -> identity (lvx/stvx identity rows from
    // flt_82001C98=1.0 / flt_82001CC0=0.0 at +0x010 / +0x050 / +0x090).
    mTrafficLightSpace.SetIdentity();
    mBaseDriveThruTransform.SetIdentity();
    mDriveThruTransform.SetIdentity();

    // Per-frame finish-line direction vector -> 0 (stvx of vspltisw 0 at +0x170).
    mFinishLineNorthmostDir.SetZero();

    // --- scalar / flag resets (each at its asm-attested offset; values are the exact
    //     register stored: r10=0, r11=0, r8=1, r7=3, r9=-1, r27=-1, f0=0.0) ---
    mpEventJLBox                        = nullptr; // stw 0, 0x000

    mbDriveThruActive                   = false;  // stb 0, 0x0D0
    meDriveThruType                     = E_DRIVETHRU_INVALID; // stw 0, 0x0D4
    mbNewProfileIntroActive             = false;  // stb 0, 0x0D8
    mbGameIntroFlybyActive              = false;  // stb 0, 0x0D9
    mbTakedownActive                    = false;  // stb 0, 0x0DA
    mbIsRevengeTD                       = false;  // stb 0, 0x0DB
    mbIsShutdown                        = false;  // stb 0, 0x0DC
    mbIsSignatureTD                     = false;  // stb 0, 0x0DD
    meTakedownVictimID                  = E_ACTIVE_RACE_CAR_INDEX_INVALID; // stw -1, 0x0E0
    miThisFramesActionFlags             = 0;      // stw 0, 0x0E4
    // NOTE: the X360 Clear has NO store at +0x0E8 (miActionRequestedCamera) -- the
    // asm jumps 0x0E4 -> 0x0F0, leaving miActionRequestedCamera untouched here.
    mbPaybackActive                     = false;  // stb 0, 0x0F0
    meActivePaybackType = static_cast<BrnNetwork::EPaybackType>(3);  // stw 3, 0x0F4
    mbPlayerInTunnel                    = false;  // stb 0, 0x0F8
    mbCrashActive                       = false;  // stb 0, 0x0F9
    mbCrashActiveWithSlomoSinceStart    = false;  // stb 0, 0x0FA
    mbCanUseSlomo                       = true;   // stb 1, 0x100
    mbCrashNavShown                     = false;  // stb 0, 0x101
    mbColourCalibrationShown            = false;  // stb 0, 0x102
    mbShortCutMenuShown                 = false;  // stb 0, 0x103
    mbWonLastEvent                      = false;  // stb 0, 0x104
    muNumberOfCarsInIntro               = 0;      // stw 0, 0x10C
    meEventType                         = -1;     // stw -1, 0x120  (E_MODE_NONE)

    // mEventState (DataJournal<EEventState,2>): the asm seeds every slot to E_EVENT_STATE_ACTIVE
    // (stw 3, 0x124 / 0x128), write-index 0 (stw 0, 0x12C) and live size N==2 (stw 2, 0x130) --
    // exactly DataJournal::SetAll(value): fill all entries, cursor 0, size N.
    mEventState.SetAll(E_EVENT_STATE_ACTIVE);

    miEventSpecificShotGroup            = -1;     // stw -1, 0x134
    meTargetRaceCarIndex                = E_ACTIVE_RACE_CAR_INDEX_0;  // stw 0, 0x140
    meTargetVehicleRefType              = VehicleRef::E_PLAYER_CAR;   // stw 0, 0x144
    mfPadInactiveTime                   = 0.0f;   // stfs 0.0, 0x148
    mfPlayerInactiveTime                = 0.0f;   // stfs 0.0, 0x14C
    mbPlayerBeenActive                  = false;  // stb 0, 0x150
    mbShouldResetPlayerCameraThisFrame  = false;  // stb 0, 0x151
    mbNewCarAdded                       = false;  // stb 0, 0x152
    mfCarAddedPresentationTimeRemaining = 0.0f;   // stfs 0.0, 0x154
    meBeatenRivalRaceCarIndex           = E_ACTIVE_RACE_CAR_INDEX_INVALID; // stw -1, 0x15C
    mFinishLineID                       = 0;      // std 0, 0x160 (CgsID)
    meJunkyardState                     = E_JY_INACTIVE; // stw 0, 0x180
    mbIsRivalUnlock                     = false;  // stb 0, 0x184
    mbNewCarUnlockedThisFrame           = false;  // stb 0, 0x185
    mJunkyardId                         = 0;      // std 0, 0x190 (CgsID)
    miJunkyardPosIndex                  = -1;     // stw -1, 0x198

    mbOnlineCarSelectCarIsShowable      = true;   // stb 1, 0x1A2
    mbOnlineRaceIntroCanUseBars         = true;   // stb 1, 0x1A9

    mbJunkyardPosJustChanged            = false;  // stb 0, 0x19C
    mbJunkyardPosIsLeft                 = true;   // stb 1, 0x19D  (r8 == 1)
    mbJunkyardPlayerRespawnedThisFrame  = false;  // stb 0, 0x19E
    mbJunkyardSelectionChangedMessageReceivedThisFrame = false; // stb 0, 0x19F
    mbJunkyardSelectionChangedMessageWaiting           = false; // stb 0, 0x1A0
    mbJunkyardCarModActive              = false;  // stb 0, 0x1A1
    mbJunkyardCarUnlockTickedClosedThisFrame             = false; // stb 0, 0x1A3
    mbJunkyardCarUnlockTickedClosedThisFrameMessageWaiting = false; // stb 0, 0x1A4
    mbIsOnlineCarSelectActive           = false;  // stb 0, 0x1A5
    mbHasOnlineCarSelectBeenAborted     = false;  // stb 0, 0x1A6
    mbOnlineCarSelectCanStartRaceIntro  = false;  // stb 0, 0x1A7
    mbOnlineCarSelectMustClampToCar     = false;  // stb 0, 0x1A8
    mbRankUpMessageWaiting              = false;  // stb 0, 0x1AA
    mbRankUpMessageReceivedThisFrame    = false;  // stb 0, 0x1AB
    miRankUpNewRank                     = 0;      // stw 0, 0x1AC

    mbDoing100PercentSequence           = false;  // stb 0, 0x1B0
    mbPlayerAndRivalImpactOccured       = false;  // stb 0, 0x1B1
    mbPlayerWonImpactAgainstRival       = false;  // stb 0, 0x1B2
    mbPlayerCheckedTraffic              = false;  // stb 0, 0x1B3
    mbPlayerHitCheckpointThisFrame      = false;  // stb 0, 0x1B4
    mbPlayerPerformedStunt              = false;  // stb 0, 0x1B5
    mbComboWarningActive                = false;  // stb 0, 0x1B6
    mfPlayerBoostPercentage             = 0.0f;   // stfs 0.0, 0x1B8
    mfHowCloseToTotalled                = 0.0f;   // stfs 0.0, 0x1BC

    mbRoadRageOneMoreCrashToWrecked     = false;  // stb 0, 0x1C0
    mbRoadRageTotalled                  = false;  // stb 0, 0x1C1
    mbTrainingPause                     = false;  // stb 0, 0x1C2
    mbPlayerWasTakenDown                = false;  // stb 0, 0x1C3
    mePlayerKillerIndex                 = E_ACTIVE_RACE_CAR_INDEX_INVALID; // stw -1, 0x1C4
    mbStartingFreeburnDueToPlayerJoinThisFrame = false; // stb 0, 0x1C8

    // --- the tail (+0x1CC..+0x1F3), exactly the console's stores and no more ---
    // [FX-DIRECTOR 2026-09-24] The three sub-objects are named DWARF members now (see the header).
    // The console writes ONE RankUpInfo field here (mbDoingRankUp, `stb 0, 0x1D4` 0x82218B8C) and
    // leaves mbDoingRankUpIntro / mbNewRivalFocusThisFrame / meRivalIndex alone; the old blob
    // memsets zeroed all eight bytes, which the console does not.
    mRankUpInfo.mbDoingRankUp           = false;  // stb 0, 0x1D4
    mbPlayerDamageCritical              = false;  // stb 0, 0x1D0
    mbPlayerWrecked                     = false;  // stb 0, 0x1D1
    mShowTimeInfo.Clear();                        // 0x82218C30..0x82218C54 (+0x1DC..+0x1ED)
    mDirectorProfileData.Construct();             // stw 1, 0x1F0 (meCameraMode = THIRD_PERSON)
    miPlayerTeam                        = 0;      // stw 0, 0x1CC (0x82218CB4, the function's last store)
}

// ----------------------------------------------------------------------------
// BrnDirector::GameState::ResetPerFrameData
//
// ⭐ RECOVERED 2026-08-01. There is NO standalone symbol for this in the X360 image -- it is
// INLINED, and its one emission site is the head of MainDirector::ProcessInputQueue
// @0x82237350..0x822373AC, which materialises the GameState base
// (`addis r11, r31, 3; addi r11, r11, 0x37E0`) and then zeroes nineteen fields before it
// touches the event queue at all. Every store below is that list, read literally off the asm;
// each one resolves to a member of this struct with no leftovers.
//
// NAMING: the DecFIGS DWARF declares exactly one member on GameState with this shape --
// `void ResetPerFrameData()` (BrnDirectorGameState.h:258) -- and this header has carried that
// declaration unbodied since the struct landed. De-inlined here rather than open-coded in the
// director so the per-frame contract lives with the type that owns the fields.
// FLAG: the NAME is the DWARF's and the shape matches; if a later attestation shows the block
//   belongs to some other member, only the name changes -- the body is asm.
// ----------------------------------------------------------------------------
void GameState::ResetPerFrameData()
{
    mfPlayerBoostPercentage             = 0.0f;   // stfs 0.0, 0x1B8
    miThisFramesActionFlags             = 0;      // stw  0,   0x0E4

    mbJunkyardPosJustChanged            = false;  // stb 0, 0x19C
    mbNewCarUnlockedThisFrame           = false;  // stb 0, 0x185
    mbShouldResetPlayerCameraThisFrame  = false;  // stb 0, 0x151
    mbPlayerAndRivalImpactOccured       = false;  // stb 0, 0x1B1
    mbPlayerWonImpactAgainstRival       = false;  // stb 0, 0x1B2
    mbPlayerCheckedTraffic              = false;  // stb 0, 0x1B3
    mbPlayerInTunnel                    = false;  // stb 0, 0x0F8
    mbPlayerHitCheckpointThisFrame      = false;  // stb 0, 0x1B4
    mbPlayerPerformedStunt              = false;  // stb 0, 0x1B5
    mbComboWarningActive                = false;  // stb 0, 0x1B6
    mbRankUpMessageReceivedThisFrame    = false;  // stb 0, 0x1AB
    mbJunkyardPlayerRespawnedThisFrame  = false;  // stb 0, 0x19E
    mbJunkyardSelectionChangedMessageReceivedThisFrame = false;   // stb 0, 0x19F
    mbStartingFreeburnDueToPlayerJoinThisFrame         = false;   // stb 0, 0x1C8
    mbJunkyardCarUnlockTickedClosedThisFrame           = false;   // stb 0, 0x1A3

    // stb 0, 0x1D6 -- RankUpInfo::mbNewRivalFocusThisFrame, the byte IsNewRankUpRivalThisFrame()
    // reads.
    mRankUpInfo.mbNewRivalFocusThisFrame = false;

    // stb 0, 0x1E8 .. 0x1EC -- ShowTimeInfo::ResetPerFrameData (DWARF h:227): the five ThisFrame
    // bools. mbInIntro (+0x1ED) is NOT per-frame and is not touched.
    mShowTimeInfo.ResetPerFrameData();
}

} // namespace BrnDirector

// The DataJournal<EEventState,2> non-const GetCurrent() instance the X360 emits @0x821FBB80
// (linker-attributed to BrnDirector::GameState). The body is the generic inline in
// BrnDirectorDataJournal.h; this explicit member instantiation forces it out-of-line here.
template BrnDirector::GameState::EEventState&
    DataJournal<BrnDirector::GameState::EEventState, 2>::GetCurrent();
