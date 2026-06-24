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

    // BrnDirectorGameState.h:214 / :222 / :241 -- the three trailing sub-objects (RankUpInfo,
    // ShowTimeInfo, DirectorProfileData). The DecFIGS DWARF names their FIELDS, but the field
    // layouts it gives do NOT line up with the byte/word offsets GameState::Clear stores into
    // these objects (e.g. Clear writes a byte at ShowTimeInfo+0x00 where the DWARF puts an f32,
    // and stores out to DirectorProfileData+0x08 though the DWARF lists only a single 4-byte
    // enum). The DWARF sub-struct layouts are unreliable here, so the three sub-objects are
    // modelled as opaque sized blobs at their asm-attested extents (sizes pinned by Clear's
    // store range + the next member's start). Their field NAMES (DWARF :214..:237 / :247) are
    // recorded in the comments for when each sub-object's own TU recovers the real layout.
    //
    //   RankUpInfo          (+0x1CC, 8 bytes):  mbDoingRankUp/mbDoingRankUpIntro/
    //                                           mbNewRivalFocusThisFrame/meRivalIndex
    //   ShowTimeInfo        (+0x1D4, 20 bytes): mfDeformationLevel/miComboLevel/
    //                                           miTotalVehiclesHit + 6 per-frame bools
    //   DirectorProfileData (+0x1E8, 12 bytes): meCameraMode (+ un-DWARF'd trailing members;
    //                                           Clear sets its +0x08 word to 1)
    struct RankUpInfo          { u8 maOpaque[0x08]; };   // FLAG: opaque (DWARF field layout unreliable)
    struct ShowTimeInfo        { u8 maOpaque[0x14]; };   // FLAG: opaque (DWARF field layout unreliable)
    struct DirectorProfileData { u8 maOpaque[0x0C]; };   // FLAG: opaque (DWARF incomplete + unreliable)

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
    EJunkyardState      meJunkyardState;                         // :163
    bool                mbIsRivalUnlock;                         // :164
    bool                mbNewCarUnlockedThisFrame;               // :165
    CgsID               mUnlockedVehicleType;                    // :166
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
    RankUpInfo          mRankUpInfo;                             // :250
    ShowTimeInfo        mShowTimeInfo;                           // :251
    DirectorProfileData mDirectorProfileData;                    // :252

    // BrnDirectorGameState.h:255 -- ledger func @0x82218930 (defined in the .cpp).
    void Clear();

    // BrnDirectorGameState.h:258 -- not owned by this TU; declare only.
    void ResetPerFrameData();
};

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_GAME_STATE_H
