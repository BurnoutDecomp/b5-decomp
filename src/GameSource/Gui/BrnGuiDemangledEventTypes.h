#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"   // CgsGui::GuiEvent<N> (12-byte event header)
#include "GameShared/GameClasses/Core/CgsID.h"        // CgsID (GuiPlayerInfoResponse::mCarId)

// ============================================================================
// b5-decomp/src/GameSource/Gui/BrnGuiDemangledEventTypes.h
//
// Canonical homes for the BrnGui GUI-event PAYLOAD types that the 270
// CgsGui::GuiModule::AddGuiEvent<T> template instantiations queue (X360 spine).
// Each (id,size) pair is the two compile-time literals of that type's
// AddEvent(&event, id, sizeof(T)) call, read straight from the X360 ARTIST asm
// (0x823Cxxxx..0x823DAxxx) and cross-checked against gui_event_types.csv.
//
// Modelling recipe (honest placeholder per the GuiEventLoadRequest precedent in
// CgsGuiEvent.h -- the total record size + event id are X360 facts; the inner
// field breakdown is not recovered, so the payload is opaque):
//   * size >= 12 and 4-aligned: derive from CgsGui::GuiEvent<id> (the 12-byte
//     event header carrying GetEventType()==id) + u8 maPayload[size-12].
//   * otherwise (size < 12, or a size the 4-aligned GuiEvent header cannot reach
//     exactly): a raw byte struct of the attested size with its own
//     GetEventType()==id (it cannot host the 4-aligned 12-byte GuiEvent base at
//     the attested size without the compiler padding it out).
//
// The AddGuiEvent<T> body reads id via lpEvent->GetEventType() and size via
// sizeof(T); both fall out of the type, so one template body reproduces every
// per-T (id,size) constant pair store-for-store.
// ============================================================================

namespace BrnGui
{
    // NOTE (HudMessageAnalyzer keystone, wave B): the analyzer-consumed payloads formerly
    // modelled opaquely here (GuiTakedownEvent, GuiChallengeEndEvent, the dirty-trick trio,
    // GuiEventRoadRuleFail/NewHighScore, GuiEventTrophyCarUnlock, GuiImpactEvent, the
    // took-lead/last + network-player + lobby + rivalry + showtime + stunt-performed/area
    // records, GuiPlayerCrashingStateChangeEvent, GuiGenericHUDMessage kin, ...) now live
    // with their REAL DWARF field shapes in GameSource/Gui/BrnGuiEventTypeDefs.h (one
    // definition per type; same X360 (id,size) pairs, asserted there). Do not re-add them
    // here.
    struct GuiAftertouchEvent : public CgsGui::GuiEvent<403> { u8 maPayload[20]; };  // id 403 size 32 (12B GuiEvent header + opaque payload)
    struct GuiAttackScoreUpdate : public CgsGui::GuiEvent<428> { u8 maPayload[28]; };  // id 428 size 40 (12B GuiEvent header + opaque payload)
    struct GuiAutosaveRequestEvent { u8 maData[1]; s32 GetEventType() const { return 356; } };  // id 356 size 1 (raw; size not GuiEvent-shaped)
    struct GuiBHRCheckpointReachedEvent { u8 maData[8]; s32 GetEventType() const { return 454; } };  // id 454 size 8 (raw; size not GuiEvent-shaped)
    struct GuiBlueTeamIsBehindYouEvent { u8 maData[1]; s32 GetEventType() const { return 447; } };  // id 447 size 1 (raw; size not GuiEvent-shaped)
    struct GuiBlueTeamIsEscapingEvent { u8 maData[1]; s32 GetEventType() const { return 446; } };  // id 446 size 1 (raw; size not GuiEvent-shaped)
    struct GuiCarSelectAbortEvent { u8 maData[1]; s32 GetEventType() const { return 84; } };  // id 84 size 1 (raw; size not GuiEvent-shaped)
    struct GuiCarSelectOnlineTimeLeftEvent { u8 maData[4]; s32 GetEventType() const { return 82; } };  // id 82 size 4 (raw; size not GuiEvent-shaped)
    struct GuiCarSelectReadyToExitEvent { u8 maData[1]; s32 GetEventType() const { return 564; } };  // id 564 size 1 (raw; size not GuiEvent-shaped)
    struct GuiCarSelectStartEvent { u8 maData[4]; s32 GetEventType() const { return 81; } };  // id 81 size 4 (raw; size not GuiEvent-shaped)
    struct GuiCarSelectionChangedDropIn { u8 maData[8]; s32 GetEventType() const { return 565; } };  // id 565 size 8 (raw; size not GuiEvent-shaped)
    struct GuiCarSelectionChangedOnline { u8 maData[8]; s32 GetEventType() const { return 566; } };  // id 566 size 8 (raw; size not GuiEvent-shaped)
    struct GuiCarSelectionEvent : public CgsGui::GuiEvent<412> { u8 maPayload[1060]; };  // id 412 size 1072 (12B GuiEvent header + opaque payload)
    struct GuiCarUnlockEvent { u8 maData[8]; s32 GetEventType() const { return 76; } };  // id 76 size 8 (raw; size not GuiEvent-shaped)
    struct GuiCarUnlockNewCarEvent { u8 maData[8]; s32 GetEventType() const { return 73; } };  // id 73 size 8 (raw; size not GuiEvent-shaped)
    struct GuiCarUnlockStartEvent { u8 maData[1]; s32 GetEventType() const { return 75; } };  // id 75 size 1 (raw; size not GuiEvent-shaped)
    struct GuiCarUnlockedLiveryEvent : public CgsGui::GuiEvent<413> { u8 maPayload[60]; };  // id 413 size 72 (12B GuiEvent header + opaque payload)
    struct GuiChallengeNotActiveStartEvent : public CgsGui::GuiEvent<583> { u8 maPayload[28]; };  // id 583 size 40 (12B GuiEvent header + opaque payload)
    struct GuiChangeCarEvent { u8 maData[8]; s32 GetEventType() const { return 415; } };  // id 415 size 8 (raw; size not GuiEvent-shaped)
    struct GuiCompletedStuntEvent : public CgsGui::GuiEvent<390> { u8 maPayload[20]; };  // id 390 size 32 (12B GuiEvent header + opaque payload)
    struct GuiCrashComboEvent { u8 maData[8]; s32 GetEventType() const { return 347; } };  // id 347 size 8 (raw; size not GuiEvent-shaped)
    struct GuiCrashScoreUpdate : public CgsGui::GuiEvent<434> { u8 maPayload[4]; };  // id 434 size 16 (12B GuiEvent header + opaque payload)
    struct GuiDeveloperChallengesCompleted { u8 maData[8]; s32 GetEventType() const { return 596; } };  // id 596 size 8 (raw; size not GuiEvent-shaped)
    struct GuiDriftingEvent { u8 maData[4]; s32 GetEventType() const { return 385; } };  // id 385 size 4 (raw; size not GuiEvent-shaped)
    struct GuiDriveThroughEvent { u8 maData[8]; s32 GetEventType() const { return 366; } };  // id 366 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEnteredJunkyard { u8 maData[1]; s32 GetEventType() const { return 79; } };  // id 79 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventAllJunctionsDiscoveredOfType { u8 maData[4]; s32 GetEventType() const { return 313; } };  // id 313 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventAllOfRivalsShutdown { u8 maData[1]; s32 GetEventType() const { return 306; } };  // id 306 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventAllOfTypeComplete { u8 maData[4]; s32 GetEventType() const { return 305; } };  // id 305 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventBoostBarStuntInfo : public CgsGui::GuiEvent<218> {};  // id 218 size 12
    struct GuiEventBoostInfo : public CgsGui::GuiEvent<206> { u8 maPayload[16]; };  // id 206 size 28 (12B GuiEvent header + opaque payload)
    struct GuiEventBuddyNotification : public CgsGui::GuiEvent<105> { u8 maPayload[12]; };  // id 105 size 24 (12B GuiEvent header + opaque payload)
    struct GuiEventCamPicCompressed : public CgsGui::GuiEvent<569> {};  // id 569 size 12
    struct GuiEventCanSkipCrash { u8 maData[1]; s32 GetEventType() const { return 547; } };  // id 547 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventCantPaintCar { u8 maData[1]; s32 GetEventType() const { return 551; } };  // id 551 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventChallengedEventDataResponse : public CgsGui::GuiEvent<332> { u8 maPayload[20]; };  // id 332 size 32 (12B GuiEvent header + opaque payload)
    struct GuiEventChangeDistrict : public CgsGui::GuiEvent<169> {};  // id 169 size 12
    struct GuiEventCurrentStatus : public CgsGui::GuiEvent<492> { u8 maPayload[108]; };  // id 492 size 120 (12B GuiEvent header + opaque payload)
    struct GuiEventDirectorSettings { u8 maData[4]; s32 GetEventType() const { return 475; } };  // id 475 size 4 (raw; size not GuiEvent-shaped)
    // id 314 size 12. X360 BrnGui::OdometerComponent::HandleDriveThruDiscovered (@0x8242C000)
    // proves the 12-byte record is three s32 words: the drive-thru type (switch selector, 5
    // cases 0..4), the total-of-type count, and the discovered-of-type count (read at +0/+4/+8).
    // Modelled as the raw-struct form (the payload does not carry a GuiEvent header -- its own
    // GetEventType() returns the id), per this header's modelling recipe; sizeof stays 12 and
    // GetEventType() stays 314, so the AddGuiEvent<T> instantiation is unaffected.
    struct GuiEventDriveThruDiscovered
    {
        s32 meDriveThruType;    // +0x00 (switch selector: 0 junk yard, 1 gas, 2 body, 3 paint, 4 car park)
        s32 miNumTotal;         // +0x04
        s32 miNumDiscovered;    // +0x08
        s32 GetEventType() const { return 314; }
    };
    struct GuiEventEnterEventStartLocation { u8 maData[8]; s32 GetEventType() const { return 166; } };  // id 166 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventEnterLandmarkArea { u8 maData[2]; s32 GetEventType() const { return 165; } };  // id 165 size 2 (raw; size not GuiEvent-shaped)
    struct GuiEventEventStateResponse : public CgsGui::GuiEvent<556> { u8 maPayload[1392]; };  // id 556 size 1404 (12B GuiEvent header + opaque payload)
    struct GuiEventFburnChallengeEveryPlayerStatus : public CgsGui::GuiEvent<581> { u8 maPayload[2092]; };  // id 581 size 2104 (12B GuiEvent header + opaque payload)
    struct GuiEventFinishedModeResults { u8 maData[1]; s32 GetEventType() const { return 321; } };  // id 321 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventGameCompleted { u8 maData[2]; s32 GetEventType() const { return 309; } };  // id 309 size 2 (raw; size not GuiEvent-shaped)
    struct GuiEventGameCompletedOnline { u8 maData[2]; s32 GetEventType() const { return 310; } };  // id 310 size 2 (raw; size not GuiEvent-shaped)
    struct GuiEventHideDriveThru { u8 maData[8]; s32 GetEventType() const { return 201; } };  // id 201 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventInviteComplete { u8 maData[1]; s32 GetEventType() const { return 132; } };  // id 132 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventInviteFailed { u8 maData[4]; s32 GetEventType() const { return 133; } };  // id 133 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventJumpStarted { u8 maData[8]; s32 GetEventType() const { return 216; } };  // id 216 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventLiveRevengeProfileData { u8 maData[4]; s32 GetEventType() const { return 351; } };  // id 351 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventLoadImageFiles : public CgsGui::GuiEvent<359> { u8 maPayload[36]; };  // id 359 size 48 (12B GuiEvent header + opaque payload)
    struct GuiEventMedalUpdate { u8 maData[8]; s32 GetEventType() const { return 307; } };  // id 307 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventMiniMapSwitch { u8 maData[1]; s32 GetEventType() const { return 205; } };  // id 205 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventMustFixCarFirst { u8 maData[1]; s32 GetEventType() const { return 552; } };  // id 552 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventNetworkLeftGame { u8 maData[8]; s32 GetEventType() const { return 273; } };  // id 273 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventNetworkLobbyPlayerList : public CgsGui::GuiEvent<244> { u8 maPayload[444]; };  // id 244 size 456 (12B GuiEvent header + opaque payload)
    struct GuiEventNetworkPlayerImage { u8 maData[8]; s32 GetEventType() const { return 258; } };  // id 258 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventNetworkPlayerList : public CgsGui::GuiEvent<243> { u8 maPayload[156]; };  // id 243 size 168 (12B GuiEvent header + opaque payload)
    struct GuiEventNetworkPlayerStatus : public CgsGui::GuiEvent<245> { u8 maPayload[2532]; };  // id 245 size 2544 (12B GuiEvent header + opaque payload)
    struct GuiEventNetworkPostGameProcessingFinished { u8 maData[1]; s32 GetEventType() const { return 274; } };  // id 274 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventNetworkShowFreeBurnIntro { u8 maData[2]; s32 GetEventType() const { return 279; } };  // id 279 size 2 (raw; size not GuiEvent-shaped)
    struct GuiEventNetworkSplashEvent { u8 maData[4]; s32 GetEventType() const { return 269; } };  // id 269 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventOfflinePostEvent : public CgsGui::GuiEvent<289> { u8 maPayload[180]; };  // id 289 size 192 (12B GuiEvent header + opaque payload)
    struct GuiEventOnlineAccountSettings { u8 maData[3]; s32 GetEventType() const { return 125; } };  // id 125 size 3 (raw; size not GuiEvent-shaped)
    struct GuiEventOnlineNumFriendsCount { u8 maData[4]; s32 GetEventType() const { return 101; } };  // id 101 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventOnlinePostEventScalps : public CgsGui::GuiEvent<319> { u8 maPayload[56]; };  // id 319 size 68 (12B GuiEvent header + opaque payload)
    struct GuiEventOnlineReceiveFriendInfo : public CgsGui::GuiEvent<102> { u8 maPayload[660]; };  // id 102 size 672 (12B GuiEvent header + opaque payload)
    struct GuiEventOnlineTimeout { u8 maData[4]; s32 GetEventType() const { return 108; } };  // id 108 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventPlayerReachedRoadRageTarget { u8 maData[1]; s32 GetEventType() const { return 168; } };  // id 168 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventPlayerWrecked { u8 maData[1]; s32 GetEventType() const { return 548; } };  // id 548 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventPreRaceMessages : public CgsGui::GuiEvent<159> { u8 maPayload[1732]; };  // id 159 size 1744 (12B GuiEvent header + opaque payload)
    struct GuiEventPrepareForInvite { u8 maData[1]; s32 GetEventType() const { return 128; } };  // id 128 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventPrepareForModeStart : public CgsGui::GuiEvent<93> { u8 maPayload[140]; };  // id 93 size 152 (12B GuiEvent header + opaque payload)
    struct GuiEventPreraceTrigger { u8 maData[4]; s32 GetEventType() const { return 160; } };  // id 160 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventProgressionProfileData : public CgsGui::GuiEvent<350> {};  // id 350 size 12
    struct GuiEventRaceDistanceRemaining : public CgsGui::GuiEvent<239> { u8 maPayload[132]; };  // id 239 size 144 (12B GuiEvent header + opaque payload)
    struct GuiEventRaceDistanceToCheckpoint { u8 maData[4]; s32 GetEventType() const { return 240; } };  // id 240 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventRacePositionInfo : public CgsGui::GuiEvent<238> { u8 maPayload[12]; };  // id 238 size 24 (12B GuiEvent header + opaque payload)
    struct GuiEventRequestCollisionWorldEvent { u8 maData[4]; s32 GetEventType() const { return 493; } };  // id 493 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventReturnDistrict { u8 maData[8]; s32 GetEventType() const { return 196; } };  // id 196 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventRivalInfoResponse : public CgsGui::GuiEvent<444> { u8 maPayload[20]; };  // id 444 size 32 (12B GuiEvent header + opaque payload)
    struct GuiEventRivalryFullInfoResponse : public CgsGui::GuiEvent<442> { u8 maPayload[676]; };  // id 442 size 688 (12B GuiEvent header + opaque payload)
    struct GuiEventRoadRagePlayerDamage { u8 maData[8]; s32 GetEventType() const { return 348; } };  // id 348 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventRoadRageTimeExtended { u8 maData[4]; s32 GetEventType() const { return 427; } };  // id 427 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventRoadRuleBatchDataResponse : public CgsGui::GuiEvent<344> { u8 maPayload[764]; };  // id 344 size 776 (12B GuiEvent header + opaque payload)
    struct GuiEventRoadRuleBegin { u8 maData[4]; s32 GetEventType() const { return 335; } };  // id 335 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventRoadRuleChangeMode { u8 maData[4]; s32 GetEventType() const { return 343; } };  // id 343 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventRoadRuleData : public CgsGui::GuiEvent<334> { u8 maPayload[76]; };  // id 334 size 88 (12B GuiEvent header + opaque payload)
    struct GuiEventRoadRuleEnd : public CgsGui::GuiEvent<336> { u8 maPayload[12]; };  // id 336 size 24 (12B GuiEvent header + opaque payload)
    struct GuiEventRoadRuleLeave : public CgsGui::GuiEvent<340> { u8 maPayload[4]; };  // id 340 size 16 (12B GuiEvent header + opaque payload)
    struct GuiEventRoadRuleNewRulers { u8 maData[8]; s32 GetEventType() const { return 346; } };  // id 346 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventRoadRuleTickerScoreResponse : public CgsGui::GuiEvent<345> { u8 maPayload[36]; };  // id 345 size 48 (12B GuiEvent header + opaque payload)
    struct GuiEventRoadRuleUpdate : public CgsGui::GuiEvent<338> { u8 maPayload[8]; };  // id 338 size 20 (12B GuiEvent header + opaque payload)
    struct GuiEventRoadRuleUpdateTargetScores : public CgsGui::GuiEvent<339> { u8 maPayload[44]; };  // id 339 size 56 (12B GuiEvent header + opaque payload)
    struct GuiEventRunFsm : public CgsGui::GuiEvent<144> { u8 maPayload[12]; };  // id 144 size 24 (12B GuiEvent header + opaque payload)
    struct GuiEventSaveImageFileAndAutosave : public CgsGui::GuiEvent<358> { u8 maPayload[4]; };  // id 358 size 16 (12B GuiEvent header + opaque payload)
    struct GuiEventScoreUpdate : public CgsGui::GuiEvent<424> { u8 maPayload[8]; };  // id 424 size 20 (12B GuiEvent header + opaque payload)
    struct GuiEventScoreboardDownloadedChallengeable : public CgsGui::GuiEvent<123> { u8 maPayload[4]; };  // id 123 size 16 (12B GuiEvent header + opaque payload)
    struct GuiEventScoreboardResponseCategoryEvent : public CgsGui::GuiEvent<116> { u8 maPayload[460]; };  // id 116 size 472 (12B GuiEvent header + opaque payload)
    struct GuiEventScoreboardResponseEvScoreTarget { u8 maData[17]; s32 GetEventType() const { return 122; } };  // id 122 size 17 (raw; size not GuiEvent-shaped)
    struct GuiEventScoreboardResponseIndexEvent : public CgsGui::GuiEvent<117> { u8 maPayload[304]; };  // id 117 size 316 (12B GuiEvent header + opaque payload)
    struct GuiEventScoreboardResponseTableEvent : public CgsGui::GuiEvent<119> { u8 maPayload[2912]; };  // id 119 size 2924 (12B GuiEvent header + opaque payload)
    struct GuiEventScoreboardResponseVariationEvent : public CgsGui::GuiEvent<118> { u8 maPayload[2040]; };  // id 118 size 2052 (12B GuiEvent header + opaque payload)
    struct GuiEventSetAvailablePresetRaces : public CgsGui::GuiEvent<170> { u8 maPayload[716]; };  // id 170 size 728 (12B GuiEvent header + opaque payload)
    struct GuiEventSetBlackBars { u8 maData[4]; s32 GetEventType() const { return 221; } };  // id 221 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventSetRoadRuleScoreMode { u8 maData[4]; s32 GetEventType() const { return 330; } };  // id 330 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventShowFreeburnChallenge { u8 maData[8]; s32 GetEventType() const { return 582; } };  // id 582 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventShowHideHud { u8 maData[1]; s32 GetEventType() const { return 148; } };  // id 148 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventSpecificPresetRaces : public CgsGui::GuiEvent<194> { u8 maPayload[7692]; };  // id 194 size 7704 (12B GuiEvent header + opaque payload)
    struct GuiEventStatsResponse : public CgsGui::GuiEvent<436> { u8 maPayload[420]; };  // id 436 size 432 (12B GuiEvent header + opaque payload)
    struct GuiEventStopMode : public CgsGui::GuiEvent<322> {};  // id 322 size 12
    struct GuiEventStuntAllComplete { u8 maData[4]; s32 GetEventType() const { return 220; } };  // id 220 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventStuntInfo : public CgsGui::GuiEvent<217> {};  // id 217 size 12
    struct GuiEventSuperJumpFailed { u8 maData[1]; s32 GetEventType() const { return 549; } };  // id 549 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventTickerClearMessages { u8 maData[2]; s32 GetEventType() const { return 536; } };  // id 536 size 2 (raw; size not GuiEvent-shaped)
    struct GuiEventTickerCustomMessage : public CgsGui::GuiEvent<537> { u8 maPayload[2060]; };  // id 537 size 2072 (12B GuiEvent header + opaque payload)
    struct GuiEventTimeUp { u8 maData[1]; s32 GetEventType() const { return 550; } };  // id 550 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventToggleChangeCarMessage { u8 maData[1]; s32 GetEventType() const { return 540; } };  // id 540 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventTogglePictureParadise { u8 maData[1]; s32 GetEventType() const { return 222; } };  // id 222 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventTriggerOnlinePostEvent { u8 maData[1]; s32 GetEventType() const { return 320; } };  // id 320 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventUpdateEventCountdown { u8 maData[4]; s32 GetEventType() const { return 234; } };  // id 234 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventUpdateEventStarts : public CgsGui::GuiEvent<203> { u8 maPayload[8404]; };  // id 203 size 8416 (12B GuiEvent header + opaque payload)
    struct GuiEventUpdateHud : public CgsGui::GuiEvent<147> {};  // id 147 size 12
    struct GuiFinishRaceEvent { u8 maData[8]; s32 GetEventType() const { return 372; } };  // id 372 size 8 (raw; size not GuiEvent-shaped)
    struct GuiGameModeStarted : public CgsGui::GuiEvent<237> { u8 maPayload[4]; };  // id 237 size 16 (12B GuiEvent header + opaque payload)
    struct GuiGamePausedEvent { u8 maData[8]; s32 GetEventType() const { return 505; } };  // id 505 size 8 (raw; size not GuiEvent-shaped)
    struct GuiHUDMessageBHRRunnerCrashed { u8 maData[8]; s32 GetEventType() const { return 455; } };  // id 455 size 8 (raw; size not GuiEvent-shaped)
    struct GuiHUDMessageComboPerformed { u8 maData[8]; s32 GetEventType() const { return 430; } };  // id 430 size 8 (raw; size not GuiEvent-shaped)
    struct GuiHUDMessageCrushCombo { u8 maData[4]; s32 GetEventType() const { return 401; } };  // id 401 size 4 (raw; size not GuiEvent-shaped)
    struct GuiHUDMessageShowtimeMultiplier { u8 maData[8]; s32 GetEventType() const { return 399; } };  // id 399 size 8 (raw; size not GuiEvent-shaped)
    struct GuiHUDMessageSignSmashed { u8 maData[4]; s32 GetEventType() const { return 400; } };  // id 400 size 4 (raw; size not GuiEvent-shaped)
    struct GuiHUDMessageStuntTimeUp { u8 maData[1]; s32 GetEventType() const { return 431; } };  // id 431 size 1 (raw; size not GuiEvent-shaped)
    struct GuiHitVehicleEvent : public CgsGui::GuiEvent<394> { u8 maPayload[12]; };  // id 394 size 24 (12B GuiEvent header + opaque payload)
    struct GuiImageGalleryCollectedCountEvent { u8 maData[8]; s32 GetEventType() const { return 520; } };  // id 520 size 8 (raw; size not GuiEvent-shaped)
    struct GuiImageGalleryCollectedDataEvent : public CgsGui::GuiEvent<522> { u8 maPayload[4]; };  // id 522 size 16 (12B GuiEvent header + opaque payload)
    struct GuiImageGalleryImageInfoEvent : public CgsGui::GuiEvent<518> { u8 maPayload[36]; };  // id 518 size 48 (12B GuiEvent header + opaque payload)
    struct GuiInAirEvent { u8 maData[8]; s32 GetEventType() const { return 387; } };  // id 387 size 8 (raw; size not GuiEvent-shaped)
    struct GuiInEventFinisher { u8 maData[8]; s32 GetEventType() const { return 423; } };  // id 423 size 8 (raw; size not GuiEvent-shaped)
    struct GuiInEventLeaderSplit : public CgsGui::GuiEvent<420> { u8 maPayload[12]; };  // id 420 size 24 (12B GuiEvent header + opaque payload)
    struct GuiInEventNeckAndNeck { u8 maData[1]; s32 GetEventType() const { return 421; } };  // id 421 size 1 (raw; size not GuiEvent-shaped)
    struct GuiInEventRivalProgress : public CgsGui::GuiEvent<422> { u8 maPayload[12]; };  // id 422 size 24 (12B GuiEvent header + opaque payload)
    struct GuiInProgressStuntEvent : public CgsGui::GuiEvent<391> { u8 maPayload[12]; };  // id 391 size 24 (12B GuiEvent header + opaque payload)
    struct GuiLastBlueTeamMemberEvent { u8 maData[1]; s32 GetEventType() const { return 452; } };  // id 452 size 1 (raw; size not GuiEvent-shaped)
    struct GuiLeaderPassedKMBoundaryEvent { u8 maData[8]; s32 GetEventType() const { return 449; } };  // id 449 size 8 (raw; size not GuiEvent-shaped)
    struct GuiLeaderPassedMileBoundaryEvent { u8 maData[8]; s32 GetEventType() const { return 448; } };  // id 448 size 8 (raw; size not GuiEvent-shaped)
    struct GuiLeaptVehicleEvent { u8 maData[1]; s32 GetEventType() const { return 393; } };  // id 393 size 1 (raw; size not GuiEvent-shaped)
    struct GuiLocalPlayerEliminatedEvent { u8 maData[1]; s32 GetEventType() const { return 451; } };  // id 451 size 1 (raw; size not GuiEvent-shaped)
    struct GuiMugshotControlEvent : public CgsGui::GuiEvent<325> { u8 maPayload[12]; };  // id 325 size 24 (12B GuiEvent header + opaque payload)
    struct GuiNearMissEvent { u8 maData[8]; s32 GetEventType() const { return 384; } };  // id 384 size 8 (raw; size not GuiEvent-shaped)
    struct GuiNetworkLastStunRunEvent { u8 maData[1]; s32 GetEventType() const { return 490; } };  // id 490 size 1 (raw; size not GuiEvent-shaped)
    struct GuiNetworkStuntRunEliminationEvent : public CgsGui::GuiEvent<487> {};  // id 487 size 12
    struct GuiNetworkStuntRunLeadingEvent : public CgsGui::GuiEvent<488> {};  // id 488 size 12
    struct GuiNetworkStuntRunVictoryEvent : public CgsGui::GuiEvent<489> {};  // id 489 size 12
    struct GuiNetworkSuntRunInfoMessageEvent : public CgsGui::GuiEvent<491> {};  // id 491 size 12
    struct GuiOncomingEvent { u8 maData[4]; s32 GetEventType() const { return 388; } };  // id 388 size 4 (raw; size not GuiEvent-shaped)
    struct GuiOnlineCarStatusEvent { u8 maData[8]; s32 GetEventType() const { return 563; } };  // id 563 size 8 (raw; size not GuiEvent-shaped)
    struct GuiOverlayWaitFinishRequest { u8 maData[8]; s32 GetEventType() const { return 188; } };  // id 188 size 8 (raw; size not GuiEvent-shaped)
    struct GuiOvertakeEvent { u8 maData[8]; s32 GetEventType() const { return 371; } };  // id 371 size 8 (raw; size not GuiEvent-shaped)
    struct GuiPFXHookEvent : public CgsGui::GuiEvent<495> { u8 maPayload[52]; };  // id 495 size 64 (12B GuiEvent header + opaque payload)
    struct GuiPFXHookStopEvent : public CgsGui::GuiEvent<496> { u8 maPayload[28]; };  // id 496 size 40 (12B GuiEvent header + opaque payload)
    struct GuiPFXStartBackgroundHookEvent : public CgsGui::GuiEvent<498> { u8 maPayload[32]; };  // id 498 size 44 (12B GuiEvent header + opaque payload)
    struct GuiPFXStopBackgroundHookEvent : public CgsGui::GuiEvent<499> { u8 maPayload[28]; };  // id 499 size 40 (12B GuiEvent header + opaque payload)
    struct GuiPaybackReceivedEvent { u8 maData[4]; s32 GetEventType() const { return 182; } };  // id 182 size 4 (raw; size not GuiEvent-shaped)
    struct GuiPlayerCarColourResponse { u8 maData[8]; s32 GetEventType() const { return 414; } };  // id 414 size 8 (raw; size not GuiEvent-shaped)
    struct GuiPlayerDrivableFromCrash { u8 maData[1]; s32 GetEventType() const { return 378; } };  // id 378 size 1 (raw; size not GuiEvent-shaped)
    struct GuiPlayerEliminatedEvent { u8 maData[4]; s32 GetEventType() const { return 450; } };  // id 450 size 4 (raw; size not GuiEvent-shaped)
    struct GuiPlayerEngineEvent { u8 maData[4]; s32 GetEventType() const { return 379; } };  // id 379 size 4 (raw; size not GuiEvent-shaped)
    struct GuiPlayerInShortcutEvent { u8 maData[1]; s32 GetEventType() const { return 380; } };  // id 380 size 1 (raw; size not GuiEvent-shaped)
    // id 406 size 64 (12B GuiEvent header + payload). PARTIAL LAYOUT RECOVERY
    // (BrnCarSelectMain wave G): the CarSelectMain event-406 consumer @0x824D7A24 reads the
    // responding player's car id as the qword at event+0x20 (ld r11, 0x20(event)) -- named
    // mCarId here; the surrounding payload bytes stay opaque. On x64 the 12B GuiEvent header
    // + 20B maPayload0 land mCarId at the same +0x20 with no inserted padding (align 8).
    struct GuiPlayerInfoResponse : public CgsGui::GuiEvent<406>
    {
        u8    maPayload0[20];   // +0x0C..+0x1F (opaque)
        CgsID mCarId;           // +0x20 (the player's current car id)
        u8    maPayload1[24];   // +0x28..+0x3F (opaque)
    };
    struct GuiPlayerRaceCarIdEvent { u8 maData[8]; s32 GetEventType() const { return 376; } };  // id 376 size 8 (raw; size not GuiEvent-shaped)
    struct GuiPowerParkResult { u8 maData[8]; s32 GetEventType() const { return 404; } };  // id 404 size 8 (raw; size not GuiEvent-shaped)
    struct GuiPursuitScoreUpdate { u8 maData[4]; s32 GetEventType() const { return 432; } };  // id 432 size 4 (raw; size not GuiEvent-shaped)
    struct GuiRaceCheckpointReached : public CgsGui::GuiEvent<425> {};  // id 425 size 12
    struct GuiReplayStatusEvent : public CgsGui::GuiEvent<524> { u8 maPayload[1548]; };  // id 524 size 1560 (12B GuiEvent header + opaque payload)
    struct GuiRoadRageScoreUpdate { u8 maData[8]; s32 GetEventType() const { return 426; } };  // id 426 size 8 (raw; size not GuiEvent-shaped)
    struct GuiSetEasyDriveNotAllowedEvent { u8 maData[1]; s32 GetEventType() const { return 96; } };  // id 96 size 1 (raw; size not GuiEvent-shaped)
    struct GuiShowtimeJustBounced { u8 maData[2]; s32 GetEventType() const { return 402; } };  // id 402 size 2 (raw; size not GuiEvent-shaped)
    struct GuiShowtimeScoreUpdate : public CgsGui::GuiEvent<396> {};  // id 396 size 12
    struct GuiShowtimeTriggered { u8 maData[1]; s32 GetEventType() const { return 392; } };  // id 392 size 1 (raw; size not GuiEvent-shaped)
    struct GuiShutdownEvent { u8 maData[8]; s32 GetEventType() const { return 373; } };  // id 373 size 8 (raw; size not GuiEvent-shaped)
    struct GuiShutdownFinishedEvent { u8 maData[1]; s32 GetEventType() const { return 374; } };  // id 374 size 1 (raw; size not GuiEvent-shaped)
    struct GuiSoftTakedownEvent : public CgsGui::GuiEvent<364> { u8 maPayload[20]; };  // id 364 size 32 (12B GuiEvent header + opaque payload)
    struct GuiSpinningEvent { u8 maData[4]; s32 GetEventType() const { return 386; } };  // id 386 size 4 (raw; size not GuiEvent-shaped)
    struct GuiTailgatingEvent { u8 maData[4]; s32 GetEventType() const { return 389; } };  // id 389 size 4 (raw; size not GuiEvent-shaped)
    struct GuiTrafficCheckEvent { u8 maData[4]; s32 GetEventType() const { return 383; } };  // id 383 size 4 (raw; size not GuiEvent-shaped)

    // ============================================================================
    // OUTPUT-FAMILY GUI-event PAYLOAD homes (ADDITIVE GROW -- output GUI event wave).
    // Canonical homes for the BrnGui payload types queued by CgsGui::StateInterface::
    // OutputGuiEvent<T> (channel 40), BrnNetwork::BrnNetworkModule::AddOutputGuiEvent<T>
    // (VEQ<4096,16>) and CgsGui::CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<T> (VEQ<18432,16>)
    // and not previously homed. Same honest-placeholder recipe as the block above: the (id,size)
    // pair is the two compile-time literals read straight from each instance's X360 AddEvent asm.
    //   * size >= 12 and 4-aligned: CgsGui::GuiEvent<id> (12B header carrying GetEventType()==id)
    //     + u8 maPayload[size-12].
    //   * otherwise: a raw byte struct of the attested size with its own GetEventType()==id.
    // `alignas(8)` is added where the OutputGuiEvent<T> asm writes offset 16 for the payload
    // (i.e. the X360 type is 8-aligned): it forces alignof==8 so the wrapper's miOutEventOffset /
    // record size match the asm, while preserving the X360 sizeof (all such sizes are 8-multiples).
    // ============================================================================
    struct CalculateRoute : public CgsGui::GuiEvent<494> { u8 maPayload[68]; };  // id 494 size 80
    // Mirror of BrnGui::GuiOverlayShowingNotification (real home BrnGuiOverlaysDirector.h, id 190,
    // 8-byte { CgsID } record). BrnGuiDemangledEventTypes.h and BrnGuiOverlaysDirector.h are
    // mutually-exclusive includes (both also define GuiOverlayWaitFinishRequest), so the event-queue
    // template TUs that include this header carry the overlay-showing payload here too. alignas(8)
    // matches the OutputGuiEvent<T> asm (offset 16 -- the real payload is an 8-byte CgsID).
    struct alignas(8) GuiOverlayShowingNotification { u8 maData[8]; s32 GetEventType() const { return 190; } };  // id 190 size 8 [8-aligned: OGE off16]
    struct alignas(8) GuiAudioEvent : public CgsGui::GuiEvent<456> { u8 maPayload[12]; };  // id 456 size 24 [8-aligned: OGE off16]
    struct GuiAudioTriggerEvent : public CgsGui::GuiEvent<457> { u8 maPayload[88]; };  // id 457 size 100
    // X360-RECOVERED LAYOUT (no longer an opaque placeholder). The inner field breakdown IS
    // recovered from three attesting producers -- BrnGui::ChallengeSelector::Hide (@0x82436F70),
    // BrnGui::FriendsListComponent::Close (@0x824397E8) and ::SelectPrevious (@0x82441988), which
    // stack-build this 16-byte record and post it through
    // StateInterface::OutputGuiEvent<GuiChallengeSelectedEvent> (@0x82436778, channel 40) -- and
    // cross-checked against the consumer GameBridgeGUIToX case 573, which reads mChallengeID
    // (u64 @0x00), miSelectorAction (s32 @0x08) and miChall (s32 @0x0C). So it is modelled with
    // real fields rather than a GuiEvent<573> header + opaque maPayload (the producers write a raw
    // CgsID at offset 0, i.e. there is NO GuiEvent header inside the payload). sizeof == 16, align 8
    // and GetEventType() == 573 are preserved, so the AddGuiEvent<T> / OutputGuiEvent<T> template
    // instantiations are unaffected.
    struct alignas(8) GuiChallengeSelectedEvent
    {
        u64 mChallengeID;      // +0x00  highlighted challenge id (CgsID)
        s32 miSelectorAction;  // +0x08  selector/action code (Hide posts 3; Close posts 3 then 1; SelectPrevious posts 2)
        s32 miChall;           // +0x0C  BrnResource::ChallengeListEntry::GetChall()
        s32 GetEventType() const { return 573; }
    };  // id 573 size 16 [8-aligned: OGE off16]
    struct GuiEvent100PerCentComplete { u8 maData[1]; s32 GetEventType() const { return 469; } };  // id 469 size 1
    struct GuiEventActivateCarSelect { u8 maData[8]; s32 GetEventType() const { return 192; } };  // id 192 size 8
    struct GuiEventAudioGenericSequence { u8 maData[4]; s32 GetEventType() const { return 468; } };  // id 468 size 4
    // BrnGui::GuiEventAudioSettings -- genuinely un-homed here (the same-named type in
    // BrnMixerControl.h is BrnSound::Logic::GuiEventAudioSettings, a different type). id 463 size 8,
    // OutputGuiEvent offset 12 (payload 4-aligned per @0x82493728).
    struct GuiEventAudioSettings { u8 maData[8]; s32 GetEventType() const { return 463; } };  // id 463 size 8
    // Mirror of BrnGui::GuiEventAudioTraxUpdate (real home BrnGuiOptionsDataProfile.h). That header
    // carries several other-namespace redefinitions (EBoostType / GameStateModuleIO) that clash with
    // the event-queue instantiation TUs' other includes, so the payload is mirrored here (its .cpp
    // consumers do not include this header). id 458 size 32, OutputGuiEvent off16 (8-aligned).
    struct alignas(8) GuiEventAudioTraxUpdate : public CgsGui::GuiEvent<458> { u8 maPayload[20]; };  // id 458 size 32 [8-aligned: OGE off16]
    struct alignas(8) GuiEventAudioTraxLastPlayedIndexes : public CgsGui::GuiEvent<459> { u8 maPayload[12]; };  // id 459 size 24 [8-aligned: OGE off16]
    struct GuiEventAudioTraxPreview { u8 maData[8]; s32 GetEventType() const { return 460; } };  // id 460 size 8
    struct GuiEventAudioVoiceOver { u8 maData[4]; s32 GetEventType() const { return 466; } };  // id 466 size 4
    struct GuiEventCamStatus { u8 maData[4]; s32 GetEventType() const { return 570; } };  // id 570 size 4
    struct alignas(8) GuiEventChallengedEventDataRequest { u8 maData[8]; s32 GetEventType() const { return 331; } };  // id 331 size 8 [8-aligned: OGE off16]
    struct GuiEventControllerSettings { u8 maData[3]; s32 GetEventType() const { return 472; } };  // id 472 size 3
    struct alignas(8) GuiEventCustomeEventCreate : public CgsGui::GuiEvent<172> { u8 maPayload[108]; };  // id 172 size 120 [8-aligned: OGE off16]
    struct GuiEventKeyboardResponse { u8 maData[4]; s32 GetEventType() const { return 142; } };  // id 142 size 4
    struct GuiEventNetworkConnect { u8 maData[4]; s32 GetEventType() const { return 272; } };  // id 272 size 4
    struct GuiEventNetworkCustomMatchJoin { u8 maData[4]; s32 GetEventType() const { return 255; } };  // id 255 size 4
    struct GuiEventNetworkCustomMatchResults : public CgsGui::GuiEvent<254> { u8 maPayload[592]; };  // id 254 size 604
    struct GuiEventNetworkCustomMatchSearch : public CgsGui::GuiEvent<252> {};  // id 252 size 12
    struct GuiEventNetworkLeavingGameFailed { u8 maData[1]; s32 GetEventType() const { return 275; } };  // id 275 size 1
    struct GuiEventNetworkNewsAndTOS { u8 maData[4]; s32 GetEventType() const { return 266; } };  // id 266 size 4
    struct GuiEventNetworkOutputPlayerTexture { u8 maData[8]; s32 GetEventType() const { return 264; } };  // id 264 size 8
    struct GuiEventNetworkQuickMatch { u8 maData[2]; s32 GetEventType() const { return 251; } };  // id 251 size 2
    struct GuiEventNetworkSelectedPlayerOption { u8 maData[8]; s32 GetEventType() const { return 246; } };  // id 246 size 8
    struct GuiEventOnlineInviteEvent : public CgsGui::GuiEvent<100> { u8 maPayload[12]; };  // id 100 size 24
    struct GuiEventOnlineShowProfile : public CgsGui::GuiEvent<99> { u8 maPayload[4]; };  // id 99 size 16
    struct alignas(8) GuiEventPostEventFreeCarSequenceStart { u8 maData[8]; s32 GetEventType() const { return 302; } };  // id 302 size 8 [8-aligned: OGE off16]
    struct GuiEventPostEventRankUpSequenceStart { u8 maData[8]; s32 GetEventType() const { return 303; } };  // id 303 size 8
    struct GuiEventRequestCompressedCamPic : public CgsGui::GuiEvent<568> {};  // id 568 size 12
    struct GuiEventRequestSpecificPreSetRaces { u8 maData[4]; s32 GetEventType() const { return 193; } };  // id 193 size 4
    struct GuiEventRequestTraining { u8 maData[4]; s32 GetEventType() const { return 572; } };  // id 572 size 4
    struct alignas(8) GuiEventRoadRuleDataRequest { u8 maData[8]; s32 GetEventType() const { return 327; } };  // id 327 size 8 [8-aligned: OGE off16]
    struct GuiEventRoadRuleModeRequest { u8 maData[8]; s32 GetEventType() const { return 326; } };  // id 326 size 8
    struct GuiEventScoreboardRequestEvScoreTarget : public CgsGui::GuiEvent<121> { u8 maPayload[24]; };  // id 121 size 36
    struct GuiEventScoreboardRequestGamercardEvent : public CgsGui::GuiEvent<120> { u8 maPayload[4]; };  // id 120 size 16
    struct GuiEventScoreboardRequestIndexEvent { u8 maData[4]; s32 GetEventType() const { return 111; } };  // id 111 size 4
    struct GuiEventScoreboardRequestTableEvent { u8 maData[4]; s32 GetEventType() const { return 113; } };  // id 113 size 4
    struct GuiEventScoreboardRequestVariationEvent { u8 maData[4]; s32 GetEventType() const { return 112; } };  // id 112 size 4
    struct GuiEventSetPlayer0ControllerPort { u8 maData[4]; s32 GetEventType() const { return 143; } };  // id 143 size 4
    struct GuiEventVoipSettings { u8 maData[4]; s32 GetEventType() const { return 474; } };  // id 474 size 4
    struct GuiImageGalleryRequestCollectedDataEvent { u8 maData[4]; s32 GetEventType() const { return 521; } };  // id 521 size 4
    struct GuiImageGalleryRequestEvent : public CgsGui::GuiEvent<517> { u8 maPayload[4]; };  // id 517 size 16
    struct GuiMuteDac { u8 maData[1]; s32 GetEventType() const { return 88; } };  // id 88 size 1
    struct GuiNetworkCustomRouteCreated { u8 maData[8]; s32 GetEventType() const { return 286; } };  // id 286 size 8
    struct GuiPFXHookEnumeration : public CgsGui::GuiEvent<501> { u8 maPayload[392]; };  // id 501 size 404
    struct GuiReplayDeleteReelEvent { u8 maData[4]; s32 GetEventType() const { return 527; } };  // id 527 size 4
    struct GuiReplayRegisterSerialiser { u8 maData[4]; s32 GetEventType() const { return 595; } };  // id 595 size 4
    struct GuiReplaySetModeEvent { u8 maData[4]; s32 GetEventType() const { return 525; } };  // id 525 size 4
    struct GuiRequestCarControlChangeEvent { u8 maData[1]; s32 GetEventType() const { return 65; } };  // id 65 size 1
    struct GuiResponseTimeDateString : public CgsGui::GuiEvent<594> { u8 maPayload[24]; };  // id 594 size 36
    struct GuiSaveLoadImageExportRequested { u8 maData[17]; s32 GetEventType() const { return 361; } };  // id 361 size 17
    struct GuiTelemetryEvent : public CgsGui::GuiEvent<323> { u8 maPayload[8]; };  // id 323 size 20

    // ============================================================================
    // VIEW / INTERNAL-STATE GUI-event PAYLOAD homes (ADDITIVE GROW -- output view/internal-state
    // wave). Canonical homes for the BrnGui payload types queued by CgsGui::StateInterface::
    // OutputViewState<T> (channel 41) and OutputInternalState<T> (channel 42) and not previously
    // homed. Same honest-placeholder recipe as the blocks above: the (id,size) pair is the two
    // compile-time literals read straight from each instance's X360 AddEvent asm (offset word 12
    // = 4-aligned payload, 16 = 8-aligned payload -> alignas(8)).
    // (GuiEventShowHideHud id 148 / GuiEventNetworkPlayerImage id 258 are already homed above and
    // reused; GuiEventRenderMainMap has its real field-layout home in BrnMainMap.h and is NOT
    // duplicated here.)
    // ============================================================================
    struct GuiEventPerformOnlineMainMenuOption { u8 maData[4]; s32 GetEventType() const { return 284; } };  // id 284 size 4
    struct GuiEventPerformOnlinePauseOption { u8 maData[4]; s32 GetEventType() const { return 283; } };  // id 283 size 4
    struct GuiEventFilterEventIcons { u8 maData[4]; s32 GetEventType() const { return 557; } };  // id 557 size 4
    struct GuiEventSetInspectedEventIcon { u8 maData[4]; s32 GetEventType() const { return 558; } };  // id 558 size 4 (OutputViewState + OutputInternalState)
    struct GuiEventShowHideBoostBar { u8 maData[1]; s32 GetEventType() const { return 214; } };  // id 214 size 1
    struct GuiEventShowHideSatNav : public CgsGui::GuiEvent<213> {};  // id 213 size 12 (bare 12B GuiEvent header)
    struct alignas(8) GuiEventSetHoveredEventIcon : public CgsGui::GuiEvent<559> { u8 maPayload[12]; };  // id 559 size 24 [8-aligned: OViewState off16]
}
