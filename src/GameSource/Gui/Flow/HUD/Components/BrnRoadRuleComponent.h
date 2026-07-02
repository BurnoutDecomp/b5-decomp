#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                        // CgsID, Vector3, VecFloat
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"                    // ParameterFormatType
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                                    // GuiEventRoadRuleEnter / GuiEventRoadRuleUpcomingRoads / RoadRuleLeaderType
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                             // BrnFlapt::TextFieldRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"       // BrnFlaptComponent (base) + MovieClipRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"   // FlaptIconComponent / FlaptAnimatorComponent
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptRoadSignIconComponent.h" // FlaptRoadSignIconComponent (ESignColour)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptTimerFieldComponent.h"   // FlaptTimerFieldComponent
#include "SharedClasses/StreetData/BrnChallengeData.h"                             // BrnStreetData::ScoreType

// BrnGui::RoadRuleComponent - the road-rules HUD panel (current road sign +
// upcoming junction signs + best time/crash rows + the running timers). Class
// shape / enums / member names / method set verbatim from the DecFIGS DWARF
// (BrnRoadRuleComponent.h:66/:86/:249/:266/:363-:456). This HEADER TU owns the
// three X360-emitted header-inlines (EnterRoad @0x82441450, HandleLeaveRoadEvent
// @0x82413D78, HandleUpcomingRoadEvent @0x82441338); every other method is its
// own ledger function (declaration-only). X360 member pins agreeing with the
// DWARF order: mRoadSign @+0x30, mRoadSignLefty @+0xC0, mRoadSignRighty @+0x110,
// mLeftRoadAnimator @+0x160, mRightRoadAnimator @+0x198, mTransitionData @+0x3E8,
// mUpcomingRoadData @+0x460, mCurrentRunningRoadID @+0x4E0, mabRulePending
// @+0x4EA, mbRoadRulesVisible @+0x518, mbUpcomingRoadsVisible @+0x519.
namespace BrnGameState { enum EActiveRoadRule : s32; }

namespace BrnGui
{
    class GuiCache;   // pointer-held (BrnGuiCache.h)

    // DWARF BrnRoadRuleComponent.h:66 -- the shared road-rules component base
    // (adds nothing to BrnFlaptComponent).
    struct BaseRoadRulesComponent : public BrnFlaptComponent
    {
    };

    // DWARF :68/:70/:71/:72 -- verbatim empty subclasses (nested-typedef homes).
    struct RoadRulesRoadSign : public FlaptRoadSignIconComponent {};
    struct RoadRulesTimer    : public FlaptTimerFieldComponent {};
    struct RoadRulesIcon     : public FlaptIconComponent {};
    struct RoadRulesAnimator : public FlaptAnimatorComponent {};

    // DWARF BrnRoadRuleComponent.h:86.
    struct RoadRuleComponent : public BaseRoadRulesComponent
    {
        // DWARF :69.
        typedef BrnFlapt::TextFieldRef RoadRulesTextfield;

        // DWARF :89.
        enum ERoadSignSizes
        {
            E_ROAD_SIGN_SMALL   = 0,
            E_ROAD_SIGN_MEDIUM1 = 1,
            E_ROAD_SIGN_MEDIUM2 = 2,
            E_ROAD_SIGN_LARGE   = 3,
            E_ROAD_SIGN_COUNT   = 4,
        };

        // DWARF :249.
        enum EAnimationState
        {
            E_ANIMATION_STATE_INVISIBLE            = 0,
            E_ANIMATION_STATE_TRANSIN              = 1,
            E_ANIMATION_STATE_IDLE                 = 2,
            E_ANIMATION_STATE_TRANSOUT             = 3,
            E_ANIMATION_STATE_BASIC_COUNT          = 4,
            E_ANIMATION_STATE_UNSUCCESSFUL_IDLE    = 4,
            E_ANIMATION_STATE_UNSUCCESSFUL_TRANSOUT = 5,
            E_ANIMATION_STATE_ADVANCED_COUNT       = 6,
            E_ANIMATION_STATE_FIRST                = 0,
        };

        // DWARF :266.
        enum EUpcomingRoadAnimState
        {
            E_UPCOMING_ROAD_ANIM_INVISIBLE          = 0,
            E_UPCOMING_ROAD_ANIM_TRANS_IN           = 1,
            E_UPCOMING_ROAD_ANIM_TRANS_OUT_TAKEN    = 2,
            E_UPCOMING_ROAD_ANIM_TRANS_OUT_NOT_TAKEN = 3,
            E_UPCOMING_ROAD_ANIM_COUNT              = 4,
        };

        // ---- public surface (DWARF :105-:206) -- declared-only except the three
        //      inlines this TU owns.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, s32 liParentAptLayerIndex);       // :105
        void Prepare(const char* lpacName, const BrnFlapt::FileRef& lrFile);         // :112
        void Update(f32 lfTimeStep);                                                 // :118
        void UpdateCurrentTime(f32 lfTime);                                          // :123
        void UpdateCurrentCrash(f32 lfScore, s32 liMultiplier);                      // :129
        void UpdateRoadSignDistances(Vector3 lv3CameraPosition);                     // :133

        // @0x82413D78 (this TU, DWARF :138) -- the player left a road: if it is the
        // road the panel is running, replay the cached enter payload and start the
        // sign's transition-out.
        void HandleLeaveRoadEvent(CgsID lRoadId)
        {
            if (mCurrentRunningRoadID == lRoadId)
            {
                if (mTransitionData.mRoadId == lRoadId)
                {
                    mTransitionData.Construct();
                    SetCurrentSignState(E_ANIMATION_STATE_TRANSOUT);
                }
            }
        }

        void HandleEnterRoadEvent(const GuiEventRoadRuleEnter* lpEvent);             // :143
        void HandleRoadRuleTargetUpdate(const GuiEventRoadRuleUpdateTargetScores* lpEvent); // :148
        void HandleRoadRuleBegin(BrnStreetData::ScoreType leRuleType);               // :153
        void HandleRoadRuleEnd(const GuiEventRoadRuleEnd* lpEvent);                  // :158

        // @0x82441338 (this TU, DWARF :163, cpp:747) -- adopt the two upcoming
        // junction roads. When the panel is visible the payload is staged in a
        // local (the X360's 16-qword copy), driven through the leaders/sign
        // updates, then cached; hidden panels just cache it for the next show.
        void HandleUpcomingRoadEvent(const GuiEventRoadRuleUpcomingRoads* lpEvent)
        {
            CGS_ASSERT(lpEvent != 0, "lpEvent");   // :747 (non-gating)

            if (mbUpcomingRoadsVisible)
            {
                GuiEventRoadRuleUpcomingRoads lEventCopy = *lpEvent;
                UpdateUpcomingRoadLeaders(&lEventCopy);
                UpdateUpcomingRoadSign(GuiEventRoadRuleUpcomingRoads::E_ROAD_LEFT,
                                       &mRoadSignLefty, &mLeftRoadAnimator, &lEventCopy);
                UpdateUpcomingRoadSign(GuiEventRoadRuleUpcomingRoads::E_ROAD_RIGHT,
                                       &mRoadSignRighty, &mRightRoadAnimator, &lEventCopy);
                // X360: `if (colour word @+0x6C != 0)` -- drop back to the zero
                // colour (green) while in-event colouring runs.
                if (ShouldUseInEventColouring()
                    && mRoadSign.GetSignColour() != FlaptRoadSignIconComponent::E_SIGN_COLOUR_GREEN)
                {
                    mRoadSign.SetColour(FlaptRoadSignIconComponent::E_SIGN_COLOUR_GREEN);
                }
                mUpcomingRoadData = lEventCopy;
            }
            else
            {
                mUpcomingRoadData = *lpEvent;
                UpdateUpcomingRoadLeaders(&mUpcomingRoadData);
            }
        }

        void SwitchModes(BrnGameState::EActiveRoadRule leRule);                      // :168
        void TransitionComplete(s32 liArg);                                          // :180
        void InitialiseMode();                                                       // :184
        void EndTimers();                                                            // :188
        f32  GetCurrentCrashScore() const;                                           // :195
        void SetCachePointer(GuiCache* lpGuiCache);                                  // :201
        void SetIfInShowTime(bool lbInShowTime);                                     // :206

    private:
        // ---- private helpers (DWARF :464-:587) -- declared-only except EnterRoad.
        void GetNameOfRule(char* lpacBuffer, s32 liBufferSize,
                           const GuiEventRoadRuleEnter* lpEvent,
                           BrnStreetData::ScoreType leRuleType);                     // :464
        void RefreshBestData();                                                      // :468

        // @0x82441450 (this TU, DWARF :472) -- latch the cached enter payload's road
        // as the running road and refresh the whole panel for it (or blank the panel
        // when no road is cached).
        void EnterRoad()
        {
            mCurrentRunningRoadID = mTransitionData.mRoadId;
            if (mTransitionData.mRoadId != 0)
            {
                SetCurrentSignState(E_ANIMATION_STATE_TRANSIN);
                if (!mbRoadRulesVisible)
                    ShowRoadRules(true);
                if (!mbUpcomingRoadsVisible)
                    ShowUpcomingRoads(true);
                for (s32 liRule = 0; liRule < 2; ++liRule)
                {
                    if (mabRulePending[liRule])
                    {
                        HandleRoadRuleBegin(static_cast<BrnStreetData::ScoreType>(liRule));
                        mabRulePending[liRule] = false;
                    }
                }
                FlaptRoadSignIconComponent::ESignColour leColour =
                    GetRoadSignColour(mTransitionData.maeRoadRuleLeaderType,
                                      mTransitionData.mRoadId);
                mRoadSign.DisplayRoadFromCgsID(mCurrentRunningRoadID, false);
                mRoadSign.SetColour(leColour);
                RefreshBestData();
            }
            else
            {
                ShowRoadRules(false);
                ShowUpcomingRoads(false);
                SetCurrentSignState(E_ANIMATION_STATE_INVISIBLE);
                RefreshBestData();
            }
        }

        s32  GetNextRuleAvailable();                                                 // :476
        void ShowRoadRules(bool lbShow);                                             // :481
        void ShowUpcomingRoads(bool lbShow);                                         // :486
        void UpdateUpcomingRoadSign(GuiEventRoadRuleUpcomingRoads::ERoadSide leSide,
                                    RoadRulesRoadSign* lpSign, RoadRulesAnimator* lpAnimator,
                                    const GuiEventRoadRuleUpcomingRoads* lpEvent);   // :494
        FlaptRoadSignIconComponent::ESignColour
             GetRoadSignColour(const RoadRuleLeaderType* lpLeaderTypes, CgsID lRoadId) const; // :500
        FlaptRoadSignIconComponent::ESignColour
             GetRoadSignColourInEvent(GuiEventRoadRuleUpcomingRoads::ERoadState leState) const; // :505
        void AnimateCurrentTime(EAnimationState leState);                            // :509
        void AnimateCurrentCrash(EAnimationState leState);                           // :513
        void RefreshSignColours();                                                   // :517
        bool IsCurrentTimeLeaving();                                                 // :521
        void SetUpcomingRoadAnimation(GuiEventRoadRuleUpcomingRoads::ERoadState leState,
                                      RoadRulesAnimator* lpAnimator,
                                      EUpcomingRoadAnimState leAnimState);           // :529
        BrnStreetData::ScoreType GetActiveRoadRuleType() const;                      // :533
        bool IsSameAsCurrentRoad(GuiEventRoadRuleUpcomingRoads::ERoadSide leSide,
                                 const GuiEventRoadRuleUpcomingRoads* lpEvent) const; // :539
        bool ShouldUseInEventColouring();                                            // :543
        void SetCurrentSignState(EAnimationState leState);                           // :548
        void UpdateUpcomingRoadLeaders(GuiEventRoadRuleUpcomingRoads* lpEvent);      // :553
        void UpdateRoadSignDistance(Vector3 lv3CameraPosition,
                                    GuiEventRoadRuleUpcomingRoads::ERoadSide leSide,
                                    VecFloat lvfAdjustment, RoadRulesRoadSign* lpSign); // :561
        VecFloat GetSignOffsetSizeAdjustment(GuiEventRoadRuleUpcomingRoads::ERoadSide leSide); // :566
        bool IsRoadRuleCrash(BrnGameState::EActiveRoadRule leRule) const;            // :570
        bool IsRoadRuleTime(BrnGameState::EActiveRoadRule leRule) const;             // :574
        void RepositionUpcomingSign(GuiEventRoadRuleUpcomingRoads::ERoadSide leSide); // :581
        void TransitionCompleteCallback(void* lpUserData, u16 lu16Label);            // :587

        // ---- member layout (DWARF :363-:456 order; X360 pins in the file header) --
        BrnFlapt::MovieClipRef mRoadSignAnimationsMC;      // :363 (X360 +0x10)
        BrnFlapt::MovieClipRef mSubComponentPositionsMC;   // :364
        BrnFlapt::MovieClipRef mLeftSignAlignmentsMC;      // :365
        BrnFlapt::MovieClipRef mRightSignAlignmentsMC;     // :366
        RoadRulesRoadSign      mRoadSign;                  // :371 (X360 +0x030)
        RoadRulesAnimator      mCentralArrowAnimator;      // :372 (X360 +0x080)
        RoadRulesRoadSign      mRoadSignLefty;             // :373 (X360 +0x0C0)
        RoadRulesRoadSign      mRoadSignRighty;            // :374 (X360 +0x110)
        RoadRulesAnimator      mLeftRoadAnimator;          // :375 (X360 +0x160)
        RoadRulesAnimator      mRightRoadAnimator;         // :376 (X360 +0x198)
        Vector3                maRoadWorldPosition[2];     // :378
        EAnimationState        meCurrentSignState;         // :380
        RoadRulesTextfield     mBestCrashLabelTextField;   // :387
        RoadRulesTextfield     mBestCrashNameTextField;    // :388
        RoadRulesTextfield     mBestCrashValueTextField;   // :389
        RoadRulesTextfield     mBestTimeLabelTextField;    // :394
        RoadRulesTextfield     mBestTimeNameTextField;     // :395
        RoadRulesTextfield     mBestTimeValueTextField;    // :396
        RoadRulesAnimator      mTimeAnimator;              // :398
        RoadRulesAnimator      mCrashAnimator;             // :399
        RoadRulesIcon          mTimeLeaderIcon;            // :401
        RoadRulesIcon          mCrashLeaderIcon;           // :402
        RoadRulesTimer         mCurrentTimerField;         // :408
        RoadRulesTextfield     mCurrentCrashTextField;     // :411
        RoadRulesAnimator      mCurrentTimeAnimator;       // :413
        RoadRulesAnimator      mCurrentCrashAnimator;      // :416
        EAnimationState        meCurrentTimerAnimState;    // :419
        EAnimationState        meCurrentCrashAnimState;    // :420
        GuiEventRoadRuleEnter  mTransitionData;            // :426 (X360 +0x3E8)
        GuiEventRoadRuleUpcomingRoads mUpcomingRoadData;   // :427 (X360 +0x460)
        CgsID                  mCurrentRunningRoadID;      // :428 (X360 +0x4E0)
        bool                   mabRuleActive[2];           // :429 (X360 +0x4E8)
        bool                   mabRulePending[2];          // :430 (X360 +0x4EA)
        BrnFlapt::MovieClipRef mTargetTimeMC;              // :433
        BrnFlapt::MovieClipRef mTargetCrashMC;             // :434
        bool                   mbTimeTargetVisible;        // :436
        bool                   mbCrashTargetVisible;       // :437
        BrnGameState::EActiveRoadRule meCurrentlyActiveRule;  // :440
        BrnGameState::EActiveRoadRule mePreviousActiveRule;   // :441
        GuiCache*              mpGuiCache;                 // :444
        bool                   mbInShowTime;               // :445
        CgsLanguage::LanguageManager::ParameterFormatType mRoadRulePlayerNameFormatType; // :446
        s32                    miParentAptLayerIndex;      // :448
        bool                   mbRoadRulesVisible;         // :450 (X360 +0x518)
        bool                   mbUpcomingRoadsVisible;     // :451 (X360 +0x519)
        f32                    mfTargetCrashScore;         // :454
        f32                    mfCurrentCrashScore;        // :455
        s32                    miCrashMultiplier;          // :456
    };
}
