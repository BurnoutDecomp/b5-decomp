#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // + BrnGuiEventTypeDefs + PlayerName

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"              // CgsID (null id == 0)
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"        // GuiEventRoadRagePlayerDamage (raw-record maData)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"         // CgsModule::Event, GuiEventQueueBase, GuiStackEventQueue
#include "GameSource/Gui/BrnGuiCache.h"                     // GuiCache accessors
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"        // HudMessageDirector
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"  // IsRunning / IsShowingResults (challenge gate)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData (online name / ARCI)
#include "GameSource/GameState/Progression/BrnProfile.h"    // BrnProgression::Profile (100%-viewed flag)

// BrnGui::HudMessageAnalyzer::Update -- reconstructed from BURNOUT_X360_ARTIST.XEX @0x82525FC0.
//
// The GUI-event-stream dispatcher: it drains the module input queue, routes every record by
// its wire id to the Handle* family (three dense X360 jump tables -- id 64+case @ 82526234,
// id 313+case @ 82526838, id 419+case @ 82526BF0; folded here into one switch on the absolute
// wire id), then runs the deferred-message timer passes (event-finisher, crash boundary,
// wreck reset, challenge triggered/ended, road-rules high score, developer challenge, road-rage
// target, trophy car, game-completed countdown, player-left).
//
// Header-exposure notes (wave-C keystone; the wave-B gaps are CLOSED):
//  * GuiCache friends HudMessageAnalyzer -- the consumer-carved snapshot members
//    (mbGameplayHudActive / miGameFlowState / the +0x4930 pending cluster) are read by
//    name below, exactly the inlined X360 loads.
//  * Profile::Get/SetOneHundredHudMessageViewed are the DWARF-attested accessors
//    (BrnProfile.h:556/:560); the X360 inlines both here.
//  * HandleDeveloperChallengeMessageDEBUG(const CgsID*) is declared in the frozen header
//    (real named X360 symbol @ the case-596 `bl`; its body is its OWN ledger row).

namespace BrnGui
{

namespace
{
    // DWARF cpp:427-429 places these message-delay thresholds at .cpp file scope (internal
    // linkage). X360 rodata: flt_82001C98 == 1.0f, flt_82004270 == 3.0f, flt_82004A20 == 10.0f.
    const f32 KF_ROADRULEMESSAGEDELAY                 = 1.0f;
    const f32 KF_MIN_ROADRAGE_TARGET_ACHIEVED_DELAY   = 3.0f;
    const f32 KF_TIMEUNTIL_ONE_HUNDRED_MESSAGE        = 10.0f;

    // The X360 skillz dispatch (case 542) inlines the online-player lookup as a maPlayerInfo
    // scan keyed on each record's active-race-car index (the asserts name
    // "mpGuiCache->GetOnlinePlayerInfo( ... )"). Reproduced from the committed
    // GuiCache::GetOnlinePlayerInfo(index) accessor + the record's meActiveRaceCarIndex,
    // returning the matching record or NULL (mirrors the committed HandleDirtyTrick* helper).
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
    GetOnlinePlayerInfoByActiveRaceCarIndex(const GuiCache* lpGuiCache,
                                            EActiveRaceCarIndex leActiveRaceCarIndex)
    {
        for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                lpGuiCache->GetOnlinePlayerInfo(liIndex);

            if (static_cast<EActiveRaceCarIndex>(lpPlayerInfo->meActiveRaceCarIndex) == leActiveRaceCarIndex)
                return lpPlayerInfo;
        }
        return NULL;
    }
}

// @ 0x82525FC0
void HudMessageAnalyzer::Update(const CgsGui::GuiEventQueueBase<32768, 16>* lpGuiEventQueue,
                                CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpViewOutputQueue)
{
    // Entry tripwires (cpp:535 streamed / cpp:538) and adopt the view output queue.
    CGS_ASSERT(lpViewOutputQueue != NULL,
               "View output queue is not valid. Is it definately locked for writing?");
    mpViewOutputQueue = lpViewOutputQueue;
    CGS_ASSERT(lpGuiEventQueue != NULL, "lpEventQueue != NULL");

    bool lbHadWreckEvent = false;   // v84

    const CgsModule::Event* lpEvent = NULL;
    s32 liEventSize = 0;
    s32 liEventType = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

    if (lpEvent != NULL)
    {
        do
        {
            switch (liEventType)
            {
                case 64:   // adopt the shared GuiCache pointer delivered by the first record
                    if (mpGuiCache == NULL)
                    {
                        mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");
                    }
                    break;

                case 93:   // shutdown / sign-multiplier reset
                    mShutdownPending = 0;
                    miLastSignMultiplier = 0;
                    break;

                case 149:
                    HandleGenericHUDMessage(reinterpret_cast<const GuiGenericHUDMessage*>(lpEvent));
                    break;
                case 150:
                    HandleJoinedEvent(reinterpret_cast<const GuiEventCarJoinedEvent*>(lpEvent));
                    break;
                case 151:
                    HandleEliminatedEvent(reinterpret_cast<const GuiEventCarEliminatedFromEvent*>(lpEvent));
                    break;
                case 153:
                    TriggerMessage("WrongWay");
                    break;
                case 168:
                    mbRoadRageTargetReached = true;
                    break;
                case 177:
                    HandleDirtyTrickNew(reinterpret_cast<const GuiDirtyTrickNewEvent*>(lpEvent));
                    break;
                case 179:
                    HandleDirtyTrickTriggered(reinterpret_cast<const GuiDirtyTrickTriggerEvent*>(lpEvent));
                    break;
                case 181:
                    HandleDirtyTrickEnded(reinterpret_cast<const GuiDirtyTrickEndedEvent*>(lpEvent));
                    break;
                case 206:
                    HandleChainedBoost(reinterpret_cast<const GuiEventBoostInfo*>(lpEvent));
                    break;
                case 216:   // "SuperJump" -- only when no online start is in progress
                    if (!mpGuiCache->IsOnlineStartInProgress())
                    {
                        GuiHudMessage lMessage;
                        lMessage.Construct("SuperJump");
                        TriggerMessage(&lMessage);
                    }
                    break;
                case 217:
                    HandleStuntInfo(reinterpret_cast<const GuiEventStuntInfo*>(lpEvent));
                    break;
                case 219:
                    HandleStuntsComplete(reinterpret_cast<const GuiEventStuntAreaComplete*>(lpEvent));
                    break;
                case 220:
                    HandleStuntsComplete(reinterpret_cast<const GuiEventStuntAllComplete*>(lpEvent));
                    break;
                case 267:
                    HandleRemotePlayerDisconnect(
                        reinterpret_cast<const GuiNetworkRemotePlayerDisconnectEvent*>(lpEvent));
                    break;
                case 276:
                    HandlePlayerJoinedLobby(reinterpret_cast<const GuiEventNetworkPlayerJoinedLobby*>(lpEvent));
                    break;
                case 277:
                    HandlePlayerLeftLobby(reinterpret_cast<const GuiEventNetworkPlayerLeftLobby*>(lpEvent));
                    break;

                case 291:   // shares the pending-event-cluster reset with id 320
                case 320:
                {
                    mpGuiCache->mu64PendingEventPayload = 0;
                    if (mpGuiCache->miPendingEventStatusA == 1 || mpGuiCache->miPendingEventStatusA == 2)
                        mpGuiCache->miPendingEventStatusA = 0;
                    if (mpGuiCache->miPendingEventStatusB == 1 || mpGuiCache->miPendingEventStatusB == 2)
                        mpGuiCache->miPendingEventStatusB = 0;
                    break;
                }

                case 305:
                    CGS_ASSERT(lpEvent != NULL, "lpGuiEventAllOfTypeComplete != NULL");
                    HandleAllEventsOfTypeComplete(
                        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                            *reinterpret_cast<const s32*>(lpEvent)));
                    break;
                case 306:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("EvryRivShut");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 310:   // arm the "100% complete online" game-completed countdown
                {
                    // The payload gate is a BYTE load on the X360 (lbz event+0 @0x825266B8).
                    if (mpGuiCache->GetGameMode() != -1
                        && *reinterpret_cast<const bool*>(lpEvent)
                        && !mpGuiCache->GetProfile()->GetOneHundredHudMessageViewed())
                    {
                        mbGameCompletedHudMessagePending = true;
                        mfTimeUntilGameCompletedHudMessage = KF_TIMEUNTIL_ONE_HUNDRED_MESSAGE;
                    }
                    break;
                }
                case 312:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("EvryEventJnc");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 313:
                    CGS_ASSERT(lpEvent != NULL, "lpGuiEventAllJunctionsDiscoveredOfType != NULL");
                    HandleAllJunctionsOfTypeFound(
                        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                            *reinterpret_cast<const s32*>(lpEvent)));
                    break;
                case 315:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("EvryDrveThru");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 316:
                    HandleFailedToStartBurningRoute(reinterpret_cast<const GuiEventFailedToStartEvent*>(lpEvent));
                    break;
                case 322:
                    mbTimeExtMsgPending = false;
                    mbRoadOneMoreCrashToTotalledPending = false;
                    mbRoadRageTargetReached = false;
                    break;
                case 337:
                    HandleRoadRuleFailed(reinterpret_cast<const GuiEventRoadRuleFail*>(lpEvent));
                    break;
                case 342:
                    HandleNewRoadRulesHighScore(reinterpret_cast<const GuiEventRoadRuleNewHighScore*>(lpEvent));
                    break;
                case 348:   // road-rage "one more crash to totalled" flag (byte @ record+4)
                    if (reinterpret_cast<const GuiEventRoadRagePlayerDamage*>(lpEvent)->maData[4])
                        mbRoadOneMoreCrashToTotalledPending = true;
                    break;
                case 350:   // adopt the player profile delivered by the event
                {
                    BrnProgression::Profile* lpProfile =
                        *reinterpret_cast<BrnProgression::Profile* const*>(lpEvent);
                    mpProfile = lpProfile;
                    CGS_ASSERT(lpProfile != NULL, "mpProfile");
                    break;
                }
                case 363:
                    HandleTakedown(reinterpret_cast<const GuiTakedownEvent*>(lpEvent));
                    break;
                case 365:
                    HandleImpact(reinterpret_cast<const GuiImpactEvent*>(lpEvent));
                    break;
                case 366:
                    HandleDriveThrough(reinterpret_cast<const GuiDriveThroughEvent*>(lpEvent));
                    break;
                case 368:
                    HandleSignatureStunt(reinterpret_cast<const GuiSignatureStuntEvent*>(lpEvent));
                    break;
                case 369:
                    HandleLiveRevengeUpdate(reinterpret_cast<const GuiLiveRevengeUpdateEvent*>(lpEvent));
                    break;
                case 375:   // park the trophy-car-unlocked record (mode must be valid)
                    if (mpGuiCache->GetGameMode() != -1)
                    {
                        mbTrophyCarUnlockPending = true;
                        mTrophyCarUnlockedEvent = *reinterpret_cast<const GuiEventTrophyCarUnlock*>(lpEvent);
                    }
                    break;
                case 377:   // crash-bar boundary bookkeeping
                {
                    const GuiPlayerCrashingStateChangeEvent* lpCrashEvent =
                        reinterpret_cast<const GuiPlayerCrashingStateChangeEvent*>(lpEvent);
                    mbCrashBoundaryMessagePending = true;
                    meCrashEntryState = static_cast<s32>(lpCrashEvent->meCurrentState);
                    if (lpCrashEvent->meCurrentState != 0)
                        mbShowDrivableMessage = true;
                    break;
                }
                case 378:   // fire the deferred "Crashed" line
                    if (meCrashEntryState == 0 && mbShowDrivableMessage)
                    {
                        GuiHudMessage lMessage;
                        lMessage.Construct("Crashed");
                        TriggerMessage(&lMessage);
                        mbShowDrivableMessage = false;
                    }
                    break;
                case 397:
                    HandleShowtimeModeSwitch(reinterpret_cast<const GuiShowtimeModeSwitch*>(lpEvent));
                    break;
                case 399:
                    HandleShowtimeMultiplierMessage(reinterpret_cast<const GuiHUDMessageShowtimeMultiplier*>(lpEvent));
                    break;
                case 400:
                    HandleSignSmashMessage(reinterpret_cast<const GuiHUDMessageSignSmashed*>(lpEvent));
                    break;
                case 401:
                    HandleCrushComboMessage(reinterpret_cast<const GuiHUDMessageCrushCombo*>(lpEvent));
                    break;
                case 404:
                    HandlePowerParkingResult(reinterpret_cast<const GuiPowerParkResult*>(lpEvent));
                    break;
                case 419:
                    HandleEventDistanceToFinish(reinterpret_cast<const GuiInEventDistanceToFinish*>(lpEvent));
                    break;
                case 420:
                    HandleEventLeaderSplitTime(reinterpret_cast<const GuiInEventLeaderSplit*>(lpEvent));
                    break;
                case 421:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("EventNeck");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 423:
                    HandleEventFinisher(reinterpret_cast<const GuiInEventFinisher*>(lpEvent));
                    break;
                case 425:
                    HandleRaceCheckpointReached(reinterpret_cast<const GuiRaceCheckpointReached*>(lpEvent));
                    break;
                case 429:
                    HandleStuntPerformed(reinterpret_cast<const GuiHUDMessageStuntPerformed*>(lpEvent));
                    break;
                case 430:   // combo -- gated on a valid game-flow state
                    if (mpGuiCache != NULL && mpGuiCache->miGameFlowState != -1)
                        HandleComboPerformed(reinterpret_cast<const GuiHUDMessageComboPerformed*>(lpEvent));
                    break;
                case 431:
                    TriggerMessage("StuntTimeUp");
                    break;
                case 433:
                    HandleStartPursuitEvent();
                    break;
                case 439:
                    HandleRivalryChangeEvent(reinterpret_cast<const GuiRivalryStatusChange*>(lpEvent));
                    break;
                case 440:
                    HandleRivalFleeingEvent(reinterpret_cast<const GuiRivalIsFleeing*>(lpEvent));
                    break;
                case 445:
                    HandleOnlineTeamChangeMessage(reinterpret_cast<const GuiOnlineTeamChangeEvent*>(lpEvent));
                    break;
                case 446:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("OnlRRBlEscpe");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 447:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("OnlRRRacerBh");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 448:
                    HandleLeaderPassedMileBoundary(reinterpret_cast<const GuiLeaderPassedMileBoundaryEvent*>(lpEvent));
                    break;
                case 449:
                    HandleLeaderPassedKMBoundary(reinterpret_cast<const GuiLeaderPassedKMBoundaryEvent*>(lpEvent));
                    break;
                case 450:
                    HandlePlayerEliminated(reinterpret_cast<const s32*>(lpEvent));
                    break;
                case 451:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("OnlRRYouElim");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 452:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("OnlRRLastBl");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 453:
                    HandleTraitorousTakedown(reinterpret_cast<const GuiTraitorousTakedownEvent*>(lpEvent));
                    break;
                case 454:
                    HandleBurningHomeRunCheckpointReached(
                        reinterpret_cast<const GuiBHRCheckpointReachedEvent*>(lpEvent));
                    break;
                case 455:
                    HandleBurningHomeRunRunnerCrashes(
                        reinterpret_cast<const GuiHUDMessageBHRRunnerCrashed*>(lpEvent));
                    break;
                case 482:
                    HandleNetworkPlayerCrashedEvent(
                        reinterpret_cast<const GuiNetworkPlayerCrashingEvent*>(lpEvent));
                    break;
                case 483:
                    HandleNetworkBattling(reinterpret_cast<const GuiNetworkPlayerBattlingEvent*>(lpEvent));
                    break;
                case 484:
                    HandleTookLead(reinterpret_cast<const GuiTookLeadEvent*>(lpEvent));
                    break;
                case 485:
                    HandleTookLast(reinterpret_cast<const GuiTookLastEvent*>(lpEvent));
                    break;
                case 486:
                    HandleNetworkTailing(reinterpret_cast<const GuiNetworkPlayerOnTailEvent*>(lpEvent));
                    break;
                case 487:
                    HandleOnlineStuntRunElimination(
                        reinterpret_cast<const GuiOnlineStuntRunEliminationEvent*>(lpEvent));
                    break;
                case 488:
                    HandleOnlineStuntRunLeading(reinterpret_cast<const GuiOnlineStuntRunLeadingEvent*>(lpEvent));
                    break;
                case 489:
                    HandleOnlineStuntRunVictory(reinterpret_cast<const GuiOnlineStuntRunVictoryEvent*>(lpEvent));
                    break;
                case 490:
                    HandleOnlineStuntRunLastRun(reinterpret_cast<const GuiOnlineStuntRunLastRunEvent*>(lpEvent));
                    break;
                case 491:
                    HandleOnlineStuntRunMessage(reinterpret_cast<const GuiOnlineStuntRunMessageEvent*>(lpEvent));
                    break;

                case 538:   // ticker on/off
                    mbTickerIsActive = *reinterpret_cast<const bool*>(lpEvent);
                    break;
                case 540:   // toggle the "change car" prompt
                    mbShowChangeCarMessage = (mbShowChangeCarMessage == false);
                    break;

                case 542:   // burnout-skillz "X beat Y's record" HUD flash
                {
                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");   // cpp:1220
                    const FreeburnChallengeManager* lpChallengeManager =
                        mpGuiCache->GetFreeburnChallengeManager();

                    // Suppress the flash while a freeburn challenge is live (running / showing results).
                    if (!(lpChallengeManager->IsRunning() || lpChallengeManager->IsShowingResults()))
                    {
                        const GuiNewBurnoutHudMessageEvent* lpHudMessageEvent =
                            reinterpret_cast<const GuiNewBurnoutHudMessageEvent*>(lpEvent);
                        const CgsID lMessageId =
                            KA_SKILLZ_MESSAGE_IDS[lpHudMessageEvent->meMessageType][lpHudMessageEvent->meSkill];

                        if (lMessageId != 0)
                        {
                            GuiHudMessage lMessage;
                            lMessage.Construct(lMessageId);

                            bool lbValid = true;
                            switch (lpHudMessageEvent->meMessageType)
                            {
                                case GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_BEAT_YS:
                                {
                                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache != NULL");   // cpp:1237
                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpNewOwner =
                                        GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache,
                                                                                lpHudMessageEvent->meNewOwner);
                                    CGS_ASSERT(lpNewOwner != NULL,
                                        "mpGuiCache->GetOnlinePlayerInfo( lpHudMessageEvent->meNewOwner ) != NULL"); // cpp:1238

                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpFirst =
                                        GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache,
                                                                                lpHudMessageEvent->meNewOwner);
                                    if (lpFirst == NULL)
                                    {
                                        lbValid = false;
                                    }
                                    else
                                    {
                                        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpSecond =
                                            GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache,
                                                                                    lpHudMessageEvent->mePreviousOwner);
                                        if (lpSecond == NULL)
                                        {
                                            lbValid = false;
                                        }
                                        else
                                        {
                                            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                                                              lpFirst->mPlayerName.macName);
                                            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                                                              lpSecond->mPlayerName.macName);
                                        }
                                    }
                                    break;
                                }
                                case GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_BEAT_YOUR:
                                {
                                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache != NULL");   // cpp:1264
                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpOwner =
                                        GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache,
                                                                                lpHudMessageEvent->meNewOwner);
                                    if (lpOwner == NULL)
                                        lbValid = false;
                                    else
                                        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                                                          lpOwner->mPlayerName.macName);
                                    break;
                                }
                                case GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_GOT:
                                {
                                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache != NULL");   // cpp:1281
                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpOwner =
                                        GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache,
                                                                                lpHudMessageEvent->meNewOwner);
                                    if (lpOwner == NULL)
                                        lbValid = false;
                                    else
                                        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                                                          lpOwner->mPlayerName.macName);
                                    break;
                                }
                                case GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_YOU_GOT:
                                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache != NULL");   // cpp:1298
                                    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                                                      "PLAYER_NAME_STRING_ID");
                                    break;
                                default:
                                    // X360 streams "Invalid state : " + meMessageType (folded); the
                                    // release flow continues (lbValid untouched).
                                    CGS_ASSERT(false, "Invalid state : ");   // cpp:1305
                                    break;
                            }

                            if (lbValid)
                            {
                                if (static_cast<s32>(lpHudMessageEvent->meSkill) == 12
                                    || static_cast<s32>(lpHudMessageEvent->meSkill) == 13)
                                {
                                    CGS_ASSERT(lpHudMessageEvent->mRoadID != 0,
                                               "lpHudMessageEvent->mRoadID != kCGSID_NULL");   // cpp:1318
                                    char lacRoadNumber[12];
                                    CgsCore::SPrintf(lacRoadNumber, 12, "%llu", lpHudMessageEvent->mRoadID);
                                    const s32 liRoadSlot =
                                        (lpHudMessageEvent->meMessageType != 0) ? 2 : 0;
                                    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, liRoadSlot,
                                                      lacRoadNumber);
                                }
                                TriggerMessage(&lMessage);
                            }
                        }
                    }
                    break;
                }

                case 548:   // wrecked-state machine (marks a wreck event this frame)
                    lbHadWreckEvent = true;
                    HandleWreckedEvent(reinterpret_cast<const GuiEventPlayerWrecked*>(lpEvent));
                    break;
                case 549:
                    TriggerMessage("JumpFailed");
                    break;
                case 550:   // BYTE payload on the X360 (lbz event+0 @0x82526F00)
                    if (*reinterpret_cast<const bool*>(lpEvent))
                        TriggerMessage("TimeUpPos");
                    else
                        TriggerMessage("TimeUpNeg");
                    break;
                case 551:
                    TriggerMessage("CantPaintCar");
                    break;
                case 552:
                    TriggerMessage("NeedToFixCar");
                    break;
                case 576:   // park the challenge-triggered flag (clears any pending challenge-end)
                    mbChallengeTriggered = true;
                    mbChallengeEnded = false;
                    break;
                case 578:
                    HandleChallengeEnded(reinterpret_cast<const GuiChallengeEndEvent*>(lpEvent));
                    break;
                case 596:   // park the developer-challenge id (handler = its own ledger row)
                    HandleDeveloperChallengeMessageDEBUG(reinterpret_cast<const CgsID*>(lpEvent));
                    break;

                default:
                    break;
            }

            liEventType = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }
        while (lpEvent != NULL);
    }

    // ---- deferred-message timer passes (run once per frame after the drain) ----

    // Parked online-winner "EventRvlWins".
    if (mbOnlineWinnerWaiting)
    {
        if (mpGuiCache->mbGameplayHudActive)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
                TriggerEventFinisher();
        }
    }

    // Crash-boundary message (fires the parked takedown / dirty-trick / delayed revenge).
    if (mbCrashBoundaryMessagePending)
    {
        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");   // cpp:1471
        if (mpGuiCache->mbGameplayHudActive)
        {
            HandleCrashedEvent(
                static_cast<GuiPlayerCrashingStateChangeEvent::CrashBarState>(meCrashEntryState));
            mbCrashBoundaryMessagePending = false;
        }
    }

    // Reset the wreck-duration state machine when no wreck event arrived this frame.
    if (!lbHadWreckEvent)
    {
        mfCurrentWreckDuration = 0.0f;
        mePlayersCurrentlyWreckState = E_WRECKED_STATE_NOT_WRECKED;
    }

    // Parked challenge-triggered message.
    if (mbChallengeTriggered)
    {
        if (mpGuiCache->mbGameplayHudActive)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
                TriggerChallengeTriggeredMessage();
        }
    }

    // Parked challenge-ended message.
    if (mbChallengeEnded)
    {
        if (mpGuiCache->mbGameplayHudActive)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
                TriggerChallengeEndedMessage();
        }
    }

    // Road-rules new-high-score message delay.
    if (mbNewRoadRulesHighScores)
    {
        const s32 liGameModeType = mpGuiCache->GetGameMode();
        if (!(liGameModeType == 2 || liGameModeType == 16))
        {
            if (mpGuiCache->mbGameplayHudActive)
            {
                if (!mpGuiCache->IsRaceCarCrashing(
                        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex())))
                {
                    const f32 lfTimeout = mpGuiCache->GetTimeStep() + mfRoadRulesMessageTimeout;
                    mfRoadRulesMessageTimeout = lfTimeout;
                    if (lfTimeout > KF_ROADRULEMESSAGEDELAY)
                    {
                        TriggerNewRoadRulesHighScoreMessage();
                        mfRoadRulesMessageTimeout = 0.0f;
                        mbNewRoadRulesHighScores = false;
                    }
                }
            }
        }
    }

    // Developer-challenge message (X360 debug build).
    if (mbDeveloperChallengeMessagePending)
    {
        const s32 liGameModeType = mpGuiCache->GetGameMode();
        if (!(liGameModeType == 2 || liGameModeType == 16))
        {
            if (mpGuiCache->mbGameplayHudActive)
            {
                const s32 liGameFlowState = mpGuiCache->miGameFlowState;
                if (liGameFlowState == 1 || liGameFlowState == 3)
                {
                    TriggerDeveloperChallengeMessageDEBUG();
                    mbDeveloperChallengeMessagePending = false;
                    mDeveloperChallengeId = 0;
                }
            }
        }
    }

    // Road-rage target-achieved message delay ("RRTargGot").
    if (mbRoadRageTargetReached)
    {
        const f32 lfDelay = mpGuiCache->GetTimeStep() + mfRoadRageAchievedMessageDelayTime;
        mfRoadRageAchievedMessageDelayTime = lfDelay;
        if (lfDelay > KF_MIN_ROADRAGE_TARGET_ACHIEVED_DELAY)
        {
            if (meCrashEntryState == 1 || meCrashEntryState == 3)
            {
                GuiHudMessage lMessage;
                lMessage.Construct("RRTargGot");
                TriggerMessage(&lMessage);
                mfRoadRageAchievedMessageDelayTime = 0.0f;
                mbRoadRageTargetReached = false;
            }
        }
    }

    // Trophy-car-unlocked message.
    if (mbTrophyCarUnlockPending)
    {
        if (mpGuiCache->mbGameplayHudActive)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
            {
                HandleTrophyCarUnlocked(&mTrophyCarUnlockedEvent);
                mbTrophyCarUnlockPending = false;
                mTrophyCarUnlockedEvent.meUnlockType = 0;
                mTrophyCarUnlockedEvent.mTrophyCarID = 0;
            }
        }
    }

    // Game-completed ("100%") countdown -> "GCompOnline".
    if (mfTimeUntilGameCompletedHudMessage > 0.0f)
    {
        if (mbGameCompletedHudMessagePending)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
            {
                const f32 lfRemaining = mfTimeUntilGameCompletedHudMessage - mpGuiCache->GetTimeStep();
                mfTimeUntilGameCompletedHudMessage = lfRemaining;
                if (lfRemaining <= 0.0f)
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("GCompOnline");
                    TriggerMessage(&lMessage);

                    BrnProgression::Profile* lpProfile = mpProfile;
                    mfTimeUntilGameCompletedHudMessage = 0.0f;
                    mbGameCompletedHudMessagePending = false;
                    if (lpProfile != NULL)
                        lpProfile->SetOneHundredHudMessageViewed(true);
                }
            }
        }
    }

    // Parked player-left-lobby message.
    if (mbOnlinePlayerLeft)
    {
        if (mpGuiCache->mbGameplayHudActive)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
                TriggerPlayerLeftLobby();
        }
    }

    mpViewOutputQueue = NULL;
}

} // namespace BrnGui
