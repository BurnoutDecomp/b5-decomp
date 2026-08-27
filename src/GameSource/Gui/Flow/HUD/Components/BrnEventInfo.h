// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnEventInfo.h
//
//   BrnGui::EventInfoComponent -- the in-race HUD "event info" panel (position /
//   medal text-state, score/banking, road-rage takedown digits, timers, distance
//   interpolator, etc). Derives from BrnFlaptComponent. RaceMainHudState embeds
//   this component BY VALUE (X360 RaceMainHudState +0x170), so its shape is a hard
//   dependency of that TU as well.
//
// SHAPE AUTHORITY. Member NAMES + ORDER, the nested typedefs and the KI_/KAC_ statics
// come from the DecFIGS DWARF (BrnEventInfo.h). LAYOUT authority is the X360 ARTIST
// image: the DecFIGS dump is an older/PS3 build and is DEMONSTRABLY SHORT here.
//
// ---------------------------------------------------------------------------
// LAYOUT RE-GROWN 2026-08-27 (wave A6) -- the previously committed member list was
// wrong from maTextField onward. Three X360-attested corrections:
//
//   (1) maTextField is [7], not [6]. PrepareComponentsForGameMode @0x82429150 walks
//       KAPC_TEXTFIELD_NAMES from off_82F24B4C to off_82F24B68 (asm @0x824291B4:
//       r30 = this+0x1C, stride 0x0C, bound +0x1C == 7 entries), and Construct
//       @0x82421160 zeroes exactly 27 dwords (+0x1C..+0x84) == 7 + 2 TextFieldRefs.
//       UpdateStuntAttack @0x82429C08 addresses maTextField[6] (+0x64) directly for
//       the online add-score delta.
//
//   (2) There are THIRTEEN animators, not twelve. Construct @0x82421160 issues 13
//       vtbl[0] AnimatorComponentType::Construct calls; the FIFTH one (this+0x168,
//       "TakedownNumbersAnimator") is ABSENT FROM THE DWARF entirely. The real
//       mDistanceAnimatorRace ("raceDistance_anim") is the SIXTH, at +0x1A0.
//       The committed SetTakedownsDigitsState body was driving the right animator
//       under the wrong member name -- it now names mTakedownNumbersAnimator.
//       (The old header's own pin comment, mTextStateAnimatorRace == +0x0F8, was
//       unreachable from its own member list -- it computed to +0xEC.)
//
//   (3) The scalar tail runs to +0x494, not +0x430. ClearEventSpecificData
//       @0x82412CA8 pins the WHOLE run store-for-store and exposes three members the
//       DWARF has no name for: +0x440 (the cached add-score delta), +0x468 (the
//       cached event-state frame index) and +0x494 (a flag PrepareComponentsForGameMode
//       sets in its road-rage arm). They are spelled miAddScoreDisplayed /
//       miCurrentEventStateIndex / mbRoadRageComponentsPrepared here -- descriptive
//       X360-only names, NOT DWARF names. [FLAG X360-only members] DELETE-WHEN a
//       DWARF/PDB dump of the retail X360 build supplies the real identifiers.
//
// X360 OFFSET MAP (all attested; console 32-bit ABI):
//   +0x000 mpStateInterface / +0x004 mAptRef        (BrnFlaptComponent base, 0x0C)
//   +0x00C mEventMovieClip           +0x014 mpEventInfoComponentName
//   +0x018 mHUDFileRef               +0x01C maTextField[7]   (stride 0x0C)
//   +0x070 mAddScoreTextField        +0x07C mBankScoreTextField
//   +0x088 mAddScoreAnimator         +0x0C0 mScoreBackgroundAnimator
//   +0x0F8 mTextStateAnimatorRace    +0x130 mTextStateAnimatorRRage
//   +0x168 mTakedownNumbersAnimator  +0x1A0 mDistanceAnimatorRace
//   +0x1D8 mTimeAnimatorRRage        +0x210 mTimeAnimatorBRoute
//   +0x248 mTimeAnimatorStuntRun     +0x280 mScoreAnimator
//   +0x2B8 mBankingAnimator          +0x2F0 mStuntAnimator
//   +0x328 mMultiplierAnimator       (animator stride 0x38; 13 x 56 = 728)
//   +0x360 mDistanceInterpolator     (span 0x70 -- FlaptInterpolatorComponent's own
//                                     header says "sizeof == 0x68", but its Vector4s
//                                     force 16-byte alignment, so the real object is
//                                     0x70 on console AND on the host)
//   +0x3D0 mfDistanceWarningThreshold ... +0x494 mbRoadRageComponentsPrepared
//
// HOST LAYOUT is name-based: the pointer-bearing prefix (MovieClipRef, TextFieldRef,
// FileRef, the animators) widens on x64, so its absolute offsets CANNOT and MUST NOT
// match the console. _AssertLayout below therefore pins (a) the array bounds, (b) the
// animator ordering by stride, and (c) the ENTIRE pointer-free scalar run +0x3D0..+0x494
// as RELATIVE deltas from mfDistanceWarningThreshold -- which the widening cannot move.
// ============================================================================
#ifndef BRN_GUI_EVENT_INFO_COMPONENT_H
#define BRN_GUI_EVENT_INFO_COMPONENT_H

#include <cstddef>                                                                 // offsetof (_AssertLayout relative pins)

#include "types.hpp"
#include "BrnCommonTypes.h"                                                        // CgsID
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameSource/GameState/BrnGameStateSharedIO.h"                             // GameStateModuleIO::EGameModeType, ECurrentMedalTargetTime, BrnGameState::StuntToDisplay
#include "GameSource/GameState/BrnGameStateTypes.h"                                // BrnGameState::LandmarkIndex, EStuntType
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                             // BrnFlapt::TextFieldRef
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                                  // BrnFlapt::FileRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"       // BrnFlaptComponent (base) + MovieClipRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"   // FlaptAnimatorComponent
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptInterpolatorComponent.h" // FlaptInterpolatorComponent

namespace BrnGui
{
    class GuiCache;   // pointer-held

    // DWARF BrnEventInfo.h:70.
    class EventInfoComponent : public BrnFlaptComponent
    {
    public:
        // DWARF :50/:49/:51 nested typedefs.
        typedef BrnFlapt::TextFieldRef          TextFieldComponentType;
        typedef BrnGui::FlaptAnimatorComponent  AnimatorComponentType;
        typedef FlaptInterpolatorComponent      InterpolatorComponentType;

        // ---- lifecycle (DWARF :78/:85) --------------------------------------
        // Construct @0x82421160 -- adopt the state channel through the base, clear the
        // nine TextFieldRefs, construct the 13 animators + the distance interpolator,
        // reset the scalar run, and pick the metric/imperial distance-warning threshold.
        // The X360 leaves mEventMovieClip UNTOUCHED here (MoveAnimation binds it).
        // Call site: RaceMainHudState::OnEnter @0x82478EF8 ("EventInfo_mc", state, NULL).
        void Construct(const char* lacName,
                       CgsGui::StateInterface* lpStateInterface,
                       const char* lacParentName);

        // Prepare @0x82412C00 -- resolve lacFullName inside lFile, bind it into the BASE
        // mAptRef (NOT mEventMovieClip), reset that clip's timeline and remember the
        // name + file handle for the per-animator Prepare calls.
        void Prepare(const char* lacFullName, const BrnFlapt::FileRef& lFile);

        // PrepareComponentsForGameMode @0x82429150 -- bind the 7 KAPC_TEXTFIELD_NAMES
        // fields out of mEventMovieClip, then per-mode bind the mode's own text fields
        // and animators (arms for 0, 3, 5 and {7, 12, 14, 17}).
        void PrepareComponentsForGameMode();

        // SetEventType @0x8242FC78 (DWARF :95) -- latch the mode, move to "prewait" and
        // clear the per-event scalars. Called by RaceMainHudState::SetupEventInfo
        // @0x82474A60.
        void SetEventType(BrnGameState::GameStateModuleIO::EGameModeType leCurrentGameMode);

        // MoveAnimation @0x82429570 (DWARF :100) -- goto-and-play the mode frame on the
        // base clip, RE-BIND mEventMovieClip from it, re-run PrepareComponentsForGameMode
        // and goto-and-play lpcAnimation on the freshly bound child clip.
        // NOTE: every call invalidates previously cached TextFieldInstance pointers.
        void MoveAnimation(const char* lpcAnimation);

        // Update @0x82435430 (DWARF :117) -- the per-frame mode switch. ONE argument:
        // the X360 body is Update(this, lpCache) and dispatches
        //   0 -> UpdateRace, 2/16 -> UpdateCrash, 3 -> UpdateRoadRage,
        //   5 -> UpdateBurningRoute, 7 -> UpdateStuntAttack(cache, false),
        //   8 -> UpdateSurvivor, 10 -> UpdateOnlineRace,
        //   12/14/17 -> UpdateStuntAttack(cache, true), 15 -> UpdateFreeBurnLobby.
        // Caller: RaceMainHudState::UpdateRunning @0x8247E898.
        void Update(GuiCache* lpCache);

    private:
        // ---- statics (DWARF :127/:197/:194/:258) ----------------------------
        // X360 KI_TEXTFIELD_COUNT is 7 (the DWARF/PS3 build says 6 -- see the loop-bound
        // attestation in the banner). Component-local; nothing shared depends on it.
        static const s32 KI_TEXTFIELD_COUNT              = 7;    // :127 (X360 value)
        static const s32 KI_POSITION_WARNING_THRESHOLD   = 4;    // :197
        static const s32 KI_NUM_TIMEOUT_WEDGE_FRAMES     = 5;    // :258 (cmpwi r29,5 @0x8242A080)

        // X360 asserts leStuntType < 0x12 at BrnEventInfo.cpp:1677, i.e. EIGHTEEN string
        // ids, and indexes [8] (BOOST) / [14] (REVERSE_TAKEOFF) as the boost-trick
        // prefixes. BrnGameStateTypes.h spells E_STUNT_TYPE_COUNT == 15 -- that enum is
        // the SCORER's category count and is NOT changed by this component; the table
        // additionally carries the three rating/error pseudo-types (15..17), so the
        // component-local bound is honestly 18 and is spelled here, not borrowed.
        static const s32 KI_STUNT_TYPE_STRING_ID_COUNT   = 18;

        // X360 E_MODE_COUNT is 18 (MoveAnimation's cmplwi 0x12 at BrnEventInfo.cpp:523)
        // while BrnGameStateSharedIO.h spells E_MODE_COUNT == 17. Same rule: the
        // component-local table bound is spelled here and the shared enum is left alone.
        static const s32 KI_MODE_FRAME_NAME_COUNT        = 18;

        // The six event-state animation names UpdateStuntAttack drives (off_82F24B68).
        static const s32 KI_EVENT_STATE_NAME_COUNT       = 6;

        // .rdata tables. DECLARED ONLY -- the definitions belong with the bodies that
        // consume them (MoveAnimation / PrepareComponentsForGameMode / UpdateStuntAttack),
        // which are not homed yet. CONTENTS ARE ALREADY RECOVERED from the X360 image
        // (2026-08-27, image.bin big-endian reads); paste them verbatim when bodying:
        //
        //   KAC_MODE_FRAME_NAMES @0x82F24AB0 [18] =
        //     "Race","Race","ShowTime","RoadRage","Pursuit","BurningRoute","Eliminator",
        //     "TrafficAttack","Survivor","TrafficAttack","Race","RoadRage","TrafficAttack",
        //     "RoadRage","TrafficAttack","Invisible","ShowTime","TrafficAttack"
        //   KAPC_TEXTFIELD_NAMES @0x82F24B4C [7] =
        //     "textField_1_mc" .. "textField_7_mc"
        //   KAPC_EVENT_STATE_NAMES @0x82F24B68 [6] =
        //     "prewait","idle","YourBlueLeads","YourRedLeads","RivalBlueLeads","RivalRedLeads"
        //   KAC_STUNT_TYPE_STRING_IDS @0x82F24B80 [18] =
        //     "HUD_INFO_STUNT_SPIN","HUD_INFO_STUNT_ROLL","HUD_INFO_STUNT_AIR",
        //     "HUD_INFO_STUNT_DRIFT","HUD_INFO_STUNT_SUPER_JUMP","HUD_INFO_STUNT_SMASH",
        //     "HUD_INFO_STUNT_BILLBOARD","HUD_INFO_STUNT_BURNOUT","HUD_INFO_STUNT_BOOST",
        //     "HUD_INFO_STUNT_REVERSE","HUD_INFO_STUNT_HANDBRAKE","HUD_INFO_STUNT_POWER_PARK",
        //     "HUD_INFO_STUNT_CRASH","HUD_INFO_STUNT_PROP","HUD_INFO_STUNT_REV_TAKEOFF",
        //     "HUD_INFO_STUNT_TAKEDOWN","HUD_INFO_STUNT_LEAP_CAR","HUD_INFO_STUNT_IN_CONVOY"
        //   KAPC_TIMEOUT_WEDGE_FRAME_NAMES @0x82F24BD4 [5] =
        //     "startpulse","startpulse_wedge2","startpulse_wedge3","startpulse_wedge4",
        //     "startpulse_wedge5"
        static const char* const KAC_MODE_FRAME_NAMES[KI_MODE_FRAME_NAME_COUNT];        // :129
        static const char* const KAPC_TEXTFIELD_NAMES[KI_TEXTFIELD_COUNT];              // :163
        static const char* const KAPC_EVENT_STATE_NAMES[KI_EVENT_STATE_NAME_COUNT];     // (X360-only symbol, off_82F24B68)
        static const char* const KAC_STUNT_TYPE_STRING_IDS[KI_STUNT_TYPE_STRING_ID_COUNT]; // :194 (DWARF says [15]; X360 is [18])
        static const char* const KAPC_TIMEOUT_WEDGE_FRAME_NAMES[KI_NUM_TIMEOUT_WEDGE_FRAMES]; // :257

        // ---- methods (DWARF order) ------------------------------------------
        // @0x82412CA8 (DWARF :276) -- reset every per-event scalar. Zero callees; the
        // store list is the layout oracle for the whole +0x3D0..+0x494 run.
        void ClearEventSpecificData();

        // ---- the per-mode Update arms Update @0x82435430 dispatches to ------
        // ADDITIVE DECLARATIONS (wave A8 2026-08-27): Update's jump table
        // (jpt_824354A8, 18 cases) names all seven by symbol, so the switch cannot be
        // transcribed without them. Shapes are the DecFIGS DWARF's, which agrees with
        // the X360 calls (r3 = this, r4 = the cache; void return -- the console's
        // `result` is only the ABI leftover of the last call it made):
        //   UpdateRace          DWARF :442  X360 @0x8242FCF0  case 0
        //   UpdateCrash         DWARF :472  X360 @0x82412E98  cases 2, 16
        //   UpdateRoadRage      DWARF :460  X360 @0x82429A48  case 3
        //   UpdateBurningRoute  DWARF :475  X360 @0x8242A830  case 5
        //   UpdateSurvivor      DWARF :481  X360 @0x82421530  case 8
        //   UpdateOnlineRace    DWARF :445  X360 @0x824296B0  case 10
        //   UpdateFreeBurnLobby DWARF :448  X360 @0x8242FE98  case 15
        // (The DWARF also declares UpdatePursuit / UpdateFaceOff / UpdateEliminator /
        // UpdateTrafficAttack -- retail's jump table has NO arm for modes 4, 6, 9 or 11,
        // so they are dead in this build and are deliberately NOT declared here.)
        void UpdateRace(GuiCache* lpCache);
        void UpdateOnlineRace(GuiCache* lpCache);
        void UpdateFreeBurnLobby(GuiCache* lpCache);
        void UpdateRoadRage(GuiCache* lpCache);
        void UpdateCrash(GuiCache* lpCache);
        void UpdateBurningRoute(GuiCache* lpCache);
        void UpdateSurvivor(GuiCache* lpCache);

        // @0x82429C08 (DWARF :317) -- the stunt-run / stunt-attack readout. The X360
        // signature carries a SECOND argument the DWARF/PS3 build lacks: `clrlwi r26,r5,24`
        // @0x82429C1C, and Update passes 0 for mode 7 / 1 for modes 12, 14 and 17.
        void UpdateStuntAttack(GuiCache* lpCache, bool lbOnline);

        // @0x8241EDF8 (DWARF :373) -- set the position/medal text-state (Gold/Normal/Warning).
        void SetPositionTextState(s32 liPosition);

        // @0x824218A8 -- set the road-rage takedown digit layout (Single/Multiple).
        // X360-only (not in the DWARF method list).
        void SetTakedownsDigitsState();

        // @0x82421980 (DWARF :396) -- lerp the field colour KV4_SAFECOLOUR -> KV4_DANGERCOLOUR
        // over the last KF_DANGERTIME_START (10.0f) seconds and apply it.
        void SetTextFieldDangerColour(TextFieldComponentType* lpTextField, f32 lfTimeLeft);

        // @0x824214C8 (DWARF :389) -- the apt frame-trigger that ends a banking
        // transition. Registered by PrepareComponentsForGameMode's stunt arm via
        // mAptRef.SetFrameTriggerCallback(&BankingTransitionCompleteCallback, this), so
        // it MUST be a plain function pointer: it is STATIC here and recovers `this`
        // from lpUserData, exactly as the X360 body does. (The DecFIGS dump lists it as
        // an ordinary member -- the PS3 dumper drops the `static` keyword; the X360 call
        // is single-argument and never touches an implicit this.) luArg is unused.
        static void BankingTransitionCompleteCallback(void* lpUserData, u16 luArg);

        // ---- member layout (DWARF :135-:272 order, X360 offsets) ------------
        BrnFlapt::MovieClipRef mEventMovieClip;              // :135  X360 +0x00C
        const char*            mpEventInfoComponentName;     // :136  X360 +0x014
        BrnFlapt::FileRef      mHUDFileRef;                  // :137  X360 +0x018
        TextFieldComponentType maTextField[KI_TEXTFIELD_COUNT];  // :140  X360 +0x01C (7 x 0x0C)
        TextFieldComponentType mAddScoreTextField;           // :145  X360 +0x070
        TextFieldComponentType mBankScoreTextField;          // :146  X360 +0x07C
        AnimatorComponentType  mAddScoreAnimator;            // :147  X360 +0x088 "AddScore_anim"
        AnimatorComponentType  mScoreBackgroundAnimator;     // :149  X360 +0x0C0 "ScoreBacking_anim"
        AnimatorComponentType  mTextStateAnimatorRace;       // :150  X360 +0x0F8 "raceTextState_anim"
        AnimatorComponentType  mTextStateAnimatorRRage;      // :151  X360 +0x130 "rRageTextState_anim"
        // X360-ONLY, absent from the DecFIGS DWARF: the 5th animator, constructed with
        // the literal name "TakedownNumbersAnimator" and prepared under that name by
        // PrepareComponentsForGameMode's road-rage arm. SetTakedownsDigitsState drives it.
        AnimatorComponentType  mTakedownNumbersAnimator;     // X360 +0x168 "TakedownNumbersAnimator"
        AnimatorComponentType  mDistanceAnimatorRace;        // :152  X360 +0x1A0 "raceDistance_anim"
        AnimatorComponentType  mTimeAnimatorRRage;           // :153  X360 +0x1D8 "CurrentTimeAnimRRage_cpt"
        AnimatorComponentType  mTimeAnimatorBRoute;          // :154  X360 +0x210 "CurrentTimeAnimBRoute_cpt"
        AnimatorComponentType  mTimeAnimatorStuntRun;        // :155  X360 +0x248 "CurrentTimeAnimStuntRun_cpt"
        AnimatorComponentType  mScoreAnimator;               // :156  X360 +0x280 "Score_anim"
        AnimatorComponentType  mBankingAnimator;             // :157  X360 +0x2B8 "Banking_anim"
        AnimatorComponentType  mStuntAnimator;               // :158  X360 +0x2F0 "StuntBar_anim"
        AnimatorComponentType  mMultiplierAnimator;          // :159  X360 +0x328 "Multiplier_anim"
        InterpolatorComponentType mDistanceInterpolator;     // :161  X360 +0x360 "distanceInter_cpt"

        // ---- the pointer-free scalar run (X360 +0x3D0 .. +0x494) ------------
        f32                    mfDistanceWarningThreshold;   // :199  +0x3D0  1000.0f metric / 1609.344f imperial
        BrnGameState::GameStateModuleIO::EGameModeType meCurrentEventType;      // :205  +0x3D4
        BrnGameState::GameStateModuleIO::EGameModeType meCurrentOnlineGameMode; // :206  +0x3D8
        BrnGameState::ECurrentMedalTargetTime meCurrentTargetMedal;             // :208  +0x3DC
        f32                    mfCurrentTimeSinceLastPositionChange; // :210    +0x3E0
        bool                   mbTimeRemainingFlashing;      // :212  +0x3E4
        s32                    miCurrentPosition;            // :215  +0x3E8
        s32                    miTotalRacers;                // :216  +0x3EC
        BrnGameState::LandmarkIndex mCurrentLandmark;        // :217  +0x3F0 (s16)
        f32                    mfDistToCheckpoint;           // :218  +0x3F4
        bool                   mbNearFinish;                 // :219  +0x3F8
        f32                    mfTimeLeft;                   // :222  +0x3FC
        s32                    miCurrentTakedowns;           // :223  +0x400
        s32                    miTargetTakedowns;            // :224  +0x404
        CgsID                  mRivalCarID;                  // :227  +0x408 (8 bytes -- the std @0x82412D10)
        s32                    miTargetDamageCount;          // :228  +0x410
        s32                    miCurrentDamageCount;         // :229  +0x414
        f32                    mfRivalDistanceFromTarget;    // :230  +0x418
        s32                    miShowTimeCarsCrashed;        // :233  +0x41C
        f32                    mfShowTimeDistanceTravelled;  // :234  +0x420
        f32                    mfTargetTimeInEvent;          // :237  +0x424
        f32                    mfCurrentTimeInEvent;         // :238  +0x428
        s32                    miTargetScoreInEvent;         // :239  +0x42C
        s32                    miCurrentScoreInEvent;        // :240  +0x430
        bool                   mbInShortcut;                 // :241  +0x434
        s32                    miCurrentComboInEvent;        // :244  +0x438
        s32                    miMultiplierInEvent;          // :245  +0x43C
        // X360-ONLY (no DWARF name): the last add-score delta pushed into maTextField[6]
        // by UpdateStuntAttack's online arm; ClearEventSpecificData seeds it to -1.
        s32                    miAddScoreDisplayed;          //       +0x440
        bool                   mbComboWarningActive;         // :246  +0x444
        BrnGameState::StuntToDisplay maDisplayedStunt[1];    // :247  +0x448 (8 bytes)
        f32                    mfTotupStartScore;            // :249  +0x450
        f32                    mfTotupFinishScore;           // :250  +0x454
        f32                    mfTotupStartMultiplier;       // :251  +0x458
        f32                    mfTotupFinishMultiplier;      // :252  +0x45C
        f32                    mfTotupStartTime;             // :253  +0x460
        bool                   mbTottingUp;                  // :255  +0x464
        // X360-ONLY (no DWARF name): the cached KAPC_EVENT_STATE_NAMES index.
        // UpdateStuntAttack compares its freshly computed state against this and only
        // calls MoveAnimation on a change; PrepareComponentsForGameMode's stunt arm and
        // Construct both reset it to 0.
        s32                    miCurrentEventStateIndex;     //       +0x468
        // X360-ONLY (no DWARF name, no reader anywhere in the 26-function set): the only
        // toucher is ClearEventSpecificData's `stw r11, 0x46C(r3)` @0x82412D60. Kept as a
        // named slot so the run stays contiguous and the pins below stay meaningful.
        // [FLAG unattested-purpose member] DELETE-WHEN a consumer is found or a real
        // symbol dump names it.
        s32                    miField_46C;                  //       +0x46C
        bool                   mbFreeBurnBasicInfoShowing;   // :261  +0x470
        bool                   mbFreeBurnInfoShowing;        // :264  +0x471
        BrnGameState::GameStateModuleIO::EGameModeType meNextGameMode; // :265 +0x474
        // BrnNetwork::EBrnGameSecurity (0 public / 1 private / 2 closed). Spelled s32
        // here for the same reason BrnGuiCache.h:1444 does -- the enum has no home header
        // in the tree yet. ClearEventSpecificData seeds 3.
        s32                    meSecurity;                   // :266  +0x478
        s32                    miNumRounds;                  // :267  +0x47C
        CgsID                  mFreeburnChallengeID;         // :270  +0x480 (8 bytes -- the ld/std pair)
        s32                    maiFreeburnChallengeData[2];  // :271  +0x488
        s32                    miFreeburnChallengeDataCount; // :272  +0x490
        // X360-ONLY (no DWARF name): PrepareComponentsForGameMode's road-rage arm (case 3)
        // stores 1 here (`stb`, @0x82429150's arm); ClearEventSpecificData does NOT reset it.
        bool                   mbRoadRageComponentsPrepared; //       +0x494

        // --------------------------------------------------------------------
        // Layout oracle. Member function because the pinned members are private.
        // Only the bounds, the animator ordering and the pointer-free scalar run are
        // asserted -- see the HOST LAYOUT note in the banner.
        // --------------------------------------------------------------------
        static void _AssertLayout()
        {
            // (1) The two array bounds the X360 loop/index arithmetic proves.
            static_assert(sizeof(EventInfoComponent::maTextField)
                        / sizeof(TextFieldComponentType) == 7,
                          "maTextField is [7] on X360 (PrepareComponentsForGameMode loop bound @0x824291B4)");
            static_assert(KI_STUNT_TYPE_STRING_ID_COUNT == 18,
                          "KAC_STUNT_TYPE_STRING_IDS is [18] on X360 (cmplwi 0x12, BrnEventInfo.cpp:1677)");

            // (2) The 13 animators are one contiguous stride-sizeof run, in X360 order.
            static_assert(offsetof(EventInfoComponent, mTextStateAnimatorRace)
                        - offsetof(EventInfoComponent, mAddScoreAnimator)
                        == 2 * sizeof(AnimatorComponentType),
                          "mTextStateAnimatorRace is animator #3 (X360 +0x0F8)");
            static_assert(offsetof(EventInfoComponent, mTakedownNumbersAnimator)
                        - offsetof(EventInfoComponent, mAddScoreAnimator)
                        == 4 * sizeof(AnimatorComponentType),
                          "mTakedownNumbersAnimator is animator #5 (X360 +0x168)");
            static_assert(offsetof(EventInfoComponent, mDistanceAnimatorRace)
                        - offsetof(EventInfoComponent, mTakedownNumbersAnimator)
                        == 1 * sizeof(AnimatorComponentType),
                          "mDistanceAnimatorRace follows the takedown-numbers animator (X360 +0x1A0)");
            static_assert(offsetof(EventInfoComponent, mMultiplierAnimator)
                        - offsetof(EventInfoComponent, mAddScoreAnimator)
                        == 12 * sizeof(AnimatorComponentType),
                          "thirteen animators, contiguous (X360 +0x088..+0x35F)");

            // (3) The pointer-free scalar run, pinned as deltas from +0x3D0. Every value
            //     below is (X360 offset - 0x3D0); ClearEventSpecificData @0x82412CA8 and
            //     Construct @0x82421160 attest each store.
            #define BRN_EVENTINFO_SCALAR_PIN(member, delta)                              \
                static_assert(offsetof(EventInfoComponent, member)                        \
                            - offsetof(EventInfoComponent, mfDistanceWarningThreshold)    \
                            == (delta), "EventInfoComponent scalar run: " #member)

            BRN_EVENTINFO_SCALAR_PIN(meCurrentEventType,                     4);
            BRN_EVENTINFO_SCALAR_PIN(meCurrentOnlineGameMode,                8);
            BRN_EVENTINFO_SCALAR_PIN(meCurrentTargetMedal,                  12);
            BRN_EVENTINFO_SCALAR_PIN(mfCurrentTimeSinceLastPositionChange,  16);
            BRN_EVENTINFO_SCALAR_PIN(mbTimeRemainingFlashing,               20);
            BRN_EVENTINFO_SCALAR_PIN(miCurrentPosition,                     24);
            BRN_EVENTINFO_SCALAR_PIN(miTotalRacers,                         28);
            BRN_EVENTINFO_SCALAR_PIN(mCurrentLandmark,                      32);
            BRN_EVENTINFO_SCALAR_PIN(mfDistToCheckpoint,                    36);
            BRN_EVENTINFO_SCALAR_PIN(mbNearFinish,                          40);
            BRN_EVENTINFO_SCALAR_PIN(mfTimeLeft,                            44);
            BRN_EVENTINFO_SCALAR_PIN(miCurrentTakedowns,                    48);
            BRN_EVENTINFO_SCALAR_PIN(miTargetTakedowns,                     52);
            BRN_EVENTINFO_SCALAR_PIN(mRivalCarID,                           56);
            BRN_EVENTINFO_SCALAR_PIN(miTargetDamageCount,                   64);
            BRN_EVENTINFO_SCALAR_PIN(miCurrentDamageCount,                  68);
            BRN_EVENTINFO_SCALAR_PIN(mfRivalDistanceFromTarget,             72);
            BRN_EVENTINFO_SCALAR_PIN(miShowTimeCarsCrashed,                 76);
            BRN_EVENTINFO_SCALAR_PIN(mfShowTimeDistanceTravelled,           80);
            BRN_EVENTINFO_SCALAR_PIN(mfTargetTimeInEvent,                   84);
            BRN_EVENTINFO_SCALAR_PIN(mfCurrentTimeInEvent,                  88);
            BRN_EVENTINFO_SCALAR_PIN(miTargetScoreInEvent,                  92);
            BRN_EVENTINFO_SCALAR_PIN(miCurrentScoreInEvent,                 96);
            BRN_EVENTINFO_SCALAR_PIN(mbInShortcut,                         100);
            BRN_EVENTINFO_SCALAR_PIN(miCurrentComboInEvent,                104);
            BRN_EVENTINFO_SCALAR_PIN(miMultiplierInEvent,                  108);
            BRN_EVENTINFO_SCALAR_PIN(miAddScoreDisplayed,                  112);
            BRN_EVENTINFO_SCALAR_PIN(mbComboWarningActive,                 116);
            BRN_EVENTINFO_SCALAR_PIN(maDisplayedStunt,                     120);
            BRN_EVENTINFO_SCALAR_PIN(mfTotupStartScore,                    128);
            BRN_EVENTINFO_SCALAR_PIN(mfTotupFinishScore,                   132);
            BRN_EVENTINFO_SCALAR_PIN(mfTotupStartMultiplier,               136);
            BRN_EVENTINFO_SCALAR_PIN(mfTotupFinishMultiplier,              140);
            BRN_EVENTINFO_SCALAR_PIN(mfTotupStartTime,                     144);
            BRN_EVENTINFO_SCALAR_PIN(mbTottingUp,                          148);
            BRN_EVENTINFO_SCALAR_PIN(miCurrentEventStateIndex,             152);
            BRN_EVENTINFO_SCALAR_PIN(miField_46C,                          156);
            BRN_EVENTINFO_SCALAR_PIN(mbFreeBurnBasicInfoShowing,           160);
            BRN_EVENTINFO_SCALAR_PIN(mbFreeBurnInfoShowing,                161);
            BRN_EVENTINFO_SCALAR_PIN(meNextGameMode,                       164);
            BRN_EVENTINFO_SCALAR_PIN(meSecurity,                           168);
            BRN_EVENTINFO_SCALAR_PIN(miNumRounds,                          172);
            BRN_EVENTINFO_SCALAR_PIN(mFreeburnChallengeID,                 176);
            BRN_EVENTINFO_SCALAR_PIN(maiFreeburnChallengeData,             184);
            BRN_EVENTINFO_SCALAR_PIN(miFreeburnChallengeDataCount,         192);
            BRN_EVENTINFO_SCALAR_PIN(mbRoadRageComponentsPrepared,         196);

            #undef BRN_EVENTINFO_SCALAR_PIN
        }
    };
}

#endif // BRN_GUI_EVENT_INFO_COMPONENT_H
