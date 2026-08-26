#pragma once

#include <cstddef>   // offsetof -- GuiEventNetworkCustomMatchSearch derives its two record
                     // header words from the HOST layout, never from a console literal

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"   // CgsGui::GuiEvent<N> (12-byte event header)
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameSource/GameState/BrnCgsPlayerName.h"  // CgsNetwork::PlayerName (scoreboard request payloads)        // CgsID (GuiPlayerInfoResponse::mCarId)
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"  // BrnTraffic::VehicleClass / VehicleScoreCategory (GuiHitVehicleEvent)
#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissManager.h" // BrnWorld::ENearMissType (GuiNearMissEvent)

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
    // [E1 event-status wave 2026-08-26] GuiAttackScoreUpdate (id 428) has been RECOVERED and
    // now lives in BrnGuiEventTypeDefs.h with its real flat wire shape (the opaque
    // `GuiEvent<428> + u8[28]` shell that stood here read the 40-byte record as "12-byte
    // GuiEvent header + 28 payload"; the 40 bytes ARE the payload). Moved rather than kept
    // here because GuiCache::RecEvent -- its only consumer -- cannot include THIS header:
    // BrnGuiOptionsDataProfile.h, which BrnGuiCache.cpp needs, carries its own compile-only
    // `BrnGui::GuiEventAudioTraxUpdate` slice and the pair is a hard C2011. Deleted rather
    // than left to shadow the real home.
    struct GuiAutosaveRequestEvent { u8 maData[1]; s32 GetEventType() const { return 356; } };  // id 356 size 1 (raw; size not GuiEvent-shaped)
    // [gateui r3] GuiBHRCheckpointReachedEvent (id 454) has been RECOVERED and now lives in
    // BrnGuiEventTypeDefs.h with its real DWARF field set (DWARF :6153, sizeof 8).
    // The opaque placeholder that stood here was DELETED rather than left to shadow it.
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
    // [boost-msg wave 2026-08-26] RECOVERED (was the opaque GuiEvent<390>+u8[20] shell).
    // DWARF BrnGuiEventTypeDefs.h:4414 (PS3 :4416..:4425) supplies the field NAMES; the X360
    // wire shape is raw -- no 12-byte GuiEvent base -- pinned by three witnesses:
    //   AddGuiEvent<GuiCompletedStuntEvent> @0x823D2888 -> AddEvent(q, ev, 390, 32);
    //   BoostMessageManager::HandleOnCompletedStunt @0x82411CC8 reads muStuntActionComplete
    //   at +0x00, mfCompletedBarrelRollAngle at +0x04 and miCompletedBarrelRolls at +0x18.
    // The PS3 DWARF derives this class from GuiEvent<385> AND lists mbSuccessfulLanding
    // before miCompletedBarrelRolls; the X360 record's size (32) and the handler's own
    // reads place the count at +0x18 with the bool trailing -- binary wins.
    struct GuiCompletedStuntEvent
    {
        u32     muStuntActionComplete;             // +0x00 (bit0 barrel roll, bit3 clean landing, bit4 successful)
        f32     mfCompletedBarrelRollAngle;        // +0x04
        f32     mfCompletedAirSpinAngle;           // +0x08
        f32     mfCompletedHandbreakTurnAngle;     // +0x0C
        f32     mfCompletedDriftTime;              // +0x10
        f32     mfCompletedDriftDistance;          // +0x14
        s32     miCompletedBarrelRolls;            // +0x18 (handler read @0x82411CE8)
        bool    mbSuccessfulLanding;               // +0x1C
        u8      maPad1D[3];                        // +0x1D..+0x1F

        s32 GetEventType() const { return 390; }
    };
    static_assert(sizeof(GuiCompletedStuntEvent) == 32, "X360 AddGuiEvent size 32 (id 390)");
    static_assert(__builtin_offsetof(GuiCompletedStuntEvent, muStuntActionComplete) == 0x00 &&
                  __builtin_offsetof(GuiCompletedStuntEvent, mfCompletedBarrelRollAngle) == 0x04 &&
                  __builtin_offsetof(GuiCompletedStuntEvent, miCompletedBarrelRolls) == 0x18,
                  "X360 handler reads @0x82411CC8");
    struct GuiCrashComboEvent { u8 maData[8]; s32 GetEventType() const { return 347; } };  // id 347 size 8 (raw; size not GuiEvent-shaped)
    struct GuiCrashScoreUpdate : public CgsGui::GuiEvent<434> { u8 maPayload[4]; };  // id 434 size 16 (12B GuiEvent header + opaque payload)
    // GuiDeveloperChallengesCompleted (id 596) has been RECOVERED and now lives in
    // BrnGuiEventTypeDefs.h as a real FastBitArray<15> payload. The opaque u8[8]
    // placeholder that stood here was deleted rather than left to shadow it -- this
    // header includes BrnGuiEventTypeDefs.h, so every includer still sees the type.
    // [boost-msg wave] RECOVERED: DWARF BrnGuiEventTypeDefs.h:4404 {f32 mfDistance}; raw 4-byte
    // wire record (BoostMessageManager::RecvEvent case 385 loads the float at +0, @0x8242095C).
    struct GuiDriftingEvent { f32 mfDistance; s32 GetEventType() const { return 385; } };  // id 385 size 4
    // [gateui r3] GuiDriveThroughEvent (id 366) has been RECOVERED and now lives in
    // BrnGuiEventTypeDefs.h with its real DWARF field set (DWARF :5097 + the nested DriveThroughType enum, sizeof 8).
    // The opaque placeholder that stood here was DELETED rather than left to shadow it.
    struct GuiEnteredJunkyard { u8 maData[1]; s32 GetEventType() const { return 79; } };  // id 79 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventAllJunctionsDiscoveredOfType { u8 maData[4]; s32 GetEventType() const { return 313; } };  // id 313 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventAllOfRivalsShutdown { u8 maData[1]; s32 GetEventType() const { return 306; } };  // id 306 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventAllOfTypeComplete { u8 maData[4]; s32 GetEventType() const { return 305; } };  // id 305 size 4 (raw; size not GuiEvent-shaped)
    // [gateui] GuiEventBoostBarStuntInfo (id 218) has been RECOVERED and now lives in
    // BrnGuiEventTypeDefs.h with its real DWARF fields ({current, total, StuntType},
    // DWARF BrnGuiEventTypeDefs.h:4502). The opaque GuiEvent<218> shell that stood here
    // read the 12-byte record as "header only, no payload" -- the 12 bytes ARE the
    // payload. Deleted rather than left to shadow it.
    // [gateui r3] GuiEventBoostInfo (id 206) has been RECOVERED and now lives in
    // BrnGuiEventTypeDefs.h with its real DWARF field set (DWARF :4566 in the X360 field order, sizeof 28).
    // The opaque placeholder that stood here was DELETED rather than left to shadow it.
    struct GuiEventBuddyNotification : public CgsGui::GuiEvent<105> { u8 maPayload[12]; };  // id 105 size 24 (12B GuiEvent header + opaque payload)
    struct GuiEventCamPicCompressed : public CgsGui::GuiEvent<569> {};  // id 569 size 12
    struct GuiEventCanSkipCrash { u8 maData[1]; s32 GetEventType() const { return 547; } };  // id 547 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventCantPaintCar { u8 maData[1]; s32 GetEventType() const { return 551; } };  // id 551 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventChallengedEventDataResponse : public CgsGui::GuiEvent<332> { u8 maPayload[20]; };  // id 332 size 32 (12B GuiEvent header + opaque payload)
    // [H1 wave 2026-08-25] GuiEventChangeDistrict (id 169) has been RECOVERED and now lives
    // in BrnGuiEventTypeDefs.h with its real flat wire shape (the demangled `GuiEvent<169> {}`
    // placeholder here did not match the wire -- the record is three words at offset +0 with
    // no GuiEvent header). The opaque placeholder was DELETED rather than left to shadow it.
    // [E1 event-status wave 2026-08-26] GuiEventCurrentStatus (id 492) has been RECOVERED and
    // now lives in BrnGuiEventTypeDefs.h with its real flat wire shape. Same reason for the
    // move as GuiAttackScoreUpdate above (the GuiEventAudioTraxUpdate C2011 pair keeps
    // BrnGuiCache.cpp from including this header). Deleted rather than left to shadow it.
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
    // GuiEventProgressionProfileData (id 350, size 12) MOVED to
    // GameSource/Gui/BrnGuiEventTypeDefs.h with its real field shape
    // { BrnProgression::Profile*, const BrnProgression::ProgressionData*, bool } --
    // the auto-derived shell here read the attested record size 12 as "GuiEvent<350>
    // header, no payload", but the X360 producer (GameBridgeGameStateToX case 193
    // @0x823EBBA4) fills all 12 bytes with payload. Do not re-add it here.
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
    // GuiEventRoadRuleEnd (id 336): NO placeholder here -- UPGRADED to the real
    // hand-reconstructed home in BrnGuiEventTypeDefs.h (HUD H2 2026-08-25; named
    // fields, X360-attested). A placeholder must never shadow a real home (C2011).
    struct GuiEventRoadRuleLeave : public CgsGui::GuiEvent<340> { u8 maPayload[4]; };  // id 340 size 16 (12B GuiEvent header + opaque payload)
    struct GuiEventRoadRuleNewRulers { u8 maData[8]; s32 GetEventType() const { return 346; } };  // id 346 size 8 (raw; size not GuiEvent-shaped)
    struct GuiEventRoadRuleTickerScoreResponse : public CgsGui::GuiEvent<345> { u8 maPayload[36]; };  // id 345 size 48 (12B GuiEvent header + opaque payload)
    struct GuiEventRoadRuleUpdate : public CgsGui::GuiEvent<338> { u8 maPayload[8]; };  // id 338 size 20 (12B GuiEvent header + opaque payload)
    // GuiEventRoadRuleUpdateTargetScores (id 339): NO placeholder here -- UPGRADED to
    // the real hand-reconstructed home in BrnGuiEventTypeDefs.h (HUD H2 2026-08-25).
    // GuiEventRunFsm: NO placeholder here -- the real hand-reconstructed home is
    // BrnGuiEventTypeDefs.h (named fields, derives from CgsModule::Event). A placeholder
    // must never shadow a real home; defining both made every TU that includes both
    // headers a hard C2011 (it already broke two committed files before wave I hit it).
    // FLAG / OPEN: this catalogue read the id as 144 size 24 from the AddGuiEvent<T>
    // spine, which does NOT match the real home. One of the two is misnamed -- unresolved,
    // needs asm arbitration, deliberately not guessed here.
    struct GuiEventSaveImageFileAndAutosave : public CgsGui::GuiEvent<358> { u8 maPayload[4]; };  // id 358 size 16 (12B GuiEvent header + opaque payload)
    // [E1 event-status wave 2026-08-26] GuiEventScoreUpdate (id 424) has been RECOVERED and
    // now lives in BrnGuiEventTypeDefs.h with its real flat wire shape. Same reason for the
    // move as GuiAttackScoreUpdate above. Deleted rather than left to shadow it.
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
    // [gateui] GuiEventStuntAllComplete (id 220) and GuiEventStuntInfo (id 217) have been
    // RECOVERED and now live in BrnGuiEventTypeDefs.h with their real DWARF fields
    // (:4538 / :4515). Their consumers -- HandleStuntsComplete @0x8251F7E8 and
    // HandleStuntInfo @0x8251F650 -- read named words straight off the queued record.
    struct GuiEventSuperJumpFailed { u8 maData[1]; s32 GetEventType() const { return 549; } };  // id 549 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventTickerClearMessages { u8 maData[2]; s32 GetEventType() const { return 536; } };  // id 536 size 2 (raw; size not GuiEvent-shaped)
    struct GuiEventTickerCustomMessage : public CgsGui::GuiEvent<537> { u8 maPayload[2060]; };  // id 537 size 2072 (12B GuiEvent header + opaque payload)
    struct GuiEventTimeUp { u8 maData[1]; s32 GetEventType() const { return 550; } };  // id 550 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventToggleChangeCarMessage { u8 maData[1]; s32 GetEventType() const { return 540; } };  // id 540 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventTogglePictureParadise { u8 maData[1]; s32 GetEventType() const { return 222; } };  // id 222 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventTriggerOnlinePostEvent { u8 maData[1]; s32 GetEventType() const { return 320; } };  // id 320 size 1 (raw; size not GuiEvent-shaped)
    struct GuiEventUpdateEventCountdown { u8 maData[4]; s32 GetEventType() const { return 234; } };  // id 234 size 4 (raw; size not GuiEvent-shaped)
    struct GuiEventUpdateEventStarts : public CgsGui::GuiEvent<203> { u8 maPayload[8404]; };  // id 203 size 8416 (12B GuiEvent header + opaque payload)
    // [hud H3b tracking slice 2026-08-25] GuiEventUpdateHud (147) has been RECOVERED and
    // now lives in BrnGuiEventTypeDefs.h with its real X360 record shape (the raw
    // {speed, rpm, gear} words). The opaque GuiEvent<147> shell that stood here was
    // DELETED rather than left to shadow it (the GuiPowerParkResult precedent below).
    struct GuiFinishRaceEvent { u8 maData[8]; s32 GetEventType() const { return 372; } };  // id 372 size 8 (raw; size not GuiEvent-shaped)
    struct GuiGameModeStarted : public CgsGui::GuiEvent<237> { u8 maPayload[4]; };  // id 237 size 16 (12B GuiEvent header + opaque payload)
    struct GuiGamePausedEvent { u8 maData[8]; s32 GetEventType() const { return 505; } };  // id 505 size 8 (raw; size not GuiEvent-shaped)
    // [gateui r3] GuiHUDMessageBHRRunnerCrashed (id 455) has been RECOVERED and now lives in
    // BrnGuiEventTypeDefs.h with its real DWARF field set (DWARF :6218, sizeof 8).
    // The opaque placeholder that stood here was DELETED rather than left to shadow it.
    // [gateui] GuiHUDMessageComboPerformed (430), GuiHUDMessageCrushCombo (401),
    // GuiHUDMessageShowtimeMultiplier (399) and GuiHUDMessageSignSmashed (400) have been
    // RECOVERED and now live in BrnGuiEventTypeDefs.h with their real DWARF fields
    // (:6170 / :6199 / :5343 / :6194). Their four handlers read named words off the record.
    struct GuiHUDMessageStuntTimeUp { u8 maData[1]; s32 GetEventType() const { return 431; } };  // id 431 size 1 (raw; size not GuiEvent-shaped)
    // [boost-msg wave] RECOVERED (was the opaque GuiEvent<394>+u8[12] shell). DWARF
    // BrnGuiEventTypeDefs.h:4563 (:4565..:4570) names the fields; the wire is RAW -- pinned by
    // AddGuiEvent<GuiHitVehicleEvent> @0x823D9680 -> AddEvent(q, ev, 394, 24) and
    // BoostMessageManager::RecvEvent case 394 (@0x82420C34: category at +0x08, the score the
    // +0x0C base + +0x10 chain-bonus sum).
    struct GuiHitVehicleEvent
    {
        BrnTraffic::VehicleClass          meVehicleClass;         // +0x00
        s32                               miVehicleClassTotalHit; // +0x04
        BrnTraffic::VehicleScoreCategory  meVehicleScoreCategory; // +0x08
        s32                               miVehicleBaseScore;     // +0x0C
        s32                               miVehicleChainBonus;    // +0x10
        u16                               muVehicleIndex;         // +0x14
        u16                               maPad16;                // +0x16 trailing pad to 24

        s32 GetEventType() const { return 394; }
    };
    static_assert(sizeof(GuiHitVehicleEvent) == 24, "X360 AddGuiEvent size 24 (id 394)");
    struct GuiImageGalleryCollectedCountEvent { u8 maData[8]; s32 GetEventType() const { return 520; } };  // id 520 size 8 (raw; size not GuiEvent-shaped)
    struct GuiImageGalleryCollectedDataEvent : public CgsGui::GuiEvent<522> { u8 maPayload[4]; };  // id 522 size 16 (12B GuiEvent header + opaque payload)
    struct GuiImageGalleryImageInfoEvent : public CgsGui::GuiEvent<518> { u8 maPayload[36]; };  // id 518 size 48 (12B GuiEvent header + opaque payload)
    // [boost-msg wave] RECOVERED: DWARF BrnGuiEventTypeDefs.h:4460 {mfCumulativeAirTime,
    // mfCurrentJumpAirTime}; the manager's RecvEvent case 387 latches the CURRENT jump time
    // from +0x04 (@0x82420BB8).
    struct GuiInAirEvent
    {
        f32 mfCumulativeAirTime;   // +0x00
        f32 mfCurrentJumpAirTime;  // +0x04

        s32 GetEventType() const { return 387; }
    };  // id 387 size 8
    // [gateui r3] GuiInEventFinisher (id 423) has been RECOVERED and now lives in
    // BrnGuiEventTypeDefs.h with its real DWARF field set (DWARF :5651, sizeof 8).
    // The opaque placeholder that stood here was DELETED rather than left to shadow it.
    // [gateui r3] GuiInEventLeaderSplit (id 420) has been RECOVERED and now lives in
    // BrnGuiEventTypeDefs.h with its real DWARF field set (DWARF :5618, sizeof 24).
    // The opaque placeholder that stood here was DELETED rather than left to shadow it.
    struct GuiInEventNeckAndNeck { u8 maData[1]; s32 GetEventType() const { return 421; } };  // id 421 size 1 (raw; size not GuiEvent-shaped)
    struct GuiInEventRivalProgress : public CgsGui::GuiEvent<422> { u8 maPayload[12]; };  // id 422 size 24 (12B GuiEvent header + opaque payload)
    // [boost-msg wave] RECOVERED (was the opaque GuiEvent<391>+u8[12] shell). DWARF
    // BrnGuiEventTypeDefs.h:4433 (:4435..:4441); RAW wire pinned by AddGuiEvent @0x823D2940
    // -> AddEvent(q, ev, 391, 24) and HandleOnInProgressStunt @0x82411C70 reading the mask at
    // +0x00, the air-spin angle at +0x08 and the handbrake angle at +0x0C.
    struct GuiInProgressStuntEvent
    {
        u32 muStuntActionInProgress;            // +0x00 (bit1 air spin, bit2 handbrake turn)
        f32 mfInProgressBarrelRollAngle;        // +0x04
        f32 mfInProgressAirSpinAngle;           // +0x08 (radians; the handler converts x57.29578)
        f32 mfInProgressHandbreakTurnAngle;     // +0x0C (degrees)
        f32 mfInProgressDriftTime;              // +0x10
        f32 mfInProgressDriftDistance;          // +0x14

        s32 GetEventType() const { return 391; }
    };
    static_assert(sizeof(GuiInProgressStuntEvent) == 24, "X360 AddGuiEvent size 24 (id 391)");
    struct GuiLastBlueTeamMemberEvent { u8 maData[1]; s32 GetEventType() const { return 452; } };  // id 452 size 1 (raw; size not GuiEvent-shaped)
    // [gateui] GuiLeaderPassedKMBoundaryEvent (449) and GuiLeaderPassedMileBoundaryEvent
    // (448) have been RECOVERED and now live in BrnGuiEventTypeDefs.h as
    // { EActiveRaceCarIndex, f32 metres } (DWARF :6136 / :6126).
    struct GuiLeaptVehicleEvent { u8 maData[1]; s32 GetEventType() const { return 393; } };  // id 393 size 1 (raw; size not GuiEvent-shaped)
    struct GuiLocalPlayerEliminatedEvent { u8 maData[1]; s32 GetEventType() const { return 451; } };  // id 451 size 1 (raw; size not GuiEvent-shaped)
    struct GuiMugshotControlEvent : public CgsGui::GuiEvent<325> { u8 maPayload[12]; };  // id 325 size 24 (12B GuiEvent header + opaque payload)
    // [boost-msg wave] RECOVERED: DWARF BrnGuiEventTypeDefs.h:4392 {miCount,
    // meNearMissType}. The manager's RecvEvent case 384 reads both words and treats
    // NEAR_MISS_TYPE 2/3 (the two crash-escape flavours) as "crash escape" (@0x82420BF0).
    struct GuiNearMissEvent
    {
        s32                    miCount;         // +0x00 running near-miss chain count
        BrnWorld::ENearMissType meNearMissType;  // +0x04 (2/3 = crash-escape variants)

        s32 GetEventType() const { return 384; }
    };  // id 384 size 8
    struct GuiNetworkLastStunRunEvent { u8 maData[1]; s32 GetEventType() const { return 490; } };  // id 490 size 1 (raw; size not GuiEvent-shaped)
    struct GuiNetworkStuntRunEliminationEvent : public CgsGui::GuiEvent<487> {};  // id 487 size 12
    struct GuiNetworkStuntRunLeadingEvent : public CgsGui::GuiEvent<488> {};  // id 488 size 12
    struct GuiNetworkStuntRunVictoryEvent : public CgsGui::GuiEvent<489> {};  // id 489 size 12
    struct GuiNetworkSuntRunInfoMessageEvent : public CgsGui::GuiEvent<491> {};  // id 491 size 12
    // [boost-msg wave] RECOVERED: DWARF BrnGuiEventTypeDefs.h:4472 {f32 mfDistance};
    // RecvEvent case 388 latches it with a single float load (@0x82420B38).
    struct GuiOncomingEvent { f32 mfDistance; s32 GetEventType() const { return 388; } };  // id 388 size 4
    struct GuiOnlineCarStatusEvent { u8 maData[8]; s32 GetEventType() const { return 563; } };  // id 563 size 8 (raw; size not GuiEvent-shaped)
    struct GuiOverlayWaitFinishRequest { u8 maData[8]; s32 GetEventType() const { return 188; } };  // id 188 size 8 (raw; size not GuiEvent-shaped)
    struct GuiOvertakeEvent { u8 maData[8]; s32 GetEventType() const { return 371; } };  // id 371 size 8 (raw; size not GuiEvent-shaped)
    // [boost-msg wave] NEW HOME (no committed record existed). DWARF
    // BrnGuiEventTypeDefs.h:4369 {CgsID mShortcutId}; PS3 GuiEvent<377>, X360 id 382.
    // BoostMessageManager::RecvEvent case 382 latches the qword at +0x00 into mShortcutId
    // (@0x82420928 `ld/std`). No AddGuiEvent<T> instantiation was located for this id, so
    // the size is pinned by the consumer load (offsetof 0x00, 8-byte width), not a literal.
    struct GuiOffenceShortcutEvent
    {
        CgsID mShortcutId;   // +0x00

        s32 GetEventType() const { return 382; }
    };  // id 382 size 8 (consumer-pinned)
    struct GuiPFXHookEvent : public CgsGui::GuiEvent<495> { u8 maPayload[52]; };  // id 495 size 64 (12B GuiEvent header + opaque payload)
    struct GuiPFXHookStopEvent : public CgsGui::GuiEvent<496> { u8 maPayload[28]; };  // id 496 size 40 (12B GuiEvent header + opaque payload)
    struct GuiPFXStartBackgroundHookEvent : public CgsGui::GuiEvent<498> { u8 maPayload[32]; };  // id 498 size 44 (12B GuiEvent header + opaque payload)
    struct GuiPFXStopBackgroundHookEvent : public CgsGui::GuiEvent<499> { u8 maPayload[28]; };  // id 499 size 40 (12B GuiEvent header + opaque payload)
    struct GuiPaybackReceivedEvent { u8 maData[4]; s32 GetEventType() const { return 182; } };  // id 182 size 4 (raw; size not GuiEvent-shaped)
    struct GuiPlayerCarColourResponse { u8 maData[8]; s32 GetEventType() const { return 414; } };  // id 414 size 8 (raw; size not GuiEvent-shaped)
    struct GuiPlayerDrivableFromCrash { u8 maData[1]; s32 GetEventType() const { return 378; } };  // id 378 size 1 (raw; size not GuiEvent-shaped)
    struct GuiPlayerEliminatedEvent { u8 maData[4]; s32 GetEventType() const { return 450; } };  // id 450 size 4 (raw; size not GuiEvent-shaped)
    // GuiPlayerEngineEvent (id 379 size 4) MOVED OUT 2026-08-25 (hud reveal gate) to
    // GameSource/Gui/BrnGuiEventTypeDefs.h, per this header's own migration rule at the top:
    // it now carries a recovered ENUM (the console's own E_ENGINE_OFF/E_ENGINE_ON, lifted from
    // a baked assert string) rather than an opaque byte blob, and it sits there beside
    // GuiPlayerCrashingStateChangeEvent -- which is literally its neighbour in the producer,
    // BridgeWorldVehicleDataToGui. Do not re-add it here.
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
    // [hud H3b tracking slice 2026-08-25] GuiPlayerRaceCarIdEvent (376) has been
    // RECOVERED and now lives in BrnGuiEventTypeDefs.h with its real X360 field pair.
    // The opaque shell that stood here was DELETED rather than left to shadow it.
    // [gateui r3] GuiPowerParkResult (id 404) has been RECOVERED and now lives in
    // BrnGuiEventTypeDefs.h with its real DWARF field set (DWARF :5467, sizeof 8).
    // The opaque placeholder that stood here was DELETED rather than left to shadow it.
    struct GuiPursuitScoreUpdate { u8 maData[4]; s32 GetEventType() const { return 432; } };  // id 432 size 4 (raw; size not GuiEvent-shaped)
    // [gateui r3] GuiRaceCheckpointReached (id 425) has been RECOVERED and now lives in
    // BrnGuiEventTypeDefs.h with its real DWARF field set (DWARF :5690, sizeof 12).
    // The opaque placeholder that stood here was DELETED rather than left to shadow it.
    struct GuiReplayStatusEvent : public CgsGui::GuiEvent<524> { u8 maPayload[1548]; };  // id 524 size 1560 (12B GuiEvent header + opaque payload)
    struct GuiRoadRageScoreUpdate { u8 maData[8]; s32 GetEventType() const { return 426; } };  // id 426 size 8 (raw; size not GuiEvent-shaped)
    struct GuiSetEasyDriveNotAllowedEvent { u8 maData[1]; s32 GetEventType() const { return 96; } };  // id 96 size 1 (raw; size not GuiEvent-shaped)
    struct GuiShowtimeJustBounced { u8 maData[2]; s32 GetEventType() const { return 402; } };  // id 402 size 2 (raw; size not GuiEvent-shaped)
    struct GuiShowtimeScoreUpdate : public CgsGui::GuiEvent<396> {};  // id 396 size 12
    struct GuiShowtimeTriggered { u8 maData[1]; s32 GetEventType() const { return 392; } };  // id 392 size 1 (raw; size not GuiEvent-shaped)
    // [boost-msg wave] NEW HOME (no committed record existed). DWARF
    // BrnGuiEventTypeDefs.h:3694 {s32 miChainCount}; PS3 GuiEvent<362>, X360 id 367.
    // BoostMessageManager::RecvEvent case 367 latches word0 into miStuntChain and raises
    // mbStuntDone (@0x824208BC). No AddGuiEvent<T> instantiation located -- size pinned by
    // the consumer read, not a literal.
    struct GuiStuntEvent
    {
        s32 miChainCount;   // +0x00

        s32 GetEventType() const { return 367; }
    };  // id 367 size 4 (consumer-pinned)
    struct GuiShutdownEvent { u8 maData[8]; s32 GetEventType() const { return 373; } };  // id 373 size 8 (raw; size not GuiEvent-shaped)
    struct GuiShutdownFinishedEvent { u8 maData[1]; s32 GetEventType() const { return 374; } };  // id 374 size 1 (raw; size not GuiEvent-shaped)
    // [boost-msg wave 2026-08-26] RETIRED -- GuiSoftTakedownEvent now lives as a real raw
    // 32-byte record in BrnGuiEventTypeDefs.h next to its GuiTakedownEvent sibling (this
    // header includes that one). This retires GameBridgeGameStateToX.cpp's parked soft arm too.
    // [boost-msg wave] RECOVERED: DWARF BrnGuiEventTypeDefs.h:4449 {f32 mfSpinAngle};
    // RecvEvent case 386 asserts it >= 0 and latches it (@0x824209C0).
    struct GuiSpinningEvent { f32 mfSpinAngle; s32 GetEventType() const { return 386; } };  // id 386 size 4
    // [boost-msg wave] RECOVERED: DWARF BrnGuiEventTypeDefs.h:4483 {f32 mfDistance};
    // RecvEvent case 389 latches it (@0x82420BC8).
    struct GuiTailgatingEvent { f32 mfDistance; s32 GetEventType() const { return 389; } };  // id 389 size 4
    // [boost-msg wave] RECOVERED: DWARF BrnGuiEventTypeDefs.h:4381 {s32 miCount};
    // RecvEvent case 383 latches the count and raises mbChecking (@0x82420BD8).
    struct GuiTrafficCheckEvent { s32 miCount; s32 GetEventType() const { return 383; } };  // id 383 size 4

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
    // GuiAudioTriggerEvent: NO placeholder here -- the real hand-reconstructed home is
    // BrnGuiEventTypeDefs.h (macComponent/meAction/macLabel/macMovie, id 201, sizeof 112).
    // FLAG / OPEN: this catalogue read the id as 457 size 100 from the AddGuiEvent<T>
    // spine, which does NOT match the real home's id 201 size 112. Two different events
    // are sharing one name; one of them is misnamed. Unresolved -- needs asm arbitration.
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
        s32 miChall;           // +0x0C  the challenge STYLE: all three producers fill it from
                               //        BrnResource::ChallengeListEntry::GetChallengeStyle()
                               //        @0x823542A0 (1=NORMAL, 2=ROAD_RULES_TIME, 3=ROAD_RULES_CRASH).
                               //        FLAG consumer-named: the field name `miChall` was taken from
                               //        the IDB's 41-char-TRUNCATED symbol "…ChallengeListEntry::GetChall";
                               //        there is no such method (see ChallengeListEntry.h).
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
    // id 272. The payload word is the sign-in flavour the producer asks for (0 = full
    // sign-in, 2 = no-title); the producer-side enum name is unrecovered.
    // Shaped as a GuiEvent<272> subclass, NOT as an opaque POD, because the X360 posts it
    // through StateInterface::OutputGuiEvent -- CgsGuiStateInterface_OutputGuiEvent_Inst.cpp
    // carries the explicit instantiation for this exact type at 0x824C2E98 (inside
    // CrashNavEnterOnlineBase::OnEnter), and that template requires a CgsModule::Event.
    // FLAG: this catalogue's original entry read "size 4", which cannot be right for an
    // OutputGuiEvent payload (the GuiEvent header alone is 12) -- the 4 is the payload word
    // only. The posted size is the HOST sizeof, never a console immediate.
    struct GuiEventNetworkConnect : public CgsGui::GuiEvent<272>
    {
        s32 miConnectType;

        explicit GuiEventNetworkConnect(s32 liConnectType)
            : CgsGui::GuiEvent<272>(
                  static_cast<u32>(sizeof(GuiEventNetworkConnect) - sizeof(CgsGui::GuiEvent<272>)),
                  static_cast<u32>(sizeof(CgsGui::GuiEvent<272>)))
            , miConnectType(liConnectType) {}
    };
    struct GuiEventNetworkCustomMatchJoin { u8 maData[4]; s32 GetEventType() const { return 255; } };  // id 255 size 4
    // id 254, record 604 bytes -- HEADERLESS, and deliberately so. MEASURED: the only
    // producer, BrnNetwork::BrnNetworkModule::AddOutputGuiEvent<T> @0x82565E88, calls
    // AddEvent(queue, &event, 254, 604) with the CALLER'S OBJECT POINTER -- it does not
    // stack-build a { payload size, id, payload offset } header the way OutputGuiEvent<T>
    // does (contrast GuiEventNetworkCustomMatchSearch below). So the console record IS the
    // 604-byte payload, and the host type must be 604 bytes carrying its own
    // GetEventType() == 254 (this header's raw-struct form). The previous
    // GuiEvent<254> + u8[592] shape had the right TOTAL size but pushed all 604 bytes of
    // real data 12 bytes down behind a header the producer never writes.
    //
    // Field names and the three constants are DWARF-attested (DecFIGS
    // BrnGuiEventTypeDefs.h:3180; KI_* :3183/:3184/:3186, members :3188-:3196).
    // MEMBER ORDER IS THE X360's, NOT the DWARF's: DecFIGS declares maacGameNames first,
    // but the ARTIST asm puts the six 10-element arrays first and the names last.
    // OnlineCustomMatch::FillInTable @0x8248BA90 reads the member (this+57332) at +0
    // (maiNumPlayers, the "%d" first arg), +40 (maiMaxNumPlayers), +80 (maiGameFlags,
    // tested &2 then &4), +160 (maeGameMode, indexes the string table as [mode-10]),
    // +200 (maePreviousGameMode) and +240 (miNumGames), and the names at +244 with
    // stride 36; HandleControllerInputSelectGame @0x82497570 reads +120
    // (maiFoundGameIndex). Rung 1 arbitrates placement, and 244 + 10*36 == 604 closes the
    // record exactly.
    // FLAG (id delta): DecFIGS derives this type from GuiEvent<252>; the X360 id is 254 --
    // the same uniform +2 shift as the rest of this cluster (see GuiEventShowHideSatNav).
    struct GuiEventNetworkCustomMatchResults
    {
        static const s32 KI_HAS_FRIENDS   = 2;    // DWARF :3183 -- maiGameFlags mask -> the "Friends" icon
        static const s32 KI_HAS_RIVALS    = 4;    // DWARF :3184 -- maiGameFlags mask -> the "Rivals" icon
        static const s32 KI_MAX_NUM_GAMES = 10;   // DWARF :3186

        s32  maiNumPlayers[KI_MAX_NUM_GAMES];        // payload +0x000
        s32  maiMaxNumPlayers[KI_MAX_NUM_GAMES];     // payload +0x028
        s32  maiGameFlags[KI_MAX_NUM_GAMES];         // payload +0x050 (KI_HAS_FRIENDS / KI_HAS_RIVALS)
        s32  maiFoundGameIndex[KI_MAX_NUM_GAMES];    // payload +0x078 (the word the CustomMatchJoin payload carries)
        s32  maeGameMode[KI_MAX_NUM_GAMES];          // payload +0x0A0 -- DWARF type
                                                     //   BrnGameState::GameStateModuleIO::EGameModeType; carried as
                                                     //   s32 because that enum's home does not include cleanly here
                                                     //   (see the GuiEventAudioTraxUpdate note above). X360 values 10..17.
        s32  maePreviousGameMode[KI_MAX_NUM_GAMES];  // payload +0x0C8 -- same enum; outside 10..17 means "no previous mode"
        s32  miNumGames;                             // payload +0x0F0
        char maacGameNames[KI_MAX_NUM_GAMES][36];    // payload +0x0F4 .. +0x25C

        s32 GetEventType() const { return 254; }
    };  // id 254, record 604 bytes (headerless -- AddOutputGuiEvent posts the object itself)
    // The record size IS the X360 AddEvent literal, and the whole record is a pointer-free
    // scalar run, so it is host-stable and worth pinning.
    static_assert(sizeof(GuiEventNetworkCustomMatchResults) == 604,
                  "GuiEventNetworkCustomMatchResults is the 604-byte AddOutputGuiEvent record");
    // id 252, record 24 bytes: { 12, 252, 12 } + a 12-byte payload. MEASURED at
    // OutputGuiEvent<GuiEventNetworkCustomMatchSearch> @0x82493BE8, which stack-builds
    // v4 = { 12, 252, 12 }, copies THREE words out of the caller's object into v4[3..5] and
    // calls AddEvent(queue, v4, 40, 24). The old bare GuiEvent<252> posted an EMPTY payload
    // where the console copies 12 bytes.
    // Field names and types are DWARF-attested (DecFIGS BrnGuiEventTypeDefs.h:3164,
    // members :3165-:3168).
    // FLAG (id delta): DecFIGS derives this type from GuiEvent<250>; the X360 id is 252.
    // WIRE NOTE -- this is a CHANNEL-40 type, so it takes the BAKED-HEADER encoding, unlike
    // the wrapped channel-41/42 types (GuiEventShowHideSatNav below, which is deliberately
    // raw). Per the FLAG in CgsGuiStateInterface.h, the committed OutputGuiEvent<T> body
    // direct-passes the event (it does NOT build a GuiEventWrapper), while OutputViewState /
    // OutputInternalState do build one. So a channel-40 payload has to fold the wrapper's
    // three header words into itself through the CgsGui::GuiEvent<252> base, and the record
    // is published with AddEvent(&event, /*channel*/ 40, sizeof(event)) == 24 -- byte-for-byte
    // the console record. On the console the type is the 12-byte payload only.
    // OnlineCustomMatch::mLastSearchParams IS this type (12 bytes on the console, 24 on the
    // host -- harmless, every access is by name). If OutputGuiEvent<T> is ever rebuilt to
    // emit the real wrapper header, this base must come back off or the header ships twice.
    struct GuiEventNetworkCustomMatchSearch : public CgsGui::GuiEvent<252>
    {
        s32  meGameMode;              // payload +0x00 -- DWARF type BrnNetwork::ESearchGameModes,
                                      //   which has no committed home; carried as s32 exactly as
                                      //   BrnOnlineCustomMatch.h's StringGameModeMapping does.
                                      //   X360 producer values: 0 any, 1 race, 4 freeburn lobby.
        s32  meSearchOpponentTypes;   // payload +0x04 -- DWARF type BrnNetwork::ESearchOpponentTypes
                                      //   (same no-home treatment); widened from the toggle row's
                                      //   SIGNED s8 highlight index, so -1 is reachable.
        bool mbRanked;                // payload +0x08
        bool mbFreeburn;              // payload +0x09
        u8   maPad[2];                // payload +0x0A -- the console reads the +8 word whole (`lwz`)

        GuiEventNetworkCustomMatchSearch()
            : CgsGui::GuiEvent<252>(
                  static_cast<u32>(sizeof(GuiEventNetworkCustomMatchSearch)
                                   - offsetof(GuiEventNetworkCustomMatchSearch, meGameMode)),
                  static_cast<u32>(offsetof(GuiEventNetworkCustomMatchSearch, meGameMode)))
            , meGameMode(0), meSearchOpponentTypes(0), mbRanked(false), mbFreeburn(false)
        { maPad[0] = 0; maPad[1] = 0; }
    };  // id 252, record 24 bytes
    static_assert(sizeof(GuiEventNetworkCustomMatchSearch) == 24,
                  "GuiEventNetworkCustomMatchSearch is the 24-byte OutputGuiEvent record");
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
    // id 572 size 4 -- UPGRADED from the opaque u8[4] 2026-08-25 (HUD H2): DWARF home
    // BrnGuiEventTypeDefs.h:6556 `GuiEventRequestTraining : GuiEvent<557>` with the one
    // member meTrainingType (BrnProgression::ETrainingType; PS3 id 557 -> X360 572, the
    // same +15 drift the sibling GuiChallengeEndEvent 563 -> 578 carries). Producer
    // witness: RoadRuleComponent::SetUpcomingRoadAnimation @0x82438650 stack-builds
    // { 4, 572, 12, 9 } -- the suggested-road transin fires training message 9.
    struct GuiEventRequestTraining { s32 meTrainingType; s32 GetEventType() const { return 572; } };  // id 572 size 4
    struct alignas(8) GuiEventRoadRuleDataRequest { u8 maData[8]; s32 GetEventType() const { return 327; } };  // id 327 size 8 [8-aligned: OGE off16]
    struct GuiEventRoadRuleModeRequest { u8 maData[8]; s32 GetEventType() const { return 326; } };  // id 326 size 8
    // X360 instantiation @0x82493F88: record { 36, 121, 12, score, category, index,
    // variation, name[16], isCurrentTarget, pad[3] }, ch40, 48 bytes (GameBridgeGUIToX
    // case 121 "36-byte record": words 0..3, name @+16, byte @+32).
    struct GuiEventScoreboardRequestEvScoreTarget : public CgsGui::GuiEvent<121>
    {
        s32  miScore;             // +0x0C
        s32  miCategory;          // +0x10
        s32  miIndex;             // +0x14
        s32  miVariation;         // +0x18
        CgsNetwork::PlayerName mPlayerName;   // +0x1C (16 bytes; the named home of the same char[16])
        bool mbIsCurrentTarget;   // +0x2C  FLAG consumer-named (the producer compares the
                                  //         highlighted gamertag against the current target)
        u8   maPad[3];            // +0x2D..0x2F
        GuiEventScoreboardRequestEvScoreTarget()
            : CgsGui::GuiEvent<121>(36, 12)
            , miScore(0), miCategory(0), miIndex(0), miVariation(0), mbIsCurrentTarget(false)
        { mPlayerName.macName[0] = 0; maPad[0] = maPad[1] = maPad[2] = 0; }
    };
    // X360 instantiation @0x82493D38: record { 16, 120, 12, name[16] }, ch40, 28 bytes
    // (GameBridgeGUIToX case 120 memcpy's 16 -- the old maPayload[4] under-copied).
    struct GuiEventScoreboardRequestGamercardEvent : public CgsGui::GuiEvent<120>
    {
        CgsNetwork::PlayerName mPlayerName;   // +0x0C (16 bytes; the named home of the same char[16])
        GuiEventScoreboardRequestGamercardEvent()
            : CgsGui::GuiEvent<120>(16, 12) { mPlayerName.macName[0] = 0; }
    };
    // X360 instantiation @0x82493FF8: record { 4, 111, 12, <category> }, ch40, 16 bytes
    // (GameBridgeGUIToX reads the word at record+12 -- the old raw {maData[4]} shape
    // dropped the 12-byte header the consumer expects).
    struct GuiEventScoreboardRequestIndexEvent : public CgsGui::GuiEvent<111>
    {
        s32 miCategory;   // +0x0C
        explicit GuiEventScoreboardRequestIndexEvent(s32 liCategory)
            : CgsGui::GuiEvent<111>(4, 12), miCategory(liCategory) {}
    };
    // X360 instantiation @0x82494098: record { 4, 113, 12, <variation/road> }, ch40, 16 bytes.
    struct GuiEventScoreboardRequestTableEvent : public CgsGui::GuiEvent<113>
    {
        s32 miVariation;   // +0x0C
        explicit GuiEventScoreboardRequestTableEvent(s32 liVariation)
            : CgsGui::GuiEvent<113>(4, 12), miVariation(liVariation) {}
    };
    // X360 instantiation @0x82494048: record { 4, 112, 12, <index> }, ch40, 16 bytes.
    struct GuiEventScoreboardRequestVariationEvent : public CgsGui::GuiEvent<112>
    {
        s32 miIndex;   // +0x0C
        explicit GuiEventScoreboardRequestVariationEvent(s32 liIndex)
            : CgsGui::GuiEvent<112>(4, 12), miIndex(liIndex) {}
    };
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
    // X360 instantiation @0x824940E8: record { 20, 323, 12, type, params[16] }, ch40,
    // 32 bytes (GameBridgeGUIToX case 323 memcpy's 20 -- the old maPayload[8] was short).
    // FLAG: field names consumer-derived (the type word + the SPrintf'd parameter string
    // BrnNetwork::BrnNetworkModuleIO::TelemetryData::AddParameter appends).
    struct GuiTelemetryEvent : public CgsGui::GuiEvent<323>
    {
        s32  miTelemetryType;   // +0x0C
        char macParams[16];     // +0x10
        explicit GuiTelemetryEvent(s32 liTelemetryType)
            : CgsGui::GuiEvent<323>(20, 12), miTelemetryType(liTelemetryType) { macParams[0] = 0; }
    };

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
    // id 213, PAYLOAD 12 bytes -- and it stays a raw 12-byte payload, with no
    // CgsGui::GuiEvent<213> base, because this type only ever travels the WRAPPED channels.
    // MEASURED: OutputViewState<GuiEventShowHideSatNav> @0x82476DD8 and
    // OutputInternalState<...> @0x82476E38 stack-build v4 = { 12, 213, 12 }, copy THREE words
    // out of the caller's object (loaded from 0/4/8 of the passed event) into v4[3..5] and
    // call AddEvent(queue, v4, 41 | 42, 24) -- so the three header words are NOT part of this
    // type and sizeof(T) is the 12 in the record's first word. PreRaceFlyByState::OnLeave
    // @0x824C68F0 shows the same record inlined (`li r30,0xC` / `li r26,0xD5` / three payload
    // stores / `li r6,0x18` / `li r5,0x29` then `li r5,0x2A`: one record, both channels).
    // Consumers must therefore go through StateInterface::OutputViewState / OutputInternalState
    // (CgsGuiStateInterface.h builds the GuiEventWrapper<T,41|42>); posting this struct
    // directly on the out-queue would ship a headerless 12-byte record.
    // The previous placeholder was a bare GuiEvent<213>, i.e. a header with NO payload where
    // the console copies 12 bytes of real data.
    // The three fields are DWARF-ATTESTED, not consumer-named: DecFIGS BrnGuiEventTypeDefs.h
    // declares meMapType/mfFadeTime/mbShow private at :2042-:2044, Construct(MapType, bool,
    // float32_t) at :2015, the accessors at :2023/:2029/:2035, the nested MapType at :2005.
    // FLAG (id delta): DecFIGS derives this type from GuiEvent<211>, the X360 id is 213. The
    // whole cluster is shifted by +2 between the PS3 (Dec-2007) and X360 (Jan-2008) builds --
    // ShowHideBoostBar 212->214, CustomMatchSearch 250->252, CustomMatchResults 252->254,
    // CustomMatchJoin 253->255, every one matching this header's committed X360 ids -- so this
    // is the same type renumbered. The X360 id wins.
    struct GuiEventShowHideSatNav
    {
        enum MapType { E_MAPTYPE_MAIN = 0, E_MAPTYPE_GPS = 1 };   // DWARF :2005

        GuiEventShowHideSatNav()
            : meMapType(E_MAPTYPE_MAIN), mfFadeTime(0.0f), mbShow(false) {}

        // DWARF :2015 -- the parameter order is (map type, show, fade time) while the payload
        // order is (map type, fade time, show). Keep the DWARF order: call sites read as
        // Construct(E_MAPTYPE_MAIN, false, 0.0f) == "hide the main map, no fade".
        void Construct(MapType leMapType, bool lbShow, f32 lfFadeTime)
        {
            meMapType  = leMapType;
            mfFadeTime = lfFadeTime;
            mbShow     = lbShow;
        }

        MapType GetMapType()  const { return meMapType; }    // DWARF :2023
        bool    GetShow()     const { return mbShow; }       // DWARF :2029
        f32     GetFadeTime() const { return mfFadeTime; }   // DWARF :2035

        s32 GetEventType() const { return 213; }

    private:
        MapType meMapType;    // payload +0x00 (DWARF :2042)
        f32     mfFadeTime;   // payload +0x04 (DWARF :2043)
        bool    mbShow;       // payload +0x08 (DWARF :2044; pads out to the attested 12)
    };  // id 213, payload 12 bytes -> wrapped record 24 on channel 41 / 42
    static_assert(sizeof(GuiEventShowHideSatNav) == 12,
                  "GuiEventShowHideSatNav is the 12-byte OutputViewState/InternalState payload");
    struct alignas(8) GuiEventSetHoveredEventIcon : public CgsGui::GuiEvent<559> { u8 maPayload[12]; };  // id 559 size 24 [8-aligned: OViewState off16]
}
