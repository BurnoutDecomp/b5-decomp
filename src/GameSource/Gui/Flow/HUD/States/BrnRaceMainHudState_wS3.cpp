// BrnRaceMainHudState_wS3.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX -- part-file 2 of the BrnRaceMainHudState TU.
// The sibling part-files are BrnRaceMainHudState.cpp (the .rdata resource table +
// SetExpectedComponent) and the other _wS* part-files of the same wave; NONE of the four
// bodies below is defined anywhere else.
//
//   BrnGui::RaceMainHudState::UpdateRunning            @ 0x8247E898  (781 pseudocode lines)
//   BrnGui::RaceMainHudState::SetupEventInfo           @ 0x82474A60
//   BrnGui::RaceMainHudState::UpdateEventCountdown     @ 0x8247A608
//   BrnGui::RaceMainHudState::ConcludeEventCountdown   @ 0x824748F0
//
// Every offset in the X360 pseudocode below was resolved against BrnRaceMainHudState.h's
// member map (a1+320 == mpCache, a1+368 == mEventInfoComponent, a1+4616 == mEventCountdownIcon,
// a1+4636 == meCurrentEventCountdownState, a1+4640 == mfEventCountdownTimer, ...). Access is
// BY NAME throughout.
//
// ⭐ THE FLAG-BLOCK SHIFT (s2 scout, 2026-08-26). The retail X360 object carries TWENTY-FIVE
// enable bytes at +0x150..+0x168, not the 26 the committed header spells: OnEnter @0x82478EF8
// emits exactly 25 consecutive `stb r30, 0x150(r31)` .. `stb r30, 0x168(r31)`, and this
// function pins three of them at instruction level -- `lbz r11, 0x15C(r31)` gates
// `addi r3,r31,0x5F60 ; bl RoadRuleComponent__HandleLeaveRoadEvent` (so +0x15C IS
// mbRoadRuleComponent, not mbAboveCarIcons), `lbz 0x15F` gates PaybackComponent and
// `lbz 0x163/0x164/0x165/0x166/0x167/0x168` gate OnlineTimeout / Compass / the three
// freeburn-challenge widgets / the challenge-on component. This file therefore spells the
// flags with the CORRECTED names from the scout's table. It compiles against the header
// either way (every name used here exists in both spellings); the byte a flag lands on only
// matters once the header drops the 26th entry, which is the conductor's paired edit --
// see the request list returned with this file.
//
// ⭐ (f) THERE IS EXACTLY ONE EventInfoComponent CALL IN THIS FUNCTION. The whole 63-case
// switch carries NO arm for GuiEventCurrentStatus(492), GuiAttackScoreUpdate(428),
// GuiEventScoreUpdate(424) or GuiEventTimeInfo: the stunt score / multiplier / combo / timer
// readout is PULLED out of GuiCache by EventInfoComponent::Update @0x82435430 (whose only
// xref to UpdateStuntAttack @0x82429C08 is itself), never pushed through this state's GUI
// event queue. The single call is the per-frame tick at 0x8247FFCC:
//     lbz  r11, 0x157(r31)                ; mbEventInfo
//     addi r3, r31, 0x170                 ; &mEventInfoComponent
//     lwz  r4, 0x140(r31)                 ; mpCache
//     bl   BrnGui__EventInfoComponent__Update
//
// COMPONENT DEFERRALS. Same rule and same idiom as the sibling BrnFBurnMainHudState.cpp: an
// arm whose component TU is not on the build (tools/build/build_game_exe.bat) or whose method
// has no declaration yet keeps the X360 gate and control flow verbatim and logs the gap once
// instead of inventing a body. Deferred here: PlayerPositionTableComponent, PaybackComponent,
// OnlineTimeoutComponent, CompassComponent, ChallengeSelector, the five FriendsListComponent
// entry points that have no body anywhere, HandleMugshotEvent (not declared -- it is in the
// header's RESIDUE block) and the freeburn challenge-on arm. For E_MODE_STUNT_ATTACK (7)
// UpdateSetupState turns the gate byte OFF for every one of those except the friends list,
// so none of them executes on the stunt-race bring-up path.
#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log (deferral gap log)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // the GuiEvent* payload types
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // BrnGui::FreeburnChallengeManager

#include <cstdio>    // std::snprintf (the one-shot deferral log)

namespace BrnGui
{
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT        = 40;  // GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE     = 41;  // GuiOutViewState
        const s32 KI_CHANNEL_INTERNAL_STATE = 42;  // the internal-state mirror channel

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The black-bar / letterbox threshold the case-221 arm compares against
        // (X360 flt_82002138, read out of the XEX image).
        const f32 KF_BLACK_BARS_THRESHOLD = 0.0099999998f;

        // 16-byte GuiEvent<N> command { 1, N, 12, flag } -- the shared state-channel record
        // this state posts for ids 60/61/214/236/533/543/544 (identical to the FBurn helper
        // at BrnFBurnMainHudState.cpp:71; file-local in both, no ODR tie).
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;
            u8 maPad[3];
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel, u8 lu8Flag = 0)
        {
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }

        // 20-byte GuiEvent<465> road-rule crash-score record { 8, 465, 12, 0, f32 } -- the
        // per-frame post the road-rule tick makes (X360 @0x8248007C..0x824800B8: the payload
        // pair is stack-built as {0, mfCurrentCrashScore} and copied with one `std`).
        struct GuiRoadRuleCrashEvent20 : public CgsGui::GuiEvent<465>
        {
            s32 miReserved;   // +0x0C (the console's `stw r18` zero)
            f32 mfCrashScore; // +0x10
            explicit GuiRoadRuleCrashEvent20(f32 lfCrashScore)
                : CgsGui::GuiEvent<465>(8, 12), miReserved(0), mfCrashScore(lfCrashScore) {}
        };

        // 24-byte OutputGuiEvent<BrnGui::GuiAudioEvent> WRAPPER record -- header
        // { sizeof(payload)=24, type=456, payload offset=16 } then the 24-byte payload. The
        // countdown's four audio posts differ only in the third payload word (3/2/1/0), which
        // is the countdown step. Shape verbatim from the committed
        // BrnInGameMessagesComponent.cpp:160 GuiAudioEventRecord40 (the same X360 record).
        // ⚠ FLAG payload field NAMES: BrnGui::GuiAudioEvent is still `u8 maPayload[12]` in
        // BrnGuiDemangledEventTypes.h, so the leading words have no recovered names and are
        // spelled by role here rather than forked into that header.
        struct alignas(8) GuiAudioEventRecord40
        {
            s32   miOutEventSize;     // +0x00 = 24
            s32   miOutEventType;     // +0x04 = 456
            s32   miOutEventOffset;   // +0x08 = 16
            s32   miHeaderPad;        // +0x0C (uninitialised on the console)
            s32   miAudioParam0;      // +0x10 = 0 on all four countdown posts
            s32   miAudioParam1;      // +0x14 = 6 (the countdown audio bank)
            s32   miAudioParam2;      // +0x18 = the countdown step 3/2/1/0
            s32   miPayloadPad;       // +0x1C (uninitialised on the console)
            CgsID mAudioId;           // +0x20 = 0

            GuiAudioEventRecord40(s32 liParam0, s32 liParam1, s32 liParam2)
                : miOutEventSize(24), miOutEventType(456), miOutEventOffset(16), miHeaderPad(0)
                , miAudioParam0(liParam0), miAudioParam1(liParam1), miAudioParam2(liParam2)
                , miPayloadPad(0), mAudioId(0)
            {
            }
        };

        void PostCountdownAudio(CgsGui::StateInterface* lpInterface, s32 liStep)
        {
            // X360 @0x8247A734/A79C/A834/A8FC: { 0, 6, liStep } + an 8-byte zero tail.
            GuiAudioEventRecord40 lAudio(0, 6, liStep);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lAudio), KI_CHANNEL_GUI_OUT, 40);
        }

        // ---- GuiCache boundary (the far cache fields this state reads that BrnGuiCache.h
        // does not expose) -------------------------------------------------------------
        // Same idiom as BrnFBurnMainHudState.cpp's cache boundary (:116-:215): one helper per
        // X360 cache field, each carrying the offset so the eventual cache-member naming (or
        // a `friend struct RaceMainHudState;` grant next to the existing friends at
        // BrnGuiCache.h:992) lands exactly here.

        // ---- the cache fields that DO have accessors now -----------------------------
        // The wave's cache carve (BrnGuiCache.h) publishes the in-event gate as
        // IsEventPreparedForModeStart() @+0xA014,
        // GetPlayerRacePosition() @+0x4B24, IsPlayerRacePositionOverridden() @+0x4B25,
        // IsFriendsListOpen() @+0xB86C and GetSatNavZoomLevel() @+0x803C, so this file reads
        // them by name and only the two below still need a stand-in.

        // ⚠ FLAG accessor leaf: the "local player is the online host" byte. The member IS
        // named (GuiCache::mbIsOnlineHost @+0xB864, BrnGuiCache.h:1665) but is private and
        // this class is not one of the cache's friends, so the read is stood in for here
        // rather than forking a second copy of the member.
        // DELETE-WHEN: BrnGuiCache.h publishes `bool IsOnlineHost() const` (or grants
        // `friend struct RaceMainHudState;` beside the friends at :992); the body then
        // becomes `return lpGuiCache->mbIsOnlineHost;`.
        bool GuiCache_IsOnlineHost(const GuiCache* /*lpGuiCache*/)
        {
            return false;
        }

        // ⚠ FLAG accessor leaf: the sat-nav zoom-level WRITE. GuiCache publishes the
        // read (GetSatNavZoomLevel() @+0x803C, BrnGuiCache.h:529) and the "zoom out" step
        // (ZoomSatNavOut()), but the case-6 arm also stores 0 straight into the member and
        // there is no setter for that half.
        // DELETE-WHEN: BrnGuiCache.h publishes `void SetSatNavZoomLevel(s32)`; the body then
        // becomes `lpGuiCache->SetSatNavZoomLevel(liLevel);`.
        void GuiCache_SetSatNavZoomLevel(GuiCache* /*lpGuiCache*/, s32 /*liLevel*/)
        {
        }

        // One-shot deferral log: the un-mounted / un-declared component entry points this
        // state drives. Each call site keeps the console's gate and routing; only the body is
        // deferred, and the gap stays visible in the log instead of silently vanishing.
        // (Same helper, same 8-slot shape, as BrnFBurnMainHudState.cpp:219.)
        void LogDeferredComponent(const char* lpacComponent)
        {
            static const char* sapcNames[16];
            for (s32 li = 0; li < 16; ++li)
            {
                if (sapcNames[li] == lpacComponent)
                    return;
                if (sapcNames[li] == 0)
                {
                    sapcNames[li] = lpacComponent;
                    char lac[160];
                    std::snprintf(lac, sizeof(lac),
                                  "[RaceMainHud] %s -- component TU deferred (wave S3).\n",
                                  lpacComponent);
                    CgsDev::Log::WriteToLog(lac);
                    return;
                }
            }
        }
    }

    // =======================================================================
    //  UpdateRunning  @ 0x8247E898 -- the RUNNING per-frame event dispatch + tick tail
    // =======================================================================
    void RaceMainHudState::UpdateRunning()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

        // X360 head @0x8247E8C0..0x8247E8E4 -- the per-frame sat-nav pre-pass. Both words are
        // INSIDE the component: its player-info binding (component +0x130 == state +0x7D0)
        // and, through its icon manager (component +0x254 == state +0x8F4), the manager's
        // used-icon count (+0x990). The event pump repopulates both during this same frame.
        // ⚠ PAIRED EDIT: needs `friend struct RaceMainHudState;` beside the existing
        // `friend struct FBurnMainHudState;` in BrnSatNavComponent.h:150 and
        // BrnMapIconManager.h:254 -- the freeburn HUD makes the identical pair of stores.
        if (mbSatNav)
        {
            mSatNavComponent.mpPlayerInfo = 0;
            if (mSatNavComponent.mpIconManager != 0)
                mSatNavComponent.mpIconManager->miNumUsedIcons = 0;
        }

        for (; lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
            switch (liEventId)
            {
            case 6:      // controller input pressed
                if (mbFriendsList)
                {
                    // FLAG deferred: FriendsListComponent::HandleControllerInput @0x82443280
                    // has no body anywhere in the tree (BrnFriendsList.cpp does not define it).
                    LogDeferredComponent("FriendsListComponent::HandleControllerInput");
                }
                if (mbBurnoutSkillz && lpiPayload[1] == 38 &&
                    !mpCache->IsFriendsListOpen())
                {
                    PostCommand16<543>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                    GuiEventRoadRuleModeRequest lModeRequest;
                    lModeRequest.maData[0] = 0; lModeRequest.maData[1] = 0;
                    lModeRequest.maData[2] = 0; lModeRequest.maData[3] = 0;
                    lModeRequest.maData[4] = 0; lModeRequest.maData[5] = 0;
                    lModeRequest.maData[6] = 0; lModeRequest.maData[7] = 0;
                    mpStateInterface->OutputGuiEvent(lModeRequest);
                }
                if (mbFreeburnChallengeButtonStart)
                    mChallengeComponent.HandleButtonPress(lpiPayload[1]);
                {
                    // X360 @0x8247EFB4: `lwz r11, 4(GetFreeburnChallengeManager(mpCache))`
                    // tested against 3 and 4 -- the manager's own IsRunning/IsShowingResults
                    // pair, folded inline by the compiler.
                    const FreeburnChallengeManager* lpManager =
                        mpCache->GetFreeburnChallengeManager();
                    const bool lbChallengeLive =
                        lpManager != 0 && (lpManager->IsRunning() || lpManager->IsShowingResults());
                    if (lbChallengeLive && !mpCache->IsFriendsListOpen() &&
                        lpiPayload[1] == 38)
                    {
                        PostCommand16<544>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                    }
                }
                if (lpiPayload[1] == 53)
                {
                    // The sat-nav zoom toggle, live only in E_MODE_ONLINE_BURNING_HOME_RUN(13).
                    if (mpCache->GetGameMode() == 13)
                    {
                        if (mpCache->GetSatNavZoomLevel() == 1)
                            GuiCache_SetSatNavZoomLevel(mpCache, 0);
                        else
                            mpCache->ZoomSatNavOut();   // X360 renders this `this`-less (hazard 4)
                    }
                }
                break;
            case 7:      // controller input released
                if (mbFreeburnChallengeButtonStart)
                    mChallengeComponent.HandleButtonRelease(lpiPayload[1]);
                break;
            case 94:
                if (mbFriendsList)
                    mFriendsListChangeIcon.Hide();
                break;
            case 95:
                if (mbFriendsList)
                {
                    // FLAG deferred: FriendsListComponent::EndWait @0x82442FF0 -- no body.
                    LogDeferredComponent("FriendsListComponent::EndWait");
                }
                break;
            case 101:
                if (mbFriendsList)
                    mFriendsList.SetTotalFriends(lpiPayload[0]);
                break;
            case 102:
                if (mbFriendsList)
                {
                    // FLAG deferred: FriendsListComponent::ProcessNewEntryData @0x824430D0 -- no body.
                    LogDeferredComponent("FriendsListComponent::ProcessNewEntryData");
                }
                break;
            case 103:
                if (mbFriendsList)
                    mFriendsList.RequestRefreshedData();
                break;
            case 104:
                if (mbFriendsList && mpCache->IsOnlineStartInProgress() &&
                    GuiCache_IsOnlineHost(mpCache))
                {
                    // FLAG deferred: FriendsListComponent::ReshowShortcuts @0x82441E78 -- no body.
                    LogDeferredComponent("FriendsListComponent::ReshowShortcuts");
                }
                break;
            case 106:
                if (mbFriendsList && !mpCache->IsFriendsListOpen())
                    mFriendsListChangeIcon.AnimateIn();
                break;
            case 108:
                if (mbOnlineTimeoutTimer)
                {
                    // FLAG deferred: OnlineTimeoutComponent::SetTime @0x824157B0 is neither
                    // declared in BrnOnlineTimeoutTimerComponent.h nor on the build.
                    LogDeferredComponent("OnlineTimeoutComponent::SetTime");
                }
                break;
            case 154:
                if (mbHudMessages)
                    mHudMessageComponent.AddMessage(lpEvent);
                break;
            case 156:
                if (mbHudMessages)
                    mHudMessageComponent.TerminateMessages();
                break;
            case 177:
                if (mbPaybackComponent)
                {
                    // FLAG deferred: BrnPaybackComponent.cpp is not on the build
                    // (BeginAwardAnimation(payload[2], payload[1]) @0x8243E148).
                    LogDeferredComponent("PaybackComponent::BeginAwardAnimation");
                }
                break;
            case 179:
            case 180:
                if (mbPaybackComponent)
                {
                    // FLAG deferred: PaybackComponent::BecomeInvisible @0x8241FFE8 -- TU off the build.
                    LogDeferredComponent("PaybackComponent::BecomeInvisible");
                }
                break;
            case 182:   // hide the event HUD
            {
                mGeneralTransitionComponentApt.AddOutputAptViewState("apt_Transition", "invisible", false);
                mGeneralTransitionComponentFlapt.Run("invisible");
                if (lpiPayload[0] != 1)
                {
                    GuiEventShowHideBoostBar lBoostBar;
                    lBoostBar.maData[0] = 0;
                    mpStateInterface->OutputViewState(lBoostBar);
                }
                break;
            }
            case 183:   // show the event HUD
            {
                mGeneralTransitionComponentApt.AddOutputAptViewState("apt_Transition", "visible", false);
                mGeneralTransitionComponentFlapt.Run("visible");
                GuiEventShowHideBoostBar lBoostBar;
                lBoostBar.maData[0] = 1;
                mpStateInterface->OutputViewState(lBoostBar);
                break;
            }
            case 199:
            case 200:
                UpdateSatNav(lpEvent, liEventId);
                break;
            case 205:   // show/hide satnav passthrough: view record + the satnav mirror
            {
                GuiEventShowHideSatNav lShowHide;
                lShowHide.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS,
                                    *reinterpret_cast<const u8*>(lpEvent) != 0, 0.0f);
                mpStateInterface->OutputViewState(lShowHide);
                if (mbSatNav)
                {
                    mSatNavComponent.RecvEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lShowHide), 213);
                }
                break;
            }
            case 206:
                ProcessBoostInfo(lpEvent);
                break;
            case 221:   // the black-bars / letterbox amount (f32)
            {
                // X360 @0x8247F1B0: the whole arm is gated on NOT being in an event.
                if (!mpCache->IsEventPreparedForModeStart())
                {
                    const f32 lfBlackBars = *reinterpret_cast<const f32*>(lpEvent);
                    if (mfBlackBarsCurrentValue != lfBlackBars)
                    {
                        u8 lu8Show;
                        if (lfBlackBars >= KF_BLACK_BARS_THRESHOLD)
                        {
                            mGeneralTransitionComponentApt.AddOutputAptViewState(
                                "apt_Transition", "invisible", false);
                            mGeneralTransitionComponentFlapt.Run("invisible");
                            GuiEventShowHideBoostBar lBoostBar;
                            lBoostBar.maData[0] = 0;
                            mpStateInterface->OutputViewState(lBoostBar);
                            lu8Show = 0;
                        }
                        else
                        {
                            mGeneralTransitionComponentApt.AddOutputAptViewState(
                                "apt_Transition", "visible", false);
                            mGeneralTransitionComponentFlapt.Run("visible");
                            GuiEventShowHideBoostBar lBoostBar;
                            lBoostBar.maData[0] = 1;
                            mpStateInterface->OutputViewState(lBoostBar);
                            lu8Show = 1;
                        }
                        GuiEventShowHideSatNav lShowHide;
                        lShowHide.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS,
                                            lu8Show != 0, 0.0f);
                        mpStateInterface->OutputViewState(lShowHide);
                        mpStateInterface->OutputInternalState(lShowHide);
                        if (mbSatNav)
                        {
                            mSatNavComponent.RecvEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lShowHide), 213);
                        }
                        mfBlackBarsCurrentValue = *reinterpret_cast<const f32*>(lpEvent);
                    }
                }
                break;
            }
            case 222:   // the PP toggle
                if (mbB5Ident)
                {
                    CGS_ASSERT(lpEvent != 0, "lpPPToggle");   // cpp:1301 (non-gating)
                    if (lpiPayload[0] == 1)
                        mIdentAnimator.Run("transIn");
                    else
                        mIdentAnimator.Run("invisible");
                }
                break;
            case 226:
                PostCommand16<60>(mpStateInterface, KI_CHANNEL_VIEW_STATE);
                break;
            case 227:
                PostCommand16<61>(mpStateInterface, KI_CHANNEL_VIEW_STATE);
                break;
            case 234:
                UpdateEventCountdown(lpEvent);
                break;
            case 239:
                if (mbPlayerPositionTable)
                {
                    // FLAG deferred: BrnPlayerPositionTable.cpp is not on the build
                    // (UpdatePositionDetails @0x82441260).
                    LogDeferredComponent("PlayerPositionTableComponent::UpdatePositionDetails");
                }
                break;
            case 325:
                if (mbMugShotComponent)
                {
                    // FLAG deferred: HandleMugshotEvent @0x82475CD0 is in the header's
                    // RESIDUE block -- its GuiMugshotControlEvent parameter type is
                    // ODR-forked between GameBridgeNetworkToX.h and
                    // BrnGuiDemangledEventTypes.h, so it has no declaration to call.
                    LogDeferredComponent("RaceMainHudState::HandleMugshotEvent");
                }
                break;
            case 333:   // GuiEventRoadRuleEnter
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.HandleEnterRoadEvent(
                        reinterpret_cast<const GuiEventRoadRuleEnter*>(lpEvent));
                break;
            case 335:   // road-rule begin { ScoreType }
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.HandleRoadRuleBegin(
                        static_cast<BrnStreetData::ScoreType>(lpiPayload[0]));
                mbBounceBoostPromptNeeded = false;   // X360: the store is OUTSIDE the gate
                break;
            case 336:   // GuiEventRoadRuleEnd
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.HandleRoadRuleEnd(
                        reinterpret_cast<const GuiEventRoadRuleEnd*>(lpEvent));
                mbBounceBoostPromptNeeded = false;
                break;
            case 338:   // rule-time update { f32 time, .., f32 crashTarget, s32 multiplier }
                if (mbRoadRuleComponent)
                {
                    const f32* lpfPayload = reinterpret_cast<const f32*>(lpEvent);
                    mRoadRuleComponent.UpdateCurrentTime(lpfPayload[0]);
                    // X360 @0x8247F55C..: the crash-target pair rides the same record; a
                    // changed multiplier nudges the target by +0.01 so the eased readout
                    // re-renders. This is RoadRuleComponent::UpdateCurrentCrash inlined --
                    // it is spelled as the two named stores because that method has no body
                    // in the tree, exactly as BrnFBurnMainHudState.cpp:1050 spells it.
                    // ⚠ PAIRED EDIT: needs `friend struct RaceMainHudState;` beside
                    // `friend struct FBurnMainHudState;` at BrnRoadRuleComponent.h:51.
                    const s32 liNewMultiplier = lpiPayload[4];
                    mRoadRuleComponent.mfTargetCrashScore = lpfPayload[3];
                    if (mRoadRuleComponent.miCrashMultiplier != liNewMultiplier)
                    {
                        mRoadRuleComponent.miCrashMultiplier  = liNewMultiplier;
                        mRoadRuleComponent.mfTargetCrashScore = lpfPayload[3] + KF_BLACK_BARS_THRESHOLD;
                    }
                }
                break;
            case 339:   // GuiEventRoadRuleUpdateTargetScores
                if (mbRoadRuleComponent)
                {
                    CGS_ASSERT(lpEvent != 0, "lpRRTargetUpdate");   // cpp:1018 (non-gating)
                    mRoadRuleComponent.HandleRoadRuleTargetUpdate(
                        reinterpret_cast<const GuiEventRoadRuleUpdateTargetScores*>(lpEvent));
                }
                break;
            case 340:   // road-rule leave { CgsID }
                // Hazard 4: the asm is `addi r3,r31,0x5F60 ; ld r4,0(r29)` -- ONE 64-bit
                // payload load, not the two s32s Hex-Rays renders.
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.HandleLeaveRoadEvent(
                        *reinterpret_cast<const CgsID*>(lpEvent));
                mbBounceBoostPromptNeeded = false;
                break;
            case 341:   // GuiEventRoadRuleUpcomingRoads
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.HandleUpcomingRoadEvent(
                        reinterpret_cast<const GuiEventRoadRuleUpcomingRoads*>(lpEvent));
                break;
            case 343:   // road-rule mode change { EActiveRoadRule }
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.SwitchModes(
                        static_cast<BrnGameState::EActiveRoadRule>(lpiPayload[0]));
                break;
            case 379:   // the HUD transin / transout pair (player engine state)
            {
                CGS_ASSERT(static_cast<u32>(lpiPayload[0]) < 2u,
                           "( GuiPlayerEngineEvent::E_ENGINE_OFF == lpEngineChange->meNewEngineState )"
                           " || ( GuiPlayerEngineEvent::E_ENGINE_ON == lpEngineChange->meNewEngineState )");   // cpp:1159
                GuiEventShowHideSatNav lShowHide;
                if (lpiPayload[0] == 0)
                {
                    mGeneralTransitionComponentApt.AddOutputAptViewState("apt_Transition", "transout", false);
                    mGeneralTransitionComponentFlapt.Run("transout");
                    PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
                    lShowHide.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS, false, 0.0f);
                    mpStateInterface->OutputViewState(lShowHide);
                    mpStateInterface->OutputInternalState(lShowHide);
                }
                else if (lpiPayload[0] == 1)
                {
                    mGeneralTransitionComponentApt.AddOutputAptViewState("apt_Transition", "transin", false);
                    mGeneralTransitionComponentFlapt.Run("transin");
                    PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
                    lShowHide.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS, true, 0.0f);
                    mpStateInterface->OutputViewState(lShowHide);
                    mpStateInterface->OutputInternalState(lShowHide);
                }
                // X360 @0x8247F318: the satnav mirror is OUTSIDE the if/else.
                if (mbSatNav)
                {
                    mSatNavComponent.RecvEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lShowHide), 213);
                }
                break;
            }
            case 218:
            case 364: case 365: case 367: case 368:
            case 382: case 383: case 384: case 385: case 386: case 387:
            case 388: case 389: case 390: case 391: case 394:
            case 400: case 401:
                // X360 LABEL_120 -- the whole boost-message event family routes through the
                // manager with the LIVE event id. Note there is NO mbBoostMessages gate here;
                // only the per-frame Update below is gated.
                CGS_ASSERT(mpCache != 0, "mpCache != NULL");   // cpp:839 (non-gating)
                mBoostMessageManager.RecvEvent(lpEvent, liEventId, mpCache);
                break;
            case 398:
                mbBounceBoostPromptNeeded = (lpiPayload[0] != 0);
                break;
            case 573:   // freeburn challenge selector action
                if (lpiPayload[2] == 2 || lpiPayload[2] == 3)
                {
                    if (mbFreeburnChallengeSelector)
                    {
                        // FLAG deferred: BrnChallengeSelector.cpp / _wL_01.cpp are not on the
                        // build and their mount has three unresolved residuals
                        // (ChallengeList::GetChallengeCount, ChallengeListEntry::
                        // GetNumPlayers / GetDescriptionStringID). Action 2 =
                        // SetAvailableChallenges(cache->muChallengeSlotMirror) +
                        // SelectAvailableChallengeByID(*payload, false); action 3 = Hide().
                        LogDeferredComponent("ChallengeSelector::(action 2/3)");
                    }
                }
                else if (lpiPayload[2] > 3)
                {
                    CGS_ASSERT(false, "Unknown freeburn challenge selector action");   // cpp:1478 (streamed)
                }
                break;
            case 574:
                CGS_ASSERT(lpEvent != 0, "lpChallengeEvent");   // cpp:1361 (non-gating)
                if (*(reinterpret_cast<const u8*>(lpEvent) + 8) != 0)
                {
                    if (mbFreeburnChallengeButtonStart)
                        mChallengeComponent.Show();
                }
                else if (mbFreeburnChallengeSelector)
                {
                    CGS_ASSERT(mpCache != 0, "mpCache");   // cpp:1379 (non-gating)
                    // FLAG deferred (X360 @0x8247FCDC): SetAvailableChallenges(
                    // mpCache->muChallengeSlotMirror @+0xAC78) then
                    // SelectAvailableChallengeByID(the CgsID at payload+0 -- `ld r4,0(r29)`,
                    // a 64-bit load Hex-Rays renders as the 32-bit v4[1]), lbSelect false.
                    LogDeferredComponent("ChallengeSelector::SelectAvailableChallengeByID");
                }
                break;
            case 576:
                if (mbFreeburnChallengeButtonStart)
                    mChallengeComponent.Hide();
                if (mbFreeburnChallengeSelector)
                {
                    // FLAG deferred: `if (selector.IsVisible()) selector.Hide();`
                    LogDeferredComponent("ChallengeSelector::Hide");
                }
                break;
            case 578:
                if (mbFreeburnChallengeSelector)
                {
                    // FLAG deferred: the same Hide, additionally gated on the cache's
                    // online-host byte (X360 `!*(mpCache + 47204)`).
                    LogDeferredComponent("ChallengeSelector::Hide");
                }
                break;
            case 582:
                if (mbFreeburnChallengeSelector)
                {
                    CGS_ASSERT(lpEvent != 0, "lpShowChallengeSelectorEvent");   // cpp:887 (non-gating)
                    // FLAG deferred: SetAvailableChallenges -> GetAvailableChallengeCount>0
                    // -> Show + SelectAvailableChallengeByID/SelectAvailableChallenge.
                    LogDeferredComponent("ChallengeSelector::Show");
                }
                break;
            case 583:
                if (mbFreeburnChallengeTicker)
                    StartFreeburnChallengeNotActiveTicker();
                break;
            case 584:
                // ⭐ 2026-08-27 verify round: the FULL 16-byte {2,536,12}+{0,1} wire on
                // CHANNEL 40 (console `li r5, 0x28 ; li r6, 0x10`), not the raw 2-byte
                // record through OutputGuiEvent (2 bytes on channel 536 = a clear the
                // ticker consumer never sees). TU-local wire per the partfile precedent.
                if (mbFreeburnChallengeTicker)
                {
                    struct GuiTickerClearWire536 : public CgsGui::GuiEvent<536>
                    {
                        u8 mbForceFadeOut;            // +0x0C == 0
                        u8 mbDeleteChallengeMessages; // +0x0D == 1
                        u8 mau8Pad[2];
                        GuiTickerClearWire536()
                            : CgsGui::GuiEvent<536>(2, 12)
                            , mbForceFadeOut(0), mbDeleteChallengeMessages(1)
                        { mau8Pad[0] = mau8Pad[1] = 0; }
                    };
                    GuiTickerClearWire536 lClear;
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lClear), 40, 16);
                }
                break;
            default:
                break;
            }
        }

        // ---- the per-frame component ticks (X360 @0x8247FE54..0x824801EC) --------------
        if (mbDistrictMarker)
        {
            // The marker's own per-frame method: an ICF fold of an EMPTY body on console
            // (the pseudocode's BaseCollisionGenerator::Destruct @0x8284CB38 decompiles to
            // `{ ; }`) -- hazard 6, do NOT reconstruct a collision call here.
            mDistrictMarker.Update();

            GuiEventChangeDistrict lRecord;
            lRecord.meCounty    = mpCache->GetChangeDistrictCounty();
            lRecord.meDistrict  = mpCache->GetChangeDistrictDistrict();
            lRecord.mu8Consumed = mpCache->IsChangeDistrictConsumed() ? 1 : 0;
            lRecord.maPad[0] = lRecord.maPad[1] = lRecord.maPad[2] = 0;
            if (!lRecord.mu8Consumed ||
                (mbFirstFrame && lRecord.meDistrict != BrnWorld::E_DISTRICT_INVALID))
            {
                mDistrictMarker.SetCounty(static_cast<BrnWorld::ECounty>(lRecord.meCounty));
                mDistrictMarker.SetDistrict(static_cast<BrnWorld::EDistrict>(lRecord.meDistrict));
                lRecord.mu8Consumed = 1;
                mpCache->RecEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord), 169);
                mbFirstFrame = false;
            }
        }

        // The position indicator (X360 @0x8247FEE0..0x8247FF40). r4 carries the cache's
        // position byte into BOTH calls: the zero-position path falls straight through to
        // SetVisible(false), and the loaded path re-reads the component's own pending
        // trans-in latch between SetPosition and SetVisible(true).
        // ⚠ PAIRED EDIT: mPositionIndicatorComponent.mbFirstFrame is private -- needs
        // `friend struct RaceMainHudState;` in BrnPositionIndicator.h (the class has no
        // friend list yet; add one beside the member block).
        {
            const s32 liPosition = mpCache->GetPlayerRacePosition();
            if (liPosition == 0)
            {
                mPositionIndicatorComponent.SetVisible(false);
            }
            else if (mpCache->IsPlayerRacePositionOverridden() ||
                     (mPositionIndicatorComponent.mbFirstFrame && liPosition > 0 && liPosition <= 8))
            {
                mPositionIndicatorComponent.SetPosition(liPosition);
                if (mPositionIndicatorComponent.mbFirstFrame)
                    mPositionIndicatorComponent.SetVisible(true);
            }
        }

        // The showtime bounce-boost help item (X360 @0x8247FF44..0x8247FFC0).
        if (mbShowTimeBar)
        {
            if (mbBounceBoostPromptNeeded)
            {
                if (!mbBounceBoostPromptVisible)
                {
                    mShowtimeBounceBoostButton.SetItem(
                        "$HINT_SHOWTIME_GROUND_BREAK",
                        FlaptButtonIconComponent::E_PADBUTTON_SELECT,
                        FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                        false);
                    mbBounceBoostPromptVisible = true;
                }
            }
            else if (mbBounceBoostPromptVisible)
            {
                // X360 &unk_820046A7 -- the empty string (image byte 0x00), the "clear it" call.
                mShowtimeBounceBoostButton.SetItem(
                    "",
                    FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                    FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                    false);
                mbBounceBoostPromptVisible = false;
            }
        }

        // ⭐ THE ONE EventInfoComponent CALL (X360 @0x8247FFCC). The stunt-run score /
        // multiplier / banked-combo / event-timer readout rides this single per-frame tick;
        // there is no event-switch arm feeding it.
        if (mbEventInfo)
            mEventInfoComponent.Update(mpCache);

        if (mbOnlineTimeoutTimer)
        {
            // FLAG deferred: OnlineTimeoutComponent::Update @0x8242C1E0 is neither declared
            // in BrnOnlineTimeoutTimerComponent.h nor on the build.
            LogDeferredComponent("OnlineTimeoutComponent::Update");
        }
        if (mbSatNav)
            mSatNavComponent.Update();
        if (mbBoostMessages)
        {
            // X360 @0x8248000C: `lfs f1, 0(mpCache)` == GuiCache::mfTimeStep (GetTimeStep),
            // and `lbz r5, 0x160(this)` == mbShowTimeBar -- the showtime flag IS the second
            // argument, so in showtime the manager runs only its showtime ticker.
            mBoostMessageManager.Update(mpCache->GetTimeStep(), mbShowTimeBar);
        }
        if (mbHudMessages)
            mHudMessageComponent.Update();
        if (mbFriendsList)
        {
            // FLAG deferred: FriendsListComponent::Update @0x82442C78 -- no body.
            LogDeferredComponent("FriendsListComponent::Update");
        }
        if (mbRoadRuleComponent)
        {
            mRoadRuleComponent.Update(mpCache->GetTime());
            // Hazard 5: the operand is an `lvx128 v1, mpCache, 0x4AE0` that Hex-Rays drops
            // entirely -- the world-camera position. Same recipe as
            // BrnFBurnMainHudState.cpp:1290.
            const Vector4& lv4Camera = mpCache->GetWorldCameraPosition();
            Vector3 lv3Camera;
            lv3Camera.x = lv4Camera.x;
            lv3Camera.y = lv4Camera.y;
            lv3Camera.z = lv4Camera.z;
            lv3Camera.w = lv4Camera.w;
            mRoadRuleComponent.UpdateRoadSignDistances(lv3Camera);

            GuiRoadRuleCrashEvent20 lCrash(mRoadRuleComponent.mfCurrentCrashScore);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lCrash), KI_CHANNEL_GUI_OUT, 20);
        }
        if (mbPaybackComponent)
        {
            // FLAG deferred: PaybackComponent::Update @0x8241FF38 -- TU off the build.
            LogDeferredComponent("PaybackComponent::Update");
        }
        if (mbCompass)
        {
            // FLAG deferred: BrnCompassComponent.cpp is bodied but not on the build.
            LogDeferredComponent("CompassComponent::Update");
        }
        if (mbFreeburnChallengeOnComponent)
        {
            // FLAG deferred: the challenge-on arm. X360 @0x824800F8..0x824801E4:
            //   assert(cache->mpChallengeManager, "mpChallengeManager", BrnGuiCache.h:2390)
            //   if (manager->IsRunning() || manager->IsShowingResults())
            //       lbShow = (manager->GetCurrentAction()->GetTimeLimit() <= 0.0f);
            //   else lbShow = false;
            //   if (mbChallengeOnShowing != lbShow) {
            //       mbChallengeOnShowing = lbShow;
            //       mChallengeOnComponent.SetState(lbShow ? "transin" : "invisible");
            //   }
            // Deferred because the +0x40 read is ChallengeListEntryAction::GetTimeLimit,
            // which is declared-only in SharedClasses/DataLists/ChallengeListEntry.h (no
            // body anywhere), and GuiCache::mpChallengeManager is private to this class.
            // mbFreeburnChallengeOnComponent is 0 for every offline mode including
            // E_MODE_STUNT_ATTACK, so this arm is dead on the bring-up path.
            LogDeferredComponent("RaceMainHudState::(freeburn challenge-on arm)");
        }

        ConcludeEventCountdown();
    }

    // =======================================================================
    //  SetupEventInfo  @ 0x82474A60
    // =======================================================================
    // Ten lines: assert the cache and the enable flag, publish the live game mode to the
    // event-info panel and -- for every mode except 15 (the freeburn lobby) -- run its
    // "transin". The mode word is GuiCache::meGameModeType (X360 cache+40536).
    void RaceMainHudState::SetupEventInfo()
    {
        CGS_ASSERT(mpCache != 0, "mpCache");           // cpp:3804 (non-gating)
        CGS_ASSERT(mbEventInfo, "mbEventInfo");        // cpp:3805 (non-gating)

        const s32 liGameMode = mpCache->GetGameMode();
        mEventInfoComponent.SetEventType(
            static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(liGameMode));
        if (liGameMode != 15)
            mEventInfoComponent.MoveAnimation("transin");
    }

    // =======================================================================
    //  UpdateEventCountdown  @ 0x8247A608
    // =======================================================================
    // The pre-event 3 / 2 / 1 / GO ladder. The WHOLE body is gated on mbPreRaceCountdown, so
    // for E_MODE_STUNT_ATTACK (mode 7, where UpdateSetupState clears the flag) this is a
    // no-op. meCurrentEventCountdownState is a strictly DESCENDING ratchet: each arm runs only
    // while the state is still above the value it is about to write, so a repeated or
    // out-of-order countdown event cannot walk the icon backwards.
    void RaceMainHudState::UpdateEventCountdown(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event passed to RaceMainHudState::UpdateEventCountdown");   // cpp:3252 (streamed; non-gating)

        if (!mbPreRaceCountdown)
            return;

        const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
        switch (lpiPayload[0])
        {
        case 3:
            if (meCurrentEventCountdownState > E_EVENT_COUNTDOWN_STATE_THREE)
            {
                meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_THREE;
                if (mbPreRaceCountdownRenders)
                    mEventCountdownIcon.SetState("three");
                PostCountdownAudio(mpStateInterface, 3);
            }
            break;
        case 2:
            if (meCurrentEventCountdownState > E_EVENT_COUNTDOWN_STATE_TWO)
            {
                meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_TWO;
                if (mbPreRaceCountdownRenders)
                    mEventCountdownIcon.SetState("two");
                PostCountdownAudio(mpStateInterface, 2);
            }
            break;
        case 1:
            if (meCurrentEventCountdownState > E_EVENT_COUNTDOWN_STATE_ONE)
            {
                meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_ONE;
                if (mbPreRaceCountdownRenders)
                    mEventCountdownIcon.SetState("one");
                CGS_ASSERT(mpCache != 0, "mpCache");   // cpp:3336 (non-gating)
                mfEventCountdownTimer = mpCache->GetTime();
                PostCountdownAudio(mpStateInterface, 1);
            }
            break;
        case 0:
            if (meCurrentEventCountdownState > E_EVENT_COUNTDOWN_STATE_GO)
            {
                meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_GO;
                if (mbPreRaceCountdownRenders)
                    mEventCountdownIcon.SetState("go");

                // ⭐ HAZARD 7 -- an EXACT float sentinel, deliberately not epsilon'd. OnEnter
                // leaves mfEventCountdownTimer at 0.0f, and 0.0f is the "the ONE step never
                // arrived" marker: in that case the timer is back-dated by one second so the
                // reflection below still yields a sane GO deadline. Then the timer is
                // REFLECTED about now (`fmsubs f0, f1, 2.0, f13` == now*2 - then), turning
                // "the moment the countdown reached ONE" into "the moment the GO banner should
                // retire" -- ConcludeEventCountdown waits for it. Do not "fix" either line.
                if (mfEventCountdownTimer == 0.0f)
                    mfEventCountdownTimer = mpCache->GetTime() - 1.0f;
                CGS_ASSERT(mpCache != 0, "mpCache");   // cpp:3369 (non-gating)
                mfEventCountdownTimer = (mpCache->GetTime() * 2.0f) - mfEventCountdownTimer;

                PostCountdownAudio(mpStateInterface, 0);
                RevealHud(false);
                PostCommand16<236>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                PostCommand16<533>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            }
            break;
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            break;
        default:
            CGS_ASSERT(false,
                       "Unexpected Countdown State in RaceMainHudState::UpdateEventCountdown");   // cpp:3392
            break;
        }
    }

    // =======================================================================
    //  ConcludeEventCountdown  @ 0x824748F0
    // =======================================================================
    // Called unconditionally at the end of every UpdateRunning frame. Once the GO banner's
    // reflected deadline passes, retire the icon and disarm the timer with -1.0f (the console's
    // "already concluded" value -- distinct from the 0.0f OnEnter sentinel the GO arm reads).
    void RaceMainHudState::ConcludeEventCountdown()
    {
        if (mbPreRaceCountdown && meCurrentEventCountdownState == E_EVENT_COUNTDOWN_STATE_GO)
        {
            CGS_ASSERT(mpCache != 0, "mpCache");   // cpp:3423 (non-gating)
            if (mpCache->GetTime() >= mfEventCountdownTimer)
            {
                const bool lbRenders = mbPreRaceCountdownRenders;
                meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_DONE;
                if (lbRenders)
                    mEventCountdownIcon.SetState("invisible");
                mfEventCountdownTimer = -1.0f;
            }
        }
    }
}
