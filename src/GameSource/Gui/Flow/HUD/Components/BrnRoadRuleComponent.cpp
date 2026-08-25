#include "GameSource/Gui/Flow/HUD/Components/BrnRoadRuleComponent.h"

#include <cmath>                                                       // std::sqrt (the VMX rsqrt chain)
#include <cstring>                                                     // strncpy / strlen (X360 inlined copies)

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                // CgsCore::SPrintf
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"        // FormatCurrencyString
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface (OutputGuiEvent / GetLanguageManager)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::GuiEventSoundTrigger (id 22, 100B)
#include "GameSource/Gui/BrnGuiCache.h"                                // GuiCache (mode / score-mode / in-event gate / player name)
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                  // GuiEventRequestTraining (id 572)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                 // GsmIO::EGameModeType (in-event colouring gate)
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                      // BrnFlapt::FileRef::FindComponent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"            // MovieClipInstance::ResetTimeline
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h" // AttachToTextFieldComponent
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptRoadSignIconComponent.h" // FlaptRoadSignIconComponent
#include "SharedClasses/StreetData/BrnStreetData.h"                    // KI_INVALID_ROAD_INDEX (dword_820A766C)

// ============================================================================
// BrnGui::RoadRuleComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// HUD H2 (2026-08-25): the FULL lifecycle/handler set, written from the banked
// decompile+asm of all 28 ledger functions (scratch h2_roadrule_dump.txt) plus the
// closure reads (h2_dump2..8.txt). Bodied here:
//   Construct                 @0x8242ABD0    Prepare                @0x82442170
//   Update                    @0x824356C8    UpdateCurrentTime      @0x8243F540
//   UpdateRoadSignDistances   @0x8242FFE0    UpdateRoadSignDistance @0x8242AFF8
//   HandleEnterRoadEvent      @0x82413DD0    HandleRoadRuleTargetUpdate @0x824355A0
//   HandleRoadRuleBegin       @0x8243EDB8    HandleRoadRuleEnd      @0x8243F098
//   SwitchModes               @0x8243F2D0    InitialiseMode         @0x8243F890
//   EndTimers                 @0x82438078    TransitionComplete     @0x82441550
//   TransitionCompleteCallback @0x82441778   ShowRoadRules          @0x8243F8B8
//   ShowUpcomingRoads         @0x8243FAD8    UpdateUpcomingRoadSign @0x8243FC88
//   GetNameOfRule             @0x82413FF8    GetSignOffsetSizeAdjustment @0x824140F8
//   RefreshBestData           @0x824226B0    GetRoadSignColour      @0x82422918
//   RefreshSignColours        @0x82430050    AnimateCurrentTime     @0x82438298
//   SetUpcomingRoadAnimation  @0x82438598
// plus the earlier slice (IsSameAsCurrentRoad @0x82410458, ShouldUseInEventColouring
// @0x82410568, SetCurrentSignState @0x82410640, UpdateUpcomingRoadLeaders @0x82410720,
// SetCachePointer @0x82473520) and the X360-inlined helpers AnimateCurrentCrash /
// IsCurrentTimeLeaving / IsRoadRuleTime / IsRoadRuleCrash (each inline is attested at
// its call sites -- the DWARF declares all four).
//
// H2 CORRECTION (attested twice): the old SetCurrentSignState slot-mapping FLAG is
// resolved. Prepare binds this+0x0C = "animatedRoadSign_mc" (mRoadSignAnimationsMC),
// +0x14 = "rulesCpts" (mSubComponentPositionsMC); SetCurrentSignState PLAYS the state
// frame on +0x0C and STOPS the size frame on +0x04 (the base mAptRef -- the component
// root clip) and +0x14. HandleEnterRoadEvent @0x82413F68 repeats the same trio. The
// previous body played on mSubComponentPositionsMC and stopped mRoadSignAnimationsMC /
// mLeftSignAlignmentsMC -- fixed to the attested targets.
//
// X360-pinned RoadRuleComponent members these bodies touch (this-relative byte
// offsets from the asm): the four leading clips @+0x0C/+0x14/+0x1C/+0x24 (Prepare's
// binding order), mRoadSign @+0x30, mCentralArrowAnimator @+0x80, mRoadSignLefty
// @+0xC0, mRoadSignRighty @+0x110, mLeftRoadAnimator @+0x160, mRightRoadAnimator
// @+0x198, maRoadWorldPosition @+0x1D0, meCurrentSignState @+0x1F0, the six best
// textfields @+0x1F4..+0x23B, mTimeAnimator @+0x23C, mCrashAnimator @+0x274,
// mTimeLeaderIcon @+0x2AC, mCrashLeaderIcon @+0x2C0, mCurrentTimerField @+0x2E0,
// mCurrentCrashTextField @+0x360, mCurrentTimeAnimator @+0x36C, mCurrentCrashAnimator
// @+0x3A4, meCurrentTimerAnimState @+0x3DC, meCurrentCrashAnimState @+0x3E0,
// mTransitionData @+0x3E8, mUpcomingRoadData @+0x460, mCurrentRunningRoadID @+0x4E0,
// mabRuleActive @+0x4E8, mabRulePending @+0x4EA, mTargetTimeMC @+0x4EC, mTargetCrashMC
// @+0x4F4, mbTimeTargetVisible @+0x4FC, mbCrashTargetVisible @+0x4FD,
// meCurrentlyActiveRule @+0x500, mePreviousActiveRule @+0x504, mpGuiCache @+0x508,
// mbInShowTime @+0x50C, mRoadRulePlayerNameFormatType @+0x510, miParentAptLayerIndex
// @+0x514, mbRoadRulesVisible @+0x518, mbUpcomingRoadsVisible @+0x519,
// mfTargetCrashScore @+0x51C, mfCurrentCrashScore @+0x520, miCrashMultiplier @+0x524.
// ============================================================================

namespace BrnGui
{

namespace
{
    // BrnRoadRuleComponent.h:277 (KI_MAX_ROAD_INDEX) -- the miRoadIndex bound the
    // X360 asserts against (cmpwi 0x42).
    const s32 KI_MAX_ROAD_INDEX = 66;

    // --- SetCurrentSignState animation-frame tables (X360 off_82F24D08 /
    //     off_82F24CF8 / dword_8204BDA8). Contents RECOVERED from the image
    //     2026-08-25 (headless idat read -- scratch h2_roadrule_dump.txt).
    const char* const KAPC_ANIMATION_FRAMES[RoadRuleComponent::E_ANIMATION_STATE_ADVANCED_COUNT] =
    {
        "invisible",        // [0] E_ANIMATION_STATE_INVISIBLE
        "transin",          // [1]
        "idle",             // [2]
        "transout",         // [3]
        "idleFailed",       // [4]
        "transoutFailed",   // [5]
    };
    const char* const KAAC_ROADSIGN_SIZE_FRAME_LABELS[RoadRuleComponent::E_ROAD_SIGN_COUNT] =
    {
        "small",     // [0] E_ROAD_SIGN_SMALL
        "medium1",   // [1]
        "medium2",   // [2]
        "large",     // [3]
    };
    const s32 KAI_ROAD_INDEX_TO_SIGN_SIZE[KI_MAX_ROAD_INDEX] =
    {
        2, 0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 2, 2, 3, 2, 1, 0, 2, 3, 1, 2, 3,
        2, 1, 2, 1, 3, 1, 3, 3, 1, 1, 3, 2, 0, 0, 0, 3, 3, 2, 3, 1, 2, 3,
        1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 1, 2, 3, 2, 3, 2, 2, 2, 2,
    };

    // --- SetUpcomingRoadAnimation frame tables (X360 off_82F24D20 / off_82F24D30,
    //     read off the image 2026-08-25 -- h2_dump2.txt). Indexed by
    //     EUpcomingRoadAnimState; the SUGGESTED table is selected when the road
    //     state is E_ROADSTATE_SUGGESTED.
    const char* const KAPC_UPCOMING_ROAD_FRAMES_NORMAL[RoadRuleComponent::E_UPCOMING_ROAD_ANIM_COUNT] =
    {
        "invisible",      // [0] E_UPCOMING_ROAD_ANIM_INVISIBLE
        "transin",        // [1] E_UPCOMING_ROAD_ANIM_TRANS_IN
        "transout",       // [2] E_UPCOMING_ROAD_ANIM_TRANS_OUT_TAKEN
        "transoutfade",   // [3] E_UPCOMING_ROAD_ANIM_TRANS_OUT_NOT_TAKEN
    };
    const char* const KAPC_UPCOMING_ROAD_FRAMES_SUGGESTED[RoadRuleComponent::E_UPCOMING_ROAD_ANIM_COUNT] =
    {
        "invisible",              // [0]
        "transinSuggested",       // [1]
        "transoutSuggested",      // [2]
        "transoutfadeSuggested",  // [3]
    };

    // --- the road-rule sound triggers (X360 table off_82F277A8; only the two entries
    //     this TU reads are named -- [1] and [7] of the shared 8-name apt-trigger set).
    const char* const KPC_SOUND_COMPONENT  = "B5RoadRuleComponent";
    const char* const KPC_SOUND_ON_LEAVE   = "ON_LEAVE";    // off_82F277A8[1] @0x8206ECF0
    const char* const KPC_SOUND_ON_CHANGE  = "ON_CHANGE";   // off_82F277A8[7] @0x8206ECA8
    const char* const KPC_SOUND_LABEL      = "uninitialised";

    // --- UpdateRoadSignDistance / GetSignOffsetSizeAdjustment constants. The X360
    //     keeps these in .data vectors that are ZERO in the image and filled by the
    //     0x82C50xxx static-init thunks -- values recovered from those thunks
    //     2026-08-25 (h2_dump4/5.txt). All are lane-splat vectors; the scalar is the
    //     whole truth.
    const f32 KF_SIGN_DISTANCE_MIN        = 50.0f;    // unk_82FB2D10 (subtract)
    const f32 KF_SIGN_DISTANCE_CLAMP      = 100.0f;   // unk_82FB29F0 (max band)
    const f32 KF_SIGN_ONE_OVER_CLAMP      = 0.01f;    // unk_82FB2C10 (init thunk computes 1/100)
    const f32 KF_SIGN_DISTANCE_SCALE      = 300.0f;   // unk_82FB2CF0
    const f32 KF_SIGN_POSITION_Y          = 50.0f;    // unk_82FB2AD0 (vperm ctrl 82CDA350 takes its .y)
    const f32 KAF_SIGN_SIDE_BASE[2]       = { 0.0f, -160.0f };   // unk_82FB2AA0 [side]
    // [2*sizeIndex + side] -- the per-size x-offsets (splat lanes).
    const f32 KAF_CURRENT_SIGN_OFFSETS[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 4.0f, 4.0f, 5.0f, 6.5f };   // unk_82FB2920
    const f32 KAF_TURNING_SIGN_OFFSETS[8] = { 92.0f, 0.0f, 56.0f, -2.0f, 33.0f, 0.0f, -18.0f, 0.0f };   // unk_82FB2A10

    // The float-comparison epsilon the X360 loads for the "did anything move" gates
    // (flt_8204B630).
    const f32 KF_EPSILON = 1.1920928955078125e-07f;

    // The EXIT road's CgsID (the X360 builds the literal inline in GetRoadSignColour
    // @0x82422A80; same constant as the road-sign component's special case).
    const CgsID KID_ROAD_EXIT = 0x684561A168000000ULL;

    // CgsStringUtils.h:55 -- the X360's inlined fixed-buffer string copy: measure,
    // assert "String too long: ", strncpy 32. Reproduced as the file-local helper the
    // three sound-trigger sites and the fail-event site share.
    void CopyFixedString32(char* lpacDest, const char* lpcSource)
    {
        CGS_ASSERT(std::strlen(lpcSource) < 32u, "String too long: ");   // CgsStringUtils.h:55 (non-gating)
        std::strncpy(lpacDest, lpcSource, 32);
    }

    // A lane-splat VecFloat (the X360 vcfsx / lvlx+vspltw idiom; VecFloat here is the
    // four-lane shim, so all lanes carry the scalar).
    VecFloat MakeVecFloatSplat(f32 lfValue)
    {
        VecFloat lvfSplat;
        lvfSplat.x = lvfSplat.y = lvfSplat.z = lvfSplat.w = lfValue;
        return lvfSplat;
    }
}

// ---------------------------------------------------------------------------
// The X360-inlined sound-event emitter shared by AnimateCurrentTime (:2018),
// EndTimers (:1550) and UpdateCurrentTime (:1357): build the 100-byte
// GuiEventSoundTrigger { "B5RoadRuleComponent", <trigger>, "uninitialised", layer }
// and push it through OutputGuiEvent (X360 AddEvent {100, 22, 12}, ch40).
// ---------------------------------------------------------------------------
static void SendRoadRuleSoundTrigger(CgsGui::StateInterface* lpStateInterface,
                                     s32 liParentAptLayerIndex,
                                     const char* lpcTrigger)
{
    CGS_ASSERT(liParentAptLayerIndex != -1,
               "Cannot send audio event - no valid flash layer supplied when constructed");   // (non-gating)

    CgsGui::GuiEventSoundTrigger lSound;
    CopyFixedString32(lSound.macTypeName,   KPC_SOUND_COMPONENT);
    CopyFixedString32(lSound.macActionName, lpcTrigger);
    CopyFixedString32(lSound.macLabel,      KPC_SOUND_LABEL);
    lSound.miLayer = liParentAptLayerIndex;
    lpStateInterface->OutputGuiEvent(lSound);
}

// ---------------------------------------------------------------------------
// X360-inlined rule-family classifiers (DWARF :570/:574; every call site inlines
// the same {1,2} / {3,4} pair -- e.g. SwitchModes @0x8243F39C.., GetRoadSignColour
// @0x82422930..).
// ---------------------------------------------------------------------------
bool RoadRuleComponent::IsRoadRuleTime(BrnGameState::EActiveRoadRule leRule) const
{
    return (static_cast<s32>(leRule) == 1) || (static_cast<s32>(leRule) == 2);
}

bool RoadRuleComponent::IsRoadRuleCrash(BrnGameState::EActiveRoadRule leRule) const
{
    return (static_cast<s32>(leRule) == 3) || (static_cast<s32>(leRule) == 4);
}

// X360-inlined (DWARF :521; the {TRANSOUT, UNSUCCESSFUL_IDLE, UNSUCCESSFUL_TRANSOUT}
// test EndTimers / AnimateCurrentTime / UpdateCurrentTime / TransitionComplete share).
bool RoadRuleComponent::IsCurrentTimeLeaving()
{
    return meCurrentTimerAnimState == E_ANIMATION_STATE_TRANSOUT
        || meCurrentTimerAnimState == E_ANIMATION_STATE_UNSUCCESSFUL_IDLE
        || meCurrentTimerAnimState == E_ANIMATION_STATE_UNSUCCESSFUL_TRANSOUT;
}

// @ 0x8242ABD0 -- construct every sub-component and reset the panel state.
void RoadRuleComponent::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                  const char* lpacParentName, s32 liParentAptLayerIndex)
{
    (void)lpacName;
    (void)lpacParentName;

    // Base init (inlined): bind the state channel, clear the root clip and the four
    // leading child-clip handles (the X360 zeroes +0x04..+0x2B in one run).
    CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");   // BrnGuiFlaptComponent.h:113 (non-gating)
    mpStateInterface = lpStateInterface;
    mAptRef.SetInvalid();
    mRoadSignAnimationsMC.SetInvalid();
    mSubComponentPositionsMC.SetInvalid();
    mLeftSignAlignmentsMC.SetInvalid();
    mRightSignAlignmentsMC.SetInvalid();

    // The sign / animator sub-components, in the X360 construction order.
    mRoadSign.Construct("roadSign_mc", lpStateInterface, 0);
    mCentralArrowAnimator.Construct("arrowAnim_cpt", lpStateInterface, 0);
    mRoadSignLefty.Construct("roadSignLeft_mc", lpStateInterface, 0);
    mRoadSignRighty.Construct("roadSignRight_mc", lpStateInterface, 0);
    mLeftRoadAnimator.Construct("leftyAnim_mc", lpStateInterface, 0);
    mRightRoadAnimator.Construct("rightyAnim_mc", lpStateInterface, 0);

    // meCurrentSignState + the six best-data text fields (X360 zero run +0x1F0..+0x23B).
    meCurrentSignState = E_ANIMATION_STATE_INVISIBLE;
    mBestCrashLabelTextField.SetInvalid();
    mBestCrashNameTextField.SetInvalid();
    mBestCrashValueTextField.SetInvalid();
    mBestTimeLabelTextField.SetInvalid();
    mBestTimeNameTextField.SetInvalid();
    mBestTimeValueTextField.SetInvalid();

    mTimeLeaderIcon.Construct("timeIcon_mc", lpStateInterface, 0);
    mCrashLeaderIcon.Construct("crashIcon_mc", lpStateInterface, 0);
    mTimeAnimator.Construct("TimeTarget_anim", mpStateInterface, 0);
    mCrashAnimator.Construct("CrashTarget_anim", mpStateInterface, 0);

    // The running-timer field: counting UP, then the X360 pokes its colour pair --
    // safe {1.0, 0.8, 0.0} (255,204,0 amber) and danger {0.6, 16/255, 16/255} (dark
    // red), leaving the w lanes alone.
    mCurrentTimerField.Construct("CurrentTime_mc", mpStateInterface,
                                 FlaptTimerFieldComponent::E_TIMER_MODE_COUNTING_UP, 0);
    mCurrentCrashTextField.SetInvalid();
    mCurrentTimeAnimator.Construct("CurrentTime_anim", mpStateInterface, 0);
    mCurrentCrashAnimator.Construct("CurrentCrash_anim", mpStateInterface, 0);
    mCurrentTimerField.SetSafeColours(255, 204, 0);
    mCurrentTimerField.SetDangerColours(153, 16, 16);

    miParentAptLayerIndex = liParentAptLayerIndex;
    mfCurrentCrashScore   = -1.0f;   // X360 +0x520
    mbInShowTime          = false;   // X360 +0x50C
    mfTargetCrashScore    = -1.0f;   // X360 +0x51C
    miCrashMultiplier     = 1;       // X360 +0x524

    mTransitionData.Construct();     // @0x824F60B8
    mUpcomingRoadData.Construct();   // @0x824F6108
    mCurrentTimerField.SetTime(0.0f);

    mCurrentRunningRoadID = 0;
    for (s32 liRule = 0; liRule < 2; ++liRule)
    {
        mabRuleActive[liRule]  = false;
        mabRulePending[liRule] = false;
    }
    meCurrentTimerAnimState = E_ANIMATION_STATE_INVISIBLE;   // X360 +0x3DC (zero run)
    meCurrentCrashAnimState = E_ANIMATION_STATE_INVISIBLE;   // X360 +0x3E0
    maRoadWorldPosition[0].SetZero();   // X360 stvx zero @+0x1D0
    maRoadWorldPosition[1].SetZero();   // X360 stvx zero @+0x1E0
    mTargetTimeMC.SetInvalid();
    mTargetCrashMC.SetInvalid();
    mbTimeTargetVisible  = false;
    mbCrashTargetVisible = false;
    meCurrentlyActiveRule = static_cast<BrnGameState::EActiveRoadRule>(0);
    mePreviousActiveRule  = static_cast<BrnGameState::EActiveRoadRule>(0);
    mpGuiCache = 0;
    mRoadRulePlayerNameFormatType =
        static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(9);   // X360 +0x510 := 9 (E_FORMAT_ID_LOOKUP)
    mbRoadRulesVisible     = false;   // X360 stb 0 @+0x518
    mbUpcomingRoadsVisible = true;    // X360 stb 1 @+0x519
}

// @ 0x82442170 -- bind the component clip tree out of the loaded flapt file. Child
// names are composed "<lacName>_<child>"; the frame-trigger callback routes the
// timeline labels back into TransitionComplete.
void RoadRuleComponent::Prepare(const char* lpacName, const BrnFlapt::FileRef& lrFile)
{
    CGS_ASSERT(lpacName != 0, "lacName != NULL");   // BrnGuiFlaptComponent.h:133 (non-gating)

    lrFile.FindComponent(&mAptRef, lpacName);
    CGS_ASSERT(mAptRef.mpMovieClipInst != 0, "mpMovieClipInst");   // BrnFlaptMovieClipRef.h:272 (non-gating)
    mAptRef.mpMovieClipInst->ResetTimeline();
    mAptRef.SetFrameTriggerCallback(
        reinterpret_cast<void*>(&RoadRuleComponent::TransitionCompleteCallback), this);

    mAptRef.FindChildMovieClip(&mRoadSignAnimationsMC, "animatedRoadSign_mc");
    mAptRef.FindChildMovieClip(&mSubComponentPositionsMC, "rulesCpts");

    // The two positioning clips live under rulesCpts/upcoming{Left,Right}_mc.
    {
        BrnFlapt::MovieClipRef lUpcomingLeft;
        mSubComponentPositionsMC.FindChildMovieClip(&lUpcomingLeft, "upcomingLeft_mc");
        lUpcomingLeft.FindChildMovieClip(&mLeftSignAlignmentsMC, "leftPositions_mc");

        BrnFlapt::MovieClipRef lUpcomingRight;
        mSubComponentPositionsMC.FindChildMovieClip(&lUpcomingRight, "upcomingRight_mc");
        lUpcomingRight.FindChildMovieClip(&mRightSignAlignmentsMC, "rightPositions_mc");
    }

    char lacChildName[128];
    #define BRN_RR_CHILD(lpcChild)                                              \
        CgsCore::SnPrintf(lacChildName, 128, "%s_%s", lpacName, (lpcChild))

    BRN_RR_CHILD("roadSign_mc");        mRoadSign.Prepare(lacChildName, lrFile);
    BRN_RR_CHILD("arrowAnim_cpt");      mCentralArrowAnimator.Prepare(lacChildName, lrFile, 0);
    BRN_RR_CHILD("roadSignLeft_mc");    mRoadSignLefty.Prepare(lacChildName, lrFile);
    BRN_RR_CHILD("roadSignRight_mc");   mRoadSignRighty.Prepare(lacChildName, lrFile);
    BRN_RR_CHILD("leftyAnim_mc");       mLeftRoadAnimator.Prepare(lacChildName, lrFile, 0);
    BRN_RR_CHILD("rightyAnim_mc");      mRightRoadAnimator.Prepare(lacChildName, lrFile, 0);

    AttachToTextFieldComponent(&mBestCrashLabelTextField, "targetCrashText_txt", "targetCrashText_cpt", lpacName, lrFile);
    AttachToTextFieldComponent(&mBestCrashNameTextField,  "CrashName_txt",       "CrashNameTxt_mc",     lpacName, lrFile);
    AttachToTextFieldComponent(&mBestCrashValueTextField, "CrashValue_txt",      "CrashValueTxt_mc",    lpacName, lrFile);
    AttachToTextFieldComponent(&mBestTimeLabelTextField,  "targetTimeText_txt",  "targetTimeText_cpt",  lpacName, lrFile);
    AttachToTextFieldComponent(&mBestTimeNameTextField,   "TimeName_txt",        "TimeNameTxt_mc",      lpacName, lrFile);
    AttachToTextFieldComponent(&mBestTimeValueTextField,  "TimeValue_txt",       "TimeValueTxt_mc",     lpacName, lrFile);

    BRN_RR_CHILD("TimeTarget_anim");    mTimeAnimator.Prepare(lacChildName, lrFile, 0);
    mTimeAnimator.Run("invisible");
    BRN_RR_CHILD("CrashTarget_anim");   mCrashAnimator.Prepare(lacChildName, lrFile, 0);
    mCrashAnimator.Run("invisible");
    BRN_RR_CHILD("timeIcon_mc");        mTimeLeaderIcon.Prepare(lacChildName, lrFile, 0);
    BRN_RR_CHILD("crashIcon_mc");       mCrashLeaderIcon.Prepare(lacChildName, lrFile, 0);
    BRN_RR_CHILD("CurrentTime_mc");     mCurrentTimerField.Prepare("TimerText_txt", lacChildName, lrFile);

    AttachToTextFieldComponent(&mCurrentCrashTextField, "CrashText_txt", "CrashText_mc", lpacName, lrFile);

    BRN_RR_CHILD("CurrentTime_anim");   mCurrentTimeAnimator.Prepare(lacChildName, lrFile, 0);
    BRN_RR_CHILD("CurrentCrash_anim");  mCurrentCrashAnimator.Prepare(lacChildName, lrFile, 0);
    #undef BRN_RR_CHILD

    mSubComponentPositionsMC.FindChildMovieClip(&mTargetTimeMC, "TimeTarget_mc");
    mSubComponentPositionsMC.FindChildMovieClip(&mTargetCrashMC, "CrashTarget_mc");
    mTargetTimeMC.GotoAndPlayLabel("invisible");
    mTargetCrashMC.GotoAndPlayLabel("invisible");
}

// @ 0x824356C8 -- the per-frame tick: ease the shown crash score toward its target,
// refresh the crash readout, and re-derive the leader/best set when the active rule
// changed. The timestep argument arrives (the X360 passes the cache time in f1) but
// the body never reads it -- the crash-score easing is per-CALL, as shipped.
void RoadRuleComponent::Update(f32 lfTimeStep)
{
    (void)lfTimeStep;

    const f32 lfDelta = mfTargetCrashScore - mfCurrentCrashScore;
    const bool lbEqual = (std::fabs(lfDelta) <= KF_EPSILON) && (std::fabs(lfDelta) >= -KF_EPSILON);
    if (!lbEqual)
    {
        if (std::fabs(lfDelta) < 10.0f)
        {
            mfCurrentCrashScore = mfTargetCrashScore;
        }
        else if (std::fabs(lfDelta) * 0.04f /* flt_8204C034 */ < 10.0f)
        {
            mfCurrentCrashScore = mfCurrentCrashScore + 10.0f;
        }
        else
        {
            mfCurrentCrashScore = lfDelta * 0.04f + mfCurrentCrashScore;
        }

        if (mabRuleActive[BrnStreetData::E_SCORE_TYPE_CRASH])
        {
            const s32 liScore = static_cast<s32>(mfCurrentCrashScore);   // fctiwz
            char lacCurrency[64];
            CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
            lpLanguageManager->FormatCurrencyString(lacCurrency, liScore, 32);

            char lacText[32];
            if (miCrashMultiplier > 1)
                CgsCore::SPrintf(lacText, 31, "%s x %d", lacCurrency, miCrashMultiplier);
            else
                CgsCore::SPrintf(lacText, 31, "%s", lacCurrency);
            lacText[31] = '\0';
            mCurrentCrashTextField.SetText(lacText, false);
        }
    }

    if (mePreviousActiveRule != meCurrentlyActiveRule
        && mTransitionData.mRoadId != 0
        && mCurrentRunningRoadID != 0)
    {
        const s32 liScoreMode = mpGuiCache->GetActiveRoadRuleScoringMode();
        for (s32 liType = 0; liType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liType)
        {
            CGS_ASSERT(liType <= BrnStreetData::E_SCORE_TYPE_COUNT,
                       "leEnumIndex <= E_SCORE_TYPE_COUNT");   // BrnChallengeData.h:56 (non-gating)
            if (liScoreMode == 1)   // E_ROAD_PANEL_MODE_ONLINE
            {
                mTransitionData.maiBestValues[liType]         = mTransitionData.maiBestOnlineValues[liType];
                mTransitionData.maeRoadRuleLeaderType[liType] = mTransitionData.maeOnlineRoadRuleLeaderType[liType];
            }
            else
            {
                mTransitionData.maiBestValues[liType]         = mTransitionData.maiBestOfflineValues[liType];
                mTransitionData.maeRoadRuleLeaderType[liType] = mTransitionData.maeOfflineRoadRuleLeaderType[liType];
            }
        }
        RefreshBestData();
        RefreshSignColours();
        mePreviousActiveRule = meCurrentlyActiveRule;
    }
}

// @ 0x8243F540 -- push the running rule time into the timer field and fire the
// band-crossing side effects: leaving the safe band sends the ON_CHANGE sound;
// entering the danger band emits GuiEventRoadRuleFail + the unsuccessful-idle
// animation.
void RoadRuleComponent::UpdateCurrentTime(f32 lfTime)
{
    if (!mabRuleActive[BrnStreetData::E_SCORE_TYPE_TIME])
        return;

    const bool lbWasSafe      = mCurrentTimerField.IsTimeSafe();
    const bool lbWasDangerous = mCurrentTimerField.IsTimeDangerous();
    mCurrentTimerField.SetTime(lfTime);

    if (lbWasSafe && !mCurrentTimerField.IsTimeSafe())
    {
        // :1357 -- "Cannot send audio event ..." assert + the ON_CHANGE trigger.
        SendRoadRuleSoundTrigger(mpStateInterface, miParentAptLayerIndex, KPC_SOUND_ON_CHANGE);
    }

    if (!lbWasDangerous && mCurrentTimerField.IsTimeDangerous())
    {
        GuiEventRoadRuleFail lFail;
        lFail.mRoadID    = mTransitionData.mRoadId;
        lFail.meRuleType = BrnStreetData::E_SCORE_TYPE_TIME;
        lFail.mbRoadRuledByLocalPlayer =
            (mTransitionData.maeRoadRuleLeaderType[0] == E_ROADRULELEADERTYPE_PLAYER);
        lFail.mbRoadRuledByAI =
            (mTransitionData.maeRoadRuleLeaderType[0] == E_ROADRULELEADERTYPE_AI);

        char lacRuleName[32];
        GetNameOfRule(lacRuleName, 32, &mTransitionData, BrnStreetData::E_SCORE_TYPE_TIME);
        CopyFixedString32(lFail.macCurrentRulerName, lacRuleName);
        lFail.macCurrentRulerName[31] = '\0';
        mpStateInterface->OutputGuiEvent(lFail);

        if (!IsCurrentTimeLeaving())
            AnimateCurrentTime(E_ANIMATION_STATE_UNSUCCESSFUL_IDLE);
    }
}

// @ 0x8242FFE0 -- reposition both upcoming signs against the camera: the RIGHT sign
// slides with a +1 lane splat, the LEFT with -1 (the X360 vcfsx(+1)/vcfsx(-1) pair).
void RoadRuleComponent::UpdateRoadSignDistances(Vector3 lv3CameraPosition)
{
    UpdateRoadSignDistance(lv3CameraPosition, GuiEventRoadRuleUpcomingRoads::E_ROAD_RIGHT,
                           MakeVecFloatSplat(1.0f), &mRoadSignRighty);
    UpdateRoadSignDistance(lv3CameraPosition, GuiEventRoadRuleUpcomingRoads::E_ROAD_LEFT,
                           MakeVecFloatSplat(-1.0f), &mRoadSignLefty);
}

// @ 0x8242AFF8 -- slide one upcoming sign by the camera's horizontal distance to the
// junction: t = clamp(dist - 50, 0, 100); x = sideBase + adj * (t * 0.01 * 300) +
// sizeAdjustment; the y lane is the constant 50 (the vperm splice of unk_82FB2AD0).
// A degenerate distance (|distSq| inside the epsilon band) skips the whole move.
void RoadRuleComponent::UpdateRoadSignDistance(Vector3 lv3CameraPosition,
                                               GuiEventRoadRuleUpcomingRoads::ERoadSide leSide,
                                               VecFloat lvfAdjustment,
                                               RoadRulesRoadSign* lpSign)
{
    // pos - maRoadWorldPosition[side], with the height lane zeroed (vrlimi mask 4).
    const f32 lfDx = lv3CameraPosition.x - maRoadWorldPosition[leSide].x;
    const f32 lfDz = lv3CameraPosition.z - maRoadWorldPosition[leSide].z;
    const f32 lfDistSq = lfDx * lfDx + lfDz * lfDz;

    // The X360 vcmpgtfp gate: |distSq| must exceed the epsilon or nothing moves.
    if (!(std::fabs(lfDistSq) > KF_EPSILON))
        return;

    const f32 lfDistance = (lfDistSq == 0.0f) ? 0.0f : std::sqrt(lfDistSq);   // vrsqrte + vsel

    f32 lfBand = lfDistance - KF_SIGN_DISTANCE_MIN;
    if (lfBand < 0.0f)
        lfBand = 0.0f;
    if (lfBand > KF_SIGN_DISTANCE_CLAMP)
        lfBand = KF_SIGN_DISTANCE_CLAMP;

    const VecFloat lvfSizeAdjustment = GetSignOffsetSizeAdjustment(leSide);

    if (lpSign->GetMovieClipRef().mpMovieClipInst != 0)
    {
        const f32 lfSlide = lfBand * KF_SIGN_ONE_OVER_CLAMP * KF_SIGN_DISTANCE_SCALE;
        const f32 lfX = KAF_SIGN_SIDE_BASE[leSide]
                      + lvfAdjustment.x * lfSlide
                      + lvfSizeAdjustment.x;
        Vector2 lv2Position;
        lv2Position.x = lfX;
        lv2Position.y = KF_SIGN_POSITION_Y;
        lv2Position.z = 0.0f;
        lv2Position.w = 0.0f;
        lpSign->GetMovieClipRef().SetPosition(lv2Position);
    }
}

// @ 0x824140F8 -- the x-offset a sign picks up from its own size class plus the
// current road's size class (both through KAI_ROAD_INDEX_TO_SIGN_SIZE; a turning
// index of -2 forces size class 2, -1 zeroes the whole adjustment).
VecFloat RoadRuleComponent::GetSignOffsetSizeAdjustment(GuiEventRoadRuleUpcomingRoads::ERoadSide leSide)
{
    CGS_ASSERT(leSide == GuiEventRoadRuleUpcomingRoads::E_ROAD_LEFT
                   || leSide == GuiEventRoadRuleUpcomingRoads::E_ROAD_RIGHT,
               "leRoadSide == GuiEventRoadRuleUpcomingRoads::E_ROAD_LEFT || leRoadSide == GuiEventRoadRuleUpcomingRoads::E_ROAD_RIGHT");   // :1779 (non-gating)

    const s32 liTurningIndex = mUpcomingRoadData.maiTurningRoadIndices[leSide];
    if (liTurningIndex == BrnStreetData::KI_INVALID_ROAD_INDEX)
        return MakeVecFloatSplat(0.0f);

    CGS_ASSERT(mUpcomingRoadData.miCurrentRoadIndex >= 0,
               "mUpcomingRoadData.miCurrentRoadIndex >= 0");                      // :1787 (non-gating)
    CGS_ASSERT(liTurningIndex < KI_MAX_ROAD_INDEX,
               "mUpcomingRoadData.maiTurningRoadIndices[leRoadSide] < KI_MAX_ROAD_INDEX");   // :1789 (non-gating)
    CGS_ASSERT(mUpcomingRoadData.miCurrentRoadIndex < KI_MAX_ROAD_INDEX,
               "mUpcomingRoadData.miCurrentRoadIndex < KI_MAX_ROAD_INDEX");       // :1790 (non-gating)

    s32 liTurningSize;
    if (liTurningIndex == -2)
    {
        liTurningSize = 2;
    }
    else
    {
        CGS_ASSERT(liTurningIndex >= 0,
                   "mUpcomingRoadData.maiTurningRoadIndices[leRoadSide] >= 0");   // :1799 (non-gating)
        liTurningSize = KAI_ROAD_INDEX_TO_SIGN_SIZE[liTurningIndex];
    }
    const s32 liCurrentSize = KAI_ROAD_INDEX_TO_SIGN_SIZE[mUpcomingRoadData.miCurrentRoadIndex];

    return MakeVecFloatSplat(KAF_TURNING_SIGN_OFFSETS[2 * liTurningSize + leSide]
                           + KAF_CURRENT_SIGN_OFFSETS[2 * liCurrentSize + leSide]);
}

// @ 0x82413DD0 -- adopt a freshly-entered road: copy the payload, re-derive the
// active leader/best pairs for the current scoring mode, and if a road is already
// running (and not already transitioning out) start the sign's transition-out so the
// frame trigger re-enters via TransitionComplete(1) -> EnterRoad.
void RoadRuleComponent::HandleEnterRoadEvent(const GuiEventRoadRuleEnter* lpEvent)
{
    CGS_ASSERT(lpEvent != 0, "lpEvent");   // :499 (non-gating)

    if (lpEvent->mRoadId == mTransitionData.mRoadId)
        return;

    mTransitionData = *lpEvent;   // X360 memcpy 0x70

    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :509 (non-gating)
    const s32 liScoreMode = mpGuiCache->GetActiveRoadRuleScoringMode();

    for (s32 liType = 0; liType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liType)
    {
        CGS_ASSERT(liType <= BrnStreetData::E_SCORE_TYPE_COUNT,
                   "leEnumIndex <= E_SCORE_TYPE_COUNT");   // BrnChallengeData.h:56 (non-gating)
        if (liScoreMode == 1)   // E_ROAD_PANEL_MODE_ONLINE
        {
            mTransitionData.maiBestValues[liType]         = mTransitionData.maiBestOnlineValues[liType];
            mTransitionData.maeRoadRuleLeaderType[liType] = lpEvent->maeOnlineRoadRuleLeaderType[liType];
        }
        else
        {
            mTransitionData.maiBestValues[liType]         = mTransitionData.maiBestOfflineValues[liType];
            mTransitionData.maeRoadRuleLeaderType[liType] = lpEvent->maeOfflineRoadRuleLeaderType[liType];
        }
    }

    if (mTransitionData.mRoadId != 0 && meCurrentSignState != E_ANIMATION_STATE_TRANSOUT)
    {
        mRoadSignAnimationsMC.GotoAndPlayLabel(KAPC_ANIMATION_FRAMES[E_ANIMATION_STATE_TRANSOUT]);

        CGS_ASSERT(mTransitionData.miRoadIndex < KI_MAX_ROAD_INDEX,
                   "mTransitionData.miRoadIndex < KI_MAX_ROAD_INDEX");   // BrnRoadRuleComponent.h:767 (non-gating)
        const char* lpcSizeFrame =
            KAAC_ROADSIGN_SIZE_FRAME_LABELS[KAI_ROAD_INDEX_TO_SIGN_SIZE[mTransitionData.miRoadIndex]];
        mAptRef.GotoAndStopLabel(lpcSizeFrame);
        mSubComponentPositionsMC.GotoAndStopLabel(lpcSizeFrame);

        meCurrentSignState = E_ANIMATION_STATE_TRANSOUT;
    }
}

// @ 0x824355A0 -- adopt refreshed target scores for the running road (online scoring
// only): the active leader words, the active best values and the two 16-byte friend
// names, then re-render the best rows + colours.
void RoadRuleComponent::HandleRoadRuleTargetUpdate(const GuiEventRoadRuleUpdateTargetScores* lpEvent)
{
    CGS_ASSERT(lpEvent != 0, "lpEvent");   // :550 (non-gating)

    if (lpEvent->mRoadId != mTransitionData.mRoadId)
        return;

    const s32 liScoreMode = mpGuiCache->GetActiveRoadRuleScoringMode();
    CGS_ASSERT(liScoreMode != 2,
               "GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_COUNT != meRoadRuleScoreMode");   // BrnGuiCache.h:4210 (non-gating)

    if (liScoreMode == 1)   // E_ROAD_PANEL_MODE_ONLINE
    {
        for (s32 liType = 0; liType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liType)
        {
            CGS_ASSERT(liType <= BrnStreetData::E_SCORE_TYPE_COUNT,
                       "leEnumIndex <= E_SCORE_TYPE_COUNT");   // BrnChallengeData.h:56 (non-gating)
            mTransitionData.maeRoadRuleLeaderType[liType] = lpEvent->maeRoadRuleLeaderType[liType];
            mTransitionData.maFriendLeader[liType]        = lpEvent->maFriendLeader[liType];   // 16-byte name copy
            mTransitionData.maiBestValues[liType]         = lpEvent->maiBestValues[liType];
        }
    }
    RefreshBestData();
    RefreshSignColours();
}

// @ 0x8243EDB8 -- a rule went live. The time rule (type 0) only runs OUTSIDE
// showtime; the crash rule (type 1) only INSIDE it. With no running road the begin is
// parked in mabRulePending for EnterRoad to replay.
void RoadRuleComponent::HandleRoadRuleBegin(BrnStreetData::ScoreType leRuleType)
{
    CGS_ASSERT(leRuleType < BrnStreetData::E_SCORE_TYPE_COUNT, "Invalid rule type ( )");   // :589 (non-gating)

    if (mabRuleActive[leRuleType])
        return;

    if (leRuleType == BrnStreetData::E_SCORE_TYPE_CRASH)
    {
        if (!mbInShowTime)
            return;

        if (mCurrentRunningRoadID != 0)
        {
            AnimateCurrentCrash(E_ANIMATION_STATE_TRANSIN);
            mabRuleActive[BrnStreetData::E_SCORE_TYPE_CRASH] = true;
        }
        else
        {
            mabRulePending[BrnStreetData::E_SCORE_TYPE_CRASH] = true;
        }

        mfTargetCrashScore  = -1.0f;
        mfCurrentCrashScore = -1.0f;

        // The X360 inlines SetUpcomingRoadAnimation(state, animator,
        // TRANS_OUT_NOT_TAKEN) for both sides -- entering showtime fades the
        // upcoming signs out.
        if (mUpcomingRoadData.meRoadStates[0] != GuiEventRoadRuleUpcomingRoads::E_ROADSTATE_NORMAL)
            SetUpcomingRoadAnimation(mUpcomingRoadData.meRoadStates[0], &mLeftRoadAnimator,
                                     E_UPCOMING_ROAD_ANIM_TRANS_OUT_NOT_TAKEN);
        if (mUpcomingRoadData.meRoadStates[1] != GuiEventRoadRuleUpcomingRoads::E_ROADSTATE_NORMAL)
            SetUpcomingRoadAnimation(mUpcomingRoadData.meRoadStates[1], &mRightRoadAnimator,
                                     E_UPCOMING_ROAD_ANIM_TRANS_OUT_NOT_TAKEN);
    }
    else   // E_SCORE_TYPE_TIME
    {
        if (mbInShowTime)
            return;

        if (IsRoadRuleTime(meCurrentlyActiveRule))
        {
            if (mCurrentRunningRoadID != 0)
            {
                AnimateCurrentTime(E_ANIMATION_STATE_TRANSIN);
                mabRuleActive[BrnStreetData::E_SCORE_TYPE_TIME] = true;
            }
            else
            {
                mabRulePending[BrnStreetData::E_SCORE_TYPE_TIME] = true;
            }
        }
    }
}

// @ 0x8243F098 -- a rule ended. The time arm animates the result band and, on a
// successful faster run of the SAME road, adopts the new best (ms) + flips the
// leader to the player; the crash arm transitions the crash readout out. Both clear
// the active/pending flags.
void RoadRuleComponent::HandleRoadRuleEnd(const GuiEventRoadRuleEnd* lpEvent)
{
    CGS_ASSERT(lpEvent != 0, "lpEvent");   // :670 (non-gating)
    CGS_ASSERT(lpEvent->meRuleType < BrnStreetData::E_SCORE_TYPE_COUNT, "Invalid rule type ( )");   // :671 (non-gating)

    const s32 liRuleType = static_cast<s32>(lpEvent->meRuleType);
    if (!mabRuleActive[liRuleType])
        return;

    if (liRuleType == BrnStreetData::E_SCORE_TYPE_TIME)
    {
        if (!mbInShowTime)
        {
            AnimateCurrentTime(mCurrentTimerField.IsTimeDangerous()
                                   ? E_ANIMATION_STATE_UNSUCCESSFUL_TRANSOUT
                                   : E_ANIMATION_STATE_TRANSOUT);

            if (lpEvent->mbScoreAttempt)
            {
                const f32 lfNewMs = lpEvent->mfScore * 1000.0f;
                if (lfNewMs < static_cast<f32>(mTransitionData.maiBestValues[BrnStreetData::E_SCORE_TYPE_TIME])
                    && lpEvent->mRoadId == mTransitionData.mRoadId)
                {
                    mTransitionData.maiBestValues[BrnStreetData::E_SCORE_TYPE_TIME] =
                        static_cast<s32>(lfNewMs);   // fctiwz
                    mTransitionData.maeRoadRuleLeaderType[BrnStreetData::E_SCORE_TYPE_TIME] =
                        E_ROADRULELEADERTYPE_PLAYER;
                    RefreshSignColours();
                    RefreshBestData();
                }
            }
        }
    }
    else   // crash
    {
        if (mbInShowTime)
            AnimateCurrentCrash(E_ANIMATION_STATE_TRANSOUT);
    }

    mabRuleActive[liRuleType]  = false;
    mabRulePending[liRuleType] = false;
}

// @ 0x8243F2D0 -- switch the panel between the rule families (0 none / 1,2 time /
// 3,4 crash): transition the outgoing family's target row out, clear the rule flags,
// then (with a running road) transition the incoming row in and re-colour.
void RoadRuleComponent::SwitchModes(BrnGameState::EActiveRoadRule leRule)
{
    CGS_ASSERT(static_cast<s32>(leRule) >= 0 && static_cast<s32>(leRule) < 5,
               "Invalid rule type - ");   // :810 (non-gating)

    if (mbRoadRulesVisible)
    {
        if (IsRoadRuleTime(meCurrentlyActiveRule))
        {
            mTimeAnimator.Run("transout");
            if (mabRuleActive[BrnStreetData::E_SCORE_TYPE_TIME])
                AnimateCurrentTime(E_ANIMATION_STATE_TRANSOUT);
        }
        else if (IsRoadRuleCrash(meCurrentlyActiveRule))
        {
            mCrashAnimator.Run("transout");
            if (mabRuleActive[BrnStreetData::E_SCORE_TYPE_CRASH])
                AnimateCurrentCrash(E_ANIMATION_STATE_TRANSOUT);
        }
    }

    for (s32 liRule = 0; liRule < 2; ++liRule)
    {
        mabRuleActive[liRule]  = false;
        mabRulePending[liRule] = false;
    }
    meCurrentlyActiveRule = leRule;

    if (mCurrentRunningRoadID != 0)
    {
        if (mbRoadRulesVisible)
        {
            if (IsRoadRuleTime(leRule))
                mTimeAnimator.Run("transin");
            else if (IsRoadRuleCrash(leRule))
                mCrashAnimator.Run("transin");
        }
        RefreshSignColours();
    }
}

// @ 0x8243F890 -- adopt the cache's active road rule if it differs.
void RoadRuleComponent::InitialiseMode()
{
    const s32 liActiveRule = mpGuiCache->GetActiveRoadRule();   // cache +0xAC3C
    if (liActiveRule != static_cast<s32>(meCurrentlyActiveRule))
        SwitchModes(static_cast<BrnGameState::EActiveRoadRule>(liActiveRule));
}

// @ 0x82438078 -- stop the running time rule on leave: clear its flags and send the
// ON_LEAVE sound (only when the rule was live or its animation is mid-leave).
void RoadRuleComponent::EndTimers()
{
    if (!mabRuleActive[BrnStreetData::E_SCORE_TYPE_TIME] && !IsCurrentTimeLeaving())
        return;

    mabRuleActive[BrnStreetData::E_SCORE_TYPE_TIME]  = false;
    mabRulePending[BrnStreetData::E_SCORE_TYPE_TIME] = false;

    // :1550 -- the layer assert + the inlined OutputGuiEvent<GuiEventSoundTrigger>
    // (X360 AddEvent {100, 22, 12} on ch40).
    SendRoadRuleSoundTrigger(mpStateInterface, miParentAptLayerIndex, KPC_SOUND_ON_LEAVE);
}

// @ 0x82441550 -- the frame-trigger dispatch. Label 1 re-enters the cached road;
// 4 retires a finished leave animation; 5..7 / 8..10 step the time / crash target
// row sub-animations while their rows are visible.
void RoadRuleComponent::TransitionComplete(s32 liArg)
{
    switch (liArg)
    {
    case 0:
    case 2:
    case 3:
        break;
    case 1:
        EnterRoad();
        break;
    case 4:
        if (IsCurrentTimeLeaving())
            meCurrentTimerAnimState = E_ANIMATION_STATE_INVISIBLE;
        break;
    case 5:
        if (mbTimeTargetVisible)
            mTargetTimeMC.GotoAndPlayLabel("transinValue");
        break;
    case 6:
        if (mbTimeTargetVisible)
            mTargetTimeMC.GotoAndPlayLabel("idleValue");
        break;
    case 7:
        if (mbTimeTargetVisible)
            mTargetTimeMC.GotoAndPlayLabel("idleNameToTime");
        break;
    case 8:
        if (mbCrashTargetVisible)
            mTargetCrashMC.GotoAndPlayLabel("transinValue");
        break;
    case 9:
        if (mbCrashTargetVisible)
            mTargetCrashMC.GotoAndPlayLabel("idleValue");
        break;
    case 10:
        if (mbCrashTargetVisible)
            mTargetCrashMC.GotoAndPlayLabel("nameToValue");
        break;
    default:
        CGS_ASSERT(false, "Unexpected subcomponent identifier ( )");   // :1507 (non-gating)
        break;
    }
}

// @ 0x82441778 -- the trampoline Prepare installs on the component clip.
void RoadRuleComponent::TransitionCompleteCallback(void* lpUserData, u16 lu16Label)
{
    CGS_ASSERT(lpUserData != 0, "lpUserData");   // :2231 (non-gating)
    static_cast<RoadRuleComponent*>(lpUserData)->TransitionComplete(static_cast<s32>(lu16Label));
}

// @ 0x8243F8B8 -- show/hide the rules row set for the active family; the row's
// running readout follows (transin on show, transout on hide).
void RoadRuleComponent::ShowRoadRules(bool lbShow)
{
    if (lbShow == mbRoadRulesVisible)
        return;
    mbRoadRulesVisible = lbShow;

    const char* lpcFrame = lbShow ? "transin" : "transout";
    const EAnimationState leReadoutState =
        lbShow ? E_ANIMATION_STATE_TRANSIN : E_ANIMATION_STATE_TRANSOUT;

    if (IsRoadRuleTime(meCurrentlyActiveRule))
    {
        mTimeAnimator.Run(lpcFrame);
        if (mabRuleActive[BrnStreetData::E_SCORE_TYPE_TIME])
            AnimateCurrentTime(leReadoutState);
    }
    else if (IsRoadRuleCrash(meCurrentlyActiveRule))
    {
        mCrashAnimator.Run(lpcFrame);
        if (mabRuleActive[BrnStreetData::E_SCORE_TYPE_CRASH])
            AnimateCurrentCrash(leReadoutState);
    }
}

// @ 0x8243FAD8 -- show/hide the two upcoming junction signs. Showing re-renders each
// cached side (animation, size frame, road label, colour); hiding transitions both
// out and resets the cached payload.
void RoadRuleComponent::ShowUpcomingRoads(bool lbShow)
{
    if (lbShow == mbUpcomingRoadsVisible)
        return;
    mbUpcomingRoadsVisible = lbShow;

    if (!lbShow)
    {
        SetUpcomingRoadAnimation(mUpcomingRoadData.meRoadStates[0], &mLeftRoadAnimator,
                                 E_UPCOMING_ROAD_ANIM_INVISIBLE);
        SetUpcomingRoadAnimation(mUpcomingRoadData.meRoadStates[1], &mRightRoadAnimator,
                                 E_UPCOMING_ROAD_ANIM_INVISIBLE);
        mUpcomingRoadData.Construct();
        return;
    }

    if (mUpcomingRoadData.mRoadIds[0] != 0)
    {
        SetUpcomingRoadAnimation(mUpcomingRoadData.meRoadStates[0], &mLeftRoadAnimator,
                                 E_UPCOMING_ROAD_ANIM_TRANS_IN);
        CGS_ASSERT(mUpcomingRoadData.maiTurningRoadIndices[0] < KI_MAX_ROAD_INDEX,
                   "mUpcomingRoadData.maiTurningRoadIndices[leSignSide] < KI_MAX_ROAD_INDEX");   // :2201 (non-gating)
        mLeftSignAlignmentsMC.GotoAndStopLabel(
            KAAC_ROADSIGN_SIZE_FRAME_LABELS[KAI_ROAD_INDEX_TO_SIGN_SIZE[mUpcomingRoadData.maiTurningRoadIndices[0]]]);
        const FlaptRoadSignIconComponent::ESignColour leColour =
            GetRoadSignColour(mUpcomingRoadData.maaeLeaderTypes[0], mUpcomingRoadData.mRoadIds[0]);
        mRoadSignLefty.DisplayRoadFromCgsID(mUpcomingRoadData.mRoadIds[0], false);
        mRoadSignLefty.SetColour(leColour);
    }
    if (mUpcomingRoadData.mRoadIds[1] != 0)
    {
        SetUpcomingRoadAnimation(mUpcomingRoadData.meRoadStates[1], &mRightRoadAnimator,
                                 E_UPCOMING_ROAD_ANIM_TRANS_IN);
        CGS_ASSERT(mUpcomingRoadData.maiTurningRoadIndices[1] < KI_MAX_ROAD_INDEX,
                   "mUpcomingRoadData.maiTurningRoadIndices[leSignSide] < KI_MAX_ROAD_INDEX");   // :2201 (non-gating)
        mRightSignAlignmentsMC.GotoAndStopLabel(
            KAAC_ROADSIGN_SIZE_FRAME_LABELS[KAI_ROAD_INDEX_TO_SIGN_SIZE[mUpcomingRoadData.maiTurningRoadIndices[1]]]);
        const FlaptRoadSignIconComponent::ESignColour leColour =
            GetRoadSignColour(mUpcomingRoadData.maaeLeaderTypes[1], mUpcomingRoadData.mRoadIds[1]);
        mRoadSignRighty.DisplayRoadFromCgsID(mUpcomingRoadData.mRoadIds[1], false);
        mRoadSignRighty.SetColour(leColour);
    }
}

// @ 0x8243FC88 -- render one side of a freshly-arrived upcoming-roads payload
// against the cached one. A populated side that changed transitions in with its
// road/colour; an emptied side transitions the old sign out (taken vs not-taken by
// whether a road is running). No-op in showtime.
void RoadRuleComponent::UpdateUpcomingRoadSign(GuiEventRoadRuleUpcomingRoads::ERoadSide leSide,
                                               RoadRulesRoadSign* lpSign, RoadRulesAnimator* lpAnimator,
                                               const GuiEventRoadRuleUpcomingRoads* lpEvent)
{
    CGS_ASSERT(leSide >= 0 && leSide < GuiEventRoadRuleUpcomingRoads::E_ROAD_COUNT,
               "(0 <= leRoadSide) && (GuiEventRoadRuleUpcomingRoads::E_ROAD_COUNT > leRoadSide)");   // :1824 (non-gating)
    CGS_ASSERT(lpSign != 0, "NULL != lpSign");                 // :1825 (non-gating)
    CGS_ASSERT(lpAnimator != 0, "NULL != lpSignAnimator");     // :1826 (non-gating)
    CGS_ASSERT(lpEvent != 0, "NULL != lpEvent");               // :1827 (non-gating)

    if (mbInShowTime)
        return;

    if (lpEvent->mRoadIds[leSide] != 0)
    {
        if (!IsSameAsCurrentRoad(leSide, lpEvent)
            && lpEvent->mRoadIds[leSide] != mCurrentRunningRoadID)
        {
            SetUpcomingRoadAnimation(lpEvent->meRoadStates[leSide], lpAnimator,
                                     E_UPCOMING_ROAD_ANIM_TRANS_IN);
            FlaptRoadSignIconComponent::ESignColour leColour;
            if (ShouldUseInEventColouring())
                leColour = FlaptRoadSignIconComponent::E_SIGN_COLOUR_GREEN;
            else
                leColour = GetRoadSignColour(lpEvent->maaeLeaderTypes[leSide],
                                             lpEvent->mRoadIds[leSide]);
            lpSign->DisplayRoadFromCgsID(lpEvent->mRoadIds[leSide], false);
            lpSign->SetColour(leColour);
        }
        maRoadWorldPosition[leSide] = lpEvent->maRoadEntrancePosition[leSide];
    }
    else if (mUpcomingRoadData.mRoadIds[leSide] != 0)
    {
        // The side emptied: run the outgoing frame directly off the suggested/normal
        // table -- the X360 inlines the table pick rather than calling
        // SetUpcomingRoadAnimation (whose TRANS_IN arm would fire the training event).
        const s32 leState = lpEvent->meRoadStates[leSide];
        CGS_ASSERT(leState >= 0 && leState < GuiEventRoadRuleUpcomingRoads::E_ROADSTATE_COUNT,
                   "(0 <= leNavigationState) && (GuiEventRoadRuleUpcomingRoads::E_ROADSTATE_COUNT > leNavigationState)");   // :2160 (non-gating)
        const char* const* lppcFrames =
            (leState == GuiEventRoadRuleUpcomingRoads::E_ROADSTATE_SUGGESTED)
                ? KAPC_UPCOMING_ROAD_FRAMES_SUGGESTED
                : KAPC_UPCOMING_ROAD_FRAMES_NORMAL;
        lpAnimator->Run(lppcFrames[(mCurrentRunningRoadID != 0)
                                       ? E_UPCOMING_ROAD_ANIM_TRANS_OUT_NOT_TAKEN
                                       : E_UPCOMING_ROAD_ANIM_TRANS_OUT_TAKEN]);
    }
}

// @ 0x82413FF8 -- format the display name of the current ruler of leRuleType on
// lpEvent's road: the local player in quotes, a friend's stored 16-byte name
// (double-quoted while a best value stands), or the road's "RULERQ_<id>" string id
// for the AI. Also picks the localisation format the caller pushes the name with.
void RoadRuleComponent::GetNameOfRule(char* lpacBuffer, s32 liBufferSize,
                                      const GuiEventRoadRuleEnter* lpEvent,
                                      BrnStreetData::ScoreType leRuleType)
{
    const RoadRuleLeaderType leLeader = lpEvent->maeRoadRuleLeaderType[leRuleType];

    if (leLeader == E_ROADRULELEADERTYPE_PLAYER)
    {
        CgsCore::SPrintf(lpacBuffer, liBufferSize, "%s", mpGuiCache->GetPlayerNameInQuotes());
        lpacBuffer[liBufferSize - 1] = '\0';
        mRoadRulePlayerNameFormatType =
            static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(9);   // E_FORMAT_ID_LOOKUP
    }
    else if (leLeader == E_ROADRULELEADERTYPE_FRIEND)
    {
        const char* lpcFriendName = lpEvent->maFriendLeader[leRuleType].macName;
        if (lpEvent->maiBestValues[leRuleType] != 0)
        {
            CgsCore::SPrintf(lpacBuffer, liBufferSize, "''%s''", lpcFriendName);
            lpacBuffer[liBufferSize - 1] = '\0';
            mRoadRulePlayerNameFormatType =
                static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(0);   // literal text
        }
        else
        {
            CgsCore::SPrintf(lpacBuffer, liBufferSize, "%s", lpcFriendName);
            lpacBuffer[liBufferSize - 1] = '\0';
            mRoadRulePlayerNameFormatType =
                static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(9);
        }
    }
    else   // AI
    {
        CgsCore::SPrintf(lpacBuffer, liBufferSize - 1, "RULERQ_%llu",
                         static_cast<unsigned long long>(lpEvent->mRoadId));
        mRoadRulePlayerNameFormatType =
            static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(9);
        lpacBuffer[liBufferSize - 1] = '\0';
    }
}

// @ 0x824226B0 -- re-render the two best rows (value / name / label) for the running
// road, and re-band the running timer field around the best time.
void RoadRuleComponent::RefreshBestData()
{
    if (mTransitionData.mRoadId == 0)
        return;

    // ---- the best-time row ------------------------------------------------
    const s32 liBestTimeMs = mTransitionData.maiBestValues[BrnStreetData::E_SCORE_TYPE_TIME];
    const f32 lfBestTime   = static_cast<f32>(liBestTimeMs) * 0.001f;   // flt_82013F90
    f32 lfSafeBoundary     = lfBestTime - 3.0f;                          // fsel clamp
    if (lfSafeBoundary < 0.0f)
        lfSafeBoundary = 0.0f;

    if (liBestTimeMs != 0)
    {
        mCurrentTimerField.SetCountingMode(FlaptTimerFieldComponent::E_TIMER_MODE_COUNTING_UP);
        mBestTimeValueTextField.SetLocalisedText(lfBestTime, 2);   // @0x8246CE38, format 2 == timer
    }
    else
    {
        mCurrentTimerField.SetCountingMode(FlaptTimerFieldComponent::E_TIMER_MODE_COUNTING_NOT_COUNTING);
        mBestTimeValueTextField.SetText("-", false);
    }
    mCurrentTimerField.SetBoundaries(lfSafeBoundary, lfBestTime);

    char lacRuleName[32];
    GetNameOfRule(lacRuleName, 32, &mTransitionData, BrnStreetData::E_SCORE_TYPE_TIME);
    mBestTimeNameTextField.SetLocalisedText(lacRuleName,
                                            static_cast<s32>(mRoadRulePlayerNameFormatType));

    const s32 liScoreMode = mpGuiCache->GetActiveRoadRuleScoringMode();
    CGS_ASSERT(liScoreMode != 2,
               "GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_COUNT != meRoadRuleScoreMode");   // BrnGuiCache.h:4210 (non-gating)
    if (liScoreMode != 0)
    {
        CGS_ASSERT(liScoreMode == 1,
                   "GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE == mpGuiCache->GetActiveRoadRuleScoringMode()");   // :999 (non-gating)
        mBestTimeLabelTextField.SetLocalisedText("HUD_BESTTIME_ONLINE", 9);
    }
    else
    {
        mBestTimeLabelTextField.SetLocalisedText("HUD_BESTTIME_OFFLINE", 9);
    }

    // ---- the best-crash row -----------------------------------------------
    const s32 liBestCrash = mTransitionData.maiBestValues[BrnStreetData::E_SCORE_TYPE_CRASH];
    if (liBestCrash != 0)
        mBestCrashValueTextField.SetLocalisedText(liBestCrash, 14);   // @0x8246CF18, format 14 == money
    else
        mBestCrashValueTextField.SetText("-", false);

    GetNameOfRule(lacRuleName, 32, &mTransitionData, BrnStreetData::E_SCORE_TYPE_CRASH);
    mBestCrashNameTextField.SetLocalisedText(lacRuleName,
                                             static_cast<s32>(mRoadRulePlayerNameFormatType));

    if (liScoreMode != 0)
    {
        CGS_ASSERT(mpGuiCache->GetActiveRoadRuleScoringMode() == 1,
                   "GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE == mpGuiCache->GetActiveRoadRuleScoringMode()");   // :1028 (non-gating)
        mBestCrashLabelTextField.SetLocalisedText("HUD_BESTCRASH_ONLINE", 9);
    }
    else
    {
        mBestCrashLabelTextField.SetLocalisedText("HUD_BESTCRASH_OFFLINE", 9);
    }
}

// @ 0x82422918 -- the sign colour for a road ruled by lpLeaderTypes: GREEN with no
// active rule (or the EXIT road), GOLD when the player holds BOTH rules, SILVER when
// only the active family's, RED otherwise.
FlaptRoadSignIconComponent::ESignColour
RoadRuleComponent::GetRoadSignColour(const RoadRuleLeaderType* lpLeaderTypes, CgsID lRoadId) const
{
    const s32 liActive = static_cast<s32>(meCurrentlyActiveRule);

    s32 liTypeIndex;
    if (IsRoadRuleTime(meCurrentlyActiveRule))
        liTypeIndex = BrnStreetData::E_SCORE_TYPE_TIME;
    else if (IsRoadRuleCrash(meCurrentlyActiveRule))
        liTypeIndex = BrnStreetData::E_SCORE_TYPE_CRASH;
    else
        liTypeIndex = BrnStreetData::E_SCORE_TYPE_COUNT;

    if (liActive == 0)
        return FlaptRoadSignIconComponent::E_SIGN_COLOUR_GREEN;

    const RoadRuleLeaderType leLeader = lpLeaderTypes[liTypeIndex];
    if (leLeader == E_ROADRULELEADERTYPE_PLAYER)
    {
        if (IsRoadRuleTime(meCurrentlyActiveRule))
        {
            return (lpLeaderTypes[BrnStreetData::E_SCORE_TYPE_CRASH] == E_ROADRULELEADERTYPE_PLAYER)
                       ? FlaptRoadSignIconComponent::E_SIGN_COLOUR_GOLD
                       : FlaptRoadSignIconComponent::E_SIGN_COLOUR_SILVER;
        }
        CGS_ASSERT(IsRoadRuleCrash(meCurrentlyActiveRule),
                   "Active road rule ( ) should be crash if not time or none!");   // :1936 (non-gating)
        return (lpLeaderTypes[BrnStreetData::E_SCORE_TYPE_TIME] == E_ROADRULELEADERTYPE_PLAYER)
                   ? FlaptRoadSignIconComponent::E_SIGN_COLOUR_GOLD
                   : FlaptRoadSignIconComponent::E_SIGN_COLOUR_SILVER;
    }

    if (lRoadId == KID_ROAD_EXIT)
        return FlaptRoadSignIconComponent::E_SIGN_COLOUR_GREEN;

    CGS_ASSERT(leLeader == E_ROADRULELEADERTYPE_AI || leLeader == E_ROADRULELEADERTYPE_FRIEND,
               "If ruler is not player, should be friend or AI (not )");   // :1958 (non-gating)
    return FlaptRoadSignIconComponent::E_SIGN_COLOUR_RED;
}

// @ 0x82430050 -- re-render the current sign + both upcoming signs from the cached
// leader data.
void RoadRuleComponent::RefreshSignColours()
{
    {
        const FlaptRoadSignIconComponent::ESignColour leColour =
            GetRoadSignColour(mTransitionData.maeRoadRuleLeaderType, mCurrentRunningRoadID);
        mRoadSign.DisplayRoadFromCgsID(mCurrentRunningRoadID, false);
        mRoadSign.SetColour(leColour);
    }
    if (mUpcomingRoadData.mRoadIds[0] != 0)
    {
        const FlaptRoadSignIconComponent::ESignColour leColour =
            GetRoadSignColour(mUpcomingRoadData.maaeLeaderTypes[0], mUpcomingRoadData.mRoadIds[0]);
        mRoadSignLefty.DisplayRoadFromCgsID(mUpcomingRoadData.mRoadIds[0], false);
        mRoadSignLefty.SetColour(leColour);
    }
    if (mUpcomingRoadData.mRoadIds[1] != 0)
    {
        const FlaptRoadSignIconComponent::ESignColour leColour =
            GetRoadSignColour(mUpcomingRoadData.maaeLeaderTypes[1], mUpcomingRoadData.mRoadIds[1]);
        mRoadSignRighty.DisplayRoadFromCgsID(mUpcomingRoadData.mRoadIds[1], false);
        mRoadSignRighty.SetColour(leColour);
    }
}

// @ 0x82438298 -- drive the running-time readout to a new animation state. Leaving
// states send the ON_LEAVE sound BEFORE the switch (the X360 reads the OLD state);
// the target-row visibility byte tracks the new state.
void RoadRuleComponent::AnimateCurrentTime(EAnimationState leState)
{
    CGS_ASSERT(leState >= E_ANIMATION_STATE_FIRST && leState < E_ANIMATION_STATE_ADVANCED_COUNT,
               "Invalid state requested ( )");   // :2013 (non-gating)

    if (IsCurrentTimeLeaving())
    {
        // :2018 -- the layer assert + ON_LEAVE.
        SendRoadRuleSoundTrigger(mpStateInterface, miParentAptLayerIndex, KPC_SOUND_ON_LEAVE);
    }

    meCurrentTimerAnimState = leState;
    mCurrentTimeAnimator.Run(KAPC_ANIMATION_FRAMES[leState]);

    switch (meCurrentTimerAnimState)
    {
    case E_ANIMATION_STATE_TRANSIN:
    case E_ANIMATION_STATE_IDLE:
        mbTimeTargetVisible = true;
        break;
    case E_ANIMATION_STATE_INVISIBLE:
    case E_ANIMATION_STATE_TRANSOUT:
    case E_ANIMATION_STATE_UNSUCCESSFUL_IDLE:
        mbTimeTargetVisible = false;
        break;
    default:
        break;
    }
}

// X360-INLINED (DWARF :513 AnimateCurrentCrash; every call site --
// HandleRoadRuleBegin @0x8243EEB4, HandleRoadRuleEnd @0x8243F1A8, SwitchModes
// @0x8243F430, ShowRoadRules @0x8243F984 -- inlines this exact shape): drive the
// crash readout's animator and track the crash target row's visibility. Unlike the
// time twin there is no sound side-effect.
void RoadRuleComponent::AnimateCurrentCrash(EAnimationState leState)
{
    meCurrentCrashAnimState = leState;
    mCurrentCrashAnimator.Run(KAPC_ANIMATION_FRAMES[leState]);

    switch (meCurrentCrashAnimState)
    {
    case E_ANIMATION_STATE_TRANSIN:
    case E_ANIMATION_STATE_IDLE:
        mbCrashTargetVisible = true;
        break;
    case E_ANIMATION_STATE_INVISIBLE:
    case E_ANIMATION_STATE_TRANSOUT:
        mbCrashTargetVisible = false;
        break;
    default:
        break;
    }
}

// @ 0x82438598 -- run one upcoming-road animator to a new animation state, picking
// the suggested/normal frame table off the road state. A SUGGESTED road
// transitioning IN also posts GuiEventRequestTraining(9) -- the "follow the
// suggested road" training message (X360 AddEvent {4, 572, 12, 9} on ch40).
void RoadRuleComponent::SetUpcomingRoadAnimation(GuiEventRoadRuleUpcomingRoads::ERoadState leState,
                                                RoadRulesAnimator* lpAnimator,
                                                EUpcomingRoadAnimState leAnimState)
{
    CGS_ASSERT(leState >= 0 && leState < GuiEventRoadRuleUpcomingRoads::E_ROADSTATE_COUNT,
               "(0 <= leNavigationState) && (GuiEventRoadRuleUpcomingRoads::E_ROADSTATE_COUNT > leNavigationState)");   // :2160 (non-gating)
    CGS_ASSERT(leAnimState >= 0 && leAnimState < E_UPCOMING_ROAD_ANIM_COUNT,
               "(0 <= leNewAnim) && (E_UPCOMING_ROAD_ANIM_COUNT > leNewAnim)");   // :2161 (non-gating)
    CGS_ASSERT(lpAnimator != 0, "NULL != lpSignAnimator");   // :2162 (non-gating)

    const char* const* lppcFrames;
    if (leState == GuiEventRoadRuleUpcomingRoads::E_ROADSTATE_SUGGESTED)
    {
        lppcFrames = KAPC_UPCOMING_ROAD_FRAMES_SUGGESTED;
        if (leAnimState == E_UPCOMING_ROAD_ANIM_TRANS_IN)
        {
            GuiEventRequestTraining lTraining;
            lTraining.meTrainingType = 9;
            mpStateInterface->OutputGuiEvent(lTraining);
        }
    }
    else
    {
        lppcFrames = KAPC_UPCOMING_ROAD_FRAMES_NORMAL;
    }
    lpAnimator->Run(lppcFrames[leAnimState]);
}

// @ 0x82410458 -- BrnRoadRuleComponent.h:707/:708. Is the cached upcoming-road for
// side leSide identical to the same side in lpEvent? (H2: rewritten onto the named
// X360-ordered members; the old raw-offset transcription retired with the event
// struct's reorder.)
bool RoadRuleComponent::IsSameAsCurrentRoad(GuiEventRoadRuleUpcomingRoads::ERoadSide leSide,
                                            const GuiEventRoadRuleUpcomingRoads* lpEvent) const
{
    CGS_ASSERT(leSide < GuiEventRoadRuleUpcomingRoads::E_ROAD_COUNT,
               "(0 <= leRoadSide) && (GuiEventRoadRuleUpcomingRoads::E_ROAD_COUNT > leRoadSide)"); // :707 (non-gating)
    CGS_ASSERT(0 != lpEvent, "NULL != lpEvent");   // :708 (non-gating)

    if (lpEvent->mRoadIds[leSide] != mUpcomingRoadData.mRoadIds[leSide])
        return false;
    if (lpEvent->meRoadStates[leSide] != mUpcomingRoadData.meRoadStates[leSide])
        return false;
    if (lpEvent->maaeLeaderTypes[leSide][0] != mUpcomingRoadData.maaeLeaderTypes[leSide][0])
        return false;
    if (lpEvent->maaeLeaderTypes[leSide][1] != mUpcomingRoadData.maaeLeaderTypes[leSide][1])
        return false;
    return true;
}

// @ 0x82410568 -- BrnRoadRuleComponent.h:731. Should the road sign fall back to the
// "in-event" (green) colouring? True only in the showtime-family game modes
// (offline showtime / online showtime / free-burn lobby) AND while the cache's
// in-event colouring gate byte is set.
bool RoadRuleComponent::ShouldUseInEventColouring()
{
    CGS_ASSERT(0 != mpGuiCache, "mpGuiCache");   // :731 (non-gating)

    const s32 leGameMode = mpGuiCache->GetGameMode();
    const bool lbShowtimeFamily =
        (leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY) ||
        (leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) ||
        (leGameMode == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME);

    if (!mpGuiCache->GetInEventColouringGate())
    {
        return false;
    }
    return lbShowtimeFamily;
}

// @ 0x82410640 -- BrnRoadRuleComponent.h:755/:767. Drive the current sign to a new
// animation state: play the state's frame on the animated-road-sign clip and stop
// the component root + the rulesCpts container on the transition road's size label.
// No-op when already in that state. (H2: the play/stop targets are now ATTESTED --
// Prepare's binding names + the HandleEnterRoadEvent inline; the old slot-guess FLAG
// mapped play->rulesCpts and is retired.)
void RoadRuleComponent::SetCurrentSignState(EAnimationState leState)
{
    CGS_ASSERT((leState >= E_ANIMATION_STATE_FIRST) && (leState < E_ANIMATION_STATE_ADVANCED_COUNT),
               "( E_ANIMATION_STATE_FIRST <= leNewSignState ) && ( E_ANIMATION_STATE_ADVANCED_COUNT > leNewSignState )"); // :755 (non-gating)

    if (meCurrentSignState == leState)
    {
        return;
    }

    mRoadSignAnimationsMC.GotoAndPlayLabel(KAPC_ANIMATION_FRAMES[leState]);   // "animatedRoadSign_mc" @+0x0C

    CGS_ASSERT(mTransitionData.miRoadIndex < KI_MAX_ROAD_INDEX,
               "mTransitionData.miRoadIndex < KI_MAX_ROAD_INDEX");   // :767 (non-gating)

    const char* lpcSizeFrame =
        KAAC_ROADSIGN_SIZE_FRAME_LABELS[KAI_ROAD_INDEX_TO_SIGN_SIZE[mTransitionData.miRoadIndex]];
    mAptRef.GotoAndStopLabel(lpcSizeFrame);                    // the component root @+0x04
    mSubComponentPositionsMC.GotoAndStopLabel(lpcSizeFrame);   // "rulesCpts" @+0x14

    meCurrentSignState = leState;
}

// @ 0x82410720 -- BrnRoadRuleComponent.h:795 / BrnGuiCache.h:4210. Refresh the two
// upcoming-road panels' active leader-type slots from either the online or the
// offline leader-type set, chosen by the cache's road-rule score mode. (H2:
// rewritten onto the named X360-ordered members.)
void RoadRuleComponent::UpdateUpcomingRoadLeaders(GuiEventRoadRuleUpcomingRoads* lpEvent)
{
    CGS_ASSERT(0 != mpGuiCache, "mpGuiCache");   // :795 (non-gating)

    // The accessor reproduces the X360-inlined "E_ROAD_PANEL_MODE_COUNT !=
    // meRoadRuleScoreMode" assert (BrnGuiCache.h:4210, non-gating).
    const s32 leScoreMode = mpGuiCache->GetActiveRoadRuleScoringMode();
    const bool lbUseOnline = (leScoreMode == 1);

    for (s32 liType = 0; liType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liType)
    {
        CGS_ASSERT(liType <= BrnStreetData::E_SCORE_TYPE_COUNT,
                   "leEnumIndex <= E_SCORE_TYPE_COUNT");   // BrnChallengeData.h:56 (non-gating)
        for (s32 liSide = 0; liSide < GuiEventRoadRuleUpcomingRoads::E_ROAD_COUNT; ++liSide)
        {
            lpEvent->maaeLeaderTypes[liSide][liType] =
                lbUseOnline ? lpEvent->maaeOnlineLeaderTypes[liSide][liType]
                            : lpEvent->maaeOfflineLeaderTypes[liSide][liType];
        }
    }
}

// @ 0x82473520 -- BrnRoadRuleComponent.h:628. Adopt the GUI cache pointer.
void RoadRuleComponent::SetCachePointer(GuiCache* lpGuiCache)
{
    CGS_ASSERT(0 != lpGuiCache, "lpCache");   // :628 (non-gating)
    mpGuiCache = lpGuiCache;
}

}
