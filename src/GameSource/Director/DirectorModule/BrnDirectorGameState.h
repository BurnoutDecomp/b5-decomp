#ifndef GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_GAME_STATE_H
#define GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_GAME_STATE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Matrix44Affine, Vector3, CgsID
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameSource/BurnoutConstants.h"                                          // EActiveRaceCarIndex
#include "GameSource/Director/Utils/BrnDirectorDataJournal.h"                     // DataJournal<T,N>
#include "GameSource/Director/Utils/BrnVehicleRef.h"                              // BrnDirector::VehicleRef::EType
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                       // BrnNetwork::EPaybackType

// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorGameState.h
//
// BrnDirector::GameState -- the director's per-event game-state snapshot (which event mode is
// running, the drive-thru / junkyard / takedown / payback / showtime sub-states the camera
// director reacts to, plus the per-frame action flags). HOME for the GameState class slice this
// TU bodies (GameState::Clear @0x82218930). Layout/order/types verbatim from the DecFIGS DWARF
// (BrnDirectorGameState.h:41). The reconstructed member offsets reproduce the X360 Clear's
// store offsets exactly (X360 4-byte-pointer build; this PC reconstruction is 64-bit, so the
// single leading pointer makes the host offsets diverge past +0x00 -- expected per the project's
// host-native-pointer-width convention. The store ROLES are preserved by name).
//
// X360 Clear @0x82218930 offset->member cross-check (4-byte-pointer console offsets):
//   +0x010/+0x050/+0x090  the three Matrix44Affine -> identity (lvx/stvx identity rows)
//   +0x0D0 mbDriveThruActive=0  +0x0D4 meDriveThruType=0  +0x0D9..+0x0DD bools=0
//   +0x0E0 meTakedownVictimID=-1  +0x0FA mbCrashActiveWithSlomoSinceStart=0
//   +0x0F0 mbPaybackActive=0  +0x0F4 meActivePaybackType=3  +0x100 mbCanUseSlomo=1
//   +0x120 meEventType=-1  +0x124/+0x128 mEventState entries=3  +0x12C wr-idx=0 +0x130 size=2
//   +0x134 miEventSpecificShotGroup=-1  +0x154 mfCarAddedPresentationTimeRemaining=0
//   +0x15C meBeatenRivalRaceCarIndex=-1  +0x160 mFinishLineID=0  +0x170 northmost dir -> 0
//   +0x190 mJunkyardId=0  +0x198 miJunkyardPosIndex=-1  +0x1A2 showable=1  +0x1A9 useBars=1
//   +0x1B8/+0x1BC boost%/howClose=0  +0x1C4 mePlayerKillerIndex=-1
// (See BrnDirectorGameState.cpp for the full store-for-store map.)
// ----------------------------------------------------------------------------

namespace BrnTraffic { class JunctionLogicBox; }

namespace BrnDirector
{

struct GameState
{
    // BrnDirectorGameState.h:44
    enum EEventState
    {
        E_EVENT_STATE_PRE_INTRO  = 0,
        E_EVENT_STATE_INTRO      = 1,
        E_EVENT_STATE_COUNTDOWN  = 2,
        E_EVENT_STATE_ACTIVE     = 3,
        E_EVENT_STATE_POST_EVENT = 4,

        // Alias: the "race is live" state == ACTIVE. Kept so the VehicleTracker (which
        // previously forked a minimal GameState naming this E_EVENT_STATE_RACING) reads through
        // the real enum after its fork is removed.
        E_EVENT_STATE_RACING     = E_EVENT_STATE_ACTIVE,
    };

    // BrnDirectorGameState.h:53
    enum EDriveThru
    {
        E_DRIVETHRU_INVALID    = 0,
        E_DRIVETHRU_AUTO_PARTS = 1,
        E_DRIVETHRU_BODY_SHOP  = 2,
        E_DRIVETHRU_GAS_STATION = 3,
        E_DRIVETHRU_TUNING_SHOP = 4,
        E_DRIVETHRU_TIRE_SHOP  = 5,
        E_DRIVETHRU_COUNT      = 6,
    };

    // BrnDirectorGameState.h:77
    enum ECameraMode
    {
        E_CAMERA_MODE_FIRST_PERSON = 0,
        E_CAMERA_MODE_THIRD_PERSON = 1,
        E_CAMERA_MODE_COUNT        = 2,
    };

    // BrnDirectorGameState.h:150
    enum EJunkyardState
    {
        E_JY_INACTIVE             = 0,
        E_JY_INTRO_UNLOCKING_CARS = 1,
        E_JY_INTRO_NO_CARS        = 2,
        E_JY_CAR_UNLOCK           = 3,
        E_JY_CAR_SELECT           = 4,
        E_JY_WAITING_FOR_AUDIO    = 5,
    };

    // BrnDirectorGameState.h:214 / :222 / :241 -- the three trailing sub-objects, at their DWARF
    // field layouts. ⭐ RE-MODELLED 2026-09-24 (crash parity FX-DIRECTOR). The previous model said
    // the DWARF layouts "do not line up" with the console's stores and kept all three as opaque
    // blobs -- RankUpInfo @+0x1CC, ShowTimeInfo @+0x1D4, DirectorProfileData @+0x1E8. They DO line
    // up, exactly, once the three objects sit where the console puts them: 8 bytes later, behind
    // an 8-byte run the X360 has and the PS3 DWARF does not (the three members above mRankUpInfo,
    // see there). Pinned store for store:
    //   ShowTimeInfo @+0x1DC -- ProcessInputQueue case 39 0x82237D94..0x82237DCC and GameState::
    //       Clear 0x82218C30..0x82218C54 are the SAME inlined ShowTimeInfo::Clear off one base
    //       register (`addi r11, r11, 0x39BC` == GameState +0x1DC): stfs +0x0 (mfDeformationLevel),
    //       stw +0x8 (miTotalVehiclesHit), stw +0x4 (miComboLevel), stb +0x11 (mbInIntro),
    //       stb +0xC..+0x10 (the five ThisFrame bools) -- nine stores, nine DWARF fields, no gaps.
    //       The ProcessInputQueue prologue clears exactly the five ThisFrame bools
    //       (0x82237398..0x822373A8, == ShowTimeInfo::ResetPerFrameData, DWARF h:227).
    //   RankUpInfo @+0x1D4 -- the prologue's `stb 0, 0x1D6` (0x8223738C) is the one per-frame field,
    //       mbNewRivalFocusThisFrame (+2); ArbStateRankUp reads +0x1D4 / +0x1D6 / +0x1D8 as
    //       mbDoingRankUp / mbNewRivalFocusThisFrame / meRivalIndex.
    //   DirectorProfileData @+0x1F0 -- Clear's `stw 1, 0x1F0` (0x82218C60) is its Construct
    //       (meCameraMode = E_CAMERA_MODE_THIRD_PERSON); PostGuiUpdate stores GUI 475's word there.
    // GameState therefore ends at +0x1F4 on the console. (A GUI header, BrnGuiOptionsDataProfile.h,
    // carries its own 12-byte opaque `BrnDirector::GameState::DirectorProfileData` fork for a
    // pointer-only use; it is not this type and no TU sees both.)
    struct RankUpInfo
    {
        bool                mbDoingRankUp;              // :215  +0x1D4
        bool                mbDoingRankUpIntro;         // :216  +0x1D5
        bool                mbNewRivalFocusThisFrame;   // :217  +0x1D6 (per-frame; cleared by the
                                                        //              ProcessInputQueue prologue)
        EActiveRaceCarIndex meRivalIndex;               // :218  +0x1D8
    };

    struct ShowTimeInfo
    {
        f32  mfDeformationLevel;               // :229  +0x1DC
        s32  miComboLevel;                     // :230  +0x1E0 (ArbStateCrashMode's blur scale)
        s32  miTotalVehiclesHit;               // :231  +0x1E4
        bool mbComboLevelIncreasedThisFrame;   // :232  +0x1E8
        bool mbVehicleImpactThisFrame;         // :233  +0x1E9 (ArbStateCrashMode: the shake blur)
        bool mbCrushComboThisFrame;            // :234  +0x1EA (ArbStateCrashMode: super-slo-mo close-up)
        bool mbEarntMultiplierThisFrame;       // :235  +0x1EB (ArbStateCrashMode: close-up)
        bool mbExtraSpinThisFrame;             // :236  +0x1EC (ArbStateCrashMode: the long blur)
        bool mbInIntro;                        // :237  +0x1ED (the showtime intro latch)

        // DWARF h:224. No out-of-line X360 symbol: inlined at ProcessInputQueue case 39
        // (0x82237DAC..0x82237DCC) and GameState::Clear (0x82218C30..0x82218C54), both in this
        // store order.
        void Clear()
        {
            mfDeformationLevel = 0.0f;
            miTotalVehiclesHit = 0;
            miComboLevel       = 0;
            mbInIntro          = false;
            ResetPerFrameData();
        }

        // DWARF h:227. No out-of-line X360 symbol: inlined at the ProcessInputQueue prologue
        // (0x82237398..0x822373A8) and as Clear's tail.
        void ResetPerFrameData()
        {
            mbComboLevelIncreasedThisFrame = false;
            mbVehicleImpactThisFrame       = false;
            mbCrushComboThisFrame          = false;
            mbEarntMultiplierThisFrame     = false;
            mbExtraSpinThisFrame           = false;
        }
    };

    struct DirectorProfileData
    {
        ECameraMode meCameraMode;   // :247  +0x1F0 (PostGuiUpdate stores GUI command 475's payload)

        // DWARF h:245. No out-of-line X360 symbol: GameState::Clear inlines it as
        // `stw r8(=1), 0x1F0` (0x82218C60).
        void Construct()
        {
            meCameraMode = E_CAMERA_MODE_THIRD_PERSON;
        }
    };

    // --- members, DWARF order (BrnDirectorGameState.h:85..252) ---------------------
    const BrnTraffic::JunctionLogicBox* mpEventJLBox;            // :85
    Matrix44Affine      mTrafficLightSpace;                      // :86
    Matrix44Affine      mBaseDriveThruTransform;                 // :88
    Matrix44Affine      mDriveThruTransform;                     // :89
    bool                mbDriveThruActive;                       // :90
    EDriveThru          meDriveThruType;                         // :91
    bool                mbNewProfileIntroActive;                 // :93
    bool                mbGameIntroFlybyActive;                  // :94
    bool                mbTakedownActive;                        // :96
    bool                mbIsRevengeTD;                           // :97
    bool                mbIsShutdown;                            // :98
    bool                mbIsSignatureTD;                         // :99
    EActiveRaceCarIndex meTakedownVictimID;                      // :100
    s32                 miThisFramesActionFlags;                 // :102
    s64                 miActionRequestedCamera;                 // :105
    bool                mbPaybackActive;                         // :107
    BrnNetwork::EPaybackType meActivePaybackType;                // :108
    bool                mbPlayerInTunnel;                        // :110
    bool                mbCrashActive;                           // :111
    bool                mbCrashActiveWithSlomoSinceStart;        // :112
    f32                 mfCrashTimeRemaining;                    // :113
    bool                mbCanUseSlomo;                           // :115
    bool                mbCrashNavShown;                         // :117
    bool                mbColourCalibrationShown;                // :118
    bool                mbShortCutMenuShown;                     // :120
    bool                mbWonLastEvent;                          // :122
    bool                mbImpactTimeActive;                      // :124
    f32                 mfImpactTimeSloMoFactor;                 // :125
    u32                 muNumberOfCarsInIntro;                   // :127
    EActiveRaceCarIndex maeIntroCarID[3];                        // :128
    bool                mbGoToCrashModeAfterIntro;               // :130
    // FLAG: meEventType is BrnGameState::GameStateModuleIO::EGameModeType (DWARF :131), a
    //   `: s32` enum whose home (BrnGameStateSharedIO.h) is a heavy IO header that would pull
    //   the GameState IO cone into the director snapshot. Modelled as s32 (the enum's exact
    //   underlying width) so Clear's `meEventType = -1` (E_MODE_NONE) is byte-faithful; re-type
    //   to the real enum when the include graph is cheap. The VALUE (-1) is asm.
    s32                 meEventType;                             // :131  (EGameModeType : s32)
    DataJournal<EEventState, 2> mEventState;                     // :132
    s32                 miEventSpecificShotGroup;                // :133
    EEventState         meEventStatePreviousFrame;               // :134
    f32                 mfStateTimeLeft;                         // :135
    EActiveRaceCarIndex meTargetRaceCarIndex;                    // :136
    VehicleRef::EType   meTargetVehicleRefType;                  // :137
    f32                 mfPadInactiveTime;                       // :139
    f32                 mfPlayerInactiveTime;                    // :140
    bool                mbPlayerBeenActive;                      // :141
    bool                mbShouldResetPlayerCameraThisFrame;      // :143
    bool                mbNewCarAdded;                           // :144
    f32                 mfCarAddedPresentationTimeRemaining;     // :145
    EActiveRaceCarIndex meAddedCarID;                            // :146
    EActiveRaceCarIndex meBeatenRivalRaceCarIndex;               // :148
    CgsID               mFinishLineID;                           // :160
    Vector3             mFinishLineNorthmostDir;                 // :161
    // ⭐ THE JUNKYARD / CAR-SELECT SUB-STATE (GameState +0x180). Its ONLY writers in the whole
    // X360 image are MainDirector::ProcessInputQueue @0x822372F8 (game-action cases 62/63/64/
    // 73/77) and MainDirector::PostGuiUpdate @0x82236F88 (the E_JY_WAITING_FOR_AUDIO leg);
    // GameState::Clear seeds it to E_JY_INACTIVE. It is the gate on
    // ArbStateRoaming::ProcessActiveDrivingTransitions -> E_STATE_CAR_SELECT, i.e. on the
    // whole junkyard + retail-intro-camera ladder.
    EJunkyardState      meJunkyardState;                         // :163
    bool                mbIsRivalUnlock;                         // :164
    bool                mbNewCarUnlockedThisFrame;               // :165
    // +0x188. The "+0x186..+0x18F unaccounted gap" this file used to flag is THIS member:
    // ProcessInputQueue case 62 (E_ACTION_NEW_CAR_UNLOCKED) does `ld r10, 0(payload);
    // stdx r10, r31, 0x33968` -- a 64-bit CgsID store at GameState +0x188. Clear never stores
    // there, which is why the gap looked unaccounted.
    CgsID               mUnlockedVehicleType;                    // :166
    // +0x190. ⚠️ CORRECTION 2026-08-01: a previous wave recorded "mJunkyardId is NEVER written
    // by the director anywhere in the image" as a VERIFIED NEGATIVE. It is WRONG.
    // ProcessInputQueue case 64 (E_ACTION_CAR_SELECTION_CHANGED) writes it from the action's
    // first 8 bytes: `ld r11, 0(payload); ... ori r10, r10, 0x3970; stdx r11, r31, r10`
    // (0x33970 - 0x337E0 == 0x190). The negative came from scanning the DECOMPILER's integer
    // literals, and Hex-Rays renders that particular 64-bit indexed store as
    // `*(_R31 + HIDWORD(v74)) = v74;` -- the literal 211312 never appears. Read the asm.
    CgsID               mJunkyardId;                             // :167
    s32                 miJunkyardPosIndex;                      // :168
    bool                mbJunkyardPosJustChanged;                // :169
    bool                mbJunkyardPosIsLeft;                     // :170
    bool                mbJunkyardPlayerRespawnedThisFrame;      // :171
    bool                mbJunkyardSelectionChangedMessageReceivedThisFrame; // :172
    bool                mbJunkyardSelectionChangedMessageWaiting; // :173
    bool                mbJunkyardCarModActive;                  // :174
    bool                mbOnlineCarSelectCarIsShowable;          // :175
    bool                mbJunkyardCarUnlockTickedClosedThisFrame; // :177
    bool                mbJunkyardCarUnlockTickedClosedThisFrameMessageWaiting; // :178
    bool                mbIsOnlineCarSelectActive;               // :180
    bool                mbHasOnlineCarSelectBeenAborted;         // :181
    bool                mbOnlineCarSelectCanStartRaceIntro;      // :182
    bool                mbOnlineCarSelectMustClampToCar;         // :183
    bool                mbOnlineRaceIntroCanUseBars;             // :185
    bool                mbRankUpMessageWaiting;                  // :187
    bool                mbRankUpMessageReceivedThisFrame;        // :188
    s32                 miRankUpNewRank;                         // :189
    bool                mbDoing100PercentSequence;               // :191
    bool                mbPlayerAndRivalImpactOccured;           // :193
    bool                mbPlayerWonImpactAgainstRival;           // :194
    bool                mbPlayerCheckedTraffic;                  // :196
    bool                mbPlayerHitCheckpointThisFrame;          // :197
    bool                mbPlayerPerformedStunt;                  // :198
    bool                mbComboWarningActive;                    // :199
    f32                 mfPlayerBoostPercentage;                 // :200
    f32                 mfHowCloseToTotalled;                    // :202
    bool                mbRoadRageOneMoreCrashToWrecked;         // :203
    bool                mbRoadRageTotalled;                      // :204
    bool                mbTrainingPause;                         // :206
    bool                mbPlayerWasTakenDown;                    // :208
    EActiveRaceCarIndex mePlayerKillerIndex;                     // :209
    bool                mbStartingFreeburnDueToPlayerJoinThisFrame; // :211
    // +0x1CC..+0x1D3 -- THREE X360-ONLY MEMBERS (merge-window delta: no DecFIGS member sits between
    // :211 and :250, yet the console has 8 bytes there). Every store is asm: the ProcessInputQueue
    // prologue copies them from the input buffer every frame (0x8223740C..0x82237440: @0x7AB0 ->
    // +0x1CC word, @0x7AD6 -> +0x1D1, @0x7AD5 -> +0x1D0), GameState::Clear zeroes all three
    // (0x82218CB4 / 0x82218C14 / 0x82218C1C). Their producer is BridgeGameStateToDirector
    // (0x823CD454: the game-state output's per-car word at +0x2AFC8 + 4*player; 0x823CD4DC: its
    // per-car byte at +0x2AEB8 + player; 0x823CD510: `f32 +0x2AF4C > 0.0 ? 0 : byte +0x2AF60`).
    // FLAG: the NAMES are ours, from the one consumer, ArbStateRoaming::ProcessPossibleFX:
    //   +0x1CC is compared == 2 (0x82234BF4) and passed as SqDistanceOfNearestOpposingTeamMember's
    //          liMyTeam (0x82234CC0) -- the player's team;
    //   +0x1D0 plays "Damage_Crit" at full blend (0x82234C8C);
    //   +0x1D1 plays "Wrecked" while an opposing car is near (0x82234CB4).
    s32                 miPlayerTeam;                            // X360 +0x1CC (FLAG name)
    bool                mbPlayerDamageCritical;                  // X360 +0x1D0 (FLAG name)
    bool                mbPlayerWrecked;                         // X360 +0x1D1 (FLAG name)
    RankUpInfo          mRankUpInfo;                             // :250  +0x1D4
    ShowTimeInfo        mShowTimeInfo;                           // :251  +0x1DC
    DirectorProfileData mDirectorProfileData;                    // :252  +0x1F0

    // BrnDirectorGameState.h:255 -- ledger func @0x82218930 (defined in the .cpp).
    void Clear();

    // BrnDirectorGameState.h:258 -- BODIED in the .cpp since 2026-08-01. The X360 emits no
    // standalone symbol for it (it is inlined into MainDirector::ProcessInputQueue's prologue
    // @0x82237350); the body is that store list, de-inlined here.
    void ResetPerFrameData();

    // ---- small accessors other director TUs reach by name (declare-only) -----------------
    // The most-recent event state (the head of mEventState). The VehicleTracker gates its
    // race-end ramp on this; previously each consumer forked a minimal GameState slice -- these
    // give the real type the same named accessors so the fork can be removed.
    EEventState GetCurrentEventState() const { return mEventState.GetCurrent(); }
    // True while the pre-race countdown is running.
    bool        IsInCountdown() const { return mEventState.GetCurrent() == E_EVENT_STATE_COUNTDOWN; }

    // ---- rank-up control accessors (BrnArbStateRankUp consumes these) ---------------------
    // The director "rank up" state (ARTIST: Prepare @0x82270EE8 / Update @0x82236380) reads the
    // three RankUpInfo fields at +0x1D4 / +0x1D6 / +0x1D8. They are named DWARF members now (see
    // RankUpInfo); the accessors stay because ArbStateRankUp reads through them.
    bool IsRankUpIntroRunning() const
    {
        return mRankUpInfo.mbDoingRankUp;              // +0x1D4
    }
    bool IsNewRankUpRivalThisFrame() const
    {
        return mRankUpInfo.mbNewRivalFocusThisFrame;   // +0x1D6
    }
    EActiveRaceCarIndex GetRankUpRivalRaceCarIndex() const
    {
        return mRankUpInfo.meRivalIndex;               // +0x1D8
    }
};

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_GAME_STATE_H
