#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // EPlayerTeam
#include "GameSource/GameState/BrnGameStateTypes.h"          // BrnGameState::LandmarkIndex (StartNetworkRoundEvent)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h" // CgsContainers::FastBitArray<60> (LastSecondChallengeSuccess)
#include "GameSource/GameState/BrnCgsPlayerName.h"           // CgsNetwork::PlayerName (BuddyRemovedEvent)
#include "SharedClasses/StreetData/BrnStreetData.h"          // BrnStreetData::ChallengeIndex (road-rules events)
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h" // BrnStreetData::ChallengeHighScoreEntry (PB-recv event)

// Owning header for the BrnGameState::GameStateModuleIO GameEvent<> family slices reconstructed
// by the GameMode/ModeManager leaf batch. Minimal slices: only members the reconstructed bodies
// touch. GameEvent<T> is an empty template spine (the real build adds only a static type tag);
// its real Event base + the per-event mseType definitions land with the full BrnGameEvents TU.

namespace BrnGameState
{
namespace GameStateModuleIO
{
// Max players in a network game (== BrnWorld::KI_MAX_ACTIVE_RACE_CARS on this build).
const s32 KI_MAX_RACE_CARS = 8;

// EGameEventType discriminant. Only the slots this batch instantiates are listed; the unconfirmed
// values are placeholders used purely as template tags.
enum EGameEventType
{
    E_EVENT_TELEPORT_PLAYER_CAR     = 1,
    E_EVENT_CHANGE_PLAYER_CAR       = 2,
    E_EVENT_SELECT_PLAYER_CAR       = 4,
    E_EVENT_CHANGE_PLAYER_CAR_COLOUR = 5,
    E_EVENT_PLAYER_CAR_COLOUR_REQUEST = 6,
    E_EVENT_UNLOCKED_LIVERY_REQUEST = 82, // ARTIST; DecFIGS has 83
    E_EVENT_STREAMING_COMPLETE      = 9,   // DWARF BrnGameEvents.h:19 (full contiguous EGameEventType)
    E_EVENT_CHANGE_NETWORK_CAR      = 7,
    E_EVENT_ONLINE_PLAYER_ADDED     = 127,
    E_EVENT_ONLINE_PLAYER_FINALISED = 128,
    E_EVENT_ONLINE_PLAYER_REMOVED   = 129,   // posted as 129 by TranslateNetworkEventsToGameEvents
    E_EVENT_START_NETWORK_GAME      = 17,    // DWARF BrnGameEvents.h (full contiguous EGameEventType)
    E_EVENT_START_NETWORK_ROUND     = 18,    // DWARF BrnGameEvents.h
    // Network state manager signals. 16 and 22 equal the reference ids; 27 and 60 sit one
    // below them (reference 28 / 61), the same drift the ids 31..36 above carry. Each value is
    // the producer's AddEvent immediate and a GameStateModule::ProcessGameEvents case.
    E_EVENT_ONLINE_CAR_SELECT          = 16,    // StateManager::UpdateLaunching, 1 byte
    E_EVENT_FINISHED_SYNCING_PLAYERS   = 22,    // StateManager::UpdateSyncTime, 1 byte
    E_EVENT_PLAYER_EXITED_MODE         = 27,    // StateManager::ProcessGuiEvents, 1 byte; case 27 cancels the mode
    E_EVENT_PREPARED_FOR_INVITE        = 60,    // StateManager::PrepareForInvite, 4 bytes
    E_EVENT_LOCAL_PLAYER_DISCONNECTED  = 123,   // BrnNetworkManager::ProcessHLUpdateFlags and
                                                // ::TriggerEventFromServerInterface, 1 byte
    // Posted as 121 by TranslateNetworkEventsToGameEvents; ProcessGameEvents' case 121 is the
    // arm that turns it into the remote-player-disconnected action.
    E_EVENT_REMOTE_PLAYER_DISCONNECTED = 121,
    E_EVENT_RECORD_PROP_HIT         = 111,   // DWARF BrnGameEvents.h:121
    E_EVENT_CHANGE_WORLD_REGION     = 115,   // X360-attested: RaceCarEntityModule::
                                             // UpdateCurrentWorldRegion @0x822F5824 posts it
                                             // (8B {county,district}); GameStateModule::
                                             // ProcessGameEvents case 115 consumes it (H1 wave)
    // [gateui] DWARF BrnGameEvents.h:122 -- the immediate neighbour of RECORD_PROP_HIT, and the
    // one the world->GameState bridge's prop leg needs as its second event type. Game EVENT ids
    // are NOT subject to the +5 shift the ACTION ids carry in this range (see the long note in
    // BrnGameActions.h): 111 matches the X360 ProcessGameEvents @0x823A0A18 jump table's case 111
    // exactly, so its DWARF-contiguous neighbour 112 is trustworthy as-is.
    E_EVENT_REQUEST_PROP_PROGRESSION = 112,  // DWARF BrnGameEvents.h:122
    E_EVENT_PLAYER_ROUTE_UPDATED    = 117,   // DWARF BrnGameEvents.h:127; posted by AIModule::UpdateCarRoutes @0x827955F0 (1-byte payload)
    E_EVENT_OVERHEAD_SIGN_HIT       = 118,   // DWARF BrnGameEvents.h:76
    // [P1 sim-pause] the four pause-family events the ProcessGameEvents pause arm consumes.
    // ⚠️ X360 VALUES, not the PS3 DWARF's: in this region the PS3 ids sit ONE HIGHER
    // (PS3 34/36/37/94 for these four names) -- the X360 jump table @0x823A0A18 is the
    // authority (case 33 reads the 3-byte pause payload and calls RequestPause(2,...);
    // cases 35/36 are the replay pause pair (reason 16); case 93 is the crash-nav pair
    // (reason 4), fed by BridgeGuiToGameState's GUI-191 translation).
    // [!!] [stuntrace waveB CLOSURE round, 2026-08-26] X360-PINNED, not inferred. The PS3 DWARF
    // says 33, which is already taken here by E_EVENT_PLAYER_PAUSE_STATE_CHANGED (the pause
    // family sits one LOWER on X360, see the note below), so the wave-B partfile that needed
    // this record refused to write a value at all. It is now read straight off the dispatcher:
    // BrnGameState::GameStateModule::ProcessGameEvents @0x823A0A18, `jumptable 823A107C case 32`
    // @0x823A27F4 -> `mr r4, r25` (the event) / `addi r3, r31, 0x1020` (&mModeManager) /
    // `bl BrnGameState::ModeManager::PlayerFinishedMode` @0x823A27FC. The one function that
    // consumes a PlayerFinishedModeEvent is reached from case 32, so 32 is the discriminant.
    // ⭐⭐⭐ [boost-wave2 2026-09-14] THE RIVAL-IMPACT EVENT -- the source of trading paint /
    // nudge / slam / shunt. PINNED THE SAME WAY ITS NEIGHBOUR BELOW IS, off the dispatcher:
    // GameStateModule::ProcessGameEvents @0x823A0A18 `jumptable 823A107C case 31` @0x823A278C
    // is the arm that calls GameStateModule::SendVehicleImpactMessages, and that function is the
    // only consumer of a VehicleImpactEvent in the image -- so 31 IS the discriminant.
    // The PS3 DWARF says 32 (BrnGameEvents.h enum) -- exactly the one-higher offset the pause
    // family below already records for this region, and 32 is taken here by
    // E_EVENT_PLAYER_FINISHED_MODE, which is itself jump-table-attested.
    // PRODUCER (live, and reconstructed): BrnPhysics::Vehicle::VehicleManager::
    // HandleRaceCarRaceCarContact @0x82642F78 posts it twice -- `AddEventSafe(..., 31, 12)`
    // @0x82643808 (the grind arm) and `AddEvent(..., 31, 12)` @0x82643B58 (the impact arm) --
    // onto VehicleOutputInterface::mGameEventQueue. Both are in the tree at
    // BrnVehicleManager.cpp:378 / :438.
    E_EVENT_VEHICLE_IMPACT          = 31,    // X360 case 31 @0x823A278C (PS3 DWARF 32)
    E_EVENT_PLAYER_FINISHED_MODE    = 32,    // X360 case 32 @0x823A27F4 (PS3 DWARF 33)
    E_EVENT_PLAYER_PAUSE_STATE_CHANGED = 33, // X360 (PS3 DWARF 34)
    E_EVENT_ENTER_REPLAY            = 35,    // X360 (PS3 DWARF 36)
    E_EVENT_PLAYER_CRASH_ENDING     = 42,    // ARTIST PostPhysicsUpdate 827D3CDC; DWARF 43
    E_EVENT_LEAVE_REPLAY            = 36,    // X360 (PS3 DWARF 37)
    E_EVENT_CRASHNAV_STATE_CHANGED  = 93,    // X360 (PS3 DWARF 94)
    // ⭐⭐⭐ [returning-player wave 2026-08-28] THE JUNKYARD-ENTRY COMPLETION EVENT.
    // X360 VALUE IS JUMP-TABLE-ATTESTED, not derived: GameStateModule::ProcessGameEvents
    // @0x823A0A18 `jumptable 823A107C case 78` @0x823A4590 is the arm that tests
    // mbWaitingToPutPlayerInJunkyard and calls CarSelectManager::
    // ReallyEnterJunkyardAtStartOfGame -- i.e. 78 IS the discriminant of "the GUI has entered
    // the in-game screen, finish the start-of-game junkyard entry".
    // ⚠️ THE NAME IS THE PS3 DWARF'S NEIGHBOUR, flagged as such: PS3 BrnGameEvents.h numbers
    // E_EVENT_EVENT_STATE_REQUEST = 78 and E_GUI_HAS_STARTED_GAME = 79, and this region of the
    // enum carries the same NON-uniform -1 drift the pause family above documents. Only
    // E_GUI_HAS_STARTED_GAME matches what the X360 arm does, and it is the PS3 id one higher --
    // exactly the drift direction already attested for cases 32/33/35/36/93. The VALUE below is
    // the X360's; the spelling is the DWARF's.
    // PRODUCER: BrnGui::InGame::OnEnter @0x824D0498 posts GUI command 145 on channel 40
    // (`v19 = 0x100000091LL; AddEvent(queue, &v19, 40, 16)`), and BridgeGuiToGameState
    // @0x823DDB78 case 145 emits this 1-byte signal event.
    // ⭐ [event-state wave 2026-09-10] THE EVENT-STATE QUERY -- how the GUI learns which events the
    // profile has DISCOVERED / WON. X360 ProcessGameEvents case 77 @0x823A0A18: walk
    // Profile::maEvents[0 .. miEventCount), Append every record with flags & 1 (E_FLAG_DISCOVERED)
    // to a local Array<ProfileEvent,175>, post it as action 179 (1404 bytes). PRODUCER: GUI command
    // 555 on channel 40 (FBurnMainHudState::UpdateSetupState @0x82480EA0, CrashNavMap::OnEnter
    // @0x824CB158) -> BridgeGuiToGameState @0x823DDB78 case 555 -> this 1-byte signal. The PS3
    // DWARF numbers it 78 (the same -1 drift as its neighbours). Without this hop the minimap and
    // crash-nav drew NO discovered-event blips and no completed ticks, whatever the save held.
    E_EVENT_EVENT_STATE_REQUEST     = 77,    // X360 case 77 (PS3 DWARF 78)
    E_GUI_HAS_STARTED_GAME          = 78,    // X360 case 78 @0x823A4590 (PS3 DWARF 79)
    // ⭐⭐ [pause-stats wave 2026-08-29] THE GAME-STATS QUERY -- the sibling of the rank query
    // documented immediately below, one id lower, and the three-consecutive-arm attestation in
    // that block is what pins it: X360 case 79 @0x823A2D18 is
    // `ChallengeManager::CountCompletedChallenges` + `ProgressionManager::GetGameStats` posting
    // game action 180 (`li r5, 0xB4 / li r6, 0x160`), == PS3 E_EVENT_GAME_STATS_REQUEST (80).
    // The VALUE below is the X360's; the spelling is the DWARF's.
    // PRODUCER: BrnGui::CrashNavDriverDetails::UpdateInitSetup @0x824CF038 posts
    // GuiEventStatsRequest (GUI event 435) in the same latch that posts 437, and
    // BridgeGuiToGameState's case 435 emits this 1-byte signal event.
    // CONSUMER: GameStateModule::ProcessGameEventsGameStatsRequestBringUp (this tree's
    // extraction of case 79), which answers with game action 180 -> GUI event 436.
    E_EVENT_GAME_STATS_REQUEST      = 79,    // X360 case 79 @0x823A2D18 (PS3 DWARF 80)
    // ⭐⭐ [driver-details pause wave 2026-08-28] THE RANK-PROGRESS QUERY. Same region, same
    // NON-uniform -1 drift the block above documents, and here the drift is pinned by THREE
    // CONSECUTIVE X360 arms rather than inferred:
    //   X360 case 79 @0x823A2D18  ChallengeManager::CountCompletedChallenges +
    //                             ProgressionManager::GetGameStats -> action 180
    //                             == PS3 E_EVENT_GAME_STATS_REQUEST (80)
    //   X360 case 80 @0x823A2D54  the four GetProgressionRankForGameMode reads -> action 181
    //                             == PS3 E_EVENT_RANK_INFO_REQUEST (81)   <-- this one
    //   X360 case 81 @0x823A2E74  PlayerInfo::Construct -> action 182
    //                             == PS3 E_EVENT_PLAYER_INFO_REQUEST (82)
    // The PS3 enum's extra E_GUI_HAS_STARTED_GAME = 79 is the enumerator the X360 build lacks,
    // which is what makes every id above 78 sit one lower here. The VALUE below is the X360's;
    // the spelling is the DWARF's (BrnGameEvents.h:91).
    // PRODUCER: BrnGui::CrashNavDriverDetails::UpdateInitSetup @0x824CF038 posts
    // GuiEventRankProgressRequest (GUI event 437) when it latches the cache, and
    // BridgeGuiToGameState's case 437 emits this 1-byte signal event.
    // CONSUMER: GameStateModule::ProcessGameEventsRankInfoRequestBringUp (this tree's extraction
    // of case 80), which answers with game action 181 -> GUI event 438.
    E_EVENT_RANK_INFO_REQUEST       = 80,    // X360 case 80 @0x823A2D54 (PS3 DWARF 81)
    // Freeburn-challenge events (PS3-DWARF values; used as template tags -- the X360
    // discriminants ChallengeManager::ProcessEvent actually switches on are the raw
    // jump-table case values in that body, which drift from these).
    // The network bridge posts these two as 171 and 172 (TranslateNetworkEventsToGameEvents),
    // and ProcessGameEvents' case 172 hands the record to ModeManager::HandleChallengeSuccessEvent.
    E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE = 171,
    E_EVENT_FREEBURN_CHALLENGE_SUCCESS        = 172,
    // ---- X360-ATTESTED discriminants (ChallengeManager::ProcessEvent 0x8233D6A8 jump
    // table: r11 = type - 54, 120 slots; the assert strings name each case's event).
    // PS3-DWARF values drift NON-uniformly (noted per enumerator) -- keep the X360 values.
    E_EVENT_POWER_PARK_RESULT                 = 54,  // X360 (PS3 DWARF 55)
    E_EVENT_BOOST_TIME_COMPLETE               = 55,  // X360 (PS3 DWARF 56; "lpBoostTimeComplete")
    E_EVENT_NEAR_MISS                         = 65,  // X360 == PS3 ("lpNearMissEvent")
    E_EVENT_NEAR_MISS_CHAIN_COMPLETED         = 66,  // X360 (PS3 67; "lpNearMissCompleteEvent")
    E_EVENT_DRIFTING                          = 67,  // X360 (PS3 68; "lpDriftEvent")
    E_EVENT_ONCOMING                          = 70,  // X360 (PS3 71)
    E_EVENT_ONCOMING_COMPLETED                = 71,  // X360 (PS3 72)
    // ================================================================================
    // ⭐⭐⭐ [boost-ticker wave 2026-09-14] THE SIX WORLD EVENTS THE BOOST TICKER IS MADE OF.
    // Every one of them is PRODUCER-PINNED in the world (the `li r5,<id>` / `li r6,<size>`
    // pair at the AddEvent site) AND CONSUMER-PINNED in GameStateModule::ProcessGameEvents
    // @0x823A0A18, whose arm turns each into the game action the GUI translator forwards:
    //
    //   id  producer                                          size  ProcessGameEvents -> action
    //   64  NearMissManager::NearMissEvent   @0x822F88B8         8   case 64 -> 171 (near miss)
    //   68  BoostStrategy::Update            @0x822F8130         4   case 68 -> 173 (spin)
    //   69  AirTimeManager::Update           @0x822F8C88         8   case 69 -> 174 (in air)
    //   72  BoostStrategy::Update            @0x822F8130         8   case 72 -> 176 (tailgating)
    //   73  (traffic-check detector)                             2   case 73 -> 107 (checked)
    //   74  TrafficCheckManager::Update      @0x822F8F20         4   case 74 -> 108 (check chain)
    //
    // ⚠️ THE NEAR-MISS SPELLING. The PS3 DWARF names ids 65/66/67 NearMissEvent /
    // NearMissChainInProgressEvent / NearMissChainCompleteEvent; the X360 block is those
    // MINUS ONE (64/65/66), which is proved by the payload SIZES the console's own producer
    // posts -- NearMissManager posts {miCount, meNearMissType} (8) as 64, {miCount} (4) as
    // 65 and {miCount, mbCompletedSuccessfully} (8) as 66. So the enumerator already spelled
    // `E_EVENT_NEAR_MISS = 65` above is in fact the CHAIN-IN-PROGRESS event; it is left at
    // its value because ChallengeManager::ProcessEvent's committed arm dispatches on it and
    // the console's own case-65 assert string ("lpNearMissEvent") is the loose local name the
    // original source used there. The SCORED event -- the one the ticker needs -- is 64 and
    // is spelled with a distinct name below rather than stealing the taken one.
    E_EVENT_NEAR_MISS_SCORED                  = 64,  // FLAG spelling; DWARF calls this E_EVENT_NEAR_MISS (PS3 65)
    E_EVENT_SPINNING                          = 68,  // X360 (PS3 69)
    E_EVENT_IN_AIR                            = 69,  // X360 (PS3 70)
    E_EVENT_TAILGATING                        = 72,  // X360 (PS3 73)
    E_EVENT_TRAFFIC_CHECKING                  = 73,  // X360 (PS3 74)
    E_EVENT_TRAFFIC_CHECKING_CHAIN            = 74,  // X360 (PS3 75)
    E_EVENT_COMPLETED_STUNT                   = 119, // X360 == PS3 ("lpCompletedStuntEvent")
    E_EVENT_INPROGRESS_STUNT                  = 120, // X360 == PS3 ("lpInProgressStuntEvent")
    E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS = 165, // X360 (PS3 160; "lpActionSuccessEvent")
    E_EVENT_FREEBURN_CHALLENGE_RESET          = 166, // X360 (PS3 161; "lpResetEvent" -- reset ONE action slot)
    // X360-only discriminant: same "lpResetEvent" payload, but resets ALL action slots
    // (loop ResetActionData). No PS3 enumerator exists at this position; the NAME is
    // reconstructed from the case body (FLAG).
    E_EVENT_FREEBURN_CHALLENGE_RESET_ALL_ACTIONS = 167, // X360-attested value; FLAGGED name
    E_EVENT_ACTIVE_FREEBURN_CHALLENGE         = 173, // X360 (PS3 167; "lpActiveChallengeEvent")
    // [FX-BRIDGES CC-11, 2026-09-24] the checkpoint-distance route pair. The PS3 DWARF puts
    // E_EVENT_LANDMARK_ROUTE_REQUEST at 85 and E_EVENT_MODE_MANAGER_ROUTE_INFO at 168 (its last
    // enumerator before E_EVENT_COUNT 169); the X360 ids are the ProcessGameEvents @0x823A0A18
    // jump-table cases whose arms name the records: case 84 hands its payload to
    // SendRouteRequestAction (@0x823A18A4), case 174 asserts "lpRouteInfoEvent" (@0x823A4B20, line
    // 0x1148) and calls ModeManager::HandleCheckpointDistanceResponse. 174 is also the id
    // BridgeWorldToGameState posts (`li r5, 0xAE` @0x823E5538) -- same +6 as ACTIVE_FREEBURN above.
    E_EVENT_LANDMARK_ROUTE_REQUEST            = 84,  // X360 (PS3 85; ProcessGameEvents case 84)
    E_EVENT_MODE_MANAGER_ROUTE_INFO           = 174, // X360 (PS3 168; "lpRouteInfoEvent")
    // Road-rules events (StreetManager keystone, wave B). The network bridge posts the buddy and
    // the four online road-rules events under these ids, and ProcessGameEvents' cases 150 /
    // 130 / 131 / 132 / 133 call StreetManager::ProcessBuddyRemoved / ProcessNetworkHighScoreEvent
    // / ProcessUploadEvent / ProcessDownloadEvent / ProcessConnectedOnlineEvent.
    E_EVENT_BUDDY_REMOVED                    = 150,
    E_EVENT_ROAD_RULE_ROAD_SCORE_REQUEST     = 191,  // value unconfirmed (template tag only)
    E_EVENT_ONLINE_ROAD_RULES_PB_RECV        = 130,
    E_EVENT_ONLINE_ROAD_RULES_UPLOADED       = 131,
    E_EVENT_ONLINE_ROAD_RULES_DOWNLOADED     = 132,
    E_EVENT_ONLINE_ROAD_RULES_CONNECT_INFO   = 133,
};

template <EGameEventType T>
struct GameEvent { };

// DWARF BrnGameEvents.h:400; ARTIST posts a one-byte empty signal.
struct PlayerCrashEndingEvent : public GameEvent<E_EVENT_PLAYER_CRASH_ENDING> {};
static_assert(sizeof(PlayerCrashEndingEvent) == 1, "empty crash-ending signal");

// ARTIST debug callbacks and ProcessGameEvents cases 1/2; DWARF member names.
struct TeleportPlayerCarEvent : public GameEvent<E_EVENT_TELEPORT_PLAYER_CAR>
{
    Vector3 mPosition;
    Vector3 mDirection;
};
struct ChangePlayerCarEvent : public GameEvent<E_EVENT_CHANGE_PLAYER_CAR>
{
    CgsID mCarModelId;
    CgsID mWheelModelId;
    bool mbResetPlayerCamera;
    bool mbKeepResetSection;
};
static_assert(sizeof(TeleportPlayerCarEvent) == 32, "ARTIST teleport payload");
static_assert(sizeof(ChangePlayerCarEvent) == 24, "ARTIST car-change payload");

// ARTIST ProcessGameEvents cases 4/5/6/82; declaration names from DecFIGS.
struct SelectPlayerCarEvent : public GameEvent<E_EVENT_SELECT_PLAYER_CAR>
{
    CgsID mCarModelId;
    CgsID mWheelModelId;
};
struct ChangePlayerCarColourEvent : public GameEvent<E_EVENT_CHANGE_PLAYER_CAR_COLOUR>
{
    u32 muPaletteIndex;
    u32 muColourIndex;
};
struct PlayerCarColourRequestEvent : public GameEvent<E_EVENT_PLAYER_CAR_COLOUR_REQUEST>
{
    CgsID mCarId;
};
struct UnlockedLiveryRequest : public GameEvent<E_EVENT_UNLOCKED_LIVERY_REQUEST>
{
    CgsID mCgsID;
};

// X360 element of EventQueue<HitOverheadSignEvent,100> (DWARF BrnGameEvents.h:429). Single byte.
struct HitOverheadSignEvent : public GameEvent<E_EVENT_OVERHEAD_SIGN_HIT>
{
    u8 muRaceCarId;   // 0x00
};

// X360 element of EventQueue<RecordPropHitEvent,50> (DWARF BrnGameEvents.h:413). 16-byte aligned
// via the leading Vector3.
// DWARF BrnGameEvents.h:744 -- the "a module finished streaming" event
// (E_EVENT_STREAMING_COMPLETE == 9). WorldEntityModule::UpdateStream posts it with
// meModule == E_MODULE_WORLD_GRAPHICS when a GameAction asked to be told the world
// stream settled (the X360 16-byte payload {2, 0}).
struct StreamingCompleteEvent : public GameEvent<E_EVENT_STREAMING_COMPLETE>
{
    // BrnGameEvents.h:757 nested enum; value 2 attested by the X360 UpdateStream store.
    enum EModule
    {
        E_MODULE_RACE_CAR_ENTITY = 1,
        E_MODULE_WORLD_GRAPHICS = 2,
    };

    EModule meModule;   // :757
    CgsID   mUserId;    // :758
};

struct RecordPropHitEvent : public GameEvent<E_EVENT_RECORD_PROP_HIT>
{
    Vector3 mPosition;   // 0x00 (rw::math::vpu, 16-byte SIMD)
    u16     muZoneId;    // 0x10
    u16     muPropId;    // 0x12
    bool    mbHitBefore; // 0x14
};

// [gateui] PINNED 2026-08-20. This event's neighbours in this header were pinned and it was not,
// even though it is a WIRE IMAGE: PropEntityModule::ProcessContacts writes it into an
// EventQueue<RecordPropHitEvent,50>, VariableEventQueue<1536,16>::Append<RecordPropHitEvent,50>
// @0x827AEC10 copies it with a hard `li r6, 0x20` (== 32) size immediate, and
// GameStateModule::ProcessGameEvents' case-111 arm reads it back at the console's three literal
// offsets (`lvx128 v1,r0,r25` @+0x00, `lhz r4,0x10(r25)`, `lhz r5,0x12(r25)`). Every one of those
// four numbers has to survive the widen to the host -- and it does, because the struct is
// POINTER-FREE and the empty GameEvent<T> base folds away (EBO), so the leading 16-byte-aligned
// Vector3 fixes the whole layout. If any of these four ever fires, the prop-hit wire is silently
// corrupt at the seam and the case-111 arm is reading the wrong halfword.
static_assert(offsetof(RecordPropHitEvent, mPosition)   == 0x00, "RecordPropHitEvent position at +0x00");
static_assert(offsetof(RecordPropHitEvent, muZoneId)    == 0x10, "RecordPropHitEvent zone id at +0x10");
static_assert(offsetof(RecordPropHitEvent, muPropId)    == 0x12, "RecordPropHitEvent prop id at +0x12");
static_assert(offsetof(RecordPropHitEvent, mbHitBefore) == 0x14, "RecordPropHitEvent hit-before flag at +0x14");
static_assert(sizeof(RecordPropHitEvent) == 32, "RecordPropHitEvent is the Append<...,50> `li r6,0x20` wire size");

// [gateui] DWARF BrnGameEvents.h:442 -- EMPTY, exactly as the DWARF declares it (the event id IS
// the whole payload: "somebody wants the prop-progression census re-sent"). Homed here so owner
// `bridge` can name it on the world->GameState prop leg without forking a second declaration.
struct RequestPropProgression : public GameEvent<E_EVENT_REQUEST_PROP_PROGRESSION>
{
};

// The network state manager's "every player is synced" signal. Empty: AddEvent copies one byte.
struct FinishedSyncingPlayersEvent : public GameEvent<E_EVENT_FINISHED_SYNCING_PLAYERS>
{
};
static_assert(sizeof(FinishedSyncingPlayersEvent) == 1, "posted with size 1");

// Online car selection starts (posted once a launch succeeds). Empty signal.
struct OnlineCarSelectEvent : public GameEvent<E_EVENT_ONLINE_CAR_SELECT>
{
};
static_assert(sizeof(OnlineCarSelectEvent) == 1, "posted with size 1");

// The player left the current mode; ProcessGameEvents answers with ModeManager::
// UserCancelCurrentMode and TakedownManager::ClearRaceCarData. Empty signal.
struct PlayerExitedModeEvent : public GameEvent<E_EVENT_PLAYER_EXITED_MODE>
{
};
static_assert(sizeof(PlayerExitedModeEvent) == 1, "posted with size 1");

// A module finished preparing for an invite. The payload is the module
// (EModulePreparedForInvite: 0 game state, 1 network); the network state manager posts 1.
struct PreparedForInviteEvent : public GameEvent<E_EVENT_PREPARED_FOR_INVITE>
{
    s32 meModulePreparedForInvite;   // +0x00 FLAG: typed s32 until EModulePreparedForInvite has a home
};
static_assert(sizeof(PreparedForInviteEvent) == 4, "posted with size 4");

// The local player lost the connection. Empty signal.
typedef GameEvent<E_EVENT_LOCAL_PLAYER_DISCONNECTED> LocalPlayerDisconnectedEvent;
static_assert(sizeof(LocalPlayerDisconnectedEvent) == 1, "posted with size 1");

// mNetworkPlayerID at offset 0. 32 bytes: the network bridge builds it from
// the network OUT record 18 and ProcessGameEvents case 7 reads it. The float at +0x18 is a
// console-only member (not in the reference record); the bridge copies it through.
struct ChangeNetworkCarEvent : public GameEvent<E_EVENT_CHANGE_NETWORK_CAR>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;  // 0x00
    CgsID                       mCarModelId;       // 0x08
    CgsID                       mWheelModelId;     // 0x10
    f32                         mf18;              // 0x18 (console-only member; unnamed)

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// mNetworkPlayerID at offset 0x10 (after two CgsID). 40 bytes: ProcessGameEvents
// case 127 copies the ids, the team, the float at +0x18 and the two paint indices into the
// online-player-added action. The float is a console-only member (not in the reference record).
struct OnlinePlayerAddedEvent : public GameEvent<E_EVENT_ONLINE_PLAYER_ADDED>
{
    CgsID                       mModelID;                 // 0x00
    CgsID                       mWheelID;                 // 0x08
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;         // 0x10
    EPlayerTeam                 meTeam;                   // 0x14
    f32                         mf18;                     // 0x18 (console-only member; unnamed)
    u16                         mu16CarColourIndex;       // 0x1C
    u16                         mu16CarPaintFinishIndex;  // 0x1E
    bool                        mbIsLocalPlayer;          // 0x20

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x823A7830. mNetworkPlayerID at offset 0.
struct OnlinePlayerFinalisedEvent : public GameEvent<E_EVENT_ONLINE_PLAYER_FINALISED>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// mNetworkPlayerID at offset 0. 8 bytes.
struct OnlinePlayerRemovedEvent : public GameEvent<E_EVENT_ONLINE_PLAYER_REMOVED>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;        // 0x00
    bool                        mbIsLocalPlayerInGame;   // 0x04

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// The three records above are pointer-free, so the console offsets and sizes hold on the host.
static_assert(offsetof(ChangeNetworkCarEvent, mCarModelId)  == 0x08, "ChangeNetworkCarEvent car model at +0x08");
static_assert(offsetof(ChangeNetworkCarEvent, mWheelModelId) == 0x10, "ChangeNetworkCarEvent wheel model at +0x10");
static_assert(offsetof(ChangeNetworkCarEvent, mf18)         == 0x18, "ChangeNetworkCarEvent float at +0x18");
static_assert(sizeof(ChangeNetworkCarEvent) == 32, "ChangeNetworkCarEvent is posted as 32 bytes");
static_assert(offsetof(OnlinePlayerAddedEvent, meTeam)                  == 0x14, "OnlinePlayerAddedEvent team at +0x14");
static_assert(offsetof(OnlinePlayerAddedEvent, mf18)                    == 0x18, "OnlinePlayerAddedEvent float at +0x18");
static_assert(offsetof(OnlinePlayerAddedEvent, mu16CarColourIndex)      == 0x1C, "OnlinePlayerAddedEvent colour at +0x1C");
static_assert(offsetof(OnlinePlayerAddedEvent, mu16CarPaintFinishIndex) == 0x1E, "OnlinePlayerAddedEvent paint finish at +0x1E");
static_assert(offsetof(OnlinePlayerAddedEvent, mbIsLocalPlayer)         == 0x20, "OnlinePlayerAddedEvent local flag at +0x20");
static_assert(sizeof(OnlinePlayerAddedEvent) == 40, "OnlinePlayerAddedEvent is posted as 40 bytes");
static_assert(offsetof(OnlinePlayerRemovedEvent, mbIsLocalPlayerInGame) == 0x04, "OnlinePlayerRemovedEvent flag at +0x04");
static_assert(sizeof(OnlinePlayerRemovedEvent) == 8, "OnlinePlayerRemovedEvent is posted as 8 bytes");

// X360 0x823A7770. mNetworkPlayerID at offset 0.
struct RemotePlayerDisconnectedEvent : public GameEvent<E_EVENT_REMOTE_PLAYER_DISCONNECTED>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x82542068 (Clear) / 0x825420C8 (SetPlayerData). Full 256-byte X360 layout (grown from the
// former minimal slice): the three X360 facts that NetworkRoundManager::NetworkGameStarted bakes --
// miNumRounds @ offset 8, mbIsStartingGameAfterPlayerJoin @ offset 248, sizeof == 256 (memcpy) --
// all land EXACTLY with this member set/order. mafPlayerData[8] is RETAINED (it pushes
// mbIsStartingGameAfterPlayerJoin to 248; absent from the PS3 DWARF, but the X360 offset proves it
// real). meBoostType spelled s32 (BrnNetwork::EBoostType has no committed home; same 4B storage).
struct StartNetworkGameEvent : public GameEvent<E_EVENT_START_NETWORK_GAME>
{
    s32                         miNumRaceCars;                            // 0x00
    EGameModeType               meGameMode;                               // 0x04
    s32                         miNumRounds;                              // 0x08
    u32                         muRandomSeedForGame;                      // 0x0C
    bool                        mbRefreshOnly;                            // 0x10
    CgsID                       maCarIds[KI_MAX_RACE_CARS];               // 0x18
    u16                         mau16CarColourIndex[KI_MAX_RACE_CARS];
    u16                         mau16CarPaintFinishIndex[KI_MAX_RACE_CARS];
    EPlayerTeam                 maePlayerTeam[KI_MAX_RACE_CARS];
    BrnNetwork::NetworkPlayerID maNetworkPlayerID[KI_MAX_RACE_CARS];
    f32                         mafPlayerData[KI_MAX_RACE_CARS];          // 0xB8
    bool                        mabPlayerHasFever[KI_MAX_RACE_CARS];
    BrnNetwork::NetworkPlayerID mLocalNetworkPlayerID;                    // 0xE0 (224)
    s32                         miHostGridPosition;                       // 0xE4 (228)
    s32                         miNumRunnerCrashes;                       // 0xE8 (232)
    s32                         meBoostType;                              // 0xEC (236) BrnNetwork::EBoostType (s32)
    f32                         mfTimeLimit;                              // 0xF0 (240)
    bool                        mbRedTeamHaveInfiniteBoost;               // 0xF4 (244)
    bool                        mbIsTrafficOn;                            // 0xF5 (245)
    bool                        mbIsTrafficCheckingOn;                    // 0xF6 (246)
    bool                        mbIsRanked;                               // 0xF7 (247)
    bool                        mbIsStartingGameAfterPlayerJoin;          // 0xF8 (248)
    bool                        mbIsStartingFreeburnLobbyAfterOnlineEvent; // 0xF9 (249)
    bool                        mbForceStartFreeburnLobby;                // 0xFA (250)

    void Clear();
    // Reference shape plus the console's f32 after the paint-finish index (it lands in
    // mafPlayerData). The wheel id is taken and not stored. lbIsHost makes this slot the
    // host's grid position.
    void SetPlayerData(s32                         liPlayerIndex,
                       BrnNetwork::NetworkPlayerID lNetworkPlayerID,
                       CgsID                       lCarId,
                       CgsID                       lWheelId,
                       u16                         lu16CarColourIndex,
                       u16                         lu16CarPaintFinishIndex,
                       f32                         lfPlayerData,
                       EPlayerTeam                 lePlayerTeam,
                       bool                        lbIsHost,
                       bool                        lbPlayerHasFever);
};

// X360 element copied as 10 dwords (40 bytes) by NetworkRoundManager::NetworkRoundStarted
// (0x823589E8). sizeof == 40: 16*LandmarkIndex(2) + u32 + s32. mLightTriggerID spelled u32
// (LightTriggerId == u32; matches the BrnGameStateSharedIO.h precedent).
struct StartNetworkRoundEvent : public GameEvent<E_EVENT_START_NETWORK_ROUND>
{
    BrnGameState::LandmarkIndex maLandmarks[16];      // 0x00, 16*2 = 32 bytes
    u32                         mLightTriggerID;      // 0x20 (LightTriggerId == u32)
    s32                         miNumLandmarksInRound; // 0x24
}; // sizeof == 40

// ===== Freeburn-challenge events (ChallengeManager keystone) =====
// (LastSecondChallengeSuccess == FastBitArray<60> lives at its DWARF home,
//  BrnGameStateSharedIO.h:313, pulled in by the include above.)

// DWARF BrnGameEvents.h:2893 (member set + order); X360 field offsets attested by
// ChallengeManager::HandleSuccessUpdateEvent 0x8233CDE0 (mask u64 @0x00, arci @0x08,
// frame @0x0C -- compared against the manager's miLastChallengeResetFrame -- action @0x10).
struct FburnChallengeSuccessUpdateEvent : public GameEvent<E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE>
{
    LastSecondChallengeSuccess mChallengeSuccessUpdate; // 0x00 (FastBitArray<60>, 8 bytes)
    EActiveRaceCarIndex        meActiveRaceCarIndex;    // 0x08
    s32                        miChallengeUpdateFrame;  // 0x0C
    s32                        miActionIndex;           // 0x10
};

// DWARF BrnGameEvents.h:2910 (member set + order). X360 DRIFT: HandleChallengeSuccessEvent
// 0x82316AE0 reads a frame number @0x0C (gated against miLastChallengeResetFrame) and the
// ARCI @0x10 -- the X360 build inserted miChallengeUpdateFrame before meActiveRaceCarIndex
// (absent from the PS3 DWARF member list).
struct FburnChallengeSuccessEvent : public GameEvent<E_EVENT_FREEBURN_CHALLENGE_SUCCESS>
{
    f32                 mafActionScores[2];         // 0x00
    bool                mabSuccessfulActions[2];    // 0x08
    bool                mabAccumulationThisFrame[2];// 0x0A
    s32                 miChallengeUpdateFrame;     // 0x0C (X360-only member; asm-attested)
    EActiveRaceCarIndex meActiveRaceCarIndex;       // 0x10
};

// ===== Road-rules events (StreetManager keystone, wave B) =====
// ADDITIVE GROW. Layouts are DWARF BrnGameEvents.h:1054/:2063/:2424/:2440/:2454/:2467;
// field offsets X360-proven by the StreetManager handler bodies (the GameEvent<> base is
// empty, so payloads start at +0 -- e.g. ProcessNetworkHighScoreEvent @ 0x82349F10 copies
// the 56-byte score from event+0 and reads the challenge index at +60 / friend flag at +64;
// ProcessConnectedOnlineEvent @ 0x8234A148 reads the reset time at +0).
// The E_EVENT_* tag values below are placeholders (template tags only, X360 discriminants
// owned by the GameStateModule::ProcessGameEvents dispatcher TU) on the committed
// unconfirmed-value precedent above.

// Buddy removed from the friends list (DWARF :1054): just the removed buddy's name.
// ProcessBuddyRemoved @ 0x8234A5A8 hands &mRemovedBuddyName straight to
// ClearAllChallengeDataForBuddy (CgsNetwork::PlayerName semantics, 16 bytes).
struct BuddyRemovedEvent : public GameEvent<E_EVENT_BUDDY_REMOVED>
{
    CgsNetwork::PlayerName mRemovedBuddyName;   // 0x00 (16B)
};

// GUI requests the score breakdown for one road (DWARF :2063).
struct RoadRulesScoreRequestEvent : public GameEvent<E_EVENT_ROAD_RULE_ROAD_SCORE_REQUEST>
{
    BrnStreetData::ChallengeIndex mRoadChallengeIndex;   // 0x00 (Road::ChallengeIndex == int32)
};

// A downloaded online personal-best record (DWARF :2424).
struct OnlineRoadRulesPersonalBestRecvEvent : public GameEvent<E_EVENT_ONLINE_ROAD_RULES_PB_RECV>
{
    BrnStreetData::ChallengeHighScoreEntry mPersonalBestScore;          // 0x00 (56B)
    BrnNetwork::NetworkPlayerID            mPersonalBestPlayerID;       // 0x38 (RoadRulesRecvData::NetworkPlayerID == s32)
    BrnStreetData::ChallengeIndex          mPersonalBestChallengeIndex; // 0x3C
    bool                                   mbWasPBByFriend;             // 0x40
};

// Road-rules score upload completed for a challenge-index range (DWARF :2440).
struct OnlineRoadRulesUploadedEvent : public GameEvent<E_EVENT_ONLINE_ROAD_RULES_UPLOADED>
{
    BrnStreetData::ChallengeIndex mStartUploadIndex;   // 0x00
    BrnStreetData::ChallengeIndex mEndUploadIndex;     // 0x04
};

// Road-rules download completed (DWARF :2454).
struct OnlineRoadRulesDownloadedEvent : public GameEvent<E_EVENT_ONLINE_ROAD_RULES_DOWNLOADED>
{
    u32 muTimestampOfDownload;   // 0x00
};

// Road-rules server connect info (DWARF :2467).
struct OnlineRoadRulesConnectInfoEvent : public GameEvent<E_EVENT_ONLINE_ROAD_RULES_CONNECT_INFO>
{
    u32 muLastRoadRulesResetTime;   // 0x00
};

// ===== ChallengeManager::ProcessEvent payloads (ChallengeManager keystone, wave C) =====
// ADDITIVE GROW. The sole reconstructed consumer is ChallengeManager::ProcessEvent
// (X360 0x8233D6A8); every field offset below is attested by that body's raw loads (the
// GameEvent<> base is empty, so payloads start at +0x00). DWARF BrnGameEvents.h lines
// noted per struct; the two stunt events DRIFT heavily from the lean PS3 records and are
// modelled X360-authoritatively (flagged members).

// Power-park scored (DWARF :2812). X360 case 54: meOutcome (lwz +0) == 1 (E_PPO_SUCCESS)
// gates; miOtherPlayersInvolved (lwz +8) >= 2 selects PLAYER_POWER_PARKING(11) over
// TRAFFIC_POWER_PARKING(12); miOverallRating (lwz +4, fcfid -> f32) is the score.
// meOutcome is BrnWorld::EPowerParkOutcome stored as s32 (the enum's home is
// BrnPowerParkingManager.h, reached through BrnGameActions.h; s32 storage keeps this header
// decoupled -- StartNetworkGameEvent precedent).
struct PowerParkResultEvent : public GameEvent<E_EVENT_POWER_PARK_RESULT>
{
    s32 meOutcome;               // 0x00 (:2814, BrnWorld::EPowerParkOutcome; s32 storage)
    s32 miOverallRating;         // 0x04 (:2815)
    s32 miOtherPlayersInvolved;  // 0x08 (:2816)
};

// Boost ran out / completed (DWARF :2826). X360 case 55: the f32 at +0 feeds
// E_FREEBURN_SKILL_BOOST_TIME(14).
struct BoostTimeCompleteEvent : public GameEvent<E_EVENT_BOOST_TIME_COMPLETE>
{
    f32 mfTimeSpentBoosting;   // 0x00 (:2828)
};

// Near miss scored (DWARF :1489). X360 case 65: miCount (lwz +0, fcfid -> f32) feeds
// E_FREEBURN_SKILL_NEAR_MISS(4). meNearMissType is BrnWorld::ENearMissType stored as s32
// (its committed home is BrnNearMissManager.h; not read by this consumer).
struct NearMissEvent : public GameEvent<E_EVENT_NEAR_MISS>
{
    s32 miCount;         // 0x00 (:1490)
    s32 meNearMissType;  // 0x04 (:1491, BrnWorld::ENearMissType; s32 storage)
};

// Near-miss chain completed (DWARF :1516). X360 case 66: miCount (lwz +0, fcfid) feeds
// E_FREEBURN_SKILL_NEAR_MISS(4) -- same tail as case 65.
struct NearMissChainCompleteEvent : public GameEvent<E_EVENT_NEAR_MISS_CHAIN_COMPLETED>
{
    s32  miCount;                 // 0x00 (:1517)
    bool mbCompletedSuccessfully; // 0x04 (:1518; not read by ProcessEvent)
};

// Drift scored (DWARF :1586). X360 case 67: the f32 at +0 feeds E_FREEBURN_SKILL_DRIFT(3).
struct DriftingEvent : public GameEvent<E_EVENT_DRIFTING>
{
    f32 mfDistance;   // 0x00 (:1587)
};

// Oncoming (driving into oncoming traffic) in progress / completed (DWARF :1975/:1988).
// X360 cases 70/71: the f32 at +0 feeds E_FREEBURN_SKILL_ONCOMING(0).
struct OncomingEvent : public GameEvent<E_EVENT_ONCOMING>
{
    f32 mfDistance;   // 0x00 (:1976)
};
struct OncomingCompletedEvent : public GameEvent<E_EVENT_ONCOMING_COMPLETED>
{
    f32 mfDistance;   // 0x00 (:1989)
};

// ====================================================================================
// ⭐⭐⭐ [boost-ticker wave 2026-09-14] The remaining boost-ticker world-event payloads.
// Member names/types are the DecFIGS DWARF's (BrnGameEvents.h :1490/:1601/:1615/:1532/
// :1546/:1559); every offset is attested by the console PRODUCER's stores and re-attested
// by GameStateModule::ProcessGameEvents' arm, which copies exactly these words into the
// game action it posts.
// ====================================================================================

// The SCORED near miss (X360 id 64). Producer NearMissManager::NearMissEvent @0x822F88B8
// (`AddEvent(q, {miNearMissCount, leNearMissType}, 64, 8)`); ProcessGameEvents case 64
// copies both words into game action 171, which becomes GUI event 384.
// This is the SAME two-word shape the DWARF gives its `NearMissEvent`; the tree's
// `struct NearMissEvent` above is the ChallengeManager-facing one at id 65 (see the
// enumerator note) and is left alone.
struct NearMissScoredEvent : public GameEvent<E_EVENT_NEAR_MISS_SCORED>
{
    s32 miCount;         // 0x00 (:1490)
    s32 meNearMissType;  // 0x04 (:1491, BrnWorld::ENearMissType; s32 storage -- same
                         //       decoupling the sibling NearMissEvent above uses)
};

// Flat spin (X360 id 68). Producer BoostStrategy::Update @0x822F8130 posts the live
// mfSpinAngle whenever it is outside +/-FLT_EPSILON; ProcessGameEvents case 68 -> action 173.
struct SpinningEvent : public GameEvent<E_EVENT_SPINNING>
{
    f32 mfSpinAngle;   // 0x00 (:1601)
};

// Airborne (X360 id 69). Producer AirTimeManager::Update @0x822F8C88 (two AddEvent sites,
// both `..., 69, 8`); ProcessGameEvents case 69 copies both words into action 174 AND
// keeps the session maximum of mfCurrentJumpAirTime.
struct InAirEvent : public GameEvent<E_EVENT_IN_AIR>
{
    f32 mfCumulativeAirTime;   // 0x00 (:1615)
    f32 mfCurrentJumpAirTime;  // 0x04 (:1616)
};

// Tailgating (X360 id 72). Producer BoostStrategy::Update @0x822F8130
// (`AddEvent(q, {mfTailgatingDistance, meTailgatedCarIndex}, 72, 8)`); ProcessGameEvents
// case 72 copies both words into action 176 (the GUI event keeps only the distance).
struct TailgatingEvent : public GameEvent<E_EVENT_TAILGATING>
{
    f32 mfDistance;             // 0x00 (:1532)
    s32 meTailgatedCarIndex;    // 0x04 (:1533, EActiveRaceCarIndex; s32 storage keeps this
                                //       header free of BurnoutConstants.h)
};

// ⭐⭐⭐ [boost-wave2 2026-09-14] A rival-vs-player vehicle impact (X360 id 31, 12 bytes).
// DWARF BrnGameEvents.h:1324-1329 gives all three members and their order; the X360 consumer
// SendVehicleImpactMessages @0x82381A00 confirms every one of them:
//   `lwz r11, 0(r31)` -> the impact type, used to index the two 9-entry message-id tables
//   `lwz r30, 4(r31)` -> compared against GetPlayerActiveRaceCarIndex() to choose action 53
//                        (the player is the AGGRESSOR) vs action 54 (the player is the victim)
//   `lwz r9,  8(r31)` -> copied through untouched; ProcessGameEvents' case-31 arm compares it
//                        against the player index too, for the victim-side rumble.
struct VehicleImpactEvent : public GameEvent<E_EVENT_VEHICLE_IMPACT>
{
    s32 meImpactType;                  // 0x00 (:1327, BrnPhysics::Vehicle::EImpactType; s32
                                       //       storage keeps this header free of the physics
                                       //       constants header, same reason meTailgatedCarIndex
                                       //       above stores EActiveRaceCarIndex as s32)
    s32 meAggressorActiveRaceCarIndex; // 0x04 (:1328, EActiveRaceCarIndex)
    s32 meVictimActiveRaceCarIndex;    // 0x08 (:1329, EActiveRaceCarIndex)
};

// A traffic check landed (X360 id 73) -- the 2-byte vehicle index. Consumed by
// TrafficCheckManager::Update (which counts the chain) and by ProcessGameEvents case 73,
// which forwards it as action 107 so RaceCarEntityModule can award the boost.
struct TrafficCheckingEvent : public GameEvent<E_EVENT_TRAFFIC_CHECKING>
{
    u16 muVehicleIndex;   // 0x00 (:1546)
};

// The running traffic-check chain (X360 id 74). Producer TrafficCheckManager::Update
// @0x822F8F20 (`AddEvent(out, &miCurrentCheckChain, 74, 4)`); ProcessGameEvents case 74
// forwards it as action 108, which becomes GUI event 383.
struct TrafficCheckingChainEvent : public GameEvent<E_EVENT_TRAFFIC_CHECKING_CHAIN>
{
    s32 miChainSize;   // 0x00 (:1559)
};

// A freeburn-challenge action succeeded (network echo) (DWARF :2507). X360 case 165:
// mChallengeID (64-bit compare vs mpCurrentChallenge's id), miActionIndex (bound-asserted
// "lpActionSuccessEvent->miActionIndex < mpCurrentChallenge->GetNumActions()").
struct FreeburnChallengeActionSuccessEvent : public GameEvent<E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS>
{
    CgsID mChallengeID;  // 0x00 (:2509)
    s32   miActionIndex; // 0x08 (:2510)
};

// A freeburn challenge (or one of its action slots) was reset (DWARF :2522). SHARED by the
// X360 cases 166 (reset the one slot miActionIndex) and 167 (reset slots 0..miActionIndex)
// -- both assert "lpResetEvent" and 64-bit-compare mChallengeID vs the current challenge's
// (Hex-Rays renders that cmpld as a nonsense self-compare of the entry's id halves; the raw
// asm is `ld +0(event); ld +0xC0(challenge); cmpld`).
struct FreeburnChallengeResetEvent : public GameEvent<E_EVENT_FREEBURN_CHALLENGE_RESET>
{
    CgsID mChallengeID;  // 0x00 (:2524)
    s32   miActionIndex; // 0x08 (:2525)
};

// The active-challenge announcement received on join (DWARF :2927). X360 case 173 reads
// count @0x28, walks the ARCI block from +0x00 (bound asserts spell
// "lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex] ...") and resolves the
// challenge from the CgsID @0x20. NOTE: this event KEEPS the PS3 member order -- the
// matching ActiveFburnChallengeAction (BrnGameActions.h) is the one whose X360 order drifts.
struct ActiveFburnChallengeEvent : public GameEvent<E_EVENT_ACTIVE_FREEBURN_CHALLENGE>
{
    static const s32 KI_MAX_NETWORK_PLAYERS = 7;

    EActiveRaceCarIndex maePlayersInChallengeARCI[KI_MAX_NETWORK_PLAYERS]; // 0x00..0x1B (:2929)
    CgsID               mChallengeID;             // 0x20 (:2930; 8-aligned)
    s32                 miNumPlayersInChallenge;  // 0x28 (:2931)
}; // sizeof == 0x30

// ---- X360 stunt events (heavy X360-vs-DWARF drift; modelled X360-authoritatively) ----
// The PS3 DWARF records CompletedStuntEvent (:497) / InProgressStuntEvent (:524) as lean
// scalar structs. The X360 build's records are LARGER: they carry a per-stunt-run-slot
// score/flag block (the 19 X360-only stunt-run skills) and, on the in-progress event, the
// same convoy block OnStuntElementCompleteAction (BrnGameActions.h) carries at identical
// offsets (member ids @0x44, count @0x64). Every named field below is offset-attested by
// a ProcessEvent raw load; DWARF names are applied where the consuming skill id proves the
// semantic (noted); X360-only members are FLAGGED. maReserved* gaps are DWARF-silent.
//
// The stunt-run block: both events read f32 @ (4 * K) and (completed only) the byte
// @ (0x44 + K - 1), where K == the type-22/23 action's GetTargetValue(1) (the action's
// 1-BASED stunt-run slot; ChallengeListEntryAction+0x38). So mafStuntRunScores[K-1] sits
// at +0x04 and mabStuntRunScored[K-1] at +0x44. The [12] extents are the largest that fit
// the attested neighbours (scores < +0x34 tail region, flags < +0x50) -- FLAGGED.
struct CompletedStuntEvent : public GameEvent<E_EVENT_COMPLETED_STUNT>
{
    u32  muStuntActionComplete;        // 0x00 (:499; completed-stunt-kind bit mask: 0x1 barrel
                                       //       roll, 0x2 flatspin, 0x10 landing, 0x40/0x80 drift,
                                       //       0x100 air, 0x200 air distance, 0x400 stunt-run,
                                       //       0x800 skill-37 count)
    f32  mafStuntRunScores[12];        // 0x04..0x33 FLAG: X360-only, 1-based slot K reads [K-1]
    u8   maReserved0x34[0x10];         // 0x34..0x43 (DWARF-silent gap; true members unrecovered)
    bool mabStuntRunScored[12];        // 0x44..0x4F FLAG: X360-only ("slot K scored" gate)
    s32  miCompletedBarrelRolls;       // 0x50 (:511; lwz+fcfid, feeds BARREL_ROLL(9)/(10))
    s32  miCompletedSkill37Count;      // 0x54 FLAG: X360-only count (lwz+fcfid, feeds drifted skill 37)
    u8   maReserved0x58[4];            // 0x58..0x5B
    f32  mfCompletedAirSpinAngle;      // 0x5C (:502; radians -- consumer scales by 57.29578,
                                       //       feeds FLATSPIN(1)/FLATSPIN_REVERSE(2))
    u8   maReserved0x60[8];            // 0x60..0x67
    f32  mfCompletedDriftDistance;     // 0x68 (feeds DRIFT(3); drift skill is distance-valued per
                                       //       DriftingEvent -- DWARF also lists a DriftTime, FLAG)
    f32  mfCompletedAirTime;           // 0x6C (:506; feeds AIR(15))
    f32  mfCompletedAirDistance;       // 0x70 (:508; feeds AIR_DISTANCE(16))
    u8   maReserved0x74[0xC];          // 0x74..0x7F
    bool mbSuccessfulLanding;          // 0x80 (:507; feeds SUCCESSFUL_LANDING(5)/(6))
    bool mbInReverse;                  // 0x81 (:509; selects the _REVERSE twin skill)
    bool mbStuntRunEnded;              // 0x82 FLAG: X360-only (latches drifted skill 19 to 0.0)
};

struct InProgressStuntEvent : public GameEvent<E_EVENT_INPROGRESS_STUNT>
{
    u32  muStuntActionInProgress;      // 0x00 (:526; in-progress bit mask: 0x1 barrel roll,
                                       //       0x2 flatspin, 0x20 air, 0x40 air distance,
                                       //       0x80 convoy/stunt-run)
    f32  mafStuntRunScores[12];        // 0x04..0x33 FLAG: X360-only (same 1-based slot block)
    u8   maReserved0x34[0x10];         // 0x34..0x43 (mirrors OnStuntElementCompleteAction's
                                       //       maConvoyLegDistances region; unread here -- pad)
    s32  maConvoyMemberARCIs[8];       // 0x44..0x63 FLAG: X360-only (walked player-by-player;
                                       //       same offsets as OnStuntElementCompleteAction's
                                       //       maConvoyMemberIds)
    s32  miConvoyMemberCount;          // 0x64 FLAG: X360-only (the `count > 0` loop bound)
    f32  mfInProgressBarrelRollAngle;  // 0x68 (:528; radians -- consumer converts to whole rolls,
                                       //       feeds BARREL_ROLL(9)/(10))
    f32  mfInProgressAirSpinAngle;     // 0x6C (:529; radians, feeds FLATSPIN(1)/(2))
    u8   maReserved0x70[0xC];          // 0x70..0x7B
    f32  mfTimeInAir;                  // 0x7C (:533; feeds AIR(15))
    f32  mfDistanceInAir;              // 0x80 (:534; feeds AIR_DISTANCE(16))
    u8   maReserved0x84[0xC];          // 0x84..0x8F
    bool mbInReverse;                  // 0x90 (:535; selects the _REVERSE twin skill)
};

// ============================================================================================
// [!!] [stuntrace waveB CLOSURE round, 2026-08-26] PlayerFinishedModeEvent -- RE-HOMED HERE.
//
// This record was DEFINED IN A .cpp (BrnModeManager_UpdateMode.cpp, at real
// BrnGameState::GameStateModuleIO scope, i.e. with external linkage) because BrnGameEvents.h had
// no owning definition and that partfile could not edit this header. That copy is DELETED in the
// same change as this one: two external-linkage definitions of one class is an ODR violation the
// compiler cannot see across TUs, and the moment this header became reachable from that TU it
// would have been a hard redefinition. Do not re-add it there.
//
// DWARF home: GameSource/GameState/BrnGameEvents.h:1342
// (`struct PlayerFinishedModeEvent : public GameEvent<E_EVENT_PLAYER_FINISHED_MODE>`), which IS
// this file -- so this is the true owning home, not a re-home of convenience. The GameEvent<T>
// base is the empty template tag (no instance data, no vtable -- :98 above), so mbTimedOut is at
// +0x00, which is what the consumer's byte offsets require.
//
// LAYOUT IS ASM-PINNED. BrnGameState::ModeManager::PlayerFinishedMode @0x823280D8 reads exactly
// three bytes off the record (r30 == the event, r31 == the ModeManager):
//     0x82328118  lbz r11, 0(r30)  -> if set, `stbx r28(1), r31, 0x94FD`
//     0x82328134  lbz r11, 1(r30)  -> if set, `stbx r28(1), r31, 0x94FE`
//     0x8232814C  lbz r11, 2(r30)  -> if set, ScoringSystem::RegisterFinishForCar(1, player, simTime)
// The PS3 DWARF declares only the first TWO (:1344 mbTimedOut, :1345 mbCarDestroyed); byte 2 is
// an X360 addition and its name is taken from its ONLY consumer (it is what registers a finish
// for the player car), so it is FLAGGED.
struct PlayerFinishedModeEvent : public GameEvent<E_EVENT_PLAYER_FINISHED_MODE>
{
    bool mbTimedOut;             // +0x00  DWARF BrnGameEvents.h:1344
    bool mbCarDestroyed;         // +0x01  DWARF BrnGameEvents.h:1345
    bool mbCrossedFinishLine;    // +0x02  X360-only; FLAG: named from its sole consumer
};

// Pin the attested offsets (both structs are pointer-free -> absolute on the x64 gate).
static_assert(offsetof(PlayerFinishedModeEvent, mbTimedOut) == 0x00, "PlayerFinishedModeEvent timed-out byte at +0 (lbz 0(r30) @0x82328118)");
static_assert(offsetof(PlayerFinishedModeEvent, mbCarDestroyed) == 0x01, "PlayerFinishedModeEvent car-destroyed byte at +1 (lbz 1(r30) @0x82328134)");
static_assert(offsetof(PlayerFinishedModeEvent, mbCrossedFinishLine) == 0x02, "PlayerFinishedModeEvent crossed-finish byte at +2 (lbz 2(r30) @0x8232814C)");
static_assert(offsetof(CompletedStuntEvent, mabStuntRunScored) == 0x44, "CompletedStuntEvent stunt-run flags at +0x44");
static_assert(offsetof(CompletedStuntEvent, miCompletedBarrelRolls) == 0x50, "CompletedStuntEvent barrel rolls at +0x50");
static_assert(offsetof(CompletedStuntEvent, mfCompletedAirSpinAngle) == 0x5C, "CompletedStuntEvent air-spin angle at +0x5C");
static_assert(offsetof(CompletedStuntEvent, mbSuccessfulLanding) == 0x80, "CompletedStuntEvent landing flag at +0x80");
static_assert(offsetof(InProgressStuntEvent, maConvoyMemberARCIs) == 0x44, "InProgressStuntEvent convoy ids at +0x44");
static_assert(offsetof(InProgressStuntEvent, miConvoyMemberCount) == 0x64, "InProgressStuntEvent convoy count at +0x64");
static_assert(offsetof(InProgressStuntEvent, mfTimeInAir) == 0x7C, "InProgressStuntEvent air time at +0x7C");
static_assert(offsetof(InProgressStuntEvent, mbInReverse) == 0x90, "InProgressStuntEvent reverse flag at +0x90");
static_assert(offsetof(ActiveFburnChallengeEvent, mChallengeID) == 0x20, "ActiveFburnChallengeEvent id at +0x20");
static_assert(offsetof(ActiveFburnChallengeEvent, miNumPlayersInChallenge) == 0x28, "ActiveFburnChallengeEvent count at +0x28");

// ============================================================================================
// [FX-BRIDGES CC-11, 2026-09-24] THE CHECKPOINT-DISTANCE ROUTE PAIR.
//
// LandmarkRouteRequestEvent -- DWARF BrnGameEvents.h:1679..1695. A two-point route question: each
// end is a landmark (by id), a junction or the player's position (by world position). It travels
// as game event 84 (the GUI's requests) and, for the mode manager's checkpoint distances, straight
// into GameStateModule::SendRouteRequestAction @0x82381DC8, which reads every member below at the
// DWARF's offsets: node types `lwz 0(r26)` from +0x20 (stride 4), landmark ids `ld 0(r20)` from
// +0x30 (stride 8), positions `lvx128 v127, r0, r29` from +0x00 (stride 16), the event id
// `lhz 0x44(r30)`. ModeManager::UpdateCheckpointDistanceRequests @0x823279B8 is the producer the
// mode manager runs (its stores @0x82327AC8..0x82327B84 land on the same offsets).
struct alignas(16) LandmarkRouteRequestEvent : public GameEvent<E_EVENT_LANDMARK_ROUTE_REQUEST>
{
    // DWARF BrnGameEvents.h:1679
    enum ERouteEndPointType
    {
        E_ROUTE_END_POINT_TYPE_LANDMARK  = 0,
        E_ROUTE_END_POINT_TYPE_JUNCTION  = 1,
        E_ROUTE_END_POINT_TYPE_PLAYERPOS = 2,
        E_ROUTE_END_POINT_TYPE_COUNT     = 3
    };

    static const s32 KI_MAX_POINTS = 2;            // :1688

    Vector3            maPositions[KI_MAX_POINTS];      // +0x00  :1690
    ERouteEndPointType mePointTypes[KI_MAX_POINTS];     // +0x20  :1691
    u32                muJunctionIDs[KI_MAX_POINTS];    // +0x28  :1692
    CgsID              maLandmarkIDs[KI_MAX_POINTS];    // +0x30  :1693
    u16                maSectionIndices[KI_MAX_POINTS]; // +0x40  :1694
    u16                mu16EventID;                     // +0x44  :1695
};
static_assert(offsetof(LandmarkRouteRequestEvent, mePointTypes)     == 0x20, "node types at +0x20 (lwz 0(r26), r26 = event + 0x20 @0x82381E5C)");
static_assert(offsetof(LandmarkRouteRequestEvent, muJunctionIDs)    == 0x28, "junction ids at +0x28 (DWARF order)");
static_assert(offsetof(LandmarkRouteRequestEvent, maLandmarkIDs)    == 0x30, "landmark ids at +0x30 (ld 0(r20), r20 = event + 0x30 @0x82381E58)");
static_assert(offsetof(LandmarkRouteRequestEvent, maSectionIndices) == 0x40, "section indices at +0x40 (sth @0x82327B7C / 0x82327B84)");
static_assert(offsetof(LandmarkRouteRequestEvent, mu16EventID)      == 0x44, "event id at +0x44 (lhz 0x44(r30) @0x82381FDC)");
static_assert(sizeof(LandmarkRouteRequestEvent) == 0x50, "the 16-byte-aligned record the producer builds on its stack");

// ModeManagerRouteInfoEvent -- DWARF BrnGameEvents.h:3008/3009. The answer to a mode-manager route
// request: which checkpoint pair it measured and how long the route is. BridgeWorldToGameState
// @0x823E5368 builds it from every RouteResponse whose owner is E_OWNER_MODE_MANAGER
// (`stw r11, var_1450` = the response's u16 event id, `stfs f0, var_144C` = the distance) and posts
// it as event 174, size 8 (@0x823E5530..0x823E5540); ModeManager::HandleCheckpointDistanceResponse
// @0x8231E6C8 compares `lwz 0(r27)` against the next checkpoint and hands `lfs f1, 4(r27)` to
// ScoringSystem::SetCheckpointDistances.
struct ModeManagerRouteInfoEvent : public GameEvent<E_EVENT_MODE_MANAGER_ROUTE_INFO>
{
    s32 miEventId;          // +0x00  :3008
    f32 mfRouteDistance;    // +0x04  :3009
};
static_assert(offsetof(ModeManagerRouteInfoEvent, mfRouteDistance) == 0x04, "distance at +4 (lfs f1, 4(r27) @0x8231E75C)");
static_assert(sizeof(ModeManagerRouteInfoEvent) == 8, "posted with size 8 (li r6, 8 @0x823E5534)");
}
}
