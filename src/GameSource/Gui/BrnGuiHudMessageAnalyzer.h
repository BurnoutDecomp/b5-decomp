#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                // CgsID
#include "GameShared/GameClasses/Numeric/CgsRandom.h"      // CgsNumeric::Random (embedded)
#include "GameSource/BurnoutConstants.h"                   // EActiveRaceCarIndex
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"            // GuiHudMessage + the analyzer event-payload family
#include "GameSource/GameState/BrnCgsPlayerName.h"         // CgsNetwork::PlayerName (embedded name slots)

// ============================================================================
// GameSource/Gui/BrnGuiHudMessageAnalyzer.h
//
// BrnGui::HudMessageAnalyzer - watches the GUI event stream and decides which
// on-screen HUD messages to fire (takedowns, revenge updates, skill thresholds,
// eliminations, ...), forwarding them to the HudMessageDirector. Class shape /
// member names / enums from the DecFIGS DWARF (BrnGuiHudMessageAnalyzer.h:48),
// gated on the X360 ledger. This TU bodies TriggerMessage (both overloads),
// HandlePlayerEliminated and HandleLiveRevengeUpdate; the rest of the large DWARF
// surface (Update + the ~40 other Handle*/analysis methods and threshold statics)
// is owned by its remaining ledger functions and grows here additively.
//
// FROZEN (wave-B keystone): the FULL DWARF member tail + method surface is now
// declared -- members in DWARF order at their X360-attested offsets (Construct
// @0x82509060 is the store-map authority; per-member X360 offsets are in the
// comments and pinned by _AssertLayout below). Implementer partfiles body the
// ledger functions WITHOUT touching this header.
//
// X360-ONLY TAIL: the last two members (developer-challenge pending flag + id)
// have no PS3-DWARF entry; they are pinned by Update @0x82525FC0 (reads 0x4F9,
// clears 0x4F9/0x500 around TriggerDeveloperChallengeMessageDEBUG) -- names are
// consumer-inferred (FLAGGED).
// ============================================================================

namespace CgsGui { struct GuiAccessPointers; }
namespace BrnProgression { struct Profile; }   // mpProfile (pointer only; delivered by a GUI event)

namespace BrnGui
{
    class GuiCache;
    struct HudMessageDirector;

    // Payload types consumed only by declaration-only methods (their handlers are other
    // ledger rows / other partfile groups). Defined in BrnGuiEventTypeDefs.h or (opaque)
    // in BrnGuiDemangledEventTypes.h -- pointer-only here.
    struct GuiShutdownEvent;
    struct GuiEventPlayerWrecked;
    struct GuiDriveThroughEvent;
    struct GuiNearMissEvent;
    struct GuiEventBoostInfo;
    struct GuiOncomingEvent;
    struct GuiInAirEvent;
    struct GuiDriftingEvent;
    struct GuiSpinningEvent;
    struct GuiStuntEvent;
    struct GuiOvertakeEvent;
    struct GuiInEventDistanceToFinish;
    struct GuiInEventLeaderSplit;
    struct GuiInEventRivalProgress;
    struct GuiInEventFinisher;
    struct GuiFinishRaceEvent;
    struct GuiOffenceShortcutEvent;
    struct GuiBlueTeamIsEscapingEvent;
    struct GuiBlueTeamIsBehindYouEvent;
    struct GuiLocalPlayerEliminatedEvent;
    struct GuiLastBlueTeamMemberEvent;
    struct GuiLeaderPassedMileBoundaryEvent;
    struct GuiLeaderPassedKMBoundaryEvent;
    struct GuiNetworkPlayerBattlingEvent;
    struct GuiBHRCheckpointReachedEvent;
    struct GuiRaceCheckpointReached;
    struct GuiHUDMessageBHRRunnerCrashed;
    struct GuiEventPlayerReachedRoadRageTarget;
    struct GuiHUDMessageComboPerformed;
    struct GuiHUDMessageShowtimeMultiplier;
    struct GuiHUDMessageSignSmashed;
    struct GuiHUDMessageCrushCombo;
    struct GuiGameModeStarted;
    struct GuiPowerParkResult;
    struct GuiEventJumpStarted;
    struct GuiEventStuntInfo;
    struct GuiEventStuntAllComplete;
    struct GuiEventRoadRagePlayerDamage;

    struct HudMessageAnalyzer
    {
        // DWARF h:62.
        enum EBoostEarningStatus
        {
            E_BOOST_EARNING_STATUS_OFF     = -2,
            E_BOOST_EARNING_STATUS_ON      = -1,
            E_BOOST_EARNING_STATUS_GOOD    = 0,
            E_BOOST_EARNING_STATUS_GREAT   = 1,
            E_BOOST_EARNING_STATUS_AWESOME = 2,
            E_BOOST_EARNING_STATUS_COUNT   = 3,
        };

        // DWARF h:83.
        enum EWreckedMessageState
        {
            E_WRECKED_STATE_NOT_WRECKED   = 0,
            E_WRECKED_STATE_WRECKING      = 1,
            E_WRECKED_STATE_WRECKED       = 2,
            E_WRECKED_STATE_WRECKED_STUNT = 3,
            E_WRECKED_STATE_WRECK_HANDLED = 4,
            E_WRECKED_STATE_WRECK_ABORTED = 5,
            E_WRECKED_STATE_WRECK_COUNT   = 6,
        };

        // ---- module lifecycle (DWARF h:53/h:59/h:75) ----

        // @0x82509060 (this TU) -- zero/default every analyzer slot (statuses to
        // E_BOOST_EARNING_STATUS_OFF, meCrashEntryState to E_CRASHBARSTATE_INVALID,
        // meDirtyTrickPending to the X360 'none' sentinel 3, the change-car/drivable
        // flags to true), seed mRandom (inlined Random::Construct + the 8-slot
        // RandomFloat table refill) and adopt the director.
        void Construct(HudMessageDirector* lpHudMessageDirector);

        // @0x82525FC0 (this TU) -- drain the GUI input queue, dispatch each record by
        // its wire id to the Handle* family (three dense switch tables: id 64+case @
        // jumptable 82526234, id 313+case @ 82526838, id 419+case @ 82526BF0), then run
        // the deferred-message timers (road-rules high score, road-rage target, trophy
        // car, game-completed, player-left, developer challenge). The DWARF spells the
        // first parameter InputBuffer::GuiEventInputQueue -- the same type as this
        // GuiEventQueueBase instance (CgsGuiModuleIO.h typedef); spelled out here to
        // keep the module-IO header out of this class home.
        void Update(const CgsGui::GuiEventQueueBase<32768, 16>* lpGuiEventQueue,
                    CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpViewOutputQueue);

        // DWARF h:75 -- adopt the shared access-pointer block (its own ledger row).
        void SetAccessPointers(CgsGui::GuiAccessPointers* lpAccessPointers);

        // @0x825179E8 (this TU) -- build a parameterless message from its id and
        // hand it to the director. DWARF h:203 attests the const qualifier (wave-C
        // keystone arbitration: the whole message-firing surface is const -- the
        // const Handle* family tail-calls these overloads).
        void TriggerMessage(const char* lpcMessageId) const;

        // @0x82517AF8 -- the prebuilt-message overload (assert the message + the
        // director, then AddMessage(message, false)). Its ledger row is unnamed;
        // recovered here alongside its callers. DWARF h:208 attests const.
        void TriggerMessage(const GuiHudMessage* lpMessage) const;

        // @0x8251B058 (this TU, DWARF cpp) -- "player eliminated" road-rage message
        // with the eliminated player's online name.
        void HandlePlayerEliminated(const s32* lpiEventPayload);

        // @0x8251E1F0 (this TU, DWARF cpp:3590s) -- live-revenge status-change
        // messages ("taken down X", "X got revenge", ...), aggressor/victim keyed
        // against the local player; delayed until after the crash cam when a wreck
        // is in progress.
        void HandleLiveRevengeUpdate(const GuiLiveRevengeUpdateEvent* lpEvent);

        // @0x8251FA90 -- online-stunt-run "rival/team-mate eliminated" HUD messages (keyed on
        // whether the local player was eliminated, the team-mate flag and the "last player
        // standing" flag; the remote case tags the message with the player's online name).
        void HandleOnlineStuntRunElimination(const GuiOnlineStuntRunEliminationEvent* lpEliminationEvent);

        // @0x8251FC08 -- online-stunt-run "now leading" HUD messages (same keying as elimination;
        // the remote-rival case tags the message with the player's online name).
        void HandleOnlineStuntRunLeading(const GuiOnlineStuntRunLeadingEvent* lpLeadingEvent);

        // @0x8251FD68 -- online-stunt-run "victory" HUD messages (same keying as leading).
        void HandleOnlineStuntRunVictory(const GuiOnlineStuntRunVictoryEvent* lpVictoryEvent);

        // @0x8251FEC8 -- online-stunt-run "last run" HUD message (parameterless "OnlSRLastRun").
        void HandleOnlineStuntRunLastRun(const GuiOnlineStuntRunLastRunEvent* lpLastRunEvent);

        // @0x8251FF38 -- online-stunt-run score/time notification HUD message (a team-scored /
        // rival-scored score line, or a "time" line, plus the score/time value as an int param).
        void HandleOnlineStuntRunMessage(const GuiOnlineStuntRunMessageEvent* lpMessageEvent);

    private:
        // DWARF h:300/h:303 -- their own ledger functions (declaration-only here).
        const char* GetOnlineName(EActiveRaceCarIndex leActiveRaceCarIndex, bool* lpbValid) const;

        // DWARF h:220 (@0x824ECF10) -- the network-player-id keyed overload
        // (RoadRulesRecvData::NetworkPlayerID is a typedef of s32). PITFALL: pass a
        // genuine s32 player id -- an EActiveRaceCarIndex argument silently binds the
        // OTHER overload.
        const char* GetOnlineName(s32 liNetworkPlayerID, bool* lpbValid) const;

        // DWARF h:198 -- the hashed-id trigger (sibling of the two committed overloads).
        void TriggerMessage(CgsID lMessageIdHash) const;

        // DWARF h:225 -- landmark display-name lookup (declaration-only).
        void GetLandmarkName(char* lpcBuffer, CgsID lLandmarkId) const;

        // DWARF h:638 -- declaration-only.
        const char* GetMyString(const char* lpcString);

        // DWARF h:229 -- cancel an in-flight wrecked message (declaration-only).
        void AbortWreckedMessage();

        // ---- event handlers (DWARF h:236..h:634; this TU's ledger rows unless noted) ----

        // @0x8251C3C0 -- classify a takedown against the local player and fire the
        // matching HUD line (parks it in mPendingTakedownEvent while a crash bar is up).
        void HandleTakedown(const GuiTakedownEvent* lpTakedown);

        // @0x824F9C28 -- pick the takedown message id: multi-takedowns are skipped
        // (miMultipleTakedownCount > 1), chained takedowns (chain >= 2, non-revenge) use
        // KAPC_TAKEDOWN_CHAIN[chain-2] clamped to its last row, otherwise
        // KAPC_TAKEDOWN_TYPES[meTakedownType].
        void ConstructTakedownMessage(GuiHudMessage* lpHudMessage, const GuiTakedownEvent* lpTakedown);

        void HandleShutdown(const GuiShutdownEvent* lpEvent);            // h:247 (not in this fan-out)
        void HandleEndShutdown();                                        // h:251 (not in this fan-out)

        // @0x8251C7C0 -- crash-bar state machine: fires the parked takedown / dirty-trick
        // / delayed-revenge messages when the crash bar drops.
        void HandleCrashedEvent(GuiPlayerCrashingStateChangeEvent::CrashBarState leCrashBarState);

        // @0x8251CA88 -- wreck-duration state machine (E_WRECKED_STATE_*).
        void HandleWreckedEvent(const GuiEventPlayerWrecked* lpEvent);

        // @0x8251CBF0 -- "rival crashing" gamer-tagged message.
        void HandleNetworkPlayerCrashedEvent(const GuiNetworkPlayerCrashingEvent* lpEvent);

        // @0x824F2E48 -- impact chatter (good/bad list keyed off meImpactType).
        void HandleImpact(const GuiImpactEvent* lpImpactEvent) const;

        void HandleNetworkBattling(const GuiNetworkPlayerBattlingEvent* lpEvent) const;   // h:276 (not in this fan-out)

        // @0x8251CCD8 -- "rival on your tail" gamer-tagged message.
        void HandleNetworkTailing(const GuiNetworkPlayerOnTailEvent* lpEvent) const;

        // @0x8251D078 -- "rival joined your event".
        void HandleJoinedEvent(const GuiEventCarJoinedEvent* lpEvent) const;

        // @0x8251D0E8 -- "you were eliminated" with the finish-position string id.
        void HandleEliminatedEvent(const GuiEventCarEliminatedFromEvent* lpEvent) const;

        void HandleRoadRagePlayerDamageEvent(const GuiEventRoadRagePlayerDamage* lpEvent);  // h:296 (not in this fan-out)

        // @0x8251D1A0 -- burning-route could-not-start (special car id message).
        void HandleFailedToStartBurningRoute(const GuiEventFailedToStartEvent* lpEvent);

        // @0x8251D270 -- rivalry level changes (ERivalryLevels switch).
        void HandleRivalryChangeEvent(const GuiRivalryStatusChange* lpEvent) const;

        // @0x8251D430 -- "your rival is fleeing".
        void HandleRivalFleeingEvent(const GuiRivalIsFleeing* lpEvent) const;

        // @0x8251D4A0 -- pursuit pre-announce ("EventPrePurs" + "CAR_CAPS_<id>"). The
        // X360 body takes no payload (the PS3 DWARF's pointer parameter is unused).
        void HandleStartPursuitEvent();

        void HandleDriveThrough(const GuiDriveThroughEvent* lpEvent);    // h:321 (not in this fan-out)

        // @0x8251D7E8 -- signature-takedown announce (billboard id).
        void HandleSignatureStunt(const GuiSignatureStuntEvent* lpEvent) const;

        void HandleNearMiss(const GuiNearMissEvent* lpEvent) const;      // h:331 (not in this fan-out)

        // @0x8251D868 / @0x8251D988 / @0x8251DF08 -- online dirty-trick lifecycle.
        void HandleDirtyTrickNew(const GuiDirtyTrickNewEvent* lpEvent);
        void HandleDirtyTrickTriggered(const GuiDirtyTrickTriggerEvent* lpEvent);
        void HandleDirtyTrickEnded(const GuiDirtyTrickEndedEvent* lpEvent);

        void HandleOvertake(const GuiOvertakeEvent* lpEvent) const;                        // h:356 (not in this fan-out)
        void HandleEventDistanceToFinish(const GuiInEventDistanceToFinish* lpEvent) const; // h:361 (not in this fan-out)
        void HandleEventLeaderSplitTime(const GuiInEventLeaderSplit* lpEvent) const;       // h:366 (not in this fan-out)
        void HandleEventNeckAndNeck() const;                                               // h:370 (not in this fan-out)
        void HandleEventRivalProgress(const GuiInEventRivalProgress* lpEvent) const;       // h:375 (not in this fan-out)
        void HandleEventFinisher(const GuiInEventFinisher* lpEvent);                       // h:380 (not in this fan-out)

        // @0x8251E790 -- fire the parked online-winner message ("EventRvlWins" +
        // mOnlineWinnerName).
        void TriggerEventFinisher();

        void HandleRoadRageTimeExtension();                              // h:388 (not in this fan-out)

        // @0x8251E820 / @0x8251E938 -- took-the-lead / dropped-to-last chatter.
        void HandleTookLead(const GuiTookLeadEvent* lpEvent) const;
        void HandleTookLast(const GuiTookLastEvent* lpEvent) const;

        void HandleFinishRace(const GuiFinishRaceEvent* lpEvent) const;                    // h:403 (not in this fan-out)
        void HandleShortcut(const GuiOffenceShortcutEvent* lpEvent) const;                 // h:407 (not in this fan-out)
        void HandleBlueTeamIsEscaping(const GuiBlueTeamIsEscapingEvent* lpEvent) const;    // h:410 (not in this fan-out)
        void HandleBlueTeamIsBehindYou(const GuiBlueTeamIsBehindYouEvent* lpEvent) const;  // h:413 (not in this fan-out)
        void HandleLocalPlayerEliminated(const GuiLocalPlayerEliminatedEvent* lpEvent) const;      // h:419 (not in this fan-out)
        void HandleLastBlueTeamMember(const GuiLastBlueTeamMemberEvent* lpEvent) const;            // h:422 (not in this fan-out)
        void HandleLeaderPassedMileBoundary(const GuiLeaderPassedMileBoundaryEvent* lpEvent) const; // h:425 (not in this fan-out)
        void HandleLeaderPassedKMBoundary(const GuiLeaderPassedKMBoundaryEvent* lpEvent) const;     // h:428 (not in this fan-out)

        // @0x8251B0E0 -- online "traitorous takedown" (teammate slam).
        void HandleTraitorousTakedown(const GuiTraitorousTakedownEvent* lpEvent) const;

        // h:435..h:459 -- boost-earning threshold family (not in this fan-out).
        void HandleBoostEarningMessages(const GuiEventBoostInfo* lpEvent);
        void HandleOncomingEarningMessages(const GuiOncomingEvent* lpEvent);
        void HandleAirEarningMessages(const GuiInAirEvent* lpEvent);
        void HandleDriftEarningMessages(const GuiDriftingEvent* lpEvent);
        void HandleSpinEarningMessages(const GuiSpinningEvent* lpEvent);
        void HandleNearMissEarningMessages(const GuiNearMissEvent* lpEvent);
        void HandleStuntsEarningMessages(const GuiStuntEvent* lpEvent);

        void HandleBurningHomeRunCheckpointReached(const GuiBHRCheckpointReachedEvent* lpEvent);  // h:463 (not in this fan-out)
        void HandleRaceCheckpointReached(const GuiRaceCheckpointReached* lpEvent);                // h:468 (not in this fan-out)
        void HandleBurningHomeRunRunnerCrashes(const GuiHUDMessageBHRRunnerCrashed* lpEvent);     // h:473 (not in this fan-out)
        void HandleRoadRageTargetReached(const GuiEventPlayerReachedRoadRageTarget* lpEvent);     // h:478 (not in this fan-out)

        // @0x8251B608 -- skill/stunt chatter off the stunt snapshot's flag masks.
        void HandleStuntPerformed(const GuiHUDMessageStuntPerformed* lpEvent);

        void HandleComboPerformed(const GuiHUDMessageComboPerformed* lpEvent);              // h:486 (not in this fan-out)
        void HandleShowtimeMultiplierMessage(const GuiHUDMessageShowtimeMultiplier* lpEvent); // h:490 (not in this fan-out)
        void HandleSignSmashMessage(const GuiHUDMessageSignSmashed* lpEvent);               // h:494 (not in this fan-out)
        void HandleCrushComboMessage(const GuiHUDMessageCrushCombo* lpEvent);               // h:498 (not in this fan-out)

        // @0x8251C010 -- pass-through title/subtitle message ("Generic").
        void HandleGenericHUDMessage(const GuiGenericHUDMessage* lpEvent);

        // @0x82525E10 -- online team change (forwards to the BHR team-change message).
        void HandleOnlineTeamChangeMessage(const GuiOnlineTeamChangeEvent* lpEvent);

        void HandleChainedBoost(const GuiEventBoostInfo* lpEvent);       // h:510 (not in this fan-out)

        // @0x8251EBD0 -- remote player disconnected (gamer-tagged).
        void HandleRemotePlayerDisconnect(const GuiNetworkRemotePlayerDisconnectEvent* lpEvent) const;

        // @0x8251C110 -- burning-home-run team-change messages (runner vs chaser).
        void HandleBurningHomeRunTeamChange(EActiveRaceCarIndex leActiveRaceCarIndex);

        void HandleGameModeStarted(const GuiGameModeStarted* lpEvent);   // h:525 (not in this fan-out)

        // @0x824F32A0 -- park the 48-byte high-score record + start the road-rules
        // message-delay timer.
        void HandleNewRoadRulesHighScore(const GuiEventRoadRuleNewHighScore* lpEvent);

        // @0x8251F020 -- road-rule failed (time vs crash rule, ruled-by branches).
        void HandleRoadRuleFailed(const GuiEventRoadRuleFail* lpEvent);

        // @0x8251F240 -- showtime entry chatter (local vs remote player).
        void HandleShowtimeModeSwitch(const GuiShowtimeModeSwitch* lpEvent);

        void HandlePowerParkingResult(const GuiPowerParkResult* lpEvent);                 // h:545 (not in this fan-out)

        // @0x8251F458 -- freeburn lobby join chatter.
        void HandlePlayerJoinedLobby(const GuiEventNetworkPlayerJoinedLobby* lpEvent);

        void HandlePlayerLeftLobby(const GuiEventNetworkPlayerLeftLobby* lpEvent);        // h:555 (not in this fan-out)

        // @0x8251F5C0 -- fire the parked "OnlFBLeave" + mOnlinePlayerLeftName message.
        void TriggerPlayerLeftLobby();

        void HandleJumpStarted(const GuiEventJumpStarted* lpEvent);      // h:564 (not in this fan-out)
        void HandleStuntInfo(const GuiEventStuntInfo* lpEvent);          // h:569 (not in this fan-out)

        // @0x8251F6E8 -- "all collectibles of this family in this county done"
        // ("StntAllDone" + completion/county string ids).
        void HandleStuntsComplete(const GuiEventStuntAreaComplete* lpEvent);

        void HandleStuntsComplete(const GuiEventStuntAllComplete* lpEvent);  // h:577 (@0x8251F7E8; not in this fan-out)

        // @0x8251F8B0 -- trophy-car unlocked announce (prints mTrophyCarID).
        void HandleTrophyCarUnlocked(const GuiEventTrophyCarUnlock* lpEvent);

        // @0x8251F970 -- fire the parked challenge-triggered message.
        void TriggerChallengeTriggeredMessage();

        // @0x824F33E0 -- park the 24-byte challenge-end record (statuses 1/2/3/6).
        void HandleChallengeEnded(const GuiChallengeEndEvent* lpEvent);

        void TriggerChallengeEndedMessage();                             // h:595 (not in this fan-out)
        void TriggerNewRoadRulesHighScoreMessage();                      // h:599 (not in this fan-out)
        void HandleAllJunctionsFound();                                  // h:608 (not in this fan-out)
        void HandleAllJunctionsOfTypeFound(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType);   // h:613 (not in this fan-out)
        void HandleAllEventsOfTypeComplete(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType);   // h:618 (not in this fan-out)
        void HandleAllDriveThrusFound();                                 // h:627 (not in this fan-out)

        // FLAG: X360-only sibling (no PS3-DWARF row) -- fires the parked developer
        // challenge message; Update gates it on mbDeveloperChallengeMessagePending.
        void TriggerDeveloperChallengeMessageDEBUG();

        // FLAG: X360-only sibling (no PS3-DWARF row; real named symbol, its OWN ledger
        // row -- do not body in this TU's fan-out). Update's drain case (wire id 596)
        // calls it @0x825275D4 with the raw queue record; the payload is the
        // developer-challenge CgsID it parks (mbDeveloperChallengeMessagePending +
        // mDeveloperChallengeId). Raw-payload-pointer shape follows the committed
        // HandlePlayerEliminated(const s32*) precedent.
        void HandleDeveloperChallengeMessageDEBUG(const CgsID* lpEventPayload);

        // @0x824F3500 -- append the gamer tag (or the given fallback string id) to the
        // message; returns whether the tag resolved.
        bool AddGamerTagToMessage(GuiHudMessage* lpMessage, const char* lpcStringID,
                                  EActiveRaceCarIndex leActiveRaceCarIndex) const;

        // ---- members (DWARF h:97-112 order; the tail is the analysis TUs' concern) ----
        CgsGui::GuiAccessPointers* mpAccessPointers;              // DWARF h:97
        GuiCache*                  mpGuiCache;                    // DWARF h:98
        void*                      mpViewOutputQueue;             // DWARF h:102 (GuiEventQueueLarge*; opaque here)
        HudMessageDirector*        mpHudMessageDirector;          // DWARF h:103 (X360 +12)
        CgsNumeric::Random         mRandom;                       // DWARF h:105
        bool                       mbCrashBoundaryMessagePending; // DWARF h:107
        s32                        meCrashEntryState;             // DWARF h:108 (GuiPlayerCrashingStateChangeEvent::CrashBarState; raw s32 -- enum home pending)
        bool                       mbShowDrivableMessage;         // DWARF h:109
        GuiHudMessage              mDelayedForAfterCrashHudMessage;          // DWARF h:111 (X360 +80, 840B)
        bool                       mbDelayedForAfterCrashHudMessagePending;  // DWARF h:112 (X360 +920/0x398)

        // ---- wave-B tail (DWARF h:114-193 order; X360 offsets from Construct @0x82509060,
        //      HandleCrashedEvent @0x8251C7C0, HandleChallengeEnded @0x824F33E0,
        //      HandleNewRoadRulesHighScore @0x824F32A0, TriggerEventFinisher @0x8251E790,
        //      TriggerPlayerLeftLobby @0x8251F5C0 and Update @0x82525FC0) ----
        EWreckedMessageState       mePlayersCurrentlyWreckState;    // h:114 (X360 0x39C; ctor 0)
        f32                        mfCurrentWreckDuration;          // h:115 (X360 0x3A0; ctor 0.0f)
        bool                       mbTakedownMessagePending;        // h:117 (X360 0x3A4; ctor false)
        GuiTakedownEvent           mPendingTakedownEvent;           // h:118 (X360 0x3A8, 40B; ctor/aborts zero it)
        CgsID                      mShutdownPending;                // h:120 (X360 0x3D0; ctor 0)
        bool                       mbTimeExtMsgPending;             // h:122 (X360 0x3D8; ctor false)
        bool                       mbRoadOneMoreCrashToTotalledPending; // h:123 (X360 0x3D9; ctor false)
        bool                       mbRoadRageTargetReached;         // h:124 (X360 0x3DA; ctor false)
        BrnNetwork::EPaybackType   meDirtyTrickPending;             // h:126 (X360 0x3DC; ctor 3 == the X360 'none' sentinel)
        char                       macDTPendingVictim[64];          // h:127 (X360 0x3E0; not ctor-initialised)
        EBoostEarningStatus        meCurrentBoostStatus;            // h:131 (X360 0x420; ctor OFF)
        f32                        mfBoostDuration;                 // h:133 (X360 0x424; ctor 0.0f)
        EBoostEarningStatus        meCurrentOncomingStatus;         // h:135 (X360 0x428; ctor OFF)
        EBoostEarningStatus        meCurrentAirStatus;              // h:138 (X360 0x42C; ctor OFF)
        EBoostEarningStatus        meCurrentDriftStatus;            // h:141 (X360 0x430; ctor OFF)
        EBoostEarningStatus        meCurrentSpinStatus;             // h:144 (X360 0x434; ctor OFF)
        EBoostEarningStatus        meCurrentCheckingStatus;         // h:147 (X360 0x438; ctor OFF)
        EBoostEarningStatus        meCurrentNearMissStatus;         // h:150 (X360 0x43C; NOT ctor-initialised)
        EBoostEarningStatus        meCurrentStuntChainStatus;       // h:153 (X360 0x440; NOT ctor-initialised)
        s32                        miPlayerStatBoost;               // h:156 (X360 0x444; ctor 0)
        s32                        miPlayerStatSpeed;               // h:157 (X360 0x448; ctor 0)
        s32                        miPlayerStatControl;             // h:158 (X360 0x44C; ctor 0)
        s32                        miPlayerStatStrength;            // h:159 (X360 0x450; ctor 0)
        bool                       mbShowChangeCarMessage;          // h:161 (X360 0x454; ctor TRUE)
        bool                       mbTickerIsActive;                // h:163 (X360 0x455; ctor false)
        bool                       mbBoostOkMessageJustTriggered;   // h:166 (X360 0x456; ctor false)
        bool                       mbChallengeTriggered;            // h:168 (X360 0x457; ctor false)
        bool                       mbChallengeEnded;                // h:169 (X360 0x458; ctor false)
        GuiChallengeEndEvent       mChallengedEndedData;            // h:170 (X360 0x460, 24B copy)
        f32                        mfRoadRulesMessageTimeout;       // h:173 (X360 0x478; ctor 0.0f)
        bool                       mbNewRoadRulesHighScores;        // h:174 (X360 0x47C; ctor false)
        GuiEventRoadRuleNewHighScore mRoadRuleHighScoreData;        // h:175 (X360 0x480, 48B copy)
        s32                        miLastSignMultiplier;            // h:177 (X360 0x4B0; NOT ctor-initialised)
        f32                        mfRoadRageAchievedMessageDelayTime; // h:179 (X360 0x4B4; ctor 0.0f)
        BrnProgression::Profile*   mpProfile;                       // h:181 (X360 0x4B8; ctor NULL -- delivered by a GUI event)
        bool                       mbOnlineWinnerWaiting;           // h:183 (X360 0x4BC; ctor false)
        CgsNetwork::PlayerName     mOnlineWinnerName;               // h:184 (X360 0x4BD, 16B byte-aligned name)
        f32                        mfTimeUntilGameCompletedHudMessage; // h:186 (X360 0x4D0; ctor 0.0f)
        bool                       mbGameCompletedHudMessagePending;   // h:188 (X360 0x4D4; ctor false)
        bool                       mbTrophyCarUnlockPending;        // h:189 (X360 0x4D5; ctor false)
        GuiEventTrophyCarUnlock    mTrophyCarUnlockedEvent;         // h:190 (X360 0x4D8, 16B; ctor zeroes both qwords)
        bool                       mbOnlinePlayerLeft;              // h:192 (X360 0x4E8; ctor false)
        CgsNetwork::PlayerName     mOnlinePlayerLeftName;           // h:193 (X360 0x4E9, 16B byte-aligned name; ctor zeroes byte 0? no -- cleared on trigger)
        // FLAG: X360-only tail (no PS3-DWARF entries) -- consumer-named from Update's
        // developer-challenge block (reads 0x4F9; clears 0x4F9 + 0x500 after firing).
        bool                       mbDeveloperChallengeMessagePending; // (X360 0x4F9; ctor false)
        CgsID                      mDeveloperChallengeId;              // (X360 0x500; ctor 0)

        // ---- class statics (DWARF h:79-171; definitions in BrnGuiHudMessageAnalyzer_wB_res.cpp
        //      where this keystone recovered the values; the rest stay declaration-only
        //      until their consumer functions are reconstructed) ----
        static const CgsID KA_STUNT_INFO_MESSAGES[3];                // h:79 (declaration-only)
        static const CgsID KA_OVER_DRIVE_MESSAGES[4];                // h:80 (declaration-only)
        static const CgsID KA_PLAYER_STATS_LEVEL_UP_MESSAGES[4];     // h:81 (declaration-only)
        static const f32   KF_REQUIRED_WRECK_DURATION;               // h:95 (X360 flt_8206F8E0 == 0.6f; defined in _wB_res)
        static const u32   KU_FREQUENCY_OF_DRIVETHROUGH_MAGIC_MESSAGES = 5;  // h:100
        // h:129 -- PS3 DWARF says [4][12]; the X360 table is [4][14] (one column per
        // BurnoutSkillzData skill; Update indexes 14*row+skill @ qword_82FB56D8 and the
        // static-init @0x82C56198 fills 4x14 slots). X360 wins. Rows follow
        // GuiNewBurnoutHudMessageEvent::EBurnoutSkillzMessageTypes (AbB/AbY/Ag/Yg).
        static const CgsID KA_SKILLZ_MESSAGE_IDS[4][14];             // defined in _wB_res
        static const f32   mafBoostingThresholds[3];                 // h:132 (declaration-only)
        static const s32   maiOncomingThresholds[3];                 // h:136 (declaration-only)
        static const s32   maiAirThresholds[3];                      // h:139 (declaration-only)
        static const s32   maiDriftThresholds[3];                    // h:142 (declaration-only)
        static const s32   maiSpinThresholds[3];                     // h:145 (declaration-only)
        static const s32   maiCheckingThresholds[3];                 // h:148 (declaration-only)
        static const s32   maiNearMissThresholds[3];                 // h:151 (declaration-only)
        static const s32   maiStuntChainThresholds[3];               // h:154 (declaration-only)
        static const CgsID KA_CHALLENGE_END_MESSAGE_IDS[];           // h:171 (declaration-only; size lands with TriggerChallengeEndedMessage)

    public:
        static void _AssertLayout();   // never called; compile-time layout pins below
    };

    // ------------------------------------------------------------------------
    //  Layout pins. The X360 target is 32-bit; the MSVC gate is x64. The class
    //  head holds four pointers and the 840-byte (X360) / 856-byte (host)
    //  GuiHudMessage, so NOTHING here is pinned absolutely. Two relative
    //  anchor blocks instead:
    //    * block A -- every member from mePlayersCurrentlyWreckState (X360
    //      0x39C) up to mfRoadRageAchievedMessageDelayTime, pinned by its X360
    //      delta to that anchor (the run is pointer-free and both targets give
    //      the anchor the same mod-8 phase, so the deltas are target-invariant);
    //    * block B -- everything past the mpProfile pointer, pinned relative to
    //      mbOnlineWinnerWaiting (X360 0x4BC) the same way.
    //  A wrong width / missing pad / reordered member fails the gate.
    // ------------------------------------------------------------------------
    inline void HudMessageAnalyzer::_AssertLayout()
    {
        // embedded-record sizes (pointer-free structs; X360 == host)
        static_assert(sizeof(GuiTakedownEvent)             == 40, "mPendingTakedownEvent span 0x3A8..0x3CF");
        static_assert(sizeof(GuiChallengeEndEvent)         == 24, "mChallengedEndedData span 0x460..0x477");
        static_assert(sizeof(GuiEventRoadRuleNewHighScore) == 48, "mRoadRuleHighScoreData span 0x480..0x4AF");
        static_assert(sizeof(GuiEventTrophyCarUnlock)      == 16, "mTrophyCarUnlockedEvent span 0x4D8..0x4E7");
        static_assert(sizeof(CgsNetwork::PlayerName)       == 16, "byte-aligned 16B name slots @0x4BD/0x4E9");

        #define HMA_A(member, x360off) \
            static_assert(offsetof(HudMessageAnalyzer, member) - offsetof(HudMessageAnalyzer, mePlayersCurrentlyWreckState) \
                          == ((x360off) - 0x39C), #member " @" #x360off)
        HMA_A(mfCurrentWreckDuration,              0x3A0);
        HMA_A(mbTakedownMessagePending,            0x3A4);
        HMA_A(mPendingTakedownEvent,               0x3A8);
        HMA_A(mShutdownPending,                    0x3D0);
        HMA_A(mbTimeExtMsgPending,                 0x3D8);
        HMA_A(mbRoadOneMoreCrashToTotalledPending, 0x3D9);
        HMA_A(mbRoadRageTargetReached,             0x3DA);
        HMA_A(meDirtyTrickPending,                 0x3DC);
        HMA_A(macDTPendingVictim,                  0x3E0);
        HMA_A(meCurrentBoostStatus,                0x420);
        HMA_A(mfBoostDuration,                     0x424);
        HMA_A(meCurrentOncomingStatus,             0x428);
        HMA_A(meCurrentAirStatus,                  0x42C);
        HMA_A(meCurrentDriftStatus,                0x430);
        HMA_A(meCurrentSpinStatus,                 0x434);
        HMA_A(meCurrentCheckingStatus,             0x438);
        HMA_A(meCurrentNearMissStatus,             0x43C);
        HMA_A(meCurrentStuntChainStatus,           0x440);
        HMA_A(miPlayerStatBoost,                   0x444);
        HMA_A(miPlayerStatSpeed,                   0x448);
        HMA_A(miPlayerStatControl,                 0x44C);
        HMA_A(miPlayerStatStrength,                0x450);
        HMA_A(mbShowChangeCarMessage,              0x454);
        HMA_A(mbTickerIsActive,                    0x455);
        HMA_A(mbBoostOkMessageJustTriggered,       0x456);
        HMA_A(mbChallengeTriggered,                0x457);
        HMA_A(mbChallengeEnded,                    0x458);
        HMA_A(mChallengedEndedData,                0x460);
        HMA_A(mfRoadRulesMessageTimeout,           0x478);
        HMA_A(mbNewRoadRulesHighScores,            0x47C);
        HMA_A(mRoadRuleHighScoreData,              0x480);
        HMA_A(miLastSignMultiplier,                0x4B0);
        HMA_A(mfRoadRageAchievedMessageDelayTime,  0x4B4);
        #undef HMA_A

        #define HMA_B(member, x360off) \
            static_assert(offsetof(HudMessageAnalyzer, member) - offsetof(HudMessageAnalyzer, mbOnlineWinnerWaiting) \
                          == ((x360off) - 0x4BC), #member " @" #x360off)
        HMA_B(mOnlineWinnerName,                   0x4BD);
        HMA_B(mfTimeUntilGameCompletedHudMessage,  0x4D0);
        HMA_B(mbGameCompletedHudMessagePending,    0x4D4);
        HMA_B(mbTrophyCarUnlockPending,            0x4D5);
        #undef HMA_B

        // block C -- the 8-aligned mTrophyCarUnlockedEvent resyncs the alignment phase
        // (the host anchor sits at a different mod-8 phase than X360's 0x4BC because the
        // widened mpProfile pointer ends on an 8-boundary), so the B->C seam is 32 bytes
        // on the gate vs the X360's 28. Everything past the seam is pinned to the trophy
        // record, whose own phase matches X360 again.
        #define HMA_C(member, x360off) \
            static_assert(offsetof(HudMessageAnalyzer, member) - offsetof(HudMessageAnalyzer, mTrophyCarUnlockedEvent) \
                          == ((x360off) - 0x4D8), #member " @" #x360off)
        HMA_C(mbOnlinePlayerLeft,                  0x4E8);
        HMA_C(mOnlinePlayerLeftName,               0x4E9);
        HMA_C(mbDeveloperChallengeMessagePending,  0x4F9);
        HMA_C(mDeveloperChallengeId,               0x500);
        #undef HMA_C
    }

    // ------------------------------------------------------------------------
    //  Message-id string tables (DWARF cpp:26-252 names; values dumped from the
    //  X360 rodata -- see BrnGuiHudMessageAnalyzer_wB_res.cpp for the
    //  definitions + the per-table X360 addresses). The original TU kept these
    //  at file scope; they are hoisted to extern here because the reconstructed
    //  TU is split across partfiles.
    // ------------------------------------------------------------------------
    extern const char* const KAPC_TAKEDOWN_TYPES[13];                 // cpp:26 (0x8206F59C)
    extern const char* const KAPC_TAKEDOWN_CHAIN[9];                  // cpp:60 (0x82F278BC; overflow clamps to [8])
    extern const char* const KAPC_PAYBACK_TAKEDOWN_BD_MESSAGES[3];    // cpp:125 (0x8206F618; X360 is 3-entry, not the PS3-DWARF 4)
    extern const char* const KAPC_PAYBACK_TAKEDOWN_GD_MESSAGES[3];    // cpp:137 (0x8206F624; X360 is 3-entry)
    extern const char* const KAPC_FINISH_POSITION_MESSAGES[12];       // cpp:149 (0x82F277E0; 8 uppercase + 4 lowercase slots)
    extern const char* const KAPC_COLLECTABLE_COMPLETION_STRINGID[3]; // cpp:238 (0x8206F684)
    // FLAG: consumer-named -- the county string-id table HandleStuntsComplete indexes
    // (< 5 == BrnWorld county count); no DWARF row names it in this TU.
    extern const char* const KAPC_COUNTY_STRINGID[5];                 // (0x82F27704)
}
