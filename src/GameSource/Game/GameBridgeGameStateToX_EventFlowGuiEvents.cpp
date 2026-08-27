// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_EventFlowGuiEvents.cpp
//
// [WAVE E1 -- event GUI prerequisites, 2026-08-26]
// THE EVENT-FLOW SLICE of BrnGameModule::TranslateGameActionsToGuiEvents @0x823E9CE0.
//
// The console body is one ~105 KB switch (jpt_823EA1F0) over ~300 game actions. Two slices of
// it live in b5-decomp:
//
//   GameBridgeGameStateToX_StuntGuiEvents.cpp   -- MapStuntEnumsFromGameplayToGui @0x823AA4A8
//                                                  and the member TranslateGameActionsToGuiEvents
//                                                  itself, carrying arms 58 / 59 / 60 / 112 / 148.
//   THIS FILE                                   -- the EVENT-FLOW arms: 23, 30, 37, 38, 39, 44,
//                                                  47, 200, 201.
//
// WHY A SIBLING TU AND NOT MORE ARMS IN THE STUNT TU: the same reason that TU exists at all
// (its banner, and the ConvertTrainingTypeToStringId split of 2026-08-16) -- the parent
// GameBridgeGameStateToX.cpp cannot be mounted, and one 105 KB function reconstructed by five
// different waves in one file is a merge hazard. The member function has exactly ONE definition,
// in the stunt TU, so this file cannot re-define it; it exposes the arms as a dispatch-seam
// helper instead, shaped exactly like the switch body it came from.
//
// ✅ WIRED (2026-08-27 audit; the wire-request banner that stood here was STALE). All three
// steps landed: the declaration is in GameSource/Game/GameBridgeGameStateToX.h:87, the
// `default:` arm of TranslateGameActionsToGuiEvents calls it
// (GameBridgeGameStateToX_StuntGuiEvents.cpp:399-406), and this file is mounted (bat 4388).
// Runtime-proven: BrnGame.log shows action 201 -> gui 311 flowing and action 23 driving the
// FSM hop to PRE_FLY_BY. The declaration is repeated at the top of this file so the definition
// is checked against the same signature the header carries.
//
// WHAT THIS BUYS, IN ORDER OF VISIBILITY
//   201 -> GuiEventJunctionInfo(311)   THE ORACLE. The consumer half is COMPLETE AND MOUNTED
//                                      (BrnJunctionInfoComponent + BrnFBurnMainHudState case
//                                      311 + BrnOdometerComponent), so the moment a producer
//                                      posts action 201 the "hold accelerator + brake" junction
//                                      panel appears. Nothing else in this wave is observable
//                                      without further reconstruction.
//    23 -> GuiEventPrepareForModeStart(93) + GuiEventRunFsm(144, BRNEVENTFSM / E_GUI_HUD_EVENT)
//                                      -- the FSM hop that puts the GUI into the event HUD.
//    30 -> CgsGui::GuiEvent<164>        [ADDED 2026-08-27, stunt-race frontier round 2 / D1]
//                                      THE FLY-BY EXIT. 23 gets the GUI INTO PRE_FLY_BY; this arm
//                                      is the only thing in the image that gets it back out. Its
//                                      consumer (PreRaceFlyByState's KI_EVENT_FLYBY_END arm) is
//                                      mounted, so landing this arm closes the whole
//                                      163 -> 25 -> IntroState::OnLeave -> action 30 -> 164 ->
//                                      "BF_PROCEED" loop. See the arm's own banner.
//                                      (Online lobby/showtime take the other branch: 269 / 279.)
//    47 -> GuiEventUpdateEventCountdown(234)
//    44 -> GuiEventEnterEventStartLocation(166)
//    38 -> GuiEventFinishedModeResults(321)
//    37 -> GuiEventOfflinePostEvent(289) / GuiEventTriggerOnlinePostEvent(320) / GuiEvent<291>
//    39 -> GuiEventStopMode(322)
//   200 -> GuiEventMedalUpdate(307)
//
// ID DISCIPLINE
// ACTION ids come from BrnGameActions.h's new event-flow block, where each value is cited to a
// producer AND to this consumer's jump-table case. GUI event ids come from each type's
// CgsGui::GuiModule::AddGuiEvent<T> instantiation, whose AddEvent(&event, <id>, <size>) literals
// are quoted per record below. Game ACTION ids are X360-shifted; GUI event ids are their own
// space and are NOT shifted.
//
// THE PAYLOAD RECORDS BELOW ARE TU-LOCAL ON PURPOSE, AND THAT IS THE HOUSE PRECEDENT.
// GameSource/Gui/BrnGuiDemangledEventTypes.h CANNOT BE INCLUDED FROM THIS TU -- measured, not
// assumed: it re-defines BrnGui::GuiEventNetworkPlayerImage, which
// GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h:75 (reached through
// BrnGameModule.hpp) already defines, so the include is a hard C2011. That is the identical
// fork the stunt TU's case-58 arm documents when it parks its GuiAutosaveRequestEvent post.
// Independently of the fork, the shells in that header would be the WRONG SHAPE for this
// posting path anyway: AddGuiEvent<T> posts `&event` whole, with the payload's own fields at
// record offset +0 (see every store map quoted below), while the shells for the 4-aligned
// >= 12-byte events are `CgsGui::GuiEvent<N> + u8 maPayload[size-12]`, which puts every field
// 12 bytes late. GuiEventStopMode is the sharpest case: it is spelled
// `: public CgsGui::GuiEvent<322> {}`, i.e. its 12 bytes are ALL header and there is nowhere to
// put the three words the console actually sends -- exactly the defect that header's own
// GuiEventBoostBarStuntInfo note records ("the 12 bytes ARE the payload").
// So each record is rebuilt here from its producer store map, at its attested size, carrying its
// own GetEventType() (mandatory -- PushGuiEvent bakes id and size off the type), exactly as
// GameBridgeGameStateToX_StuntGuiEvents.cpp's TickerCustomMessageWire537 and
// BrnCarSelectLivery_Components.cpp's GuiTickerCustomMessagePayload already do.
//   SHARED_HEADER_REQUEST (owner: the Gui lane), in this order:
//     (a) remove the GuiEventNetworkPlayerImage fork from BrnNetworkPlayerImageRenderer.h:75 so
//         BrnGuiDemangledEventTypes.h becomes includable from the bridge family;
//     (b) THEN re-shape 93 / 289 / 322 in BrnGuiDemangledEventTypes.h onto the store maps below
//         -- but ONLY once a CONSUMER exists to arbitrate the field names. Today none of these
//         eight event ids has a single consumer anywhere in b5-decomp/src (grepped: only the
//         AddGuiEvent instantiation list and the header itself), so this wave deliberately
//         re-shapes NOTHING there: a producer store map proves offsets and widths, not names.
//     (c) then these TU-local records collapse into `using BrnGui::...;`.
//
// GuiEventRunFsm CARRIES THE WRONG ID IN ITS OWN HEADER. BrnGuiEventTypeDefs.h:140 has
// `GetEventType() const { return 142; }`, but AddGuiEvent<GuiEventRunFsm> @0x823D2438 bakes
// `AddEvent(q, ev, 144, 24)` and the MOUNTED consumer is BrnGuiModule.cpp:1758 `case 144`.
// 142 is dead on both ends. This TU therefore posts the record the way the tree's own live
// producer already does (BrnGameModule::BridgeGameToGui @0x823DCA10, BrnGameModule.cpp:625-637):
// build the real BrnGui::GuiEventRunFsm and AddEvent it with a literal 144.
// SHARED_HEADER_REQUEST (owner: the Gui lane): correct BrnGuiEventTypeDefs.h:140 to 144 and
// delete both hand-rolled posts.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"                // PushGuiEvent<T>

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                     // CgsID / CgsIDCompress
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                    // GuiEventJunctionInfo, GuiEventRunFsm
#include "GameSource/GameState/BrnGameActions.h"                   // the action payload homes
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // OutputBuffer
#include "GameSource/GameState/BrnGameStateSharedIO.h"             // EGameModeType
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // GameModeParams
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"             // InputBuffer::GetGuiEvents()
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint
#include <stdlib.h>                                                // getenv (the diag guard)
#include <cstring>                                                 // memset / memcpy

namespace BrnGame
{
// The declaration the WIRE REQUEST at the top asks the conductor to move into
// GameSource/Game/GameBridgeGameStateToX.h. Kept here so the definition below is checked against
// the signature the header will carry.
bool TranslateEventFlowGameActionToGuiEvent(
    s32 liActionType,
    const CgsModule::Event* lpAction,
    CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
    const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

namespace
{
    // The two mode-set tests the event-flow arms share, spelled once. Both are literal
    // transcriptions: `cmpwi 2 / beq ... cmpwi 0x10 / bne` is the SHOWTIME pair (offline 2,
    // online 16), and `cmpwi 0xF / beq ... cmpwi 0x10 / bne` is the online free-burn-lobby +
    // online-showtime pair.
    inline bool IsShowtimeMode(s32 liMode)
    {
        return liMode == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
               liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
    }
    inline bool IsOnlineLobbyOrShowtimeMode(s32 liMode)
    {
        return liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
               liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
    }

    // =====================================================================================
    // GUI-EVENT WIRE RECORDS. One per posted id; each carries its attested (id, size) pair in
    // GetEventType() + sizeof, both read off that type's AddGuiEvent<T> instantiation.
    // =====================================================================================

    // id 93 size 152 -- AddGuiEvent<GuiEventPrepareForModeStart> @0x823D27D0.
    // Store map: the case-23 arm's var_30D0-based frame, @0x823EADCC..0x823EAFD0.
    struct PrepareForModeStartWire93
    {
        CgsID mPursuedCarId;                   // +0x00  GameModeParams::mPursuedCarID (ld 0x58(r25))
        s32   miCurrentRound;                  // +0x08  action->GetCurrentRound()     (lwz 0x8A0(r31))
        s32   meGameModeType;                  // +0x0C  GameModeParams::GetGameModeType()
        u32   muEventJunctionID;               // +0x10  GameModeParams::muEventJunctionID (+0x44)
        u32   muJunctionID;                    // +0x14  GameModeParams::muJunctionID      (+0x48)
        u16   mau16CheckpointLandmark[16];     // +0x18  CheckpointData[i]+0x00, i < GetCheckpointCount()
        s32   maiCheckpointDistrict[16];       // +0x38  CheckpointData[i]+0x04
        // The medal-run pin (closure verify 2026-08-26): GameModeParams +0x60/0x64/0x68/0x6C are
        // bronze/silver/gold/mfModeTimeLimit (SetMedalModeTimer's own assert names +0x68's value
        // lfGoldTimeLimitSeconds). The console arm copies them straight through
        // (@0x823EAE00..0x823EAE2C), so the wire slots are:
        f32   mfNeedForBronze;                 // +0x78  GameModeParams+0x60
        f32   mfNeedForSilver;                 // +0x7C  GameModeParams+0x64
        f32   mfNeedForGold;                   // +0x80  GameModeParams+0x68
        f32   mfModeTimeLimit;                 // +0x84  GameModeParams+0x6C
        s32   miPursuitRivalTotalDamage;       // +0x88  GameModeParams+0x50
        u8    mu8CheckpointCount;              // +0x8C  (u8)GameModeParams::GetCheckpointCount()
        u8    mu8DifficultyLevel;              // +0x8D  GameModeParams::muDifficultyLevel (+0x70)
        u8    mu8CarCount;                     // +0x8E  mbIsOnline ? miNumNetworkPlayers : miNumRivals
        u8    mu8RoadRageThreshold;            // +0x8F  (u8)GameModeParams::miRoadRageThreshold (+0x4C)
        u8    mbIsOnline;                      // +0x90  GameModeParams::mbIsOnline (+0x94)
        u8    mbOnlineLobbyTransition;         // +0x91  see the arm (name FLAGGED there)
        u8    maPad92[6];                      // +0x92..+0x97 tail padding to the attested 152

        s32 GetEventType() const { return 93; }
    };
    static_assert(sizeof(PrepareForModeStartWire93) == 152,
                  "X360 AddGuiEvent<GuiEventPrepareForModeStart> posts 152 bytes (id 93)");

    // id 164 size 1 -- AddGuiEvent<CgsGui::GuiEvent<164>> @0x823D2B68 (`li r6,1` / `li r5,0xA4`
    // into VariableEventQueue<32768,16>::AddEvent, asm 0x823D2C04..0x823D2C10).
    // ⭐ THE FLY-BY'S EXIT SIGNAL. The mangled name carries no BrnGui type name (the console posts
    // the bare CgsGui::GuiEvent<164> tag) and the case-30 arm hands it a 1-byte stack slot it never
    // writes (`addi r4, r1, var_3543` @0x823EB330, with no preceding store), so there is no payload
    // to model. Reproduced as a zeroed byte -- the host must not post uninitialised memory; the
    // consumer never reads it. Same accommodation as FinishedModeResultsWire321 below.
    // The CONSUMER IS MOUNTED and reads only the id: BrnGui::PreRaceFlyByState::HandleIncomingEvents'
    // KI_EVENT_FLYBY_END arm (BrnPreRaceFlyBy_wJ_03.cpp:129), which plays the trans-out and ends in
    // TriggerExitState -> SendStateEvent("BF_PROCEED") -- the ONLY producer of the lua BRNEVENTFSM's
    // PRE_FLY_BY exit edge. 164 is also one of the eight ids the state registers for
    // (PreRaceFlyByState::maiEventToObserve @0x82065CAC == {6,21,64,159,160,162,164,213}).
    struct PreRaceFlyByEndWire164
    {
        u8 mu8Unused;                          // +0x00  never written by the arm
        s32 GetEventType() const { return 164; }
    };
    static_assert(sizeof(PreRaceFlyByEndWire164) == 1, "id 164 size 1");

    // id 269 size 4 -- AddGuiEvent<BrnGui::GuiEventNetworkSplashEvent> @0x823D2C20
    // (`AddEvent(q, ev, 269, 4)`). Store map: one word, `stw r19` (0) @0x823EB344 on the first post
    // and `stw r14` (1) @0x823EB37C on the second.
    // FLAG: the field NAME is role-derived from those two values only -- nothing in b5-decomp
    // consumes id 269, so there is no consumer to arbitrate it. The COPY is exact.
    struct NetworkSplashWire269
    {
        s32 miSplashState;                     // +0x00
        s32 GetEventType() const { return 269; }
    };
    static_assert(sizeof(NetworkSplashWire269) == 4, "id 269 size 4");

    // id 279 size 2 -- AddGuiEvent<BrnGui::GuiEventNetworkShowFreeBurnIntro> @0x823D2AB0
    // (`AddEvent(q, ev, 279, 2)`). Store map: `addi r4, r1, var_3568` is the record base and the
    // arm writes `stb r19, var_3567` + `stb r19, var_3568` @0x823EB35C/0x823EB364 -- both bytes 0.
    // FLAG: no consumer in b5-decomp, so the two bytes stay unnamed.
    struct NetworkShowFreeBurnIntroWire279
    {
        u8 maZero[2];                          // +0x00..+0x01
        s32 GetEventType() const { return 279; }
    };
    static_assert(sizeof(NetworkShowFreeBurnIntroWire279) == 2, "id 279 size 2");

    // id 234 size 4 -- AddGuiEvent<GuiEventUpdateEventCountdown> @0x823D25A8.
    // Store map: `lwz r11,0(r31); stw r11, var_35D8` @0x823EAD50.
    struct UpdateEventCountdownWire234
    {
        s32 miCountdownDisplay;                // +0x00
        s32 GetEventType() const { return 234; }
    };
    static_assert(sizeof(UpdateEventCountdownWire234) == 4,
                  "X360 AddGuiEvent<GuiEventUpdateEventCountdown> posts 4 bytes (id 234)");

    // id 166 size 8 -- AddGuiEvent<GuiEventEnterEventStartLocation> @0x823D1E78.
    // Store map: the case-44 arm's var_3508-based frame @0x823EA948..0x823EA970.
    struct EnterEventStartLocationWire166
    {
        u32 muLeavingStartLocation;            // +0x00  0 when the action reports "in region", else 1
        u16 mu16StartLocationId;               // +0x04  the action's id, or 0xFFFF when leaving
        u8  maPad06[2];                        // +0x06

        s32 GetEventType() const { return 166; }
    };
    static_assert(sizeof(EnterEventStartLocationWire166) == 8,
                  "X360 AddGuiEvent<GuiEventEnterEventStartLocation> posts 8 bytes (id 166)");

    // id 321 size 1 -- AddGuiEvent<GuiEventFinishedModeResults> @0x823D24F0.
    // The case-38 arm posts `addi r4, r1, var_3547` with NO preceding store: the console sends
    // one uninitialised stack byte. Reproduced as a zeroed byte (the host must not post
    // uninitialised memory; the consumer never reads it).
    struct FinishedModeResultsWire321
    {
        u8 mu8Unused;                          // +0x00
        s32 GetEventType() const { return 321; }
    };
    static_assert(sizeof(FinishedModeResultsWire321) == 1,
                  "X360 AddGuiEvent<GuiEventFinishedModeResults> posts 1 byte (id 321)");

    // id 322 size 12 -- AddGuiEvent<GuiEventStopMode> @0x823D2380.
    // Store map: the case-39 arm's var_3388-based frame @0x823EABCC..0x823EABFC.
    struct StopModeWire322
    {
        s32 meGameModeType;                    // +0x00  action+0x00
        s32 miField04;                         // +0x04  action+0x0C   FLAG: unnamed on both ends
        u8  mu8Field08;                        // +0x08  action+0x11   FLAG
        u8  mu8Field09;                        // +0x09  action+0x12   FLAG
        u8  mu8Field0A;                        // +0x0A  action+0x13   FLAG
        u8  maPad0B[1];                        // +0x0B

        s32 GetEventType() const { return 322; }
    };
    static_assert(sizeof(StopModeWire322) == 12,
                  "X360 AddGuiEvent<GuiEventStopMode> posts 12 bytes (id 322)");

    // id 307 size 8 -- AddGuiEvent<GuiEventMedalUpdate> @0x823D1A28.
    // Store map: four halfword copies @0x823EA7DC..0x823EA804.
    struct MedalUpdateWire307
    {
        s16 mi16TotalWins;                     // +0x00  action+0x00
        s16 mi16Field02;                       // +0x02  action+0x02   FLAG
        s16 mi16Field04;                       // +0x04  action+0x04   FLAG
        s16 mi16WinsToNextRank;                // +0x06  action+0x06

        s32 GetEventType() const { return 307; }
    };
    static_assert(sizeof(MedalUpdateWire307) == 8,
                  "X360 AddGuiEvent<GuiEventMedalUpdate> posts 8 bytes (id 307)");

    // id 320 size 1 -- AddGuiEvent<GuiEventTriggerOnlinePostEvent> @0x823D1F30.
    struct TriggerOnlinePostEventWire320
    {
        u8 mu8Flag;                            // +0x00  action+0xE1
        s32 GetEventType() const { return 320; }
    };
    static_assert(sizeof(TriggerOnlinePostEventWire320) == 1, "id 320 size 1");

    // id 291 size 1 -- AddGuiEvent<CgsGui::GuiEvent<291>> @0x823D1FE8 (`AddEvent(q, ev, 291, 1)`).
    // The OFFLINE post-event request. The mangled name carries no BrnGui type name (the console
    // posts the bare CgsGui::GuiEvent<291> tag), and the case-37 arm posts it from a 1-byte
    // scratch slot it never writes -- so there is no payload to model.
    struct PostEventRequestWire291
    {
        u8 mu8Unused;                          // +0x00
        s32 GetEventType() const { return 291; }
    };
    static_assert(sizeof(PostEventRequestWire291) == 1, "id 291 size 1");

    // id 356 size 1 -- AddGuiEvent<GuiAutosaveRequestEvent> @0x823D03E0. Posted with r19 == 0.
    struct AutosaveRequestWire356
    {
        u8 mu8Zero;                            // +0x00
        s32 GetEventType() const { return 356; }
    };
    static_assert(sizeof(AutosaveRequestWire356) == 1, "id 356 size 1");

    // id 96 size 1 -- AddGuiEvent<GuiSetEasyDriveNotAllowedEvent> @0x823D2158. Posted with r19 == 0.
    struct SetEasyDriveNotAllowedWire96
    {
        u8 mu8Zero;                            // +0x00
        s32 GetEventType() const { return 96; }
    };
    static_assert(sizeof(SetEasyDriveNotAllowedWire96) == 1, "id 96 size 1");

    // id 536 size 2 -- AddGuiEvent<GuiEventTickerClearMessages> @0x823D2718.
    // The case-23 arm writes both bytes 0 (`stb r19, var_3564` / `stb r19, var_3563`).
    struct TickerClearMessagesWire536
    {
        u8 maZero[2];                          // +0x00..+0x01
        s32 GetEventType() const { return 536; }
    };
    static_assert(sizeof(TickerClearMessagesWire536) == 2, "id 536 size 2");

    // id 188 size 8 -- AddGuiEvent<GuiOverlayWaitFinishRequest> @0x823CFD68.
    // The canonical type (BrnGui::GuiOverlayWaitFinishRequest, BrnGuiOverlaysDirector.h:29) is a
    // bare `CgsID mOverlayId` with an inline Construct -- but it has NO GetEventType(), so
    // PushGuiEvent cannot bake its id. Modelled here with the same single member plus the id,
    // rather than hand-rolling an AddEvent with a literal.
    // SHARED_HEADER_REQUEST (owner: the Gui lane): add
    // `s32 GetEventType() const { return 188; }` to BrnGuiOverlaysDirector.h:29 and this record
    // collapses to a `using`.
    struct OverlayWaitFinishRequestWire188
    {
        CgsID mOverlayId;                      // +0x00
        s32 GetEventType() const { return 188; }
    };
    static_assert(sizeof(OverlayWaitFinishRequestWire188) == 8, "id 188 size 8");

    // id 289 size 192 -- AddGuiEvent<GuiEventOfflinePostEvent> @0x823D20A0.
    // Store map: the case-37 arm's var_2D10-based frame @0x823EAA74..0x823EAB3C. Every member
    // below is a store the arm emits; the gaps are never written by the arm and are explicit
    // padding, so the record is byte-exact at 192 without inventing fields.
    struct OfflinePostEventWire289
    {
        u64 mu64Field00;                       // +0x00  action+0xC8      FLAG
        u64 mu64Field08;                       // +0x08  action+0x40, only when action->mbHasField40
        u8  maPad10[8];                        // +0x10..+0x17 never written by the arm
        s32 meGameModeType;                    // +0x18  action+0x00
        s32 miField1C;                         // +0x1C  action+0x08      FLAG
        f32 mfField20;                         // +0x20  action+0x0C      FLAG
        s32 miField24;                         // +0x24  action+0x10      FLAG
        s32 miField28;                         // +0x28  action+0x14      FLAG
        f32 mfField2C;                         // +0x2C  action+0x18      FLAG
        u8  maBlock30[0x70];                   // +0x30  memcpy 0x70 from action+0x58,
                                               //        only when action->mbHasBlock58
        s32 miFieldA0;                         // +0xA0  action+0x48      FLAG
        s32 miFieldA4;                         // +0xA4  action+0x4C      FLAG
        u8  maPadA8[8];                        // +0xA8..+0xAF never written by the arm
        u8  mu8FieldB0;                        // +0xB0  (u8)action+0x04  FLAG
        u8  mu8FieldB1;                        // +0xB1  action+0xE0      FLAG
        u8  mu8FieldB2;                        // +0xB2  action+0xE2      FLAG
        u8  mu8FieldB3;                        // +0xB3  action+0xE1      FLAG
        u8  mu8FieldB4;                        // +0xB4  action+0xE3      FLAG
        u8  mu8FieldB5;                        // +0xB5  action+0xDA      FLAG
        u8  mu8FieldB6;                        // +0xB6  action+0xDB      FLAG
        u8  mbHasField08;                      // +0xB7  action+0xDE (the +0x08 copy's own gate)
        u8  mu8FieldB8;                        // +0xB8  action+0xDC      FLAG
        u8  maPadB9[7];                        // +0xB9..+0xBF tail padding to the attested 192

        s32 GetEventType() const { return 289; }
    };
    static_assert(sizeof(OfflinePostEventWire289) == 192,
                  "X360 AddGuiEvent<GuiEventOfflinePostEvent> posts 192 bytes (id 289)");

    // ---------------------------------------------------------------------------------------
    // GuiEventRunFsm posts through the raw queue (24-byte controller record, not a PushGuiEvent
    // payload); GetEventType() is 144, corrected from the dead PS3 id 142 on 2026-08-27.
    // ---------------------------------------------------------------------------------------
    void PostRunFsm(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                    const char* lpacFsmName, const char* lpacInitialState,
                    BrnGui::EHUDFSMs leFsmToRun, BrnGui::GuiFlow leFlowToUse)
    {
        if (lpGuiInput == 0)
        {
            return;
        }
        BrnGui::GuiEventRunFsm lEvent;
        lEvent.mFsmId          = CgsIDCompress(lpacFsmName);
        lEvent.mInitialStateId = (lpacInitialState != 0) ? CgsIDCompress(lpacInitialState)
                                                         : static_cast<CgsID>(0);
        lEvent.meFsmToRun      = leFsmToRun;
        lEvent.meFlowToUse     = leFlowToUse;
        lpGuiInput->GetGuiEvents()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), lEvent.GetEventType(),
            static_cast<s32>(sizeof(lEvent)));
    }
}   // anonymous namespace

    // =========================================================================
    // [evt-flow E1] The event-flow arms of BrnGameModule::TranslateGameActionsToGuiEvents
    // @0x823E9CE0. `liActionType` / `lpAction` are the queue walk's current record (the caller's
    // GetFirstEvent / GetNextEvent pair, @0x823E9D88); `lpGuiInput` is the console's r20 and
    // `lpGameStateOutput` its r31-at-entry.
    //
    // Returns true when an arm consumed the action, so the caller's `default:` can keep falling
    // through for everything else. On the console this IS the default arm's continuation: every
    // case below ends with `b def_823EA1F0`, the shared "advance the queue" tail.
    //
    // The two entry asserts (GameBridgeGameStateToX.cpp:754/755) stay with the caller -- they
    // guard the whole walk, not an arm.
    // =========================================================================
    bool TranslateEventFlowGameActionToGuiEvent(
        s32 liActionType,
        const CgsModule::Event* lpAction,
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        if (lpAction == 0 || lpGuiInput == 0)
        {
            return false;
        }
        (void)lpGameStateOutput;   // read only by the parked case-37 asserts -- see that arm

        // [DIAG] the `[evt-flow]` rung. NOT IN THE X360 BINARY. Same logger, same env guard
        // (BRN_PROP_DIAG) and same first-N latch as the `[UI-gate]` ladder in the stunt TU.
        static const bool sbDiag          = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static s32        siDiagLinesLeft = 24;

        switch (liActionType)
        {

        // ---- 201  E_ACTION_EVENT_AT_JUNCTION_AVAILABLE (40 bytes) --------------------------
        // *** THE VISIBLE ORACLE. @0x823EA810..0x823EA874, instruction for instruction: eleven
        // loads off the action record, eleven stores into a 32-byte frame at var_35C0, then
        // AddGuiEvent<GuiEventJunctionInfo> (id 311, size 32 -- @0x823D1AE0).
        //   var_35C0+0x00 <- ld  0x10(r31)   -> mSpecialEventCarId
        //   var_35C0+0x08 <- lwz 0x18(r31)   -> meGameModeType
        //   var_35C0+0x0C <- lbz 0x1C(r31)   -> mi8Difficulty
        //   var_35C0+0x0D <- lbz 0x1D(r31)   -> mi8MedalAchieved
        //   var_35C0+0x10 <- lwz 0x04(r31)   -> miEventID
        //   var_35C0+0x14 <- lbz 0x1F(r31)   -> mbCanEnterEvent
        //   var_35C0+0x15 <- lbz 0x20(r31)   -> mbEventUnlocked
        //   var_35C0+0x16 <- lbz 0x1E(r31)   -> mbOnEntry
        //   var_35C0+0x17 <- lbz 0x21(r31)   -> mbSpecificCarEventValid
        //   var_35C0+0x18 <- lbz 0x22(r31)   -> mbIsNewlyDiscovered
        //   var_35C0+0x19 <- lbz 0x23(r31)   -> mbIsAutoUnlockedChallenge
        // That store map is EXACTLY the committed BrnGui::GuiEventJunctionInfo member list in
        // BrnGuiEventTypeDefs.h:1192 -- which is what pins both ends of this arm: the same
        // eleven fields, in the same eleven places. So this arm uses the SHARED type, not a
        // TU-local wire; the consumer half (BrnJunctionInfoComponent::HandleJunctionChange +
        // BrnFBurnMainHudState case 311 + BrnOdometerComponent::HandleJunctionChange) is MOUNTED
        // and reads it by name.
        //
        // HOST SIZE DEVIATION, DELIBERATE AND SELF-CONSISTENT: the console record is 32 bytes
        // (fields at +0). The PC GuiEventJunctionInfo derives from CgsGui::GuiEvent<311>, whose
        // three-word header pads to 16 under the type's CgsID alignment, so sizeof is 48 here and
        // PushGuiEvent posts 48. Both PC ends agree (BrnGuiModule::DispatchInboundGuiEvents hands
        // the record base straight to the state, which reinterpret_casts it back to this type),
        // so the panel works; the divergence is the standing project accommodation for the
        // GuiEvent<N> base and is recorded on the type, not fixed here.
        case BrnGameState::GameStateModuleIO::E_ACTION_EVENT_AT_JUNCTION_AVAILABLE:
        {
            const BrnGameState::GameStateModuleIO::JunctionInfoAction* lpJunction =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::JunctionInfoAction*>(lpAction);

            BrnGui::GuiEventJunctionInfo lEvent;
            lEvent.mSpecialEventCarId        = lpJunction->mSpecialEventCarId;
            lEvent.meGameModeType            = lpJunction->meGameModeType;
            lEvent.mi8Difficulty             = lpJunction->mi8Difficulty;
            lEvent.mi8MedalAchieved          = lpJunction->mi8MedalAchieved;
            lEvent.miEventID                 = static_cast<s32>(lpJunction->muEventJunctionID);
            lEvent.mbCanEnterEvent           = lpJunction->mbCanEnterEvent;
            lEvent.mbEventUnlocked           = lpJunction->mbEventUnlocked;
            lEvent.mbOnEntry                 = lpJunction->mbOnEntry;
            lEvent.mbSpecificCarEventValid   = lpJunction->mbSpecificCarEventValid;
            lEvent.mbIsNewlyDiscovered       = lpJunction->mbIsNewlyDiscovered;
            lEvent.mbIsAutoUnlockedChallenge = lpJunction->mbIsAutoUnlockedChallenge;
            PushGuiEvent(lEvent, lpGuiInput);

            // ⭐ 2026-08-27: OWN counter, not the shared siDiagLinesLeft. Action 201 posts EVERY
            // FRAME while the player stands in a light region, so 24 frames of idling at the
            // junction burned the whole shared budget before action 23 ever printed -- the later
            // arms then went BLIND, not quiet (the S4 audit's "e-count (never)" hazard). Edge-
            // filtered on the event id so one line per junction visit, 8 visits per run.
            static s32 siJunctionDiagLeft = 8;
            static s32 siLastDiagEventId  = -1;
            if ( sbDiag && siJunctionDiagLeft > 0 && lEvent.miEventID != siLastDiagEventId &&
                 CgsDev::Log::gpDebugPrint != 0 )
            {
                --siJunctionDiagLeft;
                siLastDiagEventId = lEvent.miEventID;
                *CgsDev::Log::gpDebugPrint
                    << "[evt-flow] action 201 -> gui 311 (event " << lEvent.miEventID
                    << " mode " << static_cast<s32>(lEvent.meGameModeType)
                    << " onEntry " << static_cast<s32>(lEvent.mbOnEntry)
                    << " canEnter " << static_cast<s32>(lEvent.mbCanEnterEvent) << ")\n";
            }
            return true;
        }

        // ---- 23  E_ACTION_PREPARE_FOR_MODE (2272 bytes) -----------------------------------
        // @0x823EAD80..0x823EB070. The arm that both DESCRIBES the event to the GUI and, for an
        // offline mode, tells the flow controller to run the EVENT FSM.
        //
        // Body order, transcribed:
        //   1. `lwz r11,0(r31); cmpwi 0; beq; cmpwi 1; bne default` -- drop unless this is the
        //      first prepare-for-mode of the (possibly split) pair.
        //   2. `addi r25, r31, 0x30` + a null assert on it ("lpParams",
        //      GameBridgeGameStateToX.cpp:1258) -- r25 is the embedded GameModeParams.
        //   3. fill the 152-byte GuiEventPrepareForModeStart frame (store map on
        //      PrepareForModeStartWire93).
        //   4. mbIsOnline fork: online takes miNumNetworkPlayers, offline takes miNumRivals; the
        //      online branch additionally posts a GuiOverlayWaitFinishRequest("GMStrOffline")
        //      when the mode is the lobby/showtime pair.
        //   5. showtime pair -> post GuiEventTickerClearMessages (536).
        //   6. `GetCheckpointCount() <= 16` assert ("Exceded landmark count", :1313) + the two
        //      per-checkpoint arrays.
        //   7. AddGuiEvent<GuiEventPrepareForModeStart>.
        //   8. RUN THE EVENT FSM: post GuiEventRunFsm{ CgsIDCompress("BRNEVENTFSM"), 0,
        //      E_GUI_HUD_EVENT(2), E_GUIFLOW_HUD(1) } when the mode is offline (< 10), or when it
        //      is the online lobby/showtime pair AND neither mbStartingFreeburnDueToPlayerJoin
        //      (action+0x8D2) nor mbFinishedOnlineEvent (action+0x8D1) is set. "BRNEVENTFSM" is
        //      the image literal at var_3524 (loaded @0x823E9DC8).
        //   9. always post GuiSetEasyDriveNotAllowedEvent (96) with a zero byte.
        //
        // FLAG -- THE CHECKPOINT BLOCK IS LEFT ZEROED (payload +0x18..+0x77 and the count byte at
        // +0x8C). This is an ACCESS hole, not a proof hole: the store map is fully recovered
        // (`Array<CheckpointData,16>::operator[](params+0x260, i)` then `lhz +0` into
        // payload+0x18+2i and `lwz +4` into payload+0x38+4i, @0x823EAF84..0x823EAFCC), but the
        // three members it needs are unreachable from here --
        //     GameModeParams::maCheckpointDataArray  is PRIVATE with no accessor;
        //     GameModeParams::GetCheckpointCount()   is DECLARED-ONLY (its body lives in the
        //                                            unmounted, non-compiling BrnGameModeParams.cpp);
        //     CheckpointData::GetLandmarkIndex() / GetDistrict() are DECLARED-ONLY over private
        //                                            members (BrnCheckpointData.h:30-38).
        // Neither header is in this wave's file list, so the fields are zeroed rather than faked.
        // CONSEQUENCE: a mode with checkpoints (race / burning route) hands the GUI a zero
        // checkpoint list; a Stunt Run has none, so this wave's own target path is unaffected.
        // SHARED_HEADER_REQUEST (owner: the GameState lane) -- add an inline
        // `const CheckpointDataArray& GetCheckpointDataArray() const` + an inline
        // GetCheckpointCount() to BrnGameModeParams.h, and inline GetLandmarkIndex() /
        // GetDistrict() to BrnCheckpointData.h; then this block is the six lines above.
        //
        // FLAG -- payload +0x91 (mbOnlineLobbyTransition) is transcribed exactly but its NAME is
        // a guess from its inputs: the console computes it as
        // `action->IsMovingBetweenOnlineLobbyModes() && IsOnlineLobbyOrShowtimeMode(mode)`
        // (@0x823EAED4..0x823EAF1C). The predicate is proven; the name is not attested.
        case BrnGameState::GameStateModuleIO::E_ACTION_PREPARE_FOR_MODE:
        {
            const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPrepare =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::PrepareForModeAction*>(lpAction);

            if (!lpPrepare->IsFirstPrepareForMode())
            {
                return true;      // consumed: the console's `beq cr6, def_823EA1F0`
            }

            const BrnGameState::GameModeParams* lpParams = lpPrepare->GetGameModeParams();
            CGS_ASSERT(lpParams != 0, "lpParams");                         // :1258
            if (lpParams == 0)
            {
                return true;
            }

            const s32 liGameModeType = static_cast<s32>(lpParams->GetGameModeType());

            PrepareForModeStartWire93 lEvent;
            std::memset(&lEvent, 0, sizeof(lEvent));
            lEvent.mPursuedCarId               = lpParams->mPursuedCarID;
            lEvent.miCurrentRound              = lpPrepare->GetCurrentRound();
            lEvent.meGameModeType              = liGameModeType;
            lEvent.muEventJunctionID           = lpParams->muEventJunctionID;
            lEvent.muJunctionID                = lpParams->muJunctionID;
            lEvent.mfNeedForBronze             = lpParams->mfNeedForBronze;      // params+0x60
            lEvent.mfNeedForSilver             = lpParams->mfNeedForSilver;      // params+0x64
            lEvent.mfNeedForGold               = lpParams->mfNeedForGold;        // params+0x68
            lEvent.mfModeTimeLimit             = lpParams->mfModeTimeLimit;      // params+0x6C
            lEvent.miPursuitRivalTotalDamage   = lpParams->miPursuitRivalTotalDamage;
            lEvent.mu8DifficultyLevel          = lpParams->muDifficultyLevel;
            lEvent.mu8RoadRageThreshold        = static_cast<u8>(lpParams->miRoadRageThreshold);
            lEvent.mbIsOnline                  = lpParams->mbIsOnline ? 1u : 0u;
            // FLAG (see the banner on this case): the checkpoint id/district arrays and the count
            // byte stay at the memset zero -- GameModeParams::maCheckpointDataArray and
            // CheckpointData's getters are unreachable from this TU.
            lEvent.mu8CheckpointCount          = 0;

            if (lpParams->mbIsOnline)
            {
                lEvent.mu8CarCount = static_cast<u8>(lpParams->miNumNetworkPlayers);

                // @0x823EAE40..0x823EAE88 -- the online branch's overlay hand-off. The overlay
                // name is the image literal at r23 ("GMStrOffline", loaded @0x823EA114).
                if (IsOnlineLobbyOrShowtimeMode(liGameModeType))
                {
                    OverlayWaitFinishRequestWire188 lWaitRequest;
                    lWaitRequest.mOverlayId = CgsIDCompress("GMStrOffline");
                    PushGuiEvent(lWaitRequest, lpGuiInput);
                }
            }
            else
            {
                lEvent.mu8CarCount = static_cast<u8>(lpParams->miNumRivals);
            }

            // @0x823EAE98..0x823EAED0 -- both showtime modes clear the ticker first.
            if (IsShowtimeMode(liGameModeType))
            {
                TickerClearMessagesWire536 lClear;
                lClear.maZero[0] = 0;
                lClear.maZero[1] = 0;
                PushGuiEvent(lClear, lpGuiInput);
            }

            lEvent.mbOnlineLobbyTransition =
                (lpPrepare->IsMovingBetweenOnlineLobbyModes() &&
                 IsOnlineLobbyOrShowtimeMode(liGameModeType)) ? 1u : 0u;

            PushGuiEvent(lEvent, lpGuiInput);

            // @0x823EAFE4..0x823EB058 -- THE FSM HOP. Offline modes always run it; the online
            // lobby/showtime pair runs it only when the mode was not entered by a joining player
            // and did not just finish an online event.
            bool lbRunEventFsm =
                (liGameModeType < BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_COUNT);
            if (!lbRunEventFsm && IsOnlineLobbyOrShowtimeMode(liGameModeType))
            {
                lbRunEventFsm = !lpPrepare->GetStartingFreeburnLobbyDueToPlayerJoin() &&
                                !lpPrepare->GetFinishedOnlineEvent();
            }
            if (lbRunEventFsm)
            {
                // [FLAG PC bring-up gate 2026-08-27, NOT in the X360 binary] The hop itself is
                // console-faithful and PROVEN live (BrnGame.log: action 23 -> PRE_FLY_BY OnEnter),
                // but today both destination states (PRE_FLY_BY / RACE_MAIN) are inert scaffolds
                // in BrnHudStatesLinkStubs.cpp, and the hop's side effect is FBurnMainHudState::
                // OnLeave tearing down the whole HUD apt (PlayAptMovie("",1) + UnRegisterForEvents)
                // with nothing to rebuild it -- the HUD vanishes for the rest of the process.
                // Until the real states land, the hop is OPT-IN: set BRN_EVENT_FSM=1 to take it;
                // default keeps the freeburn HUD (junction panel, odometer, sat-nav) alive through
                // the event. DELETE-WHEN: PreRaceFlyBy wJ set + RaceMainHudState.cpp are MOUNTED
                // with real OnEnter bodies -- then the gate inverts the console behaviour and lies.
                static const bool sbEventFsm = ( getenv( "BRN_EVENT_FSM" ) != 0 );
                if (sbEventFsm)
                {
                    PostRunFsm(lpGuiInput, "BRNEVENTFSM", 0,
                               BrnGui::E_GUI_HUD_EVENT, BrnGui::E_GUIFLOW_HUD);
                }
            }

            // @0x823EB05C..0x823EB06C -- unconditional tail.
            SetEasyDriveNotAllowedWire96 lEasyDrive;
            lEasyDrive.mu8Zero = 0;
            PushGuiEvent(lEasyDrive, lpGuiInput);

            if ( sbDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                --siDiagLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[evt-flow] action 23 -> gui 93 (mode " << liGameModeType
                    << " round " << lEvent.miCurrentRound
                    << ") runEventFsm=" << (lbRunEventFsm ? 1 : 0) << "\n";
            }
            return true;
        }

        // ---- 30  E_ACTION_STOP_MODE_INTRO (8 bytes) ----------------------------------------
        // ⭐⭐ THE OTHER HALF OF CASE 23: 23 puts the GUI INTO the event FSM's PRE_FLY_BY state,
        // and THIS arm is the only thing in the whole image that gets it back out again.
        //
        // @0x823EB304..0x823EB38C, instruction for instruction (r19 == 0, r14 == 1, r31 == the
        // action record):
        //   lwz  r11, 0(r31)            -- action->meGameMode
        //   cmpwi 0xF / beq ; cmpwi 0x10 / bne   -- r11 = (mode == 15 || mode == 16)
        //   cmplwi 0 / bne loc_823EB340          -- ONLINE lobby/showtime takes the far branch
        //   [offline]  addi r4, r1, var_3543 (a 1-byte slot, NEVER WRITTEN)
        //              bl AddGuiEvent<CgsGui::GuiEvent<164>>          ; id 164 size 1
        //              b  default
        //   [online]   stw r19, var_35D8 ; bl AddGuiEvent<GuiEventNetworkSplashEvent>      (0)
        //              stb r19, var_3567 ; stb r19, var_3568
        //              bl AddGuiEvent<GuiEventNetworkShowFreeBurnIntro>                    (0,0)
        //              lbz r11, 4(r31)   ; beq default                 -- mbMovingBetweenLobbyModes
        //              stw r14, var_35D8 ; bl AddGuiEvent<GuiEventNetworkSplashEvent>      (1)
        //
        // THE WHOLE CONSOLE CHAIN THIS CLOSES, producer to consumer, every rung already mounted
        // except this one:
        //   PreRaceFlyByState::Update tail (BrnPreRaceFlyBy_wJ_04.cpp:360) posts GUI-out 163 once
        //     mfTimeRemaining (KAF_MODE_TYPE_PRE_EVENT_DURATION[mode]) expires
        //   -> BridgeGuiToGameState case 163 -> game event 25 (GameBridgeGUIToX_GameState.cpp:153)
        //   -> GameStateModule::ProcessGameEvents case 25 -> ModeManager::FinishOfflineModeIntro
        //      -> GameMode::SendEvent(E_GME_NEXT)
        //   -> IntroState::OnLeave sets GameMode::mbIntroJustFinished (+176)
        //   -> ModeManager::UpdateCurrentMode's latch arm (BrnModeManager_UpdateMode.cpp:345)
        //      -> ModeManager::StopModeIntro posts ACTION 30
        //   -> **THIS ARM** -> GUI event 164
        //   -> PreRaceFlyByState::HandleIncomingEvents KI_EVENT_FLYBY_END -> trans-out (or an
        //      immediate TriggerExitState when nothing is on screen yet)
        //   -> TriggerExitState -> SendStateEvent("BF_PROCEED") -> the lua BRNEVENTFSM leaves
        //      PRE_FLY_BY.
        // Every rung above 30 was proven live in scratch/flow_run/20260827_134528/BrnGame.log
        // (the SECOND "[start] event 25 -> FinishOfflineModeIntro" at t+6.15 s is the fly-by's own
        // 163 arriving 0.13 s after the one-shot harness self-trigger); the flow stopped dead here
        // because action 30 had no arm and fell through the caller's `default:`.
        case BrnGameState::GameStateModuleIO::E_ACTION_STOP_MODE_INTRO:
        {
            const BrnGameState::GameStateModuleIO::StopModeIntroAction* lpStopIntro =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::StopModeIntroAction*>(lpAction);

            const s32 liGameModeType = static_cast<s32>(lpStopIntro->meGameMode);

            if (!IsOnlineLobbyOrShowtimeMode(liGameModeType))
            {
                PreRaceFlyByEndWire164 lFlyByEnd;
                lFlyByEnd.mu8Unused = 0;                 // console: the slot is left unwritten
                PushGuiEvent(lFlyByEnd, lpGuiInput);
            }
            else
            {
                NetworkSplashWire269 lSplashOff;
                lSplashOff.miSplashState = 0;            // `stw r19` @0x823EB344
                PushGuiEvent(lSplashOff, lpGuiInput);

                NetworkShowFreeBurnIntroWire279 lFreeBurnIntro;
                lFreeBurnIntro.maZero[0] = 0;            // `stb r19` @0x823EB364
                lFreeBurnIntro.maZero[1] = 0;            // `stb r19` @0x823EB35C
                PushGuiEvent(lFreeBurnIntro, lpGuiInput);

                if (lpStopIntro->mbMovingBetweenLobbyModes)   // `lbz r11, 4(r31)` @0x823EB36C
                {
                    NetworkSplashWire269 lSplashOn;
                    lSplashOn.miSplashState = 1;         // `stw r14` @0x823EB37C
                    PushGuiEvent(lSplashOn, lpGuiInput);
                }
            }

            // [DIAG] NOT IN THE X360 BINARY. OWN counter, not the shared siDiagLinesLeft: this is
            // the one line the fly-by-exit investigation needs, and actions 44/47/201 can burn the
            // shared budget long before a mode intro ever stops (the 2026-08-27 lesson on the
            // action-201 arm -- a spent shared budget makes later arms BLIND, not quiet).
            {
                static s32 siStopIntroDiagLeft = 8;
                if ( sbDiag && siStopIntroDiagLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
                {
                    --siStopIntroDiagLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[evt-flow] action 30 -> gui "
                        << (IsOnlineLobbyOrShowtimeMode(liGameModeType) ? 269 : 164)
                        << " (mode " << liGameModeType
                        << " movingBetweenLobbyModes "
                        << (lpStopIntro->mbMovingBetweenLobbyModes ? 1 : 0)
                        << ") -- fly-by exit signal\n";
                }
            }
            return true;
        }

        // ---- 47  E_ACTION_SET_COUNTDOWN (4 bytes) ------------------------------------------
        // @0x823EAD50..0x823EAD64: `lwz r11,0(r31); stw r11, var_35D8;
        // AddGuiEvent<GuiEventUpdateEventCountdown>` (id 234, size 4). One word, straight through.
        case BrnGameState::GameStateModuleIO::E_ACTION_SET_COUNTDOWN:
        {
            const BrnGameState::GameStateModuleIO::SetCountdownAction* lpCountdown =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::SetCountdownAction*>(lpAction);

            UpdateEventCountdownWire234 lEvent;
            lEvent.miCountdownDisplay = lpCountdown->miCountdownDisplay;
            PushGuiEvent(lEvent, lpGuiInput);

            if ( sbDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                --siDiagLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[evt-flow] action 47 -> gui 234 (countdown "
                    << lEvent.miCountdownDisplay << ")\n";
            }
            return true;
        }

        // ---- 44  E_ACTION_SET_IN_MODE_START_REGION (4 bytes) -------------------------------
        // @0x823EA948..0x823EA97C:
        //   lbz  r11, 2(r31)                       -- the action's in-region byte
        //   bne  -> { r11 = lhz 0(r31) ;   payload+0x00 = r19 (0) }
        //   beq  -> { payload+0x00 = r14 (1) ; r11 = lhz word_82F241B8 }
        //   sth  r11, payload+0x04
        //   AddGuiEvent<GuiEventEnterEventStartLocation>  (id 166, size 8)
        // The rodata halfword at 0x82F241B8 is 0xFFFF -- read out of the image at file offset
        // 0xF241B8 (bytes FF FF, big-endian), i.e. the invalid-start-location sentinel, NOT an
        // invented constant.
        case BrnGameState::GameStateModuleIO::E_ACTION_SET_IN_MODE_START_REGION:
        {
            const BrnGameState::GameStateModuleIO::SetInModeStartRegionAction* lpRegion =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::SetInModeStartRegionAction*>(lpAction);

            EnterEventStartLocationWire166 lEvent;
            lEvent.maPad06[0] = 0;
            lEvent.maPad06[1] = 0;
            if (lpRegion->mbInStartRegion != 0)
            {
                lEvent.muLeavingStartLocation = 0u;
                lEvent.mu16StartLocationId    = lpRegion->mu16StartLocationId;
            }
            else
            {
                lEvent.muLeavingStartLocation = 1u;
                lEvent.mu16StartLocationId    = 0xFFFFu;   // image 0x82F241B8 (BE) == FF FF
            }
            PushGuiEvent(lEvent, lpGuiInput);

            if ( sbDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                --siDiagLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[evt-flow] action 44 -> gui 166 (leaving "
                    << static_cast<s32>(lEvent.muLeavingStartLocation)
                    << " id " << static_cast<s32>(lEvent.mu16StartLocationId) << ")\n";
            }
            return true;
        }

        // ---- 38  E_ACTION_FINISHED_MODE_RESULTS (1 byte) -----------------------------------
        // @0x823EAD20..0x823EAD48. The whole arm: log the banner behind the
        // CgsDev::Message::gxMessageFilterFlags bit-0 gate, then
        // AddGuiEvent<GuiEventFinishedModeResults> (id 321, size 1) from an unwritten scratch
        // byte. The banner is the image literal loaded into var_33A4 @0x823EA130 -- and it is
        // what NAMES action 38 (see the citation in BrnGameActions.h).
        case BrnGameState::GameStateModuleIO::E_ACTION_FINISHED_MODE_RESULTS:
        {
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "***** GameBridgeGameStateToX found E_ACTION_FINISHED_MODE_RESULTS *****\n";
            }

            FinishedModeResultsWire321 lEvent;
            lEvent.mu8Unused = 0;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // ---- 39  E_ACTION_STOP_MODE (24 bytes) ---------------------------------------------
        // @0x823EABCC..0x823EAC00, five copies into a 12-byte frame at var_3388:
        //   +0x00 <- lwz 0x00(r31)   +0x04 <- lwz 0x0C(r31)
        //   +0x08 <- lbz 0x11(r31)   +0x09 <- lbz 0x12(r31)   +0x0A <- lbz 0x13(r31)
        // then AddGuiEvent<GuiEventStopMode> (id 322, size 12).
        case BrnGameState::GameStateModuleIO::E_ACTION_STOP_MODE:
        {
            const BrnGameState::GameStateModuleIO::StopModeAction* lpStop =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::StopModeAction*>(lpAction);

            StopModeWire322 lEvent;
            lEvent.meGameModeType = static_cast<s32>(lpStop->meGameModeType);
            lEvent.miField04      = lpStop->miField0C;
            lEvent.mu8Field08     = lpStop->mu8Field11;
            lEvent.mu8Field09     = lpStop->mu8Field12;
            lEvent.mu8Field0A     = lpStop->mu8Field13;
            lEvent.maPad0B[0]     = 0;
            PushGuiEvent(lEvent, lpGuiInput);

            if ( sbDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                --siDiagLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[evt-flow] action 39 -> gui 322 (mode " << lEvent.meGameModeType << ")\n";
            }
            return true;
        }

        // ---- 200  E_ACTION_UPDATE_PLAYER_MEDALS (8 bytes) ----------------------------------
        // @0x823EA784..0x823EA808. Behind the gxMessageFilterFlags bit-0 gate the console streams
        // `"Medals update: " << h0 << ", " << h2 << ", " << h4 << "\n"` (the image literals at
        // var_33BC / var_33C4 / r17, loaded @0x823EA1A0 / @0x823EA198 / @0x823EA190), then copies
        // four halfwords straight across into GuiEventMedalUpdate (id 307, size 8, @0x823D1A28).
        case BrnGameState::GameStateModuleIO::E_ACTION_UPDATE_PLAYER_MEDALS:
        {
            const BrnGameState::GameStateModuleIO::UpdatePlayerMedalsAction* lpMedals =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::UpdatePlayerMedalsAction*>(lpAction);

            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "Medals update: " << static_cast<s32>(lpMedals->mi16TotalWins)
                    << ", " << static_cast<s32>(lpMedals->mi16Field02)
                    << ", " << static_cast<s32>(lpMedals->mi16Field04) << "\n";
            }

            MedalUpdateWire307 lEvent;
            lEvent.mi16TotalWins      = lpMedals->mi16TotalWins;
            lEvent.mi16Field02        = lpMedals->mi16Field02;
            lEvent.mi16Field04        = lpMedals->mi16Field04;
            lEvent.mi16WinsToNextRank = lpMedals->mi16WinsToNextRank;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // ---- 37  E_ACTION_SHOW_MODE_RESULTS (232 bytes) ------------------------------------
        // @0x823EA984..0x823EAB78. Four posts, in this order:
        //   a. the post-event REQUEST fork on action+0xE4:
        //        set   -> GuiEventTriggerOnlinePostEvent (320, 1) carrying action+0xE1;
        //        clear -> the bare CgsGui::GuiEvent<291> (291, 1) tag, and the offline results
        //                 record below is then built unconditionally.
        //      (When +0xE4 IS set, the results record is built only for the showtime pair -- the
        //       console's r27 starts as `action+0x00 in {2,16}` and the clear branch forces it
        //       true, `mr r27, r14` @0x823EA9D8.)
        //   b. the 192-byte GuiEventOfflinePostEvent (289) -- store map on OfflinePostEventWire289.
        //   c. GuiAutosaveRequestEvent (356) with a zero byte, only when +0xE4 is clear.
        //   d. GuiSetEasyDriveNotAllowedEvent (96) with a zero byte, unconditionally.
        //
        // FLAG -- THE THREE CONSOLE ASSERTS IN THIS ARM ARE NOT REPRODUCED
        // (@0x823EA9E8..0x823EAA70, GameBridgeGameStateToX.cpp:1016/1017/1018:
        //   "lScoringOutput.mePlayerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0"
        //   "lScoringOutput.mePlayerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT"
        //   "lScoringOutput.mabValid[lScoringOutput.mePlayerRaceCarIndex]").
        // They read a scoring-output block at lpGameStateOutput + 173240 (the prologue's
        // `addis r11, r31, 3 / addi r11, r11, -0x5B48` @0x823E9D68, cached in var_3534), whose
        // layout has no home in b5-decomp -- the index is at +0xA34 and the validity array at
        // +0xA08. Hand-modelling that offset is exactly the X360-value-on-the-x64-host trap this
        // campaign keeps recording, so the asserts are NAMED AND SKIPPED rather than faked. They
        // are pure diagnostics: no store in the arm depends on them.
        //
        // FLAG -- most of this record's field NAMES are unproven (see the FLAGs on
        // ShowModeResultsAction and OfflinePostEventWire289). The COPY is exact; the meanings are
        // not. Nothing consumes id 289 in b5-decomp yet, so nothing depends on the names.
        case BrnGameState::GameStateModuleIO::E_ACTION_SHOW_MODE_RESULTS:
        {
            const BrnGameState::GameStateModuleIO::ShowModeResultsAction* lpResults =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::ShowModeResultsAction*>(lpAction);

            bool lbBuildResultsRecord = IsShowtimeMode(static_cast<s32>(lpResults->meGameModeType));

            if (lpResults->mbIsOnlinePostEvent != 0)
            {
                TriggerOnlinePostEventWire320 lOnline;
                lOnline.mu8Flag = lpResults->mu8FieldE1;
                PushGuiEvent(lOnline, lpGuiInput);
            }
            else
            {
                PostEventRequestWire291 lOffline;
                lOffline.mu8Unused = 0;
                PushGuiEvent(lOffline, lpGuiInput);
                lbBuildResultsRecord = true;                    // `mr r27, r14` @0x823EA9D8
            }

            if (lbBuildResultsRecord)
            {
                OfflinePostEventWire289 lEvent;
                std::memset(&lEvent, 0, sizeof(lEvent));
                lEvent.mu64Field00    = lpResults->mu64FieldC8;
                lEvent.meGameModeType = static_cast<s32>(lpResults->meGameModeType);
                lEvent.miField1C      = lpResults->miField08;
                lEvent.mfField20      = lpResults->mfField0C;
                lEvent.miField24      = lpResults->miField10;
                lEvent.miField28      = lpResults->miField14;
                lEvent.mfField2C      = lpResults->mfField18;
                lEvent.miFieldA0      = lpResults->miField48;
                lEvent.miFieldA4      = lpResults->miField4C;
                lEvent.mu8FieldB0     = static_cast<u8>(lpResults->miFinishPosition);
                lEvent.mu8FieldB1     = lpResults->mu8FieldE0;
                lEvent.mu8FieldB2     = lpResults->mu8FieldE2;
                lEvent.mu8FieldB3     = lpResults->mu8FieldE1;
                lEvent.mu8FieldB4     = lpResults->mu8FieldE3;
                lEvent.mu8FieldB5     = lpResults->mu8FieldDA;
                lEvent.mu8FieldB6     = lpResults->mu8FieldDB;
                lEvent.mbHasField08   = lpResults->mbHasField40;
                lEvent.mu8FieldB8     = lpResults->mu8FieldDC;
                // Unconditional console store BEFORE the gated block copy: `li r10,-1;
                // stw r10, base+0x98` @0x823EA9EC..0x823EA9F4. +0x98 falls inside maBlock30
                // (index 0x68), so when mbHasBlock58 is set the memcpy overwrites it -- but on
                // the no-block path the console posts -1 there, not the memset's 0.
                {
                    const s32 liMinusOne = -1;
                    std::memcpy(&lEvent.maBlock30[0x68], &liMinusOne, sizeof(liMinusOne));
                }
                // The two gated copies in the console's own order (@0x823EAAE0 then @0x823EAB00):
                // the 0x70 block first -- on the console it OVERWRITES two frame words the arm
                // had already written (payload+0x70 and payload+0x98 fall inside the memcpy
                // range), which is why the order matters and is reproduced.
                if (lpResults->mbHasBlock58 != 0)
                {
                    std::memcpy(lEvent.maBlock30, lpResults->maBlock58, 0x70);
                }
                if (lpResults->mbHasField40 != 0)
                {
                    lEvent.mu64Field08 = lpResults->mu64Field40;
                }
                PushGuiEvent(lEvent, lpGuiInput);
            }

            if (lpResults->mbIsOnlinePostEvent == 0)
            {
                AutosaveRequestWire356 lAutosave;
                lAutosave.mu8Zero = 0;
                PushGuiEvent(lAutosave, lpGuiInput);
            }

            SetEasyDriveNotAllowedWire96 lEasyDrive;
            lEasyDrive.mu8Zero = 0;
            PushGuiEvent(lEasyDrive, lpGuiInput);

            if ( sbDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                --siDiagLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[evt-flow] action 37 -> gui "
                    << (lpResults->mbIsOnlinePostEvent != 0 ? 320 : 291)
                    << (lbBuildResultsRecord ? " + 289" : "")
                    << " (mode " << static_cast<s32>(lpResults->meGameModeType) << ")\n";
            }
            return true;
        }

        default:
            // Not an event-flow action -- the caller keeps its own default behaviour.
            return false;
        }
    }
} // namespace BrnGame
