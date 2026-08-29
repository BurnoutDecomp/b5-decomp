#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Vector3, CgsID, EntityId
#include "GameSource/BurnoutConstants.h"                     // EActiveRaceCarIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // EPlayerScoringIndex, EPlayerTeam
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // BrnGameState::GameModeParams (PrepareForModeAction payload)
#include "GameShared/GameClasses/Containers/CgsArray.h"      // Array<T, N> (SetUpAllDriveThrusAction::maDriveThrus)
#include "GameSource/GameState/Offences/BrnDriveThruManager.h"  // BrnTrigger::GenericRegion::Type (DriveThruInfo::meType)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"     // CgsSystem::Time (OnlineGameResults::mSecondsInEvent / maRoundTimes)
#include "GameSource/GameState/BrnGameStateTypes.h"           // BrnGameState::StuntElementType (WorldStuntAction / OnStuntElementCompleteAction)
#include "SharedClasses/World/BrnWorldRegion.h"               // [gateui] BrnWorld::ECounty (OnStuntElementCompleteForCountyAction)
#include "SharedClasses/Progression/BrnTrainingTypes.h"       // [stuntrace] BrnProgression::ETrainingType (RequestGameTrainingAction)
#include "SharedClasses/Progression/BrnTrophyUnlockData.h"    // BrnProgression::TrophyUnlockData (TrophyUnlockAction::meUnlockType) -- see the note below

namespace BrnResource
{
enum ECarType : int;
}

// Owning header for the BrnGameState::GameStateModuleIO GameAction<> family slices reconstructed
// by the GameMode/ModeManager leaf batch. Each struct is a minimal slice: only the members the
// reconstructed body touches are declared. The GameAction<T> base is modelled as an empty
// template spine (the real build adds only a static type tag, no instance data/vtable); its real
// Event base + the per-action mseType definitions land with the full BrnGameActions TU.

// ⭐ [drive-thru wave 2026-08-27] THE PLACEHOLDER IS RETIRED. This used to declare a local
// `struct TrophyUnlockData { enum UnlockType { E_UNLOCKTYPE_NONE, E_UNLOCKTYPE_COUNT = 35 }; };`
// -- a HOLLOW SHELL with the enum tag and no members, whose own comment claimed it was "a
// complete 16-byte type". An empty struct is ONE byte, so the bodied
// ProgressionData::GetTrophyUnlock(i) (`&GetTrophyUnlocks()[luIndex]`) was striding the
// serialised table by 1 instead of 16. The real record now has its DWARF home; include it.

// Provisional enum home for PowerParkResultAction::meOutcome. The committed owner is
// BrnWorld::EPowerParkOutcome, whose real home (DWARF
// World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h:11) has not been
// reconstructed yet -- there is no PowerParking TU in b5-decomp. The full enum is mirrored here
// (X360 DealWithPowerPark @0x82321530 only tests meOutcome == E_PPO_SUCCESS(1), but the complete
// 4-value set is DWARF-confirmed) so PowerParkResultAction is a complete type. Re-home / replace
// this with an #include of BrnPowerParkingManager.h when that TU lands; do not let two definitions
// coexist (this is provisional only).
namespace BrnWorld
{
enum EPowerParkOutcome
{
    E_PPO_TO_BE_DETERMINED = 0,
    E_PPO_SUCCESS          = 1,
    E_PPO_FAILURE          = 2,
    E_PPO_COUNT            = 3,
};
}

namespace BrnGameState
{
namespace GameStateModuleIO
{
// EGameActionType discriminant. Only the slots this batch instantiates are listed (the full enum
// is its own TU). The two unconfirmed values are placeholders used purely as template tags.
enum EGameActionType
{
    E_ACTION_RESET_PLAYER_CAR           = 0,
    E_ACTION_REMOTE_PLAYER_DISCONNECTED = 11,
    E_ACTION_SOUND_TRIGGER              = 210,
    E_ACTION_ONLINE_PLAYER_ADDED        = 211,   // DWARF BrnGameActions.h (was placeholder 220)
    E_ACTION_SETUP_NETWORK_CAR          = 5,     // DWARF BrnGameActions.h (was placeholder 221)
    E_ACTION_ONLINE_PLAYER_REMOVED      = 212,   // DWARF BrnGameActions.h (was placeholder 222)
    // [!!] VALUE CORRECTION 2026-08-26 (stuntrace waveB CLOSURE round) -- 221 -> 229, and this is
    // the FIFTH id of the same species as the four the closure round was chartered to settle. It
    // was found because THIS FILE ALREADY CONTRADICTED ITSELF: the band table further down (the
    // "DWARF 135..226 -> +8" line) lists "ONLINE_GAME_RESULT 221->229" as one of its witnesses,
    // while the enumerator here still carried the raw PS3-DWARF 221 (dwarfdump
    // .../BrnGameActions.h:231 `E_ACTION_ONLINE_GAME_RESULT = 221`).
    // The X360 value is PRODUCER-PINNED, re-derived this pass rather than inferred from the shift:
    //     BrnGameState::ModeManager::SendGameResultsToNetwork @0x82343E88
    //       0x82343F08  bl  BrnGameState__GameStateModuleIO__OnlineGameResults__...  ; fills var_120
    //       0x82343F0C  li  r6, 0x108      (264)
    //       0x82343F10  li  r5, 0xE5       (229)
    //       0x82343F14  addi r4, r1, 0x170+var_120                                   ; that record
    //       0x82343F1C  bl  CgsModule__VariableEventQueue_13312_16___AddEvent
    // The record handed to AddEvent is the OnlineGameResults buffer the preceding call just wrote,
    // so id and record identity are attested at one site -- the same class of proof that pinned
    // E_ACTION_ONLINE_ROUND_RESULT (230) and E_ACTION_SET_UP_ALL_DRIVE_THRUS (45). DWARF 221 + 8,
    // exactly the shift its neighbour ONLINE_ROUND_RESULT (222 -> 230) carries.
    // [!] SIZE MISMATCH NOTED, NOT PAPERED OVER: the console posts 264 (0x108) while
    // `struct OnlineGameResults` below documents sizeof == 260 (65 u32 words, +0xDC + 40 == 0x104).
    // That 4-byte delta is a TAIL-PAD question for the record's owner (the same shape as
    // StartModeIntroAction's 603 -> 604), not evidence against the id: 260 is not any other
    // record's size either. Left as a flagged residue.
    // 229 was unoccupied in this enum (checked: no duplicate values remain) and NO consumer
    // anywhere in src/ dispatches on this enumerator -- grep-verified, its only uses are the
    // record's own GameAction<> tag and BrnGameStateSharedIO.h's re-home note -- so the move is a
    // pure value correction with no call-site churn.
    E_ACTION_ONLINE_GAME_RESULT         = 229,   // DWARF :231 gives 221 (+8 X360); producer-pinned, size 264
    // ⛔ VALUE CORRECTION 2026-08-26 (stuntrace waveB fix round) -- was the PS3-DWARF 222.
    // X360-attested at BOTH ends: ModeManager::SendModeStopMessages @0x8234BEC0 posts
    // `li r6,0x44` (68) + `li r5,0xE6` (230) @0x8234C3EC/0x8234C3F0, and 68 is exactly
    // sizeof(OnlineRoundResults) below -- the size match is what makes this a proof rather
    // than a shift inference. DWARF 222 + 8, the same shift this enum already records for the
    // freeburn-challenge block and REQUEST_GAME_TRAINING (141 -> 149).
    E_ACTION_ONLINE_ROUND_RESULT        = 230,   // DWARF 222 (+8 X360); size 68
    // ⛔ VALUE CORRECTION 2026-08-28 (driver-details pause wave) -- was the raw PS3-DWARF value
    // (173), which is not what the X360 posts or consumes. PRODUCER-PINNED AND CONSUMER-PINNED,
    // with the size matching at both ends:
    //   producer  GameStateModule::ProcessGameEvents @0x823A0A18, `jumptable 823A107C case 80`
    //             @0x823A2D54: after SetProgressionRanks + SetProgressionRankEventWins it posts
    //               0x823A2E50  li   r6, 0x24            (36)
    //               0x823A2E54  li   r5, 0xB5            (181)
    //               0x823A2E58  addi r4, r1, var_1A00    (the RankInfoResponseAction it just built)
    //               0x823A2E60  bl   VariableEventQueue<13312,16>::AddEvent
    //   consumer  BrnGameModule::TranslateGameActionsToGuiEvents @0x823E9CE0,
    //             `jumptable 823EA1F0 case 181` @0x823ECC90 -> GuiEventRankProgressResponse::
    //             Construct(&event, action) + AddGuiEvent (GUI event 438, also size 36).
    // 36 == sizeof(RankInfoResponseAction) below (9 s32 words), so the id and the record identity
    // are attested at one site -- the same class of proof that pinned E_ACTION_TROPHY_UNLOCK (204)
    // and E_ACTION_ONLINE_GAME_RESULT (229). DWARF 173 + 8, exactly the shift the whole 135..226
    // band takes. 181 was unoccupied (grep-verified) and nothing in the tree dispatched on 173, so
    // the correction is call-site-free -- but the producer this wave adds would have posted 173 and
    // the consumer it adds would never have heard it.
    E_ACTION_RANK_INFO_RESPONSE         = 181,   // DWARF :183 gives 173 (+8 X360); size 36
    // ⭐⭐ [pause-stats wave 2026-08-29] THE GAME-STATS RESPONSE -- 181's immediate sibling, and
    // pinned the same way, at the same site, with the same size cross-check. TAKEN FROM THE
    // `li r5,<id>` / `li r6,<size>` PAIR AT THE POST SITE, NOT FROM THE DWARF (which gives 172;
    // the previous wave's finding was that the raw DWARF id here is one no X360 consumer hears):
    //   producer  GameStateModule::ProcessGameEvents @0x823A0A18, `jumptable 823A107C case 79`
    //             @0x823A2D18: CountCompletedChallenges -> GetGameStats -> then
    //               0x823A2D3C  li   r6, 0x160           (352)
    //               0x823A2D40  li   r5, 0xB4            (180)
    //               0x823A2D44  addi r4, r1, var_E30     (the GameStats it just filled)
    //               0x823A2D4C  bl   VariableEventQueue<13312,16>::AddEvent
    //   consumer  BrnGameModule::TranslateGameActionsToGuiEvents @0x823E9CE0,
    //             `jumptable 823EA1F0 case 180` @0x823EC8A0 -> the inlined
    //             GuiEventStatsResponse build + AddGuiEvent (GUI event 436, 432 bytes).
    // 352 == sizeof(BrnGameState::GameStateModuleIO::GameStats) (BrnGameActionData.h), so the id
    // and the record identity are attested at one site.
    // ⓘ NO WRAPPER STRUCT BELOW, DELIBERATELY. The DWARF declares
    // `GameStatsResponseAction : GameAction<E_ACTION_GAME_STATS_RESPONSE> { GameStats mGameStats; }`
    // (BrnGameActions.h:2502) -- but GameAction<N> carries no data, so the wrapper is
    // layout-transparent, and the X360 producer never materialises one: case 79 hands
    // GetGameStats a bare stack GameStats (`addi r4, r1, var_E30`) and queues THAT pointer.
    // Adding the wrapper would force BrnGameActionData.h into this header for no byte's
    // difference.
    E_ACTION_GAME_STATS_RESPONSE        = 180,   // DWARF gives 172 (+8 X360); size 352
    // ⛔ VALUE CORRECTION 2026-08-27 (drive-thru link-closure wave) -- was the raw PS3-DWARF
    // value (196), which is not what the X360 posts. PRODUCER-PINNED at both ends:
    // ProgressionManager::SendTrophyUnlockUpdate @0x823892B8 posts `li r5, 0xCC` (204) with
    // `li r6, 0x10` (16) @0x82389384/0x82389388, and 16 is exactly sizeof(TrophyUnlockAction)
    // below -- the size match is what makes this a proof rather than a shift inference. It is
    // also DWARF 196 + 8, the same +8 the whole 200-band takes (UPDATE_PLAYER_MEDALS 192->200,
    // EVENT_AT_JUNCTION_AVAILABLE 193->201, ALL_EVENTS_DISCOVERED 194->202).
    // Nothing in the tree posted or consumed it by this constant, so the correction is
    // call-site-free -- but a future producer would have posted 196 and no consumer would have
    // heard it.
    E_ACTION_TROPHY_UNLOCK              = 204,   // DWARF 196 (+8 X360); size 16
    // ⛔ VALUE CORRECTION 2026-08-26 (stuntrace waveB fix round) -- this carried the raw PS3-DWARF
    // value (31), which matches NEITHER X360 action. The X360 SPLITS the DWARF's single
    // E_ACTION_FINISHED_MODE into two, and both were re-dumped from the exports this pass:
    //   35 (0x23) size 1  -- ModeManager::SendFinishedModeAction @0x82343628
    //                        (`li r6,1` @0x823436A0 + `li r5,0x23` @0x823436A4): a payload-free
    //                        "the mode finished" notification. Its enumerator NAME is UNRECOVERED,
    //                        so it is spelled with a FLAG rather than given a guessed DWARF name.
    //   36 (0x24) size 48 -- ModeManager::SendModeResults @0x82343438
    //                        (`li r6,0x30` + `li r5,0x24`; the IDA pseudocode renders the call as
    //                        `AddEvent(a2, &v19, 36, 48)`): the 48-byte results record.
    // `struct FinishedModeAction` below IS the 48-byte one (its consumer,
    // BrnNetworkStandingsManager.cpp:233, reads mFinishTime / mfDistanceFromFinish /
    // meEliminatorIndex), so E_ACTION_FINISHED_MODE takes 36. Inert today -- the tag is a template
    // parameter and nothing dispatches on 31 -- but at 31 no consumer case could ever match.
    E_ACTION_FINISHED_MODE              = 36,    // DWARF :41 gives 31 (+5 X360); size 48
    E_ACTION_FINISHED_MODE_NOTIFY       = 35,    // FLAG: name unrecovered; X360 size 1
    // The DecFIGS values are 30/65/66.  ARTIST's HandleGameActions jump table
    // pins the merged-X360 ids at 34/70/71 respectively (0x8230C7A0,
    // 0x8230C3D8 and 0x8230C40C).
    E_ACTION_START_PLAYING_MODE         = 34,
    // [stuntrace waveB fix round, 2026-08-26] The mode-INTRO pair. Both re-derived from the
    // exports this pass, not taken from the implementer report:
    //   29 (0x1D) size 604 (0x25C) -- ModeManager::StartModeIntro @0x82343018:
    //      `li r6,0x25C` @0x823432D4 + `li r5,0x1D` @0x823432D8 into
    //      VariableEventQueue<13312,16>::AddEvent @0x823432E4.  DWARF :35 == 25  (+4 X360)
    //   30 (0x1E) size 8        -- ModeManager::StopModeIntro  @0x82343F38:
    //      `li r6,8` @0x82343FC4 + `li r5,0x1E` @0x82343FCC into AddEvent @0x82343FD8.
    //      DWARF :36 == 26  (+4 X360)
    // +4 is the SAME window this enum already records for PREPARE_FOR_MODE (DWARF 19 -> 23) and
    // START_PLAYING_MODE (DWARF 30 -> 34, `li r5,0x22` @0x82343400), which bracket it. Do NOT
    // extrapolate +4 past this window -- the mode-lifecycle names above it are +5.
    E_ACTION_START_MODE_INTRO           = 29,    // DWARF 25  (+4 X360)
    E_ACTION_STOP_MODE_INTRO            = 30,    // DWARF 26  (+4 X360)
    E_ACTION_ALLOW_BOOST_EARNING        = 70,
    E_ACTION_STOP_BOOSTING              = 71,
    // ⭐ [drive-thru wave 2026-08-27] The three shop drive-thru posts, all size 144 (0x90).
    // PINNED at both ends. Producer: DriveThruManager::ProcessDriveThru @0x8239B6E8 --
    // `li r5,0x64`/`li r5,0x61`/`li r5,0x62` with `li r6,0x90` into
    // VariableEventQueue<13312,16>::AddEvent (gas @0x8239B9A0, body @0x8239BB98, paint
    // @0x8239BD9C). Consumers (an exhaustive sweep of xrefs_to GetFirstEvent @0x821FC0A0 --
    // 24 functions, 11 of which react to 97/98/99/100): RaceCarEntityModule::HandleGameActions
    // @0x8230BE08 (100 -> boost strategy slot 46; 97/98 -> per-car state + colour),
    // PhysicsModule::HandleGameActionsPostScene @0x825A70C0 (97 -> the deformation reset),
    // MainDirector::ProcessInputQueue @0x822372F8 (the drive-thru camera),
    // TranslateGameActionsToGuiEvents @0x823E9CE0 (GuiDriveThroughEvent type 1/2/3),
    // BridgeGameStateToNetwork @0x823E2398 (net msg 37/36/41), the sound logic module and the
    // traffic module. The 144-byte payload layout is documented at PostShopAction in
    // BrnDriveThruManager.cpp (transform @+0, identity matrix @+64, per-action scalars @+128).
    E_ACTION_BODY_SHOP_DRIVE_THRU       = 97,   // size 144
    E_ACTION_PAINT_SHOP_DRIVE_THRU      = 98,   // size 144
    E_ACTION_GAS_STATION_DRIVE_THRU     = 100,  // size 144
    // ⭐ [drive-thru wave 2026-08-29] PINNED AT BOTH ENDS, same standard as the three above.
    // Producer: DriveThruManager::ProcessDriveThru @0x8239B6E8's mbIsClosed early-out --
    // `li r5,0x65` (101) + `li r6,1` into the same VariableEventQueue<13312,16>::AddEvent, one
    // zero byte of payload. Consumer: TranslateGameActionsToGuiEvents @0x823E9CE0 case 101
    // @0x823EB628 -- `li r11,5` / `stb r19` (r19 == 0) into a GuiDriveThroughEvent, i.e.
    // {E_DRIVE_THROUGH_TYPE_FAILED, mbEffective = false} on GUI id 366. It is the ONLY producer
    // of the analyzer's FAILED drive-thru message. Named here (rather than only as
    // BrnDriveThruManager.cpp's TU-local KI_ACTION_STOP_DRIVE_THRU_PRES) now that both ends
    // exist in the tree.
    E_ACTION_STOP_DRIVE_THRU_PRESENTATION = 101, // size 1
    // ⛔ VALUE CORRECTION 2026-08-20 -- this carried the PS3-DWARF value (19). The X360 ARTIST
    // build posts 23, asm-pinned at BOTH ends:
    //   producer  ModeManager::PrepareForMode @0x82342930 -- `li r5,0x17` (23) + `li r6,0x8E0`
    //             (2272 == this record's size) into VariableEventQueue<13312,16>::AddEvent,
    //             asm 0x82342E80..0x82342E90.
    //   consumers RaceCarEntityModule::HandleGameActions @0x8230BE08 `case 23` ->
    //             HandlePrepareForModeAction @0x823092F0 (the ONLY setter of the module's
    //             mbIsInGameMode, +99140); TrafficEntityModule::HandlePrepareForModeAction
    //             @0x827480D8; MainDirector::HandlePrepareForModeAction @0x8221B0B0;
    //             NetworkRoadRulesManager::ProcessGameActions @0x8255CA48 (`liActionID == 23`);
    //             and WorldBridgeInputToEntityModules' prop arm, which already hardcodes 23.
    // Same species of PS3->X360 shift this enum already records for the stunt block (+5),
    // CAR_SELECT_CHANGE_COLOUR (74 -> 79) and the freeburn-challenge block (+8). Inert today
    // (nothing posts through the tag yet) but it is the id the whole gameplay-arming chain
    // turns on: at a wrong value no consumer's case would ever match.
    E_ACTION_PREPARE_FOR_MODE           = 23,    // DWARF :29 gives 19 -- PS3 value (X360-attested 23)
    // [!!] VALUE CORRECTION 2026-08-26 (stuntrace waveB CLOSURE round) -- 40 -> 45, and THE
    // PRODUCER IS NOW LOCATED, so this is a PIN and no longer the "shift is an inference" park
    // the previous pass left here. Method: an exhaustive sweep of every `AddEvent` call site in
    // the 30,095-export dump, filtered to the GameState queue
    // (CgsModule::VariableEventQueue<13312,16>), reading the live `li r5` / `li r6` immediates.
    // Exactly ONE site in the whole image posts the drive-thru table:
    //   BrnGameState::GameStateModule::SendSetUpAllDriveThrusMessage
    //     `li r6, 0x458` (1112) + `li r5, 0x2D` (45) -> AddEvent @0x82381CC0.
    // The PRODUCER SYMBOL NAME IS THE ENUMERATOR NAME -- the same class of proof that pinned
    // E_ACTION_UPDATE_PLAYER_MEDALS (200) and E_ACTION_ONLINE_ROUND_RESULT (230) -- and 1112 is
    // 46 * sizeof(DriveThruInfo) (24) + the Array<> count word and its tail pad, i.e. exactly
    // SetUpAllDriveThrusAction. DWARF 40 + 5, the same shift the mode-lifecycle block below
    // records (QUIT_MODE_OFFLINE 35 -> 40, QUIT_MODE_ONLINE 36 -> 41, IMPACT_TIME_END 38 -> 43).
    // The seat this vacates at 40 is the ONE-BYTE offline quit ModeManager::SendModeStopMessages
    // @0x8234BEC0 posts (`li r6,1` @0x8234C654 + `li r5,0x28` @0x8234C664, the byte sourced from
    // NetworkRoundManager+0x130) -- enumerated below as E_ACTION_QUIT_MODE_OFFLINE.
    // No consumer anywhere in src/ dispatches on this enumerator (grep-verified this pass: the
    // only references are the record type and its Array<> leaf instantiation), so the move is a
    // pure value correction.
    E_ACTION_SET_UP_ALL_DRIVE_THRUS     = 45,    // DWARF :40 (+5 X360); size 1112, producer-pinned
    // ⛔⛔ [gateui] VALUE CORRECTION 2026-08-20 -- THE WHOLE STUNT BLOCK WAS CARRYING DWARF (PS3)
    // VALUES, and the X360 ARTIST build shifts this range by EXACTLY +5. Both ends measured:
    //   producer StuntManager::ProcessStuntElement @0x8239CDB0 posts, in body order,
    //     `li r5,0x7F` (127) size 16 @0x8239CE30/0x8239CE2C  -- WORLD_STUNT_PERFORMED
    //     `li r5,0x3D` (61)  size 4  @0x8239D1A0             -- STUNT_ELEMENT_BOOST
    //     `li r5,0x3A` (58)  size 24 @0x8239D384             -- ON_STUNT_ELEMENT_COMPLETE
    //     `li r5,0x3B` (59)  size 8  @0x8239D3D0             -- ..._COMPLETE_FOR_COUNTY
    //     `li r5,0x3C` (60)  size 4  @0x8239D430             -- ..._COMPLETE_BY_TYPE
    //   consumer BrnGameModule::TranslateGameActionsToGuiEvents @0x823E9CE0 switches on
    //     case 58 -> GuiEventStuntInfo/BoostBarStuntInfo, 59 -> StuntAreaComplete,
    //     60 -> StuntAllComplete, 127 -> (Showtime only) CrashModeScoring::DealWithShowtimeStunt.
    // The DWARF (DecFIGS, PS3) values are ON_STUNT_ELEMENT_COMPLETE 53 (:63) and
    // WORLD_STUNT_PERFORMED 122 (:132) -- exactly 5 lower. Same +5 shift this enum already records
    // for CAR_SELECT_CHANGE_COLOUR / NEW_CAR_UNLOCKED / CAR_UNLOCK_END below.
    // ⓘ Game EVENT ids are NOT shifted (E_EVENT_RECORD_PROP_HIT == 111 matches the X360
    //   ProcessGameEvents jump table exactly); this shift is the ACTION enum only.
    // The two were inert until this wave (GameAction<T> is an empty tag and nothing posted through
    // the enum); ProcessStuntElement posts through them now, so a wrong value is a live defect.
    E_ACTION_ON_JUMP_START              = 56,    // X360 UpdateJumps @0x8239D460 (`li r5,0x38`, size 24)
    E_ACTION_SHOW_JUMP_NAME             = 57,    // X360 UpdateJumps @0x8239D460 (`li r5,0x39`, size 8)
    E_ACTION_ON_STUNT_ELEMENT_COMPLETE  = 58,    // X360 0x8239D384 (DWARF :63 gives 53 -- PS3 value)
    E_ACTION_ON_STUNT_ELEMENT_COMPLETE_FOR_COUNTY = 59,  // X360 0x8239D3D0, size 8
    E_ACTION_ON_STUNT_ELEMENT_COMPLETE_BY_TYPE    = 60,  // X360 0x8239D430, size 4
    E_ACTION_STUNT_ELEMENT_BOOST        = 61,    // X360 0x8239D1A0, size 4
    E_ACTION_WORLD_STUNT_PERFORMED      = 127,   // X360 0x8239CE30 (DWARF :132 gives 122 -- PS3 value)
    E_ACTION_POWER_PARK_RESULT          = 139,   // DWARF BrnGameActions.h:149

    // X360-ATTESTED value (NOT a DWARF-only import -- both ends agree on 15):
    //   producer  GameStateModule::ProcessGameEvents @0x823A0A18, the E_EVENT_COMPLETED_STUNT
    //             (case 119) arm: `li r5, 0xF` + `li r6, 0x20` @0x823A1964/0x823A195C into
    //             VariableEventQueue<13312,16>::AddEvent @0x823A19A0.
    //   consumer  RaceCarEntityModule::HandleGameActions @0x8230BE08 `case 15`, which asserts
    //             "lpCompletedStuntAction != NULL" (BrnRaceCarEntityModule.cpp:6744) and then
    //             calls vtable +0xC0 (== slot 48, BoostStrategy::UpdateStuntBoost) on the boost
    //             manager, handing it the event payload unchanged.
    // The PS3 DWARF enumerator is also 15 (BrnGameActions.h:25), i.e. no X360 shift here --
    // consistent with every other sub-53 slot in this enum.
    E_ACTION_COMPLETED_STUNT            = 15,    // DWARF BrnGameActions.h:25 (X360-attested)
    // DecFIGS value 190 shifted to ARTIST 198. HandleGameActions' high jump
    // table reads the 24-byte SendCarStatsAction below.
    E_ACTION_UPDATE_CAR_STATS           = 198,

    // X360-ATTESTED value: the DWARF (PS3) enumerator is 74, but every X360 producer posts
    // `li r5, 0x4F` (79) with size 8, and the X360 consumer is HandleGameActions' `case 79`.
    // That is the SAME +5 shift this enum already records for NEW_CAR_UNLOCKED (DWARF 57 ->
    // X360 62) and CAR_UNLOCK_END (DWARF 58 -> X360 63).
    E_ACTION_CAR_SELECT_CHANGE_COLOUR   = 79,    // DWARF 74 (+5 X360)

    // ⭐ [tut-ticker] X360-attested pair (2026-08-24):
    //   77  -- "the player's car settled out of car-select": HandleGameActions case 77 reads the
    //          ACTIVE player car's VehicleListEntry and refreshes the boost strategy + queues the
    //          DRIVES_*_CAR training tip. Producer: CarSelectManager::UpdateExitState (the 32-byte
    //          post its own KI_ enum mislabels ALLOW_BOOST_EARNING -- the console case ignores the
    //          payload entirely).
    //   149 -- "request a training tip": the 4-byte ETrainingType relay into the world's
    //          per-car training ring (HandleGameActions case 149 -> AddTrainingRequest); posted by
    //          CarSelectManager::UpdateExitState (payload 0 == LEAVES_JUNKYARD),
    //          ModeManager::UpdateCurrentMode/StartModeIntro, ScoringSystem, StreetManager et al.
    E_ACTION_CAR_SELECT_FINISHED        = 77,    // size 32 -- payload unread by the consumer
    E_ACTION_REQUEST_GAME_TRAINING      = 149,   // size 4  -- the ETrainingType

    // Freeburn-challenge action block (ChallengeManager keystone). X360-ATTESTED values:
    // the PS3-DWARF block is 145..153, but every ChallengeManager AddEvent callsite posts
    // id == DWARF+8 with the byte size matching the DWARF struct exactly (update action
    // id 155 size 0x94, success-update id 158 size 0x10, success id 159 size 0xC,
    // completion-status id 156 size 0x108, every-player id 157 size 0x838, show-selector
    // id 160 size 8, active-challenge id 161 size 0x30, end-not-active id 154 size 1,
    // challenge id 153 size 0x20).
    E_ACTION_FREEBURN_CHALLENGE                          = 153,  // DWARF 145 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE           = 154,  // DWARF 146 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_UPDATE                   = 155,  // DWARF 147 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_COMPLETION_STATUS        = 156,  // DWARF 148 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_EVERY_PLAYER_COMPLETION_STATUS = 157, // DWARF 149 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_SUCCESS_UPDATE           = 158,  // DWARF 150 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_SUCCESS                  = 159,  // DWARF 151 (+8 X360)
    // [!!] VALUE CORRECTION 2026-08-26 (stuntrace waveB CLOSURE round) -- 262 -> 275, PINNED.
    // The previous comment said "the X360 discriminant is owned by the GameStateModule dispatcher
    // that posts the query". That dispatcher has now been read:
    //   BrnGameState::GameStateModule::ProcessGameEvents @0x823A0A18, jumptable case 98 ->
    //     0x823A4970  bl StreetManager::FillInRoadRulesQuery   ; fills the record at r1+var_990
    //     0x823A4974  li r6, 0x308      (776)
    //     0x823A4978  li r5, 0x113      (275)
    //     0x823A4984  bl VariableEventQueue<13312,16>::AddEvent
    // 776 IS sizeof(RoadRulesBatchQueryAction) below (64*8 ids + 4 + 4*64 bools + 1, padded to 8),
    // and the record handed to AddEvent is the very buffer FillInRoadRulesQuery just wrote -- so
    // the id, the size and the record identity are all attested at one site.
    // The +13 shift is not extrapolated either: the WHOLE road-rules band is producer-symbol
    // matched at +13 in the same sweep -- RoadRulesManager::OnEnterRoad 273 (DWARF 260),
    // OnLeaveRoad 274 (261), StreetManager::SendUpcomingRoadMessage 276 (263),
    // RoadRulesManager::OnStartRule 277 (264), OnEndRule 278 (265),
    // OnUpdateActiveRoadScores 279 (266), SendActiveRuleState 282 (269),
    // UpdateTimeRule 283 (270). Nine independent witnesses, this one included.
    // [!] The X360's 262 belongs to something else entirely -- ModeManager::UpdateCurrentMode
    // posts a ONE-BYTE payload there (@0x823515E0); it is enumerated below with a FLAGGED name.
    // No consumer dispatches on this enumerator today (the filler takes the record by pointer),
    // so the move is a pure value correction.
    E_ACTION_ROAD_RULES_BATCH_QUERY                      = 275,  // DWARF 262 (+13 X360); size 776
    // ⭐ [stuntrace wave D, D3] "you are in the wrong car for this challenge". Producer
    // GameStateModule::StartModeAtLights @0x82396CF8: when the junction's RaceEventData carries a
    // mSpecialEventCarId (+0x10) that does not match GetOriginalCarId(mActivePlayerCarId), it posts
    // the 8-byte record and RETURNS WITHOUT STARTING ANYTHING --
    //     0x82396F74  std   r11, 0x430+var_3E0(r1)   ; the event's mSpecialEventCarId
    //     0x82396F80  li    r6, 8                    ; size
    //     0x82396F7C  li    r5, 0x110                ; == 272
    //     0x82396F88  bl    VariableEventQueue<13312,16>::AddEvent
    // VALUE is asm-attested; the NAME is the DWARF's own (BrnGameActions.h:269
    // E_ACTION_WRONG_CAR_FOR_CHALLENGE == 259) carried across the +13 road-rules band this enum
    // already records at nine independent witnesses -- 259 + 13 == 272 exactly, and the semantics
    // of the producer and of the DWARF name are the same event.
    E_ACTION_WRONG_CAR_FOR_CHALLENGE                     = 272,  // DWARF 259 (+13 X360); size 8
    E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR            = 160,  // DWARF 152 (+8 X360)
    E_ACTION_ACTIVE_FREEBURN_CHALLENGE                   = 161,  // DWARF 153 (+8 X360)

    // ⭐⭐ [evt-flow wave E1, 2026-08-26] THE EVENT-FLOW ACTION BLOCK. Every value below is
    // X360-attested at BOTH ends -- a producer that posts the literal id with the literal size,
    // and the TranslateGameActionsToGuiEvents @0x823E9CE0 jump-table case that consumes it
    // (jpt_823EA1F0; the arms live in GameBridgeGameStateToX_EventFlowGuiEvents.cpp).
    //
    // ⓘ THE SHIFT IS NOT ONE CONSTANT ACROSS THE ENUM. This enum already records +5 for the
    // stunt block, +8 for the freeburn-challenge block and +4 for PREPARE_FOR_MODE. Measured
    // again here: the mode-lifecycle names (DWARF 32/33/34/39/42) are at DWARF+5, and the
    // progression names (DWARF 192/193) are at DWARF+8. Do NOT extrapolate a shift -- pin
    // each value from the asm, which is what the per-line citations below do.
    //
    //   37  producer BrnGameState::ModeManager::ShowModeResults      @0x823436D0 (id 37, size 232)
    //       consumer @0x823EA984 -> GuiEventOfflinePostEvent(289) / GuiEventTriggerOnlinePostEvent(320)
    //       name from the producer symbol; DWARF :42 E_ACTION_SHOW_MODE_RESULTS == 32 (+5).
    //   38  producer BrnGameState::ModeManager::PreWorldUpdate       @0x823537B8 (id 38, size 1)
    //       consumer @0x823EAD20 -> GuiEventFinishedModeResults(321). ⭐ THE NAME IS IMAGE-CITED:
    //       the consumer's own debug line is the string literal
    //       "***** GameBridgeGameStateToX found E_ACTION_FINISHED_MODE_RESULTS *****\n"
    //       (loaded into var_33A4 @0x823EA130), i.e. the binary spells this enumerator out.
    //       DWARF :43 E_ACTION_FINISHED_MODE_RESULTS == 33 (+5).
    //   39  producer BrnGameState::ModeManager::SendModeStopMessages @0x8234BEC0 (id 39, size 24)
    //       consumer @0x823EABCC -> GuiEventStopMode(322). DWARF :44 E_ACTION_STOP_MODE == 34 (+5).
    //   44  producer BrnGameState::ModeManager::StartModeIntro       @0x82343018 (id 44, size 4)
    //       (also GameStateModule::ProcessGameEvents @0x823A0A18, same id/size)
    //       consumer @0x823EA948 -> GuiEventEnterEventStartLocation(166).
    //       DWARF :49 E_ACTION_SET_IN_MODE_START_REGION == 39 (+5).
    //   47  producer BrnGameState::ModeManager::CheckCountdownDisplay@0x82342898 (id 47, size 4)
    //       consumer @0x823EAD50 -> GuiEventUpdateEventCountdown(234).
    //       DWARF :52 E_ACTION_SET_COUNTDOWN == 42 (+5).
    //  200  producer BrnProgression::ProgressionManager::UpdatePlayerMedals @0x8239FE50 (id 200, size 8)
    //       consumer @0x823EA784 -> GuiEventMedalUpdate(307).
    //       DWARF :192 E_ACTION_UPDATE_PLAYER_MEDALS == 192 (+8) -- the producer symbol name IS
    //       the enumerator name.
    //  201  producers BrnGameState::GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent
    //       @0x82390418 and BrnGameState::DriveThruManager::UnlockCarChallengeForCar @0x82386840
    //       (both id 201, size 40); consumer @0x823EA810 -> GuiEventJunctionInfo(311).
    //       DWARF :203 E_ACTION_EVENT_AT_JUNCTION_AVAILABLE == 193 (+8).
    E_ACTION_SHOW_MODE_RESULTS                           = 37,   // DWARF 32  (+5 X360)
    E_ACTION_FINISHED_MODE_RESULTS                       = 38,   // DWARF 33  (+5 X360); name image-cited
    E_ACTION_STOP_MODE                                   = 39,   // DWARF 34  (+5 X360)
    E_ACTION_SET_IN_MODE_START_REGION                    = 44,   // DWARF 39  (+5 X360)
    E_ACTION_SET_COUNTDOWN                               = 47,   // DWARF 42  (+5 X360)
    //   75  ⭐⭐⭐ [returning-player wave 2026-08-28] THE GUI HALF OF THE JUNKYARD CAR SELECT.
    //       producer BrnGameState::CarSelectManager::StartCarSelectState @0x823872D0 (id 75,
    //       size 4, payload word 1 -- BrnCarSelectManager.cpp:560 already posts it);
    //       consumer @0x823EA700 `lwz r11,0(r31); stw r11,var_35D8;
    //       AddGuiEvent<GuiCarSelectStartEvent>` -> GUI event 81 size 4 (the instantiation
    //       @0x823D1690 ends `AddEvent(q, payload, 81, 4)`).
    //       DWARF :80 E_ACTION_CAR_SELECT_READY == 70 (+5) -- the same +5 the whole car-select
    //       band takes (CHANGE_COLOUR 74->79 immediately below is the neighbouring proof).
    //       ⓘ The X360 switch has a SECOND arm onto the same GUI event, case 72
    //       (DWARF :77 E_ACTION_CAR_SELECT_START_GUI_ON_GAME_START == 67, +5) @0x823EA6E8,
    //       which posts the literal 1 instead of forwarding the action's word. Nothing in this
    //       tree posts 72 yet, so only the 75 arm is landed.
    E_ACTION_CAR_SELECT_READY                            = 75,   // DWARF 70  (+5 X360)
    E_ACTION_UPDATE_PLAYER_MEDALS                        = 200,  // DWARF 192 (+8 X360)
    E_ACTION_EVENT_AT_JUNCTION_AVAILABLE                 = 201,  // DWARF 193 (+8 X360)
    //  202  producer BrnGameState::GameStateModule::CheckForAllEventsBeingFound @0x82382460
    //       (`li r5, 0xCA` / `li r6, 1` @0x82382530, size 1). DWARF :204
    //       E_ACTION_ALL_EVENTS_DISCOVERED == 194 (+8) -- the same +8 the whole 200-band takes.
    //       Its sibling E_ACTION_ALL_EVENTS_OF_TYPE_DISCOVERED (DWARF 195 -> X360 203) belongs to
    //       CheckForAllEventsOfATypeFound @0x823822C8, which has no body in this tree yet; it is
    //       deliberately NOT enumerated here until its producer lands.
    E_ACTION_ALL_EVENTS_DISCOVERED                       = 202,  // DWARF 194 (+8 X360); size 1
    // [drive-thru wave 2026-08-27] 208, size 8. Producer ProgressionManager::
    // SendGameCompletionResults @0x82395C28 (`li r5, 0xD0` / `li r6, 8` @0x82395CC0/C4). It is
    // posted from the ONE place the game decides it is finished, so the name is the producer's
    // own symbol name (PINNED tier), not a band extrapolation.
    E_ACTION_GAME_COMPLETION_RESULTS                     = 208,  // size 8

    // =========================================================================================
    // [!!] [stuntrace waveB CLOSURE round, 2026-08-26] THE MODE-LIFECYCLE / TRANSMIT BLOCK.
    //
    // Twenty-one ids the wave-B ModeManager + ProgressionManager bodies post that had NO
    // enumerator here, so nine partfiles were each carrying their own `KI_ACTION_*` mirror --
    // three of them already duplicated across two files. Every VALUE below is the live
    // `li r5, <id>` immediate at a named AddEvent call site and every SIZE is the live
    // `li r6, <n>` on the same path; both were re-dumped THIS pass by an exhaustive sweep of
    // all 30,095 exports, filtered to the GameState queue VariableEventQueue<13312,16>
    // (2,547 AddEvent sites image-wide, 402 of them on this queue).
    //
    // NAMES. Three tiers, and each line says which tier it is:
    //   PINNED      the producer's own SYMBOL NAME is the enumerator name (7, 31, 45, 275).
    //   BAND        the DWARF name at the measured X360 shift, AND the producer's branch
    //               condition matches that name's semantics. The shift is not extrapolated: it
    //               is monotone and witnessed at both ends of each band --
    //                 DWARF   0..15  -> +0   (RESET_PLAYER_CAR 0, SETUP_NETWORK_CAR 5,
    //                                         REMOTE_PLAYER_DISCONNECTED 11, COMPLETED_STUNT 15)
    //                 DWARF  18..30  -> +4   (PREPARE_FOR_MODE 19->23, MARKED_MAN_LOADED 27->31
    //                                         [producer-pinned], START_PLAYING_MODE 30->34)
    //                 DWARF  31..122 -> +5   (SHOW_MODE_RESULTS 32->37, STOP_MODE 34->39,
    //                                         SET_COUNTDOWN 42->47, REQUEST_ROUTE_INFO 45->50
    //                                         [producer-pinned SendRouteRequestAction],
    //                                         SET_LANDMARK_RACES 46->51 [producer-pinned
    //                                         SendSetLandmarkRacesAction], WORLD_STUNT 122->127)
    //                 DWARF 135..226 -> +8   (SHOWTIME_MODE_SWITCH 135->143, GAME_TRAINING
    //                                         140->148 [TrainingManager::SendTrainingTickerMessage],
    //                                         GAME_TRAINING_REQUEST 141->149, GAME_TRAINING_PAUSE
    //                                         142->150, GAME_TRAINING_UNPAUSE 143->151
    //                                         [TrainingManager::ForceUnpause], the freeburn block,
    //                                         ONLINE_GAME_RESULT 221->229, ONLINE_ROUND_RESULT
    //                                         222->230, ROAD_RULES_CHALLENGE_SCORES 224->232)
    //                 DWARF 260..    -> +13  (the nine road-rules witnesses listed above)
    //   FLAG        the value is pinned but NO DWARF enumerator fits the payload; the name is
    //               descriptive, taken from what the producer builds, and must not be "tidied".
    //
    // Deleting the TU-local mirrors is part of this change; do not re-add them.
    // =========================================================================================

    // ---- ShowModeResults / SendModeStopMessages tail -----------------------------------------
    // PINNED. BrnGameState::DriveThruManager::SetPlayerCarDriver posts id 7 size 48
    // (`li r5,7` @0x82386824 + `li r6,0x30` @0x8238681C -> AddEvent @0x82386830) -- the producer
    // symbol IS the DWARF name (:7, sub-16 region, unshifted). ModeManager posts the same record
    // from ShowModeResults (@0x82343E4C) and SendModeStopMessages (@0x8234C710), and
    // CarSelectManager posts it on junkyard exit; the leading word selects who is driving.
    E_ACTION_SET_PLAYER_CAR_DRIVER                       = 7,    // DWARF :7 (+0); size 48
    // FLAG -- NAME UNRECOVERED. ModeManager::ShowModeResults @0x82343BC8 posts id 18 size 16
    // (`li r5,0x12` @0x82343BBC + `li r6,0x10` @0x82343BB8) with payload
    // {+0x00 CgsID event id, +0x08 s32 EGameModeType, +0x0C s32 score}, and the very next console
    // call is Profile::SetEventScoreToUpload with those same three values -- which is where the
    // descriptive name comes from. NO DWARF enumerator fits: DWARF 18 is
    // E_ACTION_CHECK_FOR_LOADING_SCREEN (whose X360 seat is 22, below) and neither DWARF 13 (+5)
    // nor DWARF 14 (+4) is a score upload. Value pinned, name provisional.
    E_ACTION_EVENT_SCORE_TO_UPLOAD                       = 18,   // FLAG name; X360 size 16
    // BAND (+4). ModeManager::SetupGameMode posts id 22 size 1 at BOTH of its exits
    // (`li r5,0x16` @0x8234B4B8 and @0x8234B86C, `li r6,1` @0x8234B4B4/@0x8234B868) and the
    // payload stack slot is NEVER WRITTEN -- a bare tag action. SetupGameMode is exactly the
    // console function that then waits for ModeManager::HandleLoadingScreenLoaded, so the DWARF
    // name at this band's shift describes this producer. No other site in the image posts 22.
    E_ACTION_CHECK_FOR_LOADING_SCREEN                    = 22,   // DWARF 18 (+4 X360); size 1  BAND
    // BAND (+4). ModeManager::PrepareForMode posts id 24 size 48 (`li r5,0x18` @0x82342F60 +
    // `li r6,0x30` @0x82342F5C): 36 bytes copied verbatim off a Landmark's TriggerRegion base
    // (the `li r9,9` dword loop @0x82342F3C..0x82342F54) then the landmark CgsID sign-extended
    // into +0x28. A per-landmark trigger box + id broadcast is what the DWARF name describes.
    // (The record itself stays TU-local in BrnModeManager_Prepare.cpp as ModeLandmarkAction --
    // it is a 9-dword copy of a type this header does not include.)
    E_ACTION_BROADCAST_MODE_FINISH_LINES                 = 24,   // DWARF 20 (+4 X360); size 48 BAND
    // ---- the SendModeStopMessages exit fan-out (all from @0x8234BEC0) ------------------------
    // Five ids share the merged AddEvent call site @0x8234C698; each arm sets its own `li r5`
    // and `li r6` before branching to loc_8234C694, so the pairing below is a HAND-TRACE of the
    // branch structure, not an automated backward scan (an automated scan mis-attributes size 4
    // to id 25 -- the `li r6,4` at 0x8234C67C is on the action-149 path only).
    //   BAND (+4), offline and not timed out: `li r5,0x19` @0x8234C690, size 1 from
    //   `li r6,1` @0x8234C654, payload = the var_130 byte.
    E_ACTION_STOP_MODE_OFFLINE                           = 25,   // DWARF 21 (+4 X360); size 1  BAND
    //   BAND (+4), online, not the last round: `li r5,0x1A` @0x8234C63C + `li r6,8` @0x8234C638.
    E_ACTION_FINISH_MODE_ONLINE                          = 26,   // DWARF 22 (+4 X360); size 8  BAND
    //   BAND (+4), online, last round: `li r5,0x1B` @0x8234C628 + `li r6,4` @0x8234C620.
    E_ACTION_FINISH_MODE_FINAL_ONLINE                    = 27,   // DWARF 23 (+4 X360); size 4  BAND
    //   BAND (+5), offline and timed out: `li r5,0x28` @0x8234C664 + `li r6,1` @0x8234C654,
    //   payload = NetworkRoundManager+0x130. THIS is the seat E_ACTION_SET_UP_ALL_DRIVE_THRUS
    //   used to occupy (see its banner above).
    E_ACTION_QUIT_MODE_OFFLINE                           = 40,   // DWARF 35 (+5 X360); size 1  BAND
    //   BAND (+5), online quit: `li r5,0x29` @0x8234C584 and @0x8234C5CC + `li r6,8`.
    E_ACTION_QUIT_MODE_ONLINE                            = 41,   // DWARF 36 (+5 X360); size 8  BAND
    //   BAND (+5), posted unconditionally last by SendModeStopMessages (`li r5,0x2B`
    //   @0x8234C718 + `li r6,1` @0x8234C714) AND by ModeManager::FinishCurrentMode
    //   (`li r5,0x2B` @0x8234BB5C + `li r6,1` @0x8234BB58, the showtime pair). One
    //   uninitialised byte both times. FinishCurrentMode's very next statement restores the sim
    //   timestep multiplier to 1.0 for OFFLINE_SHOWTIME -- i.e. the end of crash-mode impact
    //   time, which is what the DWARF name says.
    E_ACTION_IMPACT_TIME_END                             = 43,   // DWARF 38 (+5 X360); size 1  BAND
    // ---- ⭐⭐⭐ [bounce wave] THE PARTNER THAT WAS MISSING -------------------------------------
    // BAND (+5), and it is the ONLY producer of impact time in the whole image. Recovered by
    // scanning the ARTIST image for `li r5,<id>` sites that carry a `li r6,<size>` before them
    // and a `bl` after -- i.e. real posts rather than the constant 42 (which occurs 100+ times):
    //   `li r6,8` @0x823814FC + `li r5,0x2A` @0x82381500 + `bl VariableEventQueue<13312,16>::
    //   AddEvent` @0x82381508, inside GameStateModule::UpdateRoadRulesManager @0x82381258.
    // ⭐ THE SAME QUEUE AND THE SAME AddEvent (0x8233FAE8) as BOTH proven action-43 posts above.
    // Every other `li r5,42` in the image resolves to a different callee.
    // Payload (8 bytes, asm 0x823814C4..0x82381504): f32 duration @+0 (IMAGE-CITED
    // flt_82001C98 == 0x3F800000 == 1.0f, loaded from the SAME address on the offline and the
    // online arm -- not a Hex-Rays fold), u8 @+4, u8 @+5, two bytes of stack residue.
    // ⭐⭐ It meets the consumer on the byte: PhysicsModule::HandleGameActions' case-42 arm
    // (BrnPhysicsModuleGameActions.cpp) reads f32 @+0 and u8 @+4 -- and that decode was recovered
    // from the OTHER end, from the consumer's asm, by a different wave.
    E_ACTION_IMPACT_TIME_START                           = 42,   // DWARF 37 (+5 X360); size 8  BAND
    // ---- ProgressionManager::OnEventFinishUpdateProfile @0x823A0040 --------------------------
    // BAND (+4). `li r5,0x1C` @0x823A0608 + `li r6,4` @0x823A0604. Independently corroborated:
    // BrnGameState::GameStateModule::OnProfileLoaded posts the same id at the same size -- a
    // rank-derived traffic scale is exactly what both a profile load and an event finish would
    // republish.
    E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK             = 28,   // DWARF 24 (+4 X360); size 4  BAND
    // BAND (+5). `li r5,0x37` @0x823A05DC + `li r6,1` @0x823A05D8. Corroborated by three more
    // producers posting the same id/size for the same reason: DriveThruManager::ProcessDriveThru
    // (twice), DriveThruManager::UnlockCarChallengeForCar and StreetManager::ProcessNewRoadScore
    // -- every one of them a "the profile just changed, save it" moment. The tree's
    // BrnCarSelectManager.cpp already carries 55 under this name as a TU-local.
    E_ACTION_REQUEST_AUTOSAVE                            = 55,   // DWARF 50 (+5 X360); size 1  BAND
    // ---- the mode-lifecycle latches ----------------------------------------------------------
    // PINNED. BrnGameState::ModeManager::MarkedManLoaded posts id 31 size 8; the producer symbol
    // IS the DWARF name (:27) and it is what proves 31 is NOT E_ACTION_FINISHED_MODE (see that
    // enumerator's correction banner above). Enumerated here so nothing re-claims the seat.
    E_ACTION_MARKED_MAN_LOADED                           = 31,   // DWARF 27 (+4 X360); size 8
    // BAND (+4). ModeManager::UpdateCurrentMode posts it when the mode's
    // countdown-just-finished latch fires (`li r5,0x21` @0x82351134 + `li r6,1` @0x82351130).
    E_ACTION_STOP_MODE_COUNTDOWN                         = 33,   // DWARF 29 (+4 X360); size 1  BAND
    // BAND (+8). The delayed showtime mode-switch broadcast, posted by
    // ModeManager::SendModeStopMessages (`li r5,0x8F` @0x8234C000 + `li r6,0x10` @0x8234BFF8)
    // and by ModeManager::UpdateCurrentMode (@0x82350F70/@0x82350F6C) when
    // miFramesUntilModeSwitchSend hits exactly 0. PrepareForMode arms that counter for modes 2
    // and 16 ONLY -- the offline/online showtime pair -- which is the corroboration.
    E_ACTION_SHOWTIME_MODE_SWITCH                        = 143,  // DWARF 135 (+8 X360); size 16 BAND
    // BAND (+8), DWARF 138. THE SHOWTIME INTRO LATCH BROADCAST. Producer: the DetectModeStarts
    // @0x8239A428 `else` arm, twice -- `li r6,0x20 / li r5,0x92` at 0x8239A684/0x8239A688 (the
    // arm-the-intro post, mbStart == 1) and again at 0x8239A8E0/0x8239A8E4 (the cancel post,
    // mbStart == 0). Consumer: PhysicsModule::HandleGameActions' case-146 arm, already committed
    // and mounted (BrnPhysicsModuleGameActions.cpp), which reads the record's FIRST 16 BYTES as
    // the aim vector and hands it to RaceCarPhysics::SetShowtimeAimDirection.
    E_ACTION_SHOWTIME_INTRO_START                        = 146,  // DWARF 138 (+8 X360); size 32 BAND
    // FLAG -- NAME UNRECOVERED. ModeManager::UpdateCurrentMode @0x82351824 (`li r5,0x98`
    // @0x82351808 + `li r6,8` @0x82351804), mode 13 (E_MODE_ONLINE_BURNING_HOME_RUN) only, and
    // the payload is a live CgsSystem::Time copied straight out of
    // ScoringSystem::GetModeTimeRemaining {+0x00 s32 seconds, +0x04 f32 fraction}. The DWARF
    // entry at this band's shift is E_ACTION_MODE_TIME_TIMEOUT (144), but a running clock is not
    // a timeout notification, so the name is NOT taken. Value pinned, name descriptive.
    E_ACTION_MODE_TIME_REMAINING                         = 152,  // FLAG name; X360 size 8
    // FLAG -- NAME UNRECOVERED, and this is the seat E_ACTION_ROAD_RULES_BATCH_QUERY used to
    // occupy (see its correction banner above). ModeManager::UpdateCurrentMode @0x823515E0
    // (`li r5,0x106` @0x823515D4 + `li r6,1` @0x823515D0) posts ONE BOOL:
    // `(*(this+0x58F0) >= *(this+0x58FC)) ? 1 : 0` @0x823515A0..0x823515CC (r21==1, r22==0 from
    // the prologue @0x82350F2C/0x82350F34). No DWARF enumerator fits -- the nearest by meaning,
    // E_ACTION_HUD_MESSAGE_TIME_UP (255), would need a shift of +7 between the +5 and +8 bands,
    // and the measured shift is monotone. Value pinned, name descriptive.
    E_ACTION_MODE_TIME_UP                                = 262,  // FLAG name; X360 size 1
    // ---- ModeManager's checkpoint/finish transmit trio (all BAND, +5) -------------------------
    // TransmitAndIncrementCheckPointsReached @0x82342098:
    //   `li r5,0x71` @0x823422B0 + `li r6,0x10` @0x823422AC   -> id 113 size 16
    //   `li r5,0x73` @0x823422E4 + `li r6,1`    @0x823422E0   -> id 115 size 1
    // TransmitAndIncrementFinishReached @0x823424D0:
    //   `li r5,0x72` @0x823426AC + `li r6,8`    @0x823426A4   -> id 114 size 8
    // TransmitCheckPointDistancesToFinishLine @0x82341FF8:
    //   `li r5,0x76` @0x82342078 + `li r6,0x44` @0x82342074   -> id 118 size 68
    // The +5 band is pinned on both sides of this run (WORLD_STUNT_PERFORMED 122 -> 127 above,
    // REQUEST_ROUTE_INFO 45 -> 50 below it), and all four DWARF names describe exactly what
    // these three producers do.
    E_ACTION_RACE_CAR_REACHED_CHECKPOINT                 = 113,  // DWARF 108 (+5 X360); size 16 BAND
    E_ACTION_RACE_CAR_REACHED_FINISH                     = 114,  // DWARF 109 (+5 X360); size 8  BAND
    E_ACTION_PLAYER_REACHED_PENULTIMATE_CHECKPOINT       = 115,  // DWARF 110 (+5 X360); size 1  BAND
    E_ACTION_SET_WAYPOINT_DISTANCES_TO_FINISH            = 118,  // DWARF 113 (+5 X360); size 68 BAND
};

template <EGameActionType T>
struct GameAction { };

// DecFIGS BrnGameActions.h:3977.  ARTIST case 70 reads the payload with one
// `lbz 0(r27)` before forwarding it to BoostManager.
struct AllowBoostEarningAction : public GameAction<E_ACTION_ALLOW_BOOST_EARNING>
{
    bool mbAllowBoostEarning;
};

// DecFIGS names an empty tag record for case 71; ARTIST consumes no payload.
struct StopBoostingAction : public GameAction<E_ACTION_STOP_BOOSTING>
{
};

// X360 0x822A0250 (HasToChangeLocation).
//
// ⭐ GROWN to the FULL 80-byte record (reset-player-car wave 2026-08-01). It was a two-member
// slice while SIX producers built the payload as raw `u8 lac[80]` + memcpy at hand-written
// offsets -- which is exactly how five of them ended up building it at the WRONG offsets
// (corrected the previous wave) and the sixth, CarSelectManager::EnterJunkyardAtStartOfGame,
// was STILL wrong when this wave started. The whole record is now typed, so the producers
// assign named members and the class of bug cannot recur.
//
// Every offset below is read (or written) at that offset by BOTH ends:
//   consumer  RaceCarEntityModule::HandleResetPlayerCarAction @0x82304FE8
//             (lvx128 r16+0x00 / r16+0x10; ld 0x20 / 0x28; lwz 0x30 / 0x38 / 0x3C;
//              lfs 0x34; lbz 0x40 / 0x41 / 0x43)
//   producer  CarSelectManager::EnterJunkyardAtStartOfGame @0x82393080 and the five
//             CarSelectManager builders at 0x82387D38 / 0x82392F78 / 0x82398B3C /
//             0x82392D64 / 0x82398DDC.
// Sizeof is pinned at 80 by the AddEvent liSize every producer passes (`li r6, 0x50`).
struct alignas(16) ResetPlayerCarAction : public GameAction<E_ACTION_RESET_PLAYER_CAR>
{
    Vector3 mPosition;                 // +0x00  SpawnLocation::mPosition
    Vector3 mDirection;                // +0x10  SpawnLocation::mDirection
    CgsID   mCarModelId;               // +0x20  0 == "do not re-spawn, just teleport"
    CgsID   mWheelModelId;             // +0x28  0 == "derive from the car's default wheel name"
    EPlayerScoringIndex mePlayerScoringIndex; // +0x30  >= E_PLAYER_SCORING_INDEX_COUNT == none
    f32     mfDeformationAmount;       // +0x34  < 0 == "no unlock deform" (the -1.0 sentinel)
    // +0x38 -- ⭐ RETYPED s32 2026-08-24 (deform-land wave; was the provisional
    // `f32 mfDeformationAmount2`). Every writing producer stores an INTEGER here (the
    // CarChange builders: `stw 1` / memcpy of s32 1), and the consumer
    // (HandleResetPlayerCarAction arm 4, landed this wave) copies the word into
    // ActiveRaceCar::meBaseDeformationType (+0x7C8, an s32 the create chain forwards as
    // BrnPhysics::Deformation::DeformationResetType) -- the same seat
    // AddRaceCarToStartingGridOrFreeburnLobby @0x82300B38 stores its ±1 int into.
    // EnterJunkyardAtStartOfGame leaves this field UNWRITTEN (its -0x50 record is not
    // memset), safe precisely because it posts the -1.0 sentinel at +0x34.
    s32     miBaseDeformationType;     // +0x38  (1 on the car-select path)
    s32     miInCarModification;       // +0x3C  copied to the module's +0x186CC word.
                                       //        TeleportCurrentVehicle sets it on the in-car-mod
                                       //        path, UpdateUnlockState posts 2. Name provisional.
    bool    mbInCarSelectScreen;       // +0x40  -> RaceCarEntityModule::mbInCarSelectScreen
    bool    mbCarSelectDontStreamAudio;// +0x41  -> RaceCarEntityModule::mbCarSelectDontStreamAudio
    u8      muReserved0x42;            // +0x42  written 0 by every producer; NO consumer reads it
    bool    mbKeepResetSection;        // +0x43  -> SpawnRaceCar's 5th arg -> the DWARF-named
                                       //        AttachAIControlEvent::mbKeepResetSection
    u8      maReserved0x44[0x0C];      // +0x44..+0x4F  tail of the 80-byte AddEvent image

    bool HasToChangeLocation() const;
};
static_assert(sizeof(ResetPlayerCarAction) == 80,
              "ResetPlayerCarAction must be the 80-byte image every AddEvent(.., 0, 80) posts");

// Game action 64 -- "the player's car selection changed": which junkyard, and where in it.
// TYPED 2026-08-01 (reset-player-car wave); it was an opaque forward declaration while three
// producers stamped it by hand and MainDirector::ProcessInputQueue read it by hand -- which is
// how the +0x30 "pos is left" bit ended up being written at +0x10 for a whole campaign.
// Both ends agree on every offset below:
//   producer  CarSelectManager::SpawnInStartCar @0x82387D9C..0x82387E18 (AddEvent .., 64, 64)
//             CarSelectManager::EnterJunkyardAtStartOfGame @0x82393178..0x823931E0 (by pointer)
//   consumer  MainDirector::ProcessInputQueue case 64 (BrnMainDirector.cpp:753, X360
//             `lbz r11, 0x30(r30)` @0x82238418 -> maGameState.mbJunkyardPosIsLeft)
struct alignas(16) CarSelectionChangedAction
{
    CgsID   mJunkyardId;        // +0x00
    u8      maReserved0x08[8];  // +0x08  never written by any producer
    Vector3 mPosition;          // +0x10  SpawnLocation::mPosition
    Vector3 mDirection;         // +0x20  SpawnLocation::mDirection
    bool    mbJunkyardPosIsLeft;// +0x30  == (SpawnLocation::GetType() == E_TYPE_CAR_SELECT_LEFT)
    bool    mbReserved0x31;     // +0x31  written 0
    u8      maReserved0x32[14]; // +0x32..+0x3F  tail of the 64-byte AddEvent image
};
static_assert(sizeof(CarSelectionChangedAction) == 64,
              "CarSelectionChangedAction must be the 64-byte image AddEvent(.., 64, 64) posts");
static_assert(offsetof(CarSelectionChangedAction, mbJunkyardPosIsLeft) == 0x30,
              "CarSelectionChangedAction::mbJunkyardPosIsLeft at +0x30 (the consumer's lbz offset)");

// Game action 79 -- "paint the player's car": the palette (paint finish) and colour indices the
// car-select / junkyard flow resolved for the car that is now in the world. It is the ONLY route
// by which a car's AUTHORED default colour reaches BrnWorld::RaceCar: HandleResetPlayerCarAction
// spawns every fresh car at 0/0 (it only carries an OLD car's colour across a same-model
// respawn), and this action is what the console posts immediately afterwards.
//
// ⭐ PALETTE FIRST. Both ends are attested and they agree:
//   producer  CarSelectManager::ReallyEnterJunkyardAtStartOfGame @0x823931F8
//             (`r5 = &sp+0x5C` == the COLOUR out-param, `r6 = &sp+0x58` == the PALETTE
//              out-param, then `AddEvent(sp+0x58, 79, 8)` -- the payload BASE is the palette),
//             CarSelectManager::UpdateUnlockState @0x82398920 and
//             CarSelectManager::TeleportCurrentVehicle @0x82392EF0
//             (both `GetCarColourAndPalette(.., &v + 4, &v); AddEvent(&v, 79, 8)`).
//   consumer  RaceCarEntityModule::ChangePlayerCarColour @0x822D27B0, reached from
//             HandleGameActions' `case 79` as `ChangePlayerCarColour(payload[0], payload[4])`,
//             which stores arg0 to RaceCar+152 (miColourPalette) and arg1 to +148
//             (miColourIndex).
// The member names and widths are the DWARF's (BrnGameActions.h:2811 CarSelectChangeColourAction
// : GameAction<E_ACTION_CAR_SELECT_CHANGE_COLOUR> { uint32_t muPaletteIndex; uint32_t
// muColourIndex; }) -- which independently puts the palette first.
//
// ⚠️ NOT to be confused with the DWARF's CarUnlockAction: that is
// GameAction<E_ACTION_GUI_CAR_UNLOCK> (PS3 id 2) and holds a single `CgsID mCurrentCarToUnlock`
// -- the other 8-byte event UpdateUnlockState posts (`AddEvent(&v17, 2, 8)`).
struct CarSelectChangeColourAction : public GameAction<E_ACTION_CAR_SELECT_CHANGE_COLOUR>
{
    u32 muPaletteIndex;   // +0x00
    u32 muColourIndex;    // +0x04
};
static_assert(sizeof(CarSelectChangeColourAction) == 8,
              "CarSelectChangeColourAction must be the 8-byte image AddEvent(.., 79, 8) posts");
static_assert(offsetof(CarSelectChangeColourAction, muPaletteIndex) == 0,
              "the palette index is payload word 0 (ChangePlayerCarColour's first argument)");
static_assert(offsetof(CarSelectChangeColourAction, muColourIndex) == 4,
              "the colour index is payload word 1 (ChangePlayerCarColour's second argument)");

// X360 0x82355178 (IsEmpty).
struct SoundTriggerAction : public GameAction<E_ACTION_SOUND_TRIGGER>
{
    enum eType { E_TYPE_INVALID = 0, E_TYPE_AT_ENTITY = 1, E_TYPE_AHEAD_OF_ENTITY = 2, E_TYPE_COUNT = 3 };

    Vector3  mQueryPos;
    EntityId mEntityId;
    eType    meResultType;
    u32      muActiveTriggers;

    bool IsEmpty();
};

// X360 0x8230FE98 / 0x8230FF00.
struct RemotePlayerDisconnectedAction : public GameAction<E_ACTION_REMOTE_PLAYER_DISCONNECTED>
{
    EActiveRaceCarIndex         meActiveRaceCarIndex;
    BrnNetwork::NetworkPlayerID mPlayerID;

    void SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex);
    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lPlayerID);
};

// X360 0x82355088 (Construct). DWARF: 6 members / 0x38 bytes.
struct alignas(16) SetupNetworkCarAction : public GameAction<E_ACTION_SETUP_NETWORK_CAR>
{
    Vector3             mWorldSpacePosition;   // 0x00
    Vector3             mAt;                   // 0x10
    CgsID               mModelId;              // 0x20
    CgsID               mWheelModelId;         // 0x28
    EActiveRaceCarIndex meActiveRaceCarIndex;  // 0x30
    EPlayerScoringIndex mePlayerScoringIndex;  // 0x34

    void Construct(EPlayerScoringIndex lePlayerScoringIndex,
                   EActiveRaceCarIndex leActiveRaceCarIndex,
                   Vector3             lPos,
                   Vector3             lAt,
                   CgsID               lModelId,
                   CgsID               lWheelModelId);
};

// X360 0x823551F0 (SetPlayerScoringIndex). Layout per the Feb-2007 partial source (this X360 build).
struct OnlinePlayerAddedAction : public GameAction<E_ACTION_ONLINE_PLAYER_ADDED>
{
    CgsID               mModelID;             // 0x00
    CgsID               mWheelID;             // 0x08
    EPlayerScoringIndex mePlayerScoringIndex; // 0x10
    EPlayerTeam         meTeam;               // 0x14

    void SetPlayerScoringIndex(EPlayerScoringIndex lePlayerScoringIndex);
};

// X360 0x82355258 (SetActiveRaceCarIndex). Minimal slice: only the member the body touches.
struct OnlinePlayerRemovedAction : public GameAction<E_ACTION_ONLINE_PLAYER_REMOVED>
{
    EActiveRaceCarIndex meActiveRaceCarIndex;   // 0x00
    bool                mbIsLocalPlayerInGame;  // 0x04 (DWARF BrnGameActions.h:4161; untouched by SetActiveRaceCarIndex)

    void SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex);
};

// X360 0x823554B0 (SetProgressionRanks) / 0x82355328 (SetProgressionRankEventWins). The rank-info
// response action: the player's overall rank + per-mode ranks (0x00-0x10), then the per-mode
// rank-win counts (0x14-0x20).
struct RankInfoResponseAction : public GameAction<E_ACTION_RANK_INFO_RESPONSE>
{
    static const s32 KI_PLAYER_HAS_FINISHED_LAST_RANK = -1;

    s32 miPlayerRank;            // 0x00
    s32 miOfflineRace;           // 0x04
    s32 miRoadRage;              // 0x08
    s32 miStuntAttack;           // 0x0C
    s32 miMarkedMan;             // 0x10
    s32 miOfflineRaceRankWins;   // 0x14
    s32 miRoadRageRankWins;      // 0x18
    s32 miStuntAttackRankWins;   // 0x1C
    s32 miMarkedManRankWins;     // 0x20

    void SetProgressionRanks(s32 liPlayerRank, s32 liRankCount, s32 liOfflineRace,
                             s32 liRoadRage, s32 liStuntAttack, s32 liMarkedMan);
    void SetProgressionRankEventWins(s32 liOfflineRaceRankWins, s32 liRoadRageRankWins,
                                     s32 liStuntAttackRankWins, s32 liMarkedManRankWins);
};

// X360 element of Array<TrophyUnlockAction,12> @ 0x8235E1F0 / ::Erase @ 0x8235E318. DWARF
// BrnGameActions.h:2438; 16-byte stride. GameAction<T> base carries only a static type tag (no
// instance data/vtable), so the struct is exactly the two members.
//
// ⚠️⚠️ THE FIELD ORDER IS THE X360's, AND IT IS THE REVERSE OF THE DWARF's DECLARATION ORDER
// (fixed 2026-08-27; it used to be UnlockType@0x00 + CgsID@0x08, taken from DWARF :2440/:2441).
// BOTH ENDS PROVE THE CONSOLE ORDER, and the consumer even carries the field names:
//   * PRODUCER UnlockCarFromTrophy @0x8237B0E8 builds the record in a 16-byte frame slot and
//     hands its base to Append -- `std r30, 0x90+var_40(r1)` (r1+0x50, the CgsID) and
//     `stw r28, 0x90+var_38(r1)` (r1+0x58, the type), with `addi r4, r1, 0x90+var_40`.
//   * CONSUMER SendTrophyUnlockUpdate @0x823892B8 asserts on both fields BY NAME:
//         lwz  r11, 8(r3)  -> "mQueueOfTrophyCarUnLocks[...].meUnlockType != ...E_UNLOCKTYPE_NONE"
//         ld   r11, 0(r3)  -> "mQueueOfTrophyCarUnLocks[...].mCarToUnlock != kCGSID_NULL"
//     -- an `ld` at ZERO for the car id and an `lwz` at EIGHT for the type.
// It then posts the record verbatim as game action 204, size 16, so a swapped pair would have
// handed the GUI the low half of a car id as an unlock type and vice-versa. Rung 1 over rung 2.
struct TrophyUnlockAction : public GameAction<E_ACTION_TROPHY_UNLOCK>
{
    CgsID                                        mCarToUnlock;  // 0x00  (DWARF BrnGameActions.h:2441)
    BrnProgression::TrophyUnlockData::UnlockType meUnlockType;  // 0x08  (DWARF BrnGameActions.h:2440)
};
static_assert(sizeof(TrophyUnlockAction) == 16,
              "SendTrophyUnlockUpdate posts the record as action 204 with `li r6, 0x10`");
static_assert(offsetof(TrophyUnlockAction, mCarToUnlock) == 0x00 &&
              offsetof(TrophyUnlockAction, meUnlockType) == 0x08,
              "the producer's std/stw pair and the consumer's ld 0(r3) / lwz 8(r3) asserts");

// X360 0x8230FDF0 (Construct), 0x822A0198 (GetPlayerDisconnected), 0x8230FD60 (SetPlayerDisconnected).
// True owning home (DWARF BrnGameActions.h:853); all DWARF members/methods declared, only the three
// reconstructed bodies are defined in BrnGameActions.cpp. Layout follows DWARF source order; exact byte
// offsets are NOT X360-faithful on the x64 gate (GameModeParams / Vector3 strides differ) -- parity is
// by named member, per the BrnGameModeParams.h precedent.
struct PrepareForModeAction : public GameAction<E_ACTION_PREPARE_FOR_MODE>
{
    // DWARF BrnGameActions.h:856 (nested).
    enum EPrepareForModeStage
    {
        E_PFM_STAGE_ALL_IN_ONE    = 0,
        E_PFM_STAGE_FIRST_OF_TWO  = 1,
        E_PFM_STAGE_SECOND_OF_TWO = 2,
        E_PFM_STAGE_COUNT         = 3
    };

    // == BrnWorld::KI_MAX_ACTIVE_RACE_CARS (the disconnect asserts spell that name). Both fixed
    // arrays are [8]. KI_MAX_DISCONNECTED_NETWORK_PLAYERS is the bound the disconnect getters assert.
    static const s32 KI_MAX_PLAYERS                       = 8;
    static const s32 KI_MAX_DISCONNECTED_NETWORK_PLAYERS  = 8;

    void                  Construct(const GameModeParams* lpGameModeParams, s32 liCurrentRound,
                                    bool lbComingFromOnlineLobbyMode);   // X360 0x8230FDF0 (defined)
    const GameModeParams* GetGameModeParams() const;                     // [gateui] DEFINED in BrnGameActions.cpp (round 8)
    // ⭐ [evt-flow E1] THE FOUR TRIVIAL GETTERS BELOW WERE DECLARED-ONLY AND ARE NOW INLINE.
    // TranslateGameActionsToGuiEvents' case-23 arm (@0x823EAD80, the PrepareForModeStart /
    // RunFsm arm) reads exactly these four fields off the action record, and the X360 compiler
    // inlined every one of them at that call site (the arm loads `0(r31)`, `0x8A0(r31)`,
    // `0x8D0(r31)`, `0x8D1(r31)`, `0x8D2(r31)` directly -- there is no `bl` to any accessor).
    // Leaving them declared-only made the arm an LNK2019 against a body that exists nowhere in
    // b5-decomp/src. Same treatment, same reason, as GameModeParams::GetFlag in
    // BrnGameModeParams.h -- inline body per the console semantics, fold back into the full
    // BrnGameActions TU if it ever defines them out-of-line (it does not today: BrnGameActions.cpp
    // defines only Construct / GetGameModeParams / Get+SetPlayerDisconnected).
    s32                   GetCurrentRound() const { return miCurrentRound; }
    bool                  IsMovingBetweenOnlineLobbyModes() const { return mbComingFromOnlineLobbyMode; }
    void                  SetPlayerScoringIndex(s32 liIndex, EPlayerScoringIndex leIndex); // declared-only
    EPlayerScoringIndex   GetPlayerScoringIndex(s32 liIndex) const;      // declared-only
    bool                  GetPlayerDisconnected(BrnNetwork::NetworkPlayerID lNetworkPlayerID) const; // X360 0x822A0198 (defined)
    void                  SetPlayerDisconnected(BrnNetwork::NetworkPlayerID lPlayerID);   // X360 0x8230FD60 (defined)
    f32                   GetPlayerBoostEarning() const;                 // declared-only
    void                  SetPlayerBoostEarning(f32 lfBoostEarning);     // declared-only
    s32                   GetShotGroup() const;                          // declared-only
    void                  SetShotGroup(s32 liShotGroup);                 // declared-only
    bool                  GetFinishedOnlineEvent() const { return mbFinishedOnlineEvent; }   // [evt-flow E1] inline (see above)
    void                  SetFinishedOnlineEvent(bool lbFinished);       // declared-only
    // The console tests `stage == E_PFM_STAGE_ALL_IN_ONE || stage == E_PFM_STAGE_FIRST_OF_TWO`
    // inline at the head of the case-23 arm (@0x823EAD80: `lwz r11,0(r31); cmpwi 0; beq; cmpwi 1`),
    // which IS this predicate -- an all-in-one prepare and the first of a split pair are both
    // "first"; the second of two is not.
    bool                  IsFirstPrepareForMode() const
    {
        return mePrepareForModeStage == E_PFM_STAGE_ALL_IN_ONE ||
               mePrepareForModeStage == E_PFM_STAGE_FIRST_OF_TWO;
    }
    void                  SetPrepareStage(EPrepareForModeStage leStage); // declared-only
    void                  SetStartingFreeburnLobbyDueToPlayerJoin(bool lbStarting); // declared-only
    bool                  GetStartingFreeburnLobbyDueToPlayerJoin() const { return mbStartingFreeburnDueToPlayerJoin; }  // [evt-flow E1] inline

private:
    // Data members in DWARF source order (BrnGameActions.h:936-949).
    EPrepareForModeStage        mePrepareForModeStage;                         // X360 +0x0000
    EPlayerScoringIndex         maePlayerScoringIndex[KI_MAX_PLAYERS];         // X360 +0x0004
    GameModeParams              mGameModeParams;                               // X360 +0x0030 (2160 bytes)
    s32                         miCurrentRound;                                // X360 +0x08A0 (2208)
    BrnNetwork::NetworkPlayerID maDisconnectedNetworkPlayerID[KI_MAX_PLAYERS]; // X360 +0x08A4 (2212)
    s32                         miNumPlayersDisconnected;                      // X360 +0x08C4 (2244)
    f32                         mfPlayerBoostEarning;                          // X360 +0x08C8 (2248)
    s32                         miShotGroup;                                   // X360 +0x08CC (2252)
    bool                        mbComingFromOnlineLobbyMode;                   // X360 +0x08D0 (2256)
    bool                        mbFinishedOnlineEvent;                         // X360 +0x08D1 (2257)
    bool                        mbStartingFreeburnDueToPlayerJoin;             // X360 +0x08D2 (2258)
};

// X360 action: Array<DriveThruInfo,46>::Append @ 0x8235C8E8 / ::GetLength @ 0x823AC200. Minimal slice:
// the nested DriveThruInfo element record + the fixed-capacity Array<> member that owns it
// (DWARF BrnGameActions.h:1710-1723).
struct SetUpAllDriveThrusAction : public GameAction<E_ACTION_SET_UP_ALL_DRIVE_THRUS>
{
    // DWARF BrnGameActions.h:1714-1718. 24-byte stride (verified vs the X360 element copy: 3 QWORD
    // stores into &maElements[miCount]); CgsID is u64 (8B aligned at 0x08), the trailing enum at 0x10
    // pads the record to 0x18.
    struct DriveThruInfo
    {
        f32                             mfXCoord;     // 0x00  (BrnGameActions.h:1715)
        f32                             mfZCoord;     // 0x04  (BrnGameActions.h:1716)
        CgsID                           mDriveThruId; // 0x08  (BrnGameActions.h:1717)
        BrnTrigger::GenericRegion::Type meType;       // 0x10  (BrnGameActions.h:1718)
    };

    Array<DriveThruInfo, 46> maDriveThrus;            // 0x00  (DWARF BrnGameActions.h:1723)
};

// X360 0x8231CA38 (SetPosition) / 0x82558580 (GetPosition). Per-player online-round result action:
// a position-indexed table of finishing slots plus a list of mid-round disconnects. DWARF
// BrnGameActions.h:5227 (true owning home). Minimal slice -- only the members the two reconstructed
// bodies touch are declared; Construct/GetWinner are declared-only (own ledger entries). GameAction<T>
// is an empty tag base, so instance data starts at +0x00 (matches the X360 a1[0] / (a1+8) access:
// maPlayerPosition[8] at +0x00, maDisconnectedPlayers Array<NetworkPlayerID,8> at +0x20 with its count
// word at +0x40 == a1[16]). NetworkPlayerID == s32 (committed BrnNetwork::NetworkPlayerID).
struct OnlineRoundResults : public GameAction<E_ACTION_ONLINE_ROUND_RESULT>
{
    static const s32 KI_POSITION_DISCONNECTED = -1;   // DWARF :5230 (GetPosition's disconnected sentinel)
    static const s32 KI_MAX_PLAYERS           = 8;    // == BrnWorld::KI_MAX_ACTIVE_RACE_CARS

    void                        Construct();                                                       // declared-only
    void                        SetPosition(BrnNetwork::NetworkPlayerID lNetworkPlayerID, s32 liPosition); // 0x8231CA38
    s32                         GetPosition(BrnNetwork::NetworkPlayerID lNetworkPlayerID) const;           // 0x82558580
    BrnNetwork::NetworkPlayerID GetWinner() const;                                                 // declared-only

private:
    BrnNetwork::NetworkPlayerID                        maPlayerPosition[KI_MAX_PLAYERS];   // +0x00 (DWARF :5248)
    Array<BrnNetwork::NetworkPlayerID, KI_MAX_PLAYERS> maDisconnectedPlayers;              // +0x20 (DWARF :5249)
};

// X360 online end-of-game results (DWARF home BrnGameActions.h:5153,
// : public GameAction<E_ACTION_ONLINE_GAME_RESULT>). Re-homed here from the provisional
// u32 mauWords[65] blob that used to live in BrnGameStateSharedIO.h (kept it there only because
// GameAction<>/EGameActionType weren't visible -- they are here). sizeof == 260 (65 u32 words),
// matching the old blob + the X360 operator= word count. LAYOUT IS X360-AUTHORITATIVE (the PS3
// DecFIGS DWARF lists a different/leaner field set past +0x24); the X360 anchors force round-count
// @0x28 and the EGameModeType discriminant @0x34. Methods Clear/Set*/Get*/operator= are defined in
// BrnGameActions.cpp.
struct OnlineGameResults : public GameAction<E_ACTION_ONLINE_GAME_RESULT>
{
    static const s32 KI_MAX_ROUNDS = 10;   // per-round array capacity (Clear loops 10; bounds use miNumberOfRounds)

    void Clear();                                                                          // 0x8230F178
    void SetRaceResults(s32 liRoundIndex, const f32* lpfRoundTime, f32 lfRoundDistance);    // 0x8230F220
    void GetRaceResults(s32 liRoundIndex, f32* lpfRoundTime, f32* lpfRoundDistance) const;  // 0x82580C60
    void SetStuntResults(s32 liRoundIndex, s32 liScore, s32 liMultiplier);                  // 0x8230F370
    void GetStuntResults(s32 liRoundIndex, s32* lpiScore, s32* lpiMultiplier) const;        // 0x82580DA8
    OnlineGameResults& operator=(const OnlineGameResults& lOther);                          // 0x82311C30

    // ---- DWARF-confirmed scalar header (0x00..0x24, BrnGameActions.h:5157-5167) ----
    CgsID           mCarUsed;                     // +0x00 (8)  DWARF:5157
    CgsSystem::Time mSecondsInEvent;              // +0x08 (8)  DWARF:5159 ({s32 seconds; f32 fraction})
    f32             mfMetersDriven;               // +0x10      DWARF:5160
    s32             miTakedownsFor;               // +0x14      DWARF:5162
    s32             miTakedownsAgainst;           // +0x18      DWARF:5163
    s32             miTraitorousTakedownsFor;     // +0x1C      DWARF:5164
    s32             miTraitorousTakedownsAgainst; // +0x20      DWARF:5165
    s32             miMarkedManTakedownsFor;      // +0x24      DWARF:5167
    // ---- X360-only / behaviour-anchored scalars (PS3 DWARF order diverges past +0x24) ----
    s32             miNumberOfRounds;             // +0x28  round-count (bounds: "... rounds N")
    s32             miReserved0x2C;               // +0x2C  zeroed by Clear; name not recoverable
    s32             miReserved0x30;               // +0x30  zeroed by Clear; name not recoverable
    s32             miEventType;                  // +0x34  EGameModeType discriminant (Clear -> -1)
    s32             miReserved0x38;               // +0x38  zeroed by Clear; name not recoverable
    // ---- Per-round arrays (X360 layout; 10 rounds each) ----
    CgsSystem::Time maRoundTimes[KI_MAX_ROUNDS];            // +0x3C (80) race round time
    f32             mafRoundDistances[KI_MAX_ROUNDS];       // +0x8C (40) race round distance
    s32             maiRoundStuntScores[KI_MAX_ROUNDS];     // +0xB4 (40) stunt score
    s32             maiRoundStuntMultipliers[KI_MAX_ROUNDS];// +0xDC (40) stunt multiplier
};

// X360 0x8232CEB0 (StuntModeScoring::DealWithStunt reads it). The "a world stunt element was
// performed" action: the 64-bit element key plus a stunt-element discriminant. DWARF home
// BrnGameActions.h:3469 (true owning home). Minimal slice -- exactly the two members the consumer
// reads. DealWithStunt branches on meStuntElementType (JUMP/SMASH/BILLBOARD, asserts >= COUNT is
// "Unknown world stunt type.") and Find/Inserts mId (the 64-bit CgsID stunt-element key) into the
// scorer's mRecentStuntElementSet to de-dupe repeated elements.
//
// ⛔⛔ [gateui] MEMBER ORDER CORRECTED 2026-08-20 -- THE COMMITTED ORDER WAS REVERSED, and the
// justification it cited was a misattribution. The GameAction<T> base is the empty tag, so member
// 1 sits at +0x00. Measured at BOTH ends of the wire:
//   PRODUCER  StuntManager::ProcessStuntElement @0x8239CDB0 builds the 16-byte record on the stack
//             and posts it as action 127 size 16 (@0x8239CE2C..0x8239CE40):
//               0x8239CE18  stw r24, var_78(r1)   ; var_78 == record +0x08  <- the TYPE
//               0x8239CE3C  std r11, var_80(r1)   ; var_80 == record +0x00  <- the 64-bit ID
//             (r24 is loaded from `lwz r24, 0x600(r31)` == meLastStuntElementType; r11 from
//              `lwz r11, 0x2C(r22)`/`0x24(r22)` == the region's group id / own id.)
//   CONSUMER  CrashModeScoring::DealWithShowtimeStunt @0x82320F38 does `ld r8, 0(r29)` at
//             0x82320FA8 -- a 64-bit load of the ID at +0x00.
// The committed header's cited justification ("StuntModeScoring::DealWithStunt @0x8232CEB0 reads
// it") does NOT arbitrate the layout at all: that function is called with SEPARATE (type in r4,
// id in r5) arguments by ScoringSystem::DealWithStunt @0x823384F0, never with an action pointer.
// Every tree consumer reads this struct BY NAME, so the swap is behaviour-preserving for them and
// FIXES the wire for the new producer.
struct WorldStuntAction : public GameAction<E_ACTION_WORLD_STUNT_PERFORMED>
{
    CgsID            mId;                  // +0x00  (X360 `std r11, +0x00` @0x8239CE3C)
    StuntElementType meStuntElementType;   // +0x08  (X360 `stw r24, +0x08` @0x8239CE18)
    // The X360 posts this record with size 16, i.e. the 4-byte type at +0x08 is followed by 4
    // bytes of tail padding -- which the host's natural 8-byte alignment for the leading CgsID
    // reproduces exactly (sizeof == 16 on both).
};

// [gateui] The three sibling stunt-element actions StuntManager::ProcessStuntElement @0x8239CDB0
// posts alongside 127/58. Each layout is asm-exact off the stack record the producer builds:
//
//   action 61 (size 4)  @0x8239D1A0: `HIDWORD(v51) = type` -- the type alone at +0x00.
//   action 59 (size 8)  @0x8239D3D0: `v51 = v8` where v8's HIGH dword is the type and its LOW
//                                    dword is the county -- on big-endian PPC that lands
//                                    type@+0x00, county@+0x04.
//   action 60 (size 4)  @0x8239D430: `HIDWORD(v51) = type` -- the type alone at +0x00.
//
// Homed here so the GameState producer and the BrnGame TranslateGameActionsToGuiEvents consumer
// (cases 59/60 -> GuiEventStuntAreaComplete(219) / GuiEventStuntAllComplete(220)) agree by name.
struct StuntElementBoostAction : public GameAction<E_ACTION_STUNT_ELEMENT_BOOST>
{
    StuntElementType meStuntElementType;   // +0x00
};

struct OnStuntElementCompleteForCountyAction
    : public GameAction<E_ACTION_ON_STUNT_ELEMENT_COMPLETE_FOR_COUNTY>
{
    StuntElementType  meStuntElementType;  // +0x00
    BrnWorld::ECounty meCounty;            // +0x04
};

struct OnStuntElementCompleteByTypeAction
    : public GameAction<E_ACTION_ON_STUNT_ELEMENT_COMPLETE_BY_TYPE>
{
    StuntElementType meStuntElementType;   // +0x00
};

// X360 0x82321530 (StuntModeScoring::DealWithPowerPark reads it). The power-park-scored result
// action: the park outcome enum plus the overall rating fed into the score multiplier. DWARF home
// BrnGameActions.h:3495 (true owning home). Minimal slice -- exactly the two members the consumer
// reads (meOutcome at +0x00 tested == E_PPO_SUCCESS, miOverallRating at +0x04 multiplied into the
// awarded score). meOutcome's enum type (BrnWorld::EPowerParkOutcome) is provisionally homed above;
// see that note.
struct PowerParkResultAction : public GameAction<E_ACTION_POWER_PARK_RESULT>
{
    BrnWorld::EPowerParkOutcome meOutcome;        // +0x00  (DWARF BrnGameActions.h:3497)
    s32                         miOverallRating;  // +0x04  (DWARF BrnGameActions.h:3498)
};

// "A stunt element has been completed" action. DWARF home BrnGameActions.h:3837 (true owning home);
// modelled here per the DWARF SHAPE -- the five named members the PS3 DecFIGS DWARF lists. The
// GameAction<T> base is the empty tag, so mID is at +0x00.
//
// GROWN -- X360/DWARF LAYOUT DIVERGENCE (consumer now LANDED): the X360 consumer that dereferences
// the in-progress stunt-element action, StuntModeScoring::DealWithInProgressStunt (0x82321710),
// reads a LARGER, convoy-shaped record that the lean 5-member DWARF layout does NOT express. The
// three convoy members below were GROWN ADDITIVELY (after the DWARF tail) at their X360-asm-proven
// offsets so the body can name them; the DWARF-named members above are NOT reordered/retyped:
//   +0x24  f32[8]  maConvoyLegDistances  (walked until a value >= flt_82CDB778)
//   +0x44  s32[8]  maConvoyMemberIds     (linear-searched for the player id; miss asserts
//                                         "Player not in this convoy!", BrnStuntModeScoring.cpp:1666)
//   +0x64  s32     miConvoyMemberCount   (the loop gate `count > 1`)
// The asm offsets are absolute (r31 = action base): 0x24, 0x44, 0x64. They are X360-only and
// DWARF-silent (the PS3 DecFIGS DWARF lists only the lean 5 members), so each carries an X360-proven
// FLAG. The 5 DWARF members end at +0x18 (mID@0x00 [u64,8B], meStuntElementType@0x08 [enum,4B],
// miCurrentCount@0x0C, miTotalCount@0x10, meCurrentGameMode@0x14 -- tail at +0x18); the convoy block
// starts at +0x24, leaving a 12-byte (+0x18..+0x23) DWARF-silent gap, modelled here as an explicit
// reserved pad so the convoy arrays land exactly at the asm-proven offsets. A static_assert below
// locks the three offsets.
//
// FLAG: maConvoyLegDistances / maConvoyMemberIds / miConvoyMemberCount + the +0x18 pad are
// X360-asm-proven and DWARF-silent (provisionally named per CXX_NAMING_CONVENTIONS); the +0x18..+0x23
// reserved region's true member(s) are unrecovered. Re-confirm names/extent when the full
// stunt-element action family is reconstructed against the X360 layout.
struct OnStuntElementCompleteAction : public GameAction<E_ACTION_ON_STUNT_ELEMENT_COMPLETE>
{
    CgsID            mID;                  // +0x00  (DWARF BrnGameActions.h:3839)
    StuntElementType meStuntElementType;   // +0x08  (DWARF BrnGameActions.h:3840)
    s32              miCurrentCount;        // +0x0C  (DWARF BrnGameActions.h:3841)
    s32              miTotalCount;          // +0x10  (DWARF BrnGameActions.h:3842)
    EGameModeType    meCurrentGameMode;     // +0x14  (DWARF BrnGameActions.h:3843; EGameModeType is
                                            //        nested in this GameStateModuleIO namespace)

    // ---- X360-only convoy block (GROWN additively; DWARF-silent; offsets asm-proven) ----
    u8               maReserved0x18[0x0C];  // +0x18 (X360-proven gap; DWARF-silent -- true member(s) unrecovered)
    f32              maConvoyLegDistances[8];   // +0x24 (X360-proven; DWARF-silent; walked vs flt_82CDB778)
    s32              maConvoyMemberIds[8];      // +0x44 (X360-proven; DWARF-silent; searched for player index)
    s32              miConvoyMemberCount;       // +0x64 (X360-proven; DWARF-silent; the count>1 gate)
};

// X360 BrnNetwork::StandingsManager::HandlePlayerFinishedMode (0x82550BB8) reads it; the per-player
// "this player has finished the mode/round" action. DWARF home BrnGameActions.h:1347 (the true owning
// home for the FULL member set + methods; only the SHAPE is modelled here so the StandingsManager TU
// can name the fields it dereferences). The GameAction<T> base is the empty tag, so mFinishTime is at
// +0x00, matching the X360 access pattern (HandlePlayerFinishedMode reads a3+0/+4 == mFinishTime,
// a3+20 == meEliminatorIndex, a3+32 == mfDistanceFromFinish, a3+40 == miEliminations,
// a3+45 == mbTimedOut, a3+46 == mbWonRound). Member order/offsets follow the DWARF source list
// (mFinishTime@0, mFastestLapTime@8, meFinishedGameModeType@16, meEliminatorIndex@20,
// meBeatenRivalIndex@24, miNumberOfTakedowns@28, mfDistanceFromFinish@32, miFinishPosition@36,
// miEliminations@40, mbIsOnlineGameMode@44, mbTimedOut@45), all of which the X360 asm corroborates.
//
// X360-ATTESTED, DWARF-OMITTED member: the binary reads a SEVENTH-from-end byte at +0x2E (a3+46)
// immediately after mbTimedOut and copies it into the per-player StandingsData "won round" byte and
// forwards it to PlayerFinishedRoundMessage::PrepareForSend's last argument. The PS3 DecFIGS DWARF
// models this action with mbTimedOut as its last bool and omits the +46 byte; the X360 build clearly
// carries it. Named mbWonRound here (FLAGGED -- the same round-outcome companion bool seen on
// BrnNetwork::PlayerFinishedRoundMessage; source name not independently attested).
struct FinishedModeAction : public GameAction<E_ACTION_FINISHED_MODE>
{
    CgsSystem::Time     mFinishTime;             // +0x00 (8)  DWARF BrnGameActions.h:1348
    CgsSystem::Time     mFastestLapTime;         // +0x08 (8)  DWARF BrnGameActions.h:1349
    EGameModeType       meFinishedGameModeType;  // +0x10      DWARF BrnGameActions.h:1351
    EActiveRaceCarIndex meEliminatorIndex;       // +0x14      DWARF BrnGameActions.h:1352 (a3+20)
    EGlobalRaceCarIndex meBeatenRivalIndex;      // +0x18      DWARF BrnGameActions.h:1353
    s32                 miNumberOfTakedowns;     // +0x1C      DWARF BrnGameActions.h:1355
    f32                 mfDistanceFromFinish;    // +0x20      DWARF BrnGameActions.h:1356 (a3+32)
    s32                 miFinishPosition;        // +0x24      DWARF BrnGameActions.h:1357
    s32                 miEliminations;          // +0x28      DWARF BrnGameActions.h:1358 (a3+40)
    bool                mbIsOnlineGameMode;      // +0x2C      DWARF BrnGameActions.h:1360
    bool                mbTimedOut;              // +0x2D      DWARF BrnGameActions.h:1361 (a3+45)
    bool                mbWonRound;              // +0x2E      X360-attested, DWARF-omitted (FLAGGED name) (a3+46)
};

// Lock the X360-asm-proven convoy offsets (0x24 / 0x44 / 0x64). The GameAction<T> base is the empty
// tag (no instance data/vtable), so member offsets are measured from the struct base == action base.
static_assert(offsetof(OnStuntElementCompleteAction, maConvoyLegDistances) == 0x24,
              "OnStuntElementCompleteAction::maConvoyLegDistances must be at X360 offset +0x24");
static_assert(offsetof(OnStuntElementCompleteAction, maConvoyMemberIds) == 0x44,
              "OnStuntElementCompleteAction::maConvoyMemberIds must be at X360 offset +0x44");
static_assert(offsetof(OnStuntElementCompleteAction, miConvoyMemberCount) == 0x64,
              "OnStuntElementCompleteAction::miConvoyMemberCount must be at X360 offset +0x64");

// ===== Freeburn-challenge actions (ChallengeManager keystone) =====

// DWARF BrnGameActions.h:5543 (member set + order). X360 size attested: ChallengeManager::
// WriteDataToOutput posts it with AddEvent(..., 155, 0x94) and 4+4+4+64+8+64 == 0x94 exactly.
// [2][8] == [action][player] (note the transposed order vs the manager's [player][action] grids).
struct FreeburnChallengeUpdateAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_UPDATE>
{
    f32 mfTimeLeftInChallenge;                            // 0x00 (:5546)
    s32 miCurrentActionIndex;                             // 0x04 (:5547)
    s32 miNumTargetsUsed;                                 // 0x08 (:5549)
    f32 maafIndividualTargetContributions[2][8];          // 0x0C (:5550)
    s32 maiOverallTargetRemaining[2];                     // 0x4C (:5551)
    EFreeburnChallengeSuccess maaeCompleted[2][8];        // 0x54 (:5552)
}; // sizeof == 0x94

// DWARF BrnGameActions.h:5590. X360 size attested: UpdateActionSuccess posts AddEvent(..., 158, 0x10).
struct FburnChallengeSuccessUpdateAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_SUCCESS_UPDATE>
{
    // DWARF also spells the LastSecondChallengeSuccess typedef at :3142 inside this struct;
    // the owning definition is BrnGameStateSharedIO.h:313 (FastBitArray<60>).
    LastSecondChallengeSuccess mChallengeSuccessUpdate; // 0x00 (:5592, FastBitArray<60>)
    s32 miActionIndex;                                  // 0x08 (:5593)
}; // sizeof == 0x10

// DWARF BrnGameActions.h:5604. X360 size attested: UpdateChallenge posts AddEvent(..., 159, 0xC).
struct FburnChallengeSuccessAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_SUCCESS>
{
    f32  mafActionScores[2];          // 0x00 (:5606)
    bool mabSuccessfulActions[2];     // 0x08 (:5607)
    bool mabAccumulationThisFrame[2]; // 0x0A (:5608)
}; // sizeof == 0xC

// The plain freeburn-challenge lifecycle action (id 153). DWARF BrnGameActions.h:5514 (member
// set + order). X360 size attested: SEVEN ChallengeManager sites post it with
// AddEvent(..., 153, 0x20) -- BeginChallenge 0x823505B8, TriggerFreeburnChallenge 0x82346DA0,
// EndChallenge 0x8234DE30, UpdateResults 0x82345FD0, UpdateArbitration 0x82351AF8,
// UpdateArbitrationSuccess 0x82350340, UpdateChallenge 0x82347190 -- and every field store below
// is offset-attested at those sites (CgsID @0x00, s32 @0x08/0x0C/0x10/0x14/0x18, byte @0x1C/0x1D,
// 2B tail pad -> 0x20).
// meEventType is BrnNetwork::BrnNetworkModuleIO::EChallengeEventType (DWARF :5517) stored as s32
// (same 4B storage; the StartNetworkGameEvent::meBoostType precedent -- including the heavy
// BrnNetworkModuleIO.h here is not warranted). X360-DRIFT WARNING on its VALUES: the begin(0)/
// trigger(1)/action-success(2)/reset(3) posts match the committed PS3-DWARF enumerators 1:1, but
// EndChallenge stores 5 and UpdateResults stores 6 where the PS3 enum spells ENDED=4/
// RESULTS_FINISHED=5 -- the X360 enum carries one extra (unattested) enumerator before ENDED.
// Store the raw attested value with a drift comment; do NOT "fix" 5/6 back to the PS3 names.
struct FreeburnChallengeAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE>
{
    CgsID mChallengeID;                  // 0x00 (:5516)
    s32   meEventType;                   // 0x08 (:5517, BrnNetworkModuleIO::EChallengeEventType; s32 storage)
    EChallengeStatus meChallengeStatus;  // 0x0C (:5518, BrnGameState::EChallengeStatus)
    s32   miActionIndex;                 // 0x10 (:5519)
    s32   miNumChallengesComplete;       // 0x14 (:5520; -1 == "not supplied" at most sites)
    s32   miTotalNumChallenges;          // 0x18 (:5521; -1 == "not supplied" at most sites)
    bool  mbIsHost;                      // 0x1C (:5522)
    bool  mbAbortingToStartNewChallenge; // 0x1D (:5523)

    void Construct();                    // :5527 declared-only (not in this TU's X360 ledger)
}; // sizeof == 0x20

// The empty end-not-active notification action (id 154). DWARF BrnGameActions.h:5531 (an EMPTY
// struct -- no members). X360 size attested: CancelFreeburnChallenge / RemoteEndChallenge /
// UpdateArbitrationSuccess post AddEvent(..., 154, 1) from an uninitialised stack byte.
// (The already-committed partfiles post the bare GameAction<E_ACTION_FREEBURN_CHALLENGE_END_
// NOT_ACTIVE> tag, which is layout-identical; prefer this named struct in new bodies.)
struct FburnChallengeEndNotActiveAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE>
{
}; // sizeof == 1 (empty)

// One player's freeburn-challenge completion-status action (id 156). DWARF BrnGameActions.h:5563.
// X360 size attested: NetworkPlayerFinalised 0x82347E88 posts AddEvent(..., 156, 0x108) after
// memcpy'ing the manager's 256-byte local completion bit store to +0x00 and stamping the player
// id at +0x100 (stw sp+0x180). NOTE the field ORDER differs from CompletedFburnChallengesData
// (the EventQueue element, id first) -- here the bit store leads.
struct FburnChallengeStatusAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_COMPLETION_STATUS>
{
    CompletedFburnChallenges    mCompletedChallenges; // 0x000 (:5565, FastBitArray<2000> == 256B)
    BrnNetwork::NetworkPlayerID mPlayerID;            // 0x100 (:5566)
}; // sizeof == 0x108 (4B tail pad)

// The "show the challenge selector" GUI action (id 160). DWARF BrnGameActions.h:5619.
// X360 size attested: NetworkPlayerRemoved 0x8234E420 (posts it ZEROED -- std r0 -> mChallengeID
// == 0), Disconnected-family and UpdateArbitration / UpdateArbitrationSuccess post
// AddEvent(..., 160, 8).
struct FburnChallengeShowSelectorAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR>
{
    CgsID mChallengeID;   // 0x00 (:5621)
}; // sizeof == 8

// The "this is the currently active freeburn challenge + its participants" action (id 161).
// DWARF BrnGameActions.h:5632 (member SET; see drift note). X360 size + layout attested:
// NetworkPlayerFinalised 0x82347E88 builds it on the stack (sp+0x50) and posts
// AddEvent(..., 161, 0x30): CgsID @0x00 (std, mpCurrentChallenge id), the ARCI slots filled from
// +0x08 (stwx r30, 4*count + sp+0x58), the destination player id @0x24 (stw sp+0x74) and the
// participant count @0x28 (stw sp+0x78).
// X360-vs-DWARF MEMBER-ORDER DRIFT: the PS3 DWARF lists maePlayersInChallengeARCI FIRST (id at
// +0x20) -- the X360 build stores the id at +0x00 and the ARCI block at +0x08. Member names are
// the DWARF's; the ORDER below is X360-authoritative. (The matching *event* consumed by
// ProcessEvent case 173 KEEPS the PS3 order -- see ActiveFburnChallengeEvent in BrnGameEvents.h.)
struct ActiveFburnChallengeAction : public GameAction<E_ACTION_ACTIVE_FREEBURN_CHALLENGE>
{
    // == the count the NetworkPlayerFinalised bound assert spells:
    // "lActiveChallengeAction.miNumPlayersInChallenge < KI_MAX_NETWORK_PLAYERS"
    static const s32 KI_MAX_NETWORK_PLAYERS = 7;

    CgsID                       mChallengeID;                // 0x00 (:5635; X360 FIRST -- drift)
    EActiveRaceCarIndex         maePlayersInChallengeARCI[KI_MAX_NETWORK_PLAYERS]; // 0x08..0x23 (:5634)
    BrnNetwork::NetworkPlayerID mPlayerToSendToID;           // 0x24 (:5636)
    s32                         miNumPlayersInChallenge;     // 0x28 (:5637)
}; // sizeof == 0x30 (4B tail pad)

// Pin the X360-attested offsets of the freeburn action family (all pointer-free -> absolute on
// the x64 gate too). GameAction<T> is the empty tag base, so struct base == action base.
static_assert(sizeof(FreeburnChallengeAction) == 0x20, "FreeburnChallengeAction must be 0x20 bytes (AddEvent size)");
static_assert(offsetof(FreeburnChallengeAction, meEventType) == 0x08, "FreeburnChallengeAction::meEventType at +0x08");
static_assert(offsetof(FreeburnChallengeAction, mbIsHost) == 0x1C, "FreeburnChallengeAction::mbIsHost at +0x1C");
static_assert(sizeof(FburnChallengeStatusAction) == 0x108, "FburnChallengeStatusAction must be 0x108 bytes (AddEvent size)");
static_assert(offsetof(FburnChallengeStatusAction, mPlayerID) == 0x100, "FburnChallengeStatusAction::mPlayerID at +0x100");
static_assert(sizeof(FburnChallengeShowSelectorAction) == 8, "FburnChallengeShowSelectorAction must be 8 bytes (AddEvent size)");
static_assert(sizeof(ActiveFburnChallengeAction) == 0x30, "ActiveFburnChallengeAction must be 0x30 bytes (AddEvent size)");
static_assert(offsetof(ActiveFburnChallengeAction, maePlayersInChallengeARCI) == 0x08, "ActiveFburnChallengeAction ARCI block at +0x08");
static_assert(offsetof(ActiveFburnChallengeAction, mPlayerToSendToID) == 0x24, "ActiveFburnChallengeAction::mPlayerToSendToID at +0x24");
static_assert(offsetof(ActiveFburnChallengeAction, miNumPlayersInChallenge) == 0x28, "ActiveFburnChallengeAction::miNumPlayersInChallenge at +0x28");

// ===== Road-rules batch query (StreetManager keystone, wave B) =====
// ADDITIVE GROW. DWARF BrnGameActions.h:2833 (members :4808..:4814); the member ORDER
// below is X360-authoritative from StreetManager::FillInRoadRulesQuery @ 0x823365A8,
// which stores miNumRoads at +512 (right after the ids) and the four bool arrays at
// +516/+580/+644/+708 with the owns-all flag at +772 -- the PS3 DWARF lists miNumRoads
// after the bool arrays; the X360 build places it before them.
// The two "beaten" pairs are indexed by BrnStreetData::ScoreType (0 == TIME, 1 == CRASH).
struct RoadRulesBatchQueryAction : public GameAction<E_ACTION_ROAD_RULES_BATCH_QUERY>
{
    ::CgsID maRoadIds[64];                 // +0    (:4808) one CgsID per road
    s32     miNumRoads;                    // +512  (:4813) asserted <= KI_MAX_CHALLENGES
    bool    mabPlayerBeatenParTime[64];    // +516  (:4809) HasPlayerBeatenParScore(i, TIME)  == BEATEN
    bool    mabPlayerBeatenParCrash[64];   // +580  (:4810) HasPlayerBeatenParScore(i, CRASH) == BEATEN
    bool    mabPlayerBestOnlineTime[64];   // +644  (:4811) HasPlayerBeatenFriendScore(i, TIME)  == BEATEN
    bool    mabPlayerBestOnlineCrash[64];  // +708  (:4812) HasPlayerBeatenFriendScore(i, CRASH) == BEATEN
    bool    mbPlayerOwnsAllRoadsOffline;   // +772  (:4814) ProgressionManager complete-roads tally >= 64
};
// WIRE-FORMAT PIN (added with the 262 -> 275 value correction, 2026-08-26). The record is
// pointer-free, so the X360 offsets are absolute on the x64 gate too, and the console hands
// AddEvent the literal `li r6, 0x308` == 776 at 0x823A4974 -- immediately after
// StreetManager::FillInRoadRulesQuery filled this very buffer. Nothing was checking that the host
// layout still matched that size.
static_assert(sizeof(RoadRulesBatchQueryAction) == 776,
              "X360 ProcessGameEvents posts action 275 with size 0x308 (776)");
static_assert(offsetof(RoadRulesBatchQueryAction, miNumRoads) == 512,
              "RoadRulesBatchQueryAction::miNumRoads at +512 (FillInRoadRulesQuery @0x823365A8)");
static_assert(offsetof(RoadRulesBatchQueryAction, mbPlayerOwnsAllRoadsOffline) == 772,
              "RoadRulesBatchQueryAction::mbPlayerOwnsAllRoadsOffline at +772");

// ===== Completed-stunt action (id 15) =====
// The one-shot "the player just finished a stunt" record. DWARF home BrnGameActions.h:782
// (`struct CompletedStuntAction : public GameAction<E_ACTION_COMPLETED_STUNT>`), which is this
// file -- so this is the TRUE owning home, not a re-home.
//
// It is a straight repack of the physics-side CompletedStuntEvent (BrnGameEvents.h) down to the
// eight fields the game-side consumers actually need. Every offset below is attested at BOTH ends.
//
// PRODUCER -- GameStateModule::ProcessGameEvents @0x823A0A18, the `case 119`
// (E_EVENT_COMPLETED_STUNT) arm. r25 == the incoming CompletedStuntEvent*, r1+var_1C60 == the
// 32-byte payload base handed to AddEvent (`addi r4, r1, var_1C60` @0x823A196C):
//     0x823A1950  lwz  r11, 0x00(r25)  -> 0x823A1978  stw  +0x00
//     0x823A1954  lfs  f0,  0x58(r25)  -> 0x823A1958  stfs +0x04
//     0x823A1960  lfs  f0,  0x5C(r25)  -> 0x823A1968  stfs +0x08   (event mfCompletedAirSpinAngle)
//     0x823A1970  lfs  f0,  0x60(r25)  -> 0x823A197C  stfs +0x0C
//     0x823A1984  lfs  f0,  0x64(r25)  -> 0x823A1988  stfs +0x10
//     0x823A198C  lfs  f0,  0x68(r25)  -> 0x823A1990  stfs +0x14   (event mfCompletedDriftDistance)
//     0x823A1998  lwz  r11, 0x50(r25)  -> 0x823A199C  stw  +0x18   (event miCompletedBarrelRolls)
//     0x823A1980  lbz  r11, 0x80(r25)  -> 0x823A1994  stb  +0x1C   (event mbSuccessfulLanding)
//   then `li r5, 0xF` (action id 15) + `li r6, 0x20` (SIZE 32) -> AddEvent @0x823A19A0.
//   Two of those source offsets are already NAMED on the committed CompletedStuntEvent
//   (+0x5C mfCompletedAirSpinAngle, +0x68 mfCompletedDriftDistance, +0x50 miCompletedBarrelRolls,
//   +0x80 mbSuccessfulLanding), and they land on the DWARF members of the same name here -- which
//   is what pins the ORDER of the float run, not just its extent.
//
// CONSUMERS -- the three BoostStrategy::UpdateStuntBoost overrides (vtable slot 48), which are
// byte-identical in shape: BoostBurnout2 @0x822A6478, BoostBurnout3 @0x822A6708,
// BoostBurnout5 @0x822A6F98. Taking BoostBurnout2 (r30 == the action, r31 == this):
//     0x822A64BC  lwz  r11, 0x00(r30)  -- the FIRST member; the empty GameAction<> base adds no
//                                         offset, exactly as for every sibling in this file
//     0x822A64C0  clrlwi r11, r11, 31          bit 0 -> pays mfBarrelRollEarning
//     0x822A64CC  lwz  r11, 0x18(r30) ; 0x822A64DC extsw   -- a SIGN-EXTENDED WORD, i.e. int32
//     0x822A6504  rlwinm r11, r11, 0,30,30     bit 1
//     0x822A6518  lfs  f13, 0x08(r30)          -> pays mfAirSpinEarning
//     0x822A6534  rlwinm r11, r11, 0,29,29     bit 2
//     0x822A6548  lfs  f13, 0x0C(r30)          -> pays mfHandbrake180Earning
//     0x822A6564  rlwinm r11, r11, 0,28,28     bit 3 (flat award, reads no payload)
//   (BoostBurnout3 does the same reads at 0x822A675C / 0x822A67A8 / 0x822A67D8; BoostBurnout5 at
//    0x822A6FEC / 0x822A7038 / 0x822A7068.)
//
// ORDER NOTE -- a real X360-vs-DWARF divergence. The PS3 DWARF lists `bool mbSuccessfulLanding`
// (:791) BEFORE `int32_t miCompletedBarrelRolls` (:793), which would put the bool at +0x18 and the
// int at +0x1C. The X360 build is the other way round and says so twice, independently: the
// producer `stb`s the landing byte to +0x1C and `stw`s the roll count to +0x18 (0x823A1994 /
// 0x823A199C), and all three consumers `lwz`+`extsw` a full word from +0x18. The X360 order below
// is therefore authoritative (rung 1 over rung 2); do not "correct" it back to the DWARF.
//
// muStuntActionComplete is left as the raw u32 the DWARF declares -- it is the physics detector's
// bitfield and its enumerators already have a recovered home in
// GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManagerShared.h
// (`BrnPhysics::EStuntActionComplete`, DWARF-verbatim), whose BARREL_ROLL/AIR_SPIN/HANDBREAK_TURN/
// CLEANLANDING == 1/2/4/8 are exactly the four bits the consumers above test. Consumers mask with
// that enum (or with the literals) -- no parallel enum is minted here.
struct CompletedStuntAction : public GameAction<E_ACTION_COMPLETED_STUNT>
{
    u32  muStuntActionComplete;         // +0x00 (:784) BrnPhysics::EStuntActionComplete bitfield
    f32  mfCompletedBarrelRollAngle;    // +0x04 (:786)
    f32  mfCompletedAirSpinAngle;       // +0x08 (:787)
    f32  mfCompletedHandbreakTurnAngle; // +0x0C (:788)
    f32  mfCompletedDriftTime;          // +0x10 (:789)
    f32  mfCompletedDriftDistance;      // +0x14 (:790)
    s32  miCompletedBarrelRolls;        // +0x18 (:793) X360 order -- see the ORDER NOTE above
    bool mbSuccessfulLanding;           // +0x1C (:791) X360 order -- see the ORDER NOTE above
                                        // +0x1D..+0x1F tail pad of the 32-byte AddEvent image
};

// Pointer-free -> the X360 offsets are absolute on the x64 gate too.
static_assert(sizeof(CompletedStuntAction) == 32,
              "CompletedStuntAction must be the 32-byte image AddEvent(.., 15, 32) posts");
static_assert(offsetof(CompletedStuntAction, muStuntActionComplete) == 0x00,
              "the stunt bitfield is the first member (BoostBurnout2::UpdateStuntBoost lwz 0(r30))");
static_assert(offsetof(CompletedStuntAction, mfCompletedAirSpinAngle) == 0x08,
              "air-spin angle at +0x08 (UpdateStuntBoost lfs f13, 8(r30))");
static_assert(offsetof(CompletedStuntAction, mfCompletedHandbreakTurnAngle) == 0x0C,
              "handbrake-turn angle at +0x0C (UpdateStuntBoost lfs f13, 0xC(r30))");
static_assert(offsetof(CompletedStuntAction, miCompletedBarrelRolls) == 0x18,
              "barrel-roll COUNT at +0x18 (UpdateStuntBoost lwz+extsw 0x18(r30)) -- not the DWARF's bool");
static_assert(offsetof(CompletedStuntAction, mbSuccessfulLanding) == 0x1C,
              "landing flag at +0x1C (ProcessGameEvents stb @0x823A1994)");

// ARTIST HandleGameActions case 198 reads type/+0x14, boost/+0x0C and
// control/+0x08 before calling HandleCarStatsUpdate @0x822A4700. DecFIGS
// supplies the exact source member names and declaration order.
struct SendCarStatsAction : public GameAction<E_ACTION_UPDATE_CAR_STATS>
{
    s32                   miCarSpeed;     // +0x00
    s32                   miCarStrength;  // +0x04
    s32                   miCarControl;   // +0x08 (boost-loss level at this consumer)
    s32                   miCarBoost;     // +0x0C
    f32                   mfDamageLimit;  // +0x10
    BrnResource::ECarType meCarType;      // +0x14
};

static_assert(sizeof(SendCarStatsAction) == 24,
              "ARTIST SendCarStatsAction case-198 payload is 24 bytes");
static_assert(offsetof(SendCarStatsAction, miCarControl) == 0x08,
              "HandleGameActions case 198 loads boost-loss from +0x08");
static_assert(offsetof(SendCarStatsAction, miCarBoost) == 0x0C,
              "HandleGameActions case 198 loads boost level from +0x0C");
static_assert(offsetof(SendCarStatsAction, meCarType) == 0x14,
              "HandleGameActions case 198 loads ECarType from +0x14");

// ===========================================================================================
// ⭐⭐ [evt-flow wave E1, 2026-08-26] THE EVENT-FLOW ACTION RECORDS.
//
// The seven payloads TranslateGameActionsToGuiEvents @0x823E9CE0 repacks into the event-flow GUI
// events (arms in GameSource/Game/GameBridgeGameStateToX_EventFlowGuiEvents.cpp). Every offset
// below is a STORE the producer emits or a LOAD the consumer emits -- both cited per field. The
// GameAction<T> base is the empty tag, so member offsets are measured from the action base, and
// each record's sizeof is pinned to the producer's AddEvent size literal.
//
// ⚠️ NAMING HONESTY. Where the producer's own local/symbol names the field, the member carries
// that name. Where the console only proves a width and an offset, the member is `miFieldNN` /
// `mu8FieldNN` and is FLAGGED in its comment. Do not "tidy" a FieldNN name into a guess.
// ===========================================================================================

// ---- 201 ---------------------------------------------------------------------------------
// The junction/event-availability record: what the player is standing at, and whether they may
// start it. Producer CheckIfPlayerIsAtJunctionWithAnEvent @0x82390418
// (`li r6,0x28 / li r5,0xC9` @0x82390DCC -> AddEvent(q, &var_C0, 201, 40)); the store map is the
// var_C0-based frame at 0x82390954..0x823909B8 (the "at a junction with an event" post) and
// 0x82390E6C..0x82390EA8 (the "left the junction" post). Consumer @0x823EA810 copies ten of the
// twelve fields into GuiEventJunctionInfo (id 311) -- the mapping is 1:1 by name, which is what
// pins the SEMANTICS of every field the GUI reads (BrnGuiEventTypeDefs.h GuiEventJunctionInfo).
struct JunctionInfoAction : public GameAction<E_ACTION_EVENT_AT_JUNCTION_AVAILABLE>
{
    u32   muJunctionLogicBoxId;    // +0x00 JunctionLogicBox+0x00, cached at gsm+284364. NOT read
                                   //       by the GUI arm. FLAG: field name from its source, not
                                   //       from an attested member name.
    u32   muEventJunctionID;       // +0x04 JunctionLogicBox+0x38 -> GuiEventJunctionInfo::miEventID
    u32   muLightTriggerId;        // +0x08 the packed traffic-light trigger id (tag 0x39|hull<<8|light);
                                   //       NOT read by the GUI arm.
    u8    maPad0C[4];              // +0x0C never stored (the CgsID below forces 8-alignment)
    CgsID mSpecialEventCarId;      // +0x10 RaceEventData+0x10 -> GuiEventJunctionInfo::mSpecialEventCarId
    EGameModeType meGameModeType;  // +0x18 ProgressionManager::GetEvent(...) -- the RUNTIME mode enum,
                                   //       10 (E_MODE_OFFLINE_COUNT) on the "no event here" post
    s8    mi8Difficulty;           // +0x1C -1 on the "no event here" post
    s8    mi8MedalAchieved;        // +0x1D Profile::GetMedalAchievedForEventWithID, -1 when none
    bool  mbOnEntry;               // +0x1E 1 on the arrival post, 0 on the departure post
    bool  mbCanEnterEvent;         // +0x1F gsm+284369 (the "may start" gate)
    bool  mbEventUnlocked;         // +0x20
    bool  mbSpecificCarEventValid; // +0x21
    bool  mbIsNewlyDiscovered;     // +0x22 gsm+284368
    bool  mbIsAutoUnlockedChallenge;// +0x23
    u8    maPad24[4];              // +0x24 tail padding to the attested 40
};
static_assert(sizeof(JunctionInfoAction) == 40,
              "X360 CheckIfPlayerIsAtJunctionWithAnEvent posts action 201 with size 40");
// ---- 208 ---------------------------------------------------------------------------------
// [drive-thru wave 2026-08-27] The game-completion results record. Producer
// ProgressionManager::SendGameCompletionResults @0x82395C28; every field is one of its stores
// into the var_30-based frame it hands to AddEvent (`li r6, 8`):
//     stw  r11, var_30   rec+0x00  <- lwz r11, 0xD94(mpModeManager) == meCurrentGameModeType
//     stb  r30, var_2C   rec+0x04  <- 1 on the >= 100% arm, 0 on the other (li r11,0 / stb x2)
//     stb  r10, var_2B   rec+0x05  <- lbzx manager+118400 == Profile+118032,
//                                     mb100PercentCompletionSequenceShown
// The two trailing bytes are never stored; they are the record's tail padding to the attested 8.
// ⚠️ The console does NOT clear the frame -- rec+0x06/0x07 carry stack residue, and on the
// below-100% arm rec+0x00 still holds the mode. Value-initialised here (ours, not the console's).
struct GameCompletionResultsAction : public GameAction<E_ACTION_GAME_COMPLETION_RESULTS>
{
    EGameModeType meGameMode;                  // +0x00 the mode the player was in when it landed
    bool          mbGameComplete;              // +0x04 ComputeCompletionPercentage() >= 100.0f
    bool          mbCompletionAlreadyRecorded;  // +0x05 Profile::GetSeen100PercentCompletionSequence()
    u8            maPad06[2];                   // +0x06 never stored; pads to the producer's `li r6,8`
};
static_assert(sizeof(GameCompletionResultsAction) == 8,
              "X360 SendGameCompletionResults posts action 208 with size 8");

// ---- 146 ---------------------------------------------------------------------------------
// [showtime S7b-b, 2026-08-27] The showtime-intro latch record. Producer: the DetectModeStarts
// @0x8239A428 `else` arm (both posts); consumer: PhysicsModule::HandleGameActions case 146.
//
// ⚠️ THE FIELD ORDER IS THE X360's, NOT THE DWARF's, AND THE TWO DISAGREE. DecFIGS
// (BrnGameActions.h:4771) declares `struct ShowtimeIntro { bool mbStart; Vector3 mAimDirection; }`
// -- bool first. The X360 frame is the other way round and both ends prove it:
//   * producer @0x8239A674..0x8239A67C stores the 16-byte direction at var_A0 (the record base,
//     displacement ZERO) and the bool at var_90 == record+16 (0x8239A664 / 0x8239A8D8);
//   * consumer @0x825A785C does `lvx128 v1, r0, r29` -- displacement ZERO -- on the payload
//     pointer before calling RaceCarPhysics::SetShowtimeAimDirection.
// Rung 1 (ARTIST asm) arbitrates over rung 2 (DWARF declaration shape), so the vector leads.
// The 32-byte size is the console's own `li r6,0x20` and follows from Vector3's 16-byte
// alignment (16 + 1 -> 32).
struct ShowtimeIntroAction : public GameAction<E_ACTION_SHOWTIME_INTRO_START>
{
    Vector3 mAimDirection;   // +0x00 RCEntityActiveRaceCarOutputInterface::GetPlayerDirection()
    bool    mbStart;         // +0x10 true == the intro is armed, false == it was cancelled
};
static_assert(sizeof(ShowtimeIntroAction) == 32,
              "X360 DetectModeStarts posts action 146 with size 32");
static_assert(offsetof(ShowtimeIntroAction, mAimDirection) == 0x00 &&
              offsetof(ShowtimeIntroAction, mbStart) == 0x10,
              "action-146 field offsets are the producer's var_A0/var_90 stores");

// ---- 272 ---------------------------------------------------------------------------------
// [stuntrace wave D, D3] The wrong-car abort. One CgsID: the car the junction's event demands.
// Producer StartModeAtLights @0x82396CF8 (the `std r11, var_3E0` / `li r6,8` pair cited on the
// enumerator). No consumer arm exists in the tree yet -- the console's is inside
// TranslateGameActionsToGuiEvents @0x823E9CE0, which is a partial here; the record is landed
// with the producer so the post is typed rather than a bare stack blob.
struct WrongCarForChallengeAction : public GameAction<E_ACTION_WRONG_CAR_FOR_CHALLENGE>
{
    CgsID mSpecialEventCarId;      // +0x00 RaceEventData+0x10
};
static_assert(sizeof(WrongCarForChallengeAction) == 8,
              "X360 StartModeAtLights posts action 272 with size 8");

static_assert(offsetof(JunctionInfoAction, mSpecialEventCarId) == 0x10 &&
              offsetof(JunctionInfoAction, meGameModeType) == 0x18 &&
              offsetof(JunctionInfoAction, mbIsAutoUnlockedChallenge) == 0x23,
              "action-201 field offsets are the @0x823EA810 consumer loads");

// ---- 200 ---------------------------------------------------------------------------------
// Producer ProgressionManager::UpdatePlayerMedals @0x8239FE50 (AddEvent(q, &v29, 200, 8)); the
// consumer @0x823EA784 loads four halfwords at +0/+2/+4/+6 and copies them straight into
// GuiEventMedalUpdate (id 307, size 8), after streaming the first three into the
// "Medals update: " debug line.
struct UpdatePlayerMedalsAction : public GameAction<E_ACTION_UPDATE_PLAYER_MEDALS>
{
    s16 mi16TotalWins;        // +0x00 Profile::GetTotalWinCount's first out-param
    s16 mi16Field02;          // +0x02 FLAG: GetTotalWinCount's per-medal array element [0];
                              //       which medal tier it counts is not attested.
    s16 mi16Field04;          // +0x04 FLAG: GetTotalWinCount's third out-param
    s16 mi16WinsToNextRank;   // +0x06 GetTotalWinsForNextRank() - total, clamped at 0;
                              //       -1 when the player is already at the top rank
};
static_assert(sizeof(UpdatePlayerMedalsAction) == 8,
              "X360 UpdatePlayerMedals posts action 200 with size 8");

// ============================================================================================
// [stuntrace waveB fix round, 2026-08-26] THE FOUR MODE-INTRO / PLAY RECORDS.
// Filed as header_requests R1-R4 by the intro/play partfile and applied here after re-deriving
// every id, every size and every field from the exports + the DecFIGS DWARF:
//   * field NAMES and ORDER are DWARF-exact (dwarfdump .../BrnGameActions.h:1170-1179,
//     :1191-1194, :1330-1333, :3523) -- reproduced member-for-member, nothing invented;
//   * every sizeof is the console's own `li r6,<size>` immediate at the AddEvent site.
// ============================================================================================

// ---- 29 ----------------------------------------------------------------------------------
// Producer ModeManager::StartModeIntro @0x82343018 (`li r6,0x25C` / `li r5,0x1D` @0x823432D4/D8).
// The console builds it on the stack from var_2B0 (the f32 at the record base) through var_56 and
// posts &var_2B0 with size 604. The 592-byte FlybyData sits immediately after the f32, which is
// what makes the record 4 + 592 + 4 + 3 == 603 -> 604 with one byte of tail padding.
struct StartModeIntroAction : public GameAction<E_ACTION_START_MODE_INTRO>
{
    f32           mfDurationSeconds;         // +0x000  mode vtbl+32 GetIntroDurationSeconds()
    FlybyData     mFlybyData;                // +0x004  592 B, filled by FlybyData::Prepare/AddCar
    EGameModeType meGameMode;                // +0x254  ModeManager+3476 (0xD94)
    bool          mbFinishedOnlineEvent;     // +0x258  ModeManager+38151 (0x9507)
    bool          mbFinishedOnlineLobbyMode; // +0x259  ModeManager+38152 (0x9508)
    bool          mbDoIntro;                 // +0x25A  mfDurationSeconds > 0.0f
    u8            maPad25B[1];               // +0x25B  tail padding to the attested 604
};
static_assert(sizeof(StartModeIntroAction) == 604,
              "X360 StartModeIntro posts action 29 with size 0x25C");
static_assert(offsetof(StartModeIntroAction, meGameMode) == 0x254,
              "action-29 FlybyData must be the console's 592-byte block at +0x004");

// ---- 30 ----------------------------------------------------------------------------------
// Producer ModeManager::StopModeIntro @0x82343F38 (`li r6,8` / `li r5,0x1E` @0x82343FC4/CC).
struct StopModeIntroAction : public GameAction<E_ACTION_STOP_MODE_INTRO>
{
    EGameModeType meGameMode;               // +0x00  ModeManager+3476 (`lwz r11,0xD94` @0x82343F7C)
    bool          mbMovingBetweenLobbyModes;// +0x04  IsOnlineModeWithInstantIntro()
    u8            maPad05[3];               // +0x05  tail padding to the attested 8
};
static_assert(sizeof(StopModeIntroAction) == 8,
              "X360 StopModeIntro posts action 30 with size 8");

// ---- 34 ----------------------------------------------------------------------------------
// Producer ModeManager::StartPlayingMode @0x82343340 (`li r6,0x10` / `li r5,0x22`
// @0x823433FC/0x82343400). The CgsID is 8-byte aligned, so it lands at +0x08 and the record is 16.
struct StartPlayingModeAction : public GameAction<E_ACTION_START_PLAYING_MODE>
{
    EGameModeType meGameMode;               // +0x00  `lwz r10,0xD94` -> var_40
    u8            maPad04[4];               // +0x04  alignment ahead of the 8-byte id
    CgsID         mDestinationLandmarkID;   // +0x08  zeroed first (`std r27`), then maLandmarkCgsIDs[next]
};
static_assert(sizeof(StartPlayingModeAction) == 16,
              "X360 StartPlayingMode posts action 34 with size 0x10");
static_assert(offsetof(StartPlayingModeAction, mDestinationLandmarkID) == 8,
              "action-34 landmark id is the second qword of the 16-byte record");

// ---- 149 ---------------------------------------------------------------------------------
// Producer ModeManager::StartModeIntro @0x82343018's online-stunt training tip (`li r6,4` /
// `li r5,0x95` @0x8234331C/0x82343320); ModeManager::UpdateCurrentMode, CarSelectManager and
// StreetManager post the same 4-byte record. The consumer (BrnRaceCarEntityModule.cpp:2469,
// HandleGameActions case 149 -> AddTrainingRequest) reads the payload as a bare s32, so the wire
// format was already pinned at both ends -- only the producer-side record was missing.
struct RequestGameTrainingAction : public GameAction<E_ACTION_REQUEST_GAME_TRAINING>
{
    BrnProgression::ETrainingType meTrainingType;   // +0x00  DWARF BrnGameActions.h:3525
};
static_assert(sizeof(RequestGameTrainingAction) == 4,
              "X360 posts action 149 with size 4");

// ---- 47 ----------------------------------------------------------------------------------
// Producer ModeManager::CheckCountdownDisplay @0x82342898: the single out-param of
// GameMode::HasCountdownDisplayChanged, posted only when that returns true.
// Consumer @0x823EAD50 copies the word into GuiEventUpdateEventCountdown (id 234, size 4).
struct SetCountdownAction : public GameAction<E_ACTION_SET_COUNTDOWN>
{
    s32 miCountdownDisplay;   // +0x00
};
static_assert(sizeof(SetCountdownAction) == 4,
              "X360 CheckCountdownDisplay posts action 47 with size 4");

// ---- 75 ----------------------------------------------------------------------------------
// Producer CarSelectManager::StartCarSelectState @0x823872D0 (`li r5,0x4B / li r6,4`), whose
// only value is 1 ("the junkyard car-select screen is ready -- bring the GUI up"). The record
// is already posted by this tree (BrnCarSelectManager.cpp:560, `s32 liReady = 1`).
// Consumer @0x823EA700 forwards the word verbatim into GuiCarSelectStartEvent (id 81, size 4);
// BrnGui::InGame::Update's case-81 arm then does SendStateEvent("TO_CSELECT").
struct CarSelectReadyAction : public GameAction<E_ACTION_CAR_SELECT_READY>
{
    s32 miCarSelectFlow;      // +0x00  1 == the offline junkyard flow (the only value posted)
};
static_assert(sizeof(CarSelectReadyAction) == 4,
              "X360 StartCarSelectState posts action 75 with size 4");

// ---- 44 ----------------------------------------------------------------------------------
// Producer ModeManager::StartModeIntro @0x82343018 (`li r6,4 / li r5,0x2C` @0x82343060): stores
// the halfword at +0x00 from the file-scope word_82CDB7D4 and ZERO into the byte at +0x02.
// Consumer @0x823EA948 branches on +0x02 and emits GuiEventEnterEventStartLocation (id 166, size 8).
struct SetInModeStartRegionAction : public GameAction<E_ACTION_SET_IN_MODE_START_REGION>
{
    u16 mu16StartLocationId;  // +0x00 FLAG: name from the consumer's use (it becomes the GUI
                              //       record's id word); the producer sources it from word_82CDB7D4.
    u8  mbInStartRegion;      // +0x02 0 == not in a start region (StartModeIntro always posts 0)
    u8  maPad03[1];           // +0x03
};
static_assert(sizeof(SetInModeStartRegionAction) == 4,
              "X360 StartModeIntro posts action 44 with size 4");

// ---- 38 ----------------------------------------------------------------------------------
// Producer ModeManager::PreWorldUpdate @0x823537B8 posts one byte. The consumer @0x823EAD20 reads
// NOTHING off it -- it logs the (image-cited) banner and posts a payload-free
// GuiEventFinishedModeResults (id 321, size 1).
struct FinishedModeResultsAction : public GameAction<E_ACTION_FINISHED_MODE_RESULTS>
{
    u8 mu8Unused;             // +0x00 posted uninitialised on the console; never read
};
static_assert(sizeof(FinishedModeResultsAction) == 1,
              "X360 ModeManager::PreWorldUpdate posts action 38 with size 1");

// ---- 39 ----------------------------------------------------------------------------------
// Producer ModeManager::SendModeStopMessages @0x8234BEC0 (AddEvent(q, v80, 39, 24)). Consumer
// @0x823EABCC reads +0x00, +0x0C, +0x11, +0x12, +0x13 into GuiEventStopMode (id 322, size 12).
// [stuntrace waveB fix round, 2026-08-26] FIVE OF THE SIX FLAGs CLEARED. Every identity below was
// re-derived from SendModeStopMessages' own record build this pass (record base == var_110; the post
// is `addi r4, r1, 0x180+var_110 / li r6,0x18 / li r5,0x27` @0x8234C0C4..0x8234C0C0), not taken from
// the implementer report. The FIELD NAMES are left alone on purpose -- renaming them would touch a
// partfile this fix round does not own -- so only the comments change.
struct StopModeAction : public GameAction<E_ACTION_STOP_MODE>
{
    EGameModeType meGameModeType;  // +0x00 ModeManager+3476 (the mode being stopped)
                                   //       `lwz r11, 0xD94(r28) / stw r11, var_110` @0x8234BF0C/24
    s32  miField04;                // +0x04 SendModeStopMessages' `a4` == leNextGameModeType
                                   //       (`stw r14, var_10C` @0x8234BF14; r14 is the 3rd argument,
                                   //       and it is what the +0x14 showtime test below compares)
    s32  miField08;                // +0x08 IDENTITY CLEARED: the 0-based NETWORK ROUND INDEX, or 0
                                   //       when the exiting mode is offline. Console @0x8234BF68-90:
                                   //         if (mpCurrentGameMode && mode->+0xAC)   // IsOnline
                                   //             var_108 = *(nrm+0x12C) - *(nrm+0x128) - 1;
                                   //         else var_108 = 0;
                                   //       i.e. NetworkRoundManager::GetCurrentRound() under the
                                   //       round-accessor ruling (BrnNetworkRoundManager.cpp).
    s32  miField0C;                // +0x0C ModeManager+3496; copied to GuiEventStopMode+0x04
                                   //       (`lwz r9, 0xDA8(r28) / stw r9, var_104` @0x8234BF10/28)
    u8   mu8Field10;               // +0x10 IDENTITY CLEARED: mpCurrentGameMode ? mode->IsOnline() : 0
                                   //       (`lwz r11,0xD98 / lbz r11,0xAC(r11)` @0x8234BF3C-4C, then
                                   //       `stb r16(1), var_100` / `stb r20(0), var_100`).
                                   //       [x] SETTLED 2026-08-26 (closure round): that +0xAC byte
                                   //       had been transcribed as mbConstructed; it is mbIsOnline
                                   //       (GameModes/BrnGameMode.h:353, +172), and both derived
                                   //       Construct bodies now write it by that name. The producer
                                   //       (BrnModeManager_Start.cpp:621/:626) posts the real
                                   //       IsOnline() -- this field no longer posts a forced 0.
    u8   mu8Field11;               // +0x11 SendModeStopMessages' `a3`; -> GuiEventStopMode+0x08
                                   //       (`stb r15, var_FF` @0x8234BF1C)
    u8   mu8Field12;               // +0x12 IDENTITY CLEARED: (NetworkRoundManager::miRoundsRemaining
                                   //       == 0), i.e. "this was the last round".
                                   //       `lwz r11, 0x128(nrm) / subf r11,r20(0),r11 / cntlzw /
                                   //        extrwi 1,26 / stb r11, var_FE` @0x8234C0CC..0x8234C0DC.
                                   //       -> GuiEventStopMode+0x09
    u8   mu8Field13;               // +0x13 ModeManager+38144 (forced true for a <2-player mode)
                                   //       -> GuiEventStopMode+0x0A  FLAG (name still unrecovered)
    u8   mu8Field14;               // +0x14 IDENTITY CLEARED: (leNextGameModeType == 2 ||
                                   //       leNextGameModeType == 16), the showtime pair, tested on
                                   //       r14 at 0x8234BF90..0x8234BFA4 -> `stb r10, var_FC`.
    u8   mu8Field15;               // +0x15 IDENTITY CLEARED: NetworkRoundManager+0x130 ==
                                   //       mbStartingGameDueToPlayerJoin
                                   //       (`lbz r10, 0x130(r10) / stb r10, var_FB` @0x8234BF20/2C).
    u8   maPad16[2];               // +0x16 tail padding to the attested 24
};
static_assert(sizeof(StopModeAction) == 24,
              "X360 SendModeStopMessages posts action 39 with size 24");
static_assert(offsetof(StopModeAction, miField0C) == 0x0C &&
              offsetof(StopModeAction, mu8Field11) == 0x11,
              "action-39 offsets are the @0x823EABCC consumer loads");

// ---- 37 ----------------------------------------------------------------------------------
// Producer ModeManager::ShowModeResults @0x823436D0 (`memset(v42, 0, 232)` then AddEvent(q, v42,
// 37, 232)). The record is the whole offline post-event results block. ONLY the fields the
// @0x823EA984 consumer arm loads are named here; the rest is explicit padding, so the struct is
// byte-exact at 232 without inventing members. FLAG: this is a SLICE, not the full DWARF record.
struct ShowModeResultsAction : public GameAction<E_ACTION_SHOW_MODE_RESULTS>
{
    EGameModeType meGameModeType; // +0x00 ModeManager+3476 (0xD94): producer stw @0x823437AC into the
                              //       record BASE; the @0x823EA984 consumer gates on +0x00 against the
                              //       showtime pair {2,16}, and the producer itself re-reads +0x00
                              //       (`cmpwi r11,2` @0x823E08). Order verified 2026-08-26 verify wave.
    s32  miFinishPosition;    // +0x04 ModeManager::GetPlayersFinishPosition (`stw r3, base+4`
                              //       @0x823439EC); the consumer copies its LOW BYTE to the GUI record
    // [!!] +0x0C AND +0x18 WERE DESCRIBED THE WRONG WAY ROUND (corrected 2026-08-26, stuntrace
    // waveB CLOSURE round). Both identities below were re-derived from ShowModeResults
    // @0x823436D0 this pass, with the record base pinned first: the AddEvent payload pointer is
    // `addi r4, r1, 0x1E0+var_180` @0x82343DF0, so var_180 IS record+0x00 and every stack name
    // below is (0x180 - var_NNN) bytes into the record.
    s32  miField08;           // +0x08 IDENTITY CLEARED: the SCORE. Showtime arm (mode 2 or 16),
                              //       @0x82343864..0x82343888:
                              //         v = (s32)(*(f32*)(this+0x10D8) * flt_820DB5A8)
                              //             (flt_820DB5A8 == 1.0936133, metres -> yards; image-dumped)
                              //         var_178 = (v * 100 + *(s32*)(this+0x10AC)) * *(s32*)(this+0x10B4)
                              //       Every other mode, @0x823438C0..0x823438D8: the word at +0x10
                              //       of the selected scorer (mScoringSystem + 0x350 offline /
                              //       + 0x2620 online). FLAG: the scorer field's own name is
                              //       unrecovered; the ROLE (the posted score) is not.
    f32  mfField0C;           // +0x0C IDENTITY CLEARED: THE FINISH TIME, not a distance.
                              //       mode 3: `stfs f31, var_174` @0x82343A18 with f31 loaded from
                              //       flt_82001CC0 == 0.0f (image-dumped) -- an AUTHORED ZERO.
                              //       otherwise: ScoringSystem::GetFinishTime @0x82343A2C, then
                              //         f13 = time.mfFraction(+4) + (f32)(s64)time.miSeconds(+0)
                              //         f0  = *(f32*)(this+0x8024)
                              //         fsel f0, f13 - f0, f0, f13      @0x82343A5C..0x82343A60
                              //       i.e. the finish time CLAMPED at that member; and on the
                              //       "did not finish" arm (@0x82343A68) it is the literal
                              //       flt_820282B4 == 1.0e8f sentinel. Read back at 0x82343B70 and
                              //       multiplied by flt_820DB5C8 == 1000.0f for mode 5, which is
                              //       seconds -> milliseconds and is the second, independent proof
                              //       that this word is a TIME.
    s32  miField10;           // +0x10 FLAG: *(s32*)(this+0x10AC) on the showtime arm
                              //       (@0x82343858), 0 on every other arm (@0x823438D0)
    s32  miField14;           // +0x14 FLAG: *(s32*)(this+0x10B4) on the showtime arm
                              //       (@0x82343860), 1 on every other arm (@0x823438D4)
    f32  mfField18;           // +0x18 IDENTITY CLEARED: the SHOWTIME DISTANCE, not a finish-time
                              //       delta. Showtime arm: `lfs f0, 0x10D8(this)` @0x82343840 ->
                              //       `stfs f0, var_168` @0x82343848 -- the raw metres value that
                              //       the +0x08 score above converts to yards. Every other mode:
                              //       `stfs f31, var_168` @0x823438C4 == 0.0f (flt_82001CC0).
    u8   maPad1C[0x24];       // +0x1C..+0x3F never read by the arm
    u64  mu64Field40;         // +0x40 FLAG: copied to the GUI record only when mbField DE is set
    s32  miField48;           // +0x48 FLAG
    s32  miField4C;           // +0x4C FLAG
    u8   maPad50[8];          // +0x50..+0x57
    u8   maBlock58[0x70];     // +0x58..+0xC7 memcpy'd wholesale into the GUI record when mbFieldDF
                              //       is set (0x70 bytes; ShowModeResults' per-player results table)
    u64  mu64FieldC8;         // +0xC8 FLAG
    u8   maPadD0[0x0A];       // +0xD0..+0xD9
    u8   mu8FieldDA;          // +0xDA FLAG
    u8   mu8FieldDB;          // +0xDB FLAG
    u8   mu8FieldDC;          // +0xDC FLAG
    u8   maPadDD[1];          // +0xDD
    u8   mbHasField40;        // +0xDE gate for the +0x40 qword copy  FLAG (name from its use)
    u8   mbHasBlock58;        // +0xDF gate for the +0x58 block copy  FLAG (name from its use)
    u8   mu8FieldE0;          // +0xE0 FLAG
    u8   mu8FieldE1;          // +0xE1 also the whole payload of GuiEventTriggerOnlinePostEvent(320)
    u8   mu8FieldE2;          // +0xE2 FLAG
    u8   mu8FieldE3;          // +0xE3 FLAG
    u8   mbIsOnlinePostEvent; // +0xE4 picks the online (320) vs offline (GuiEvent<291>) post-event
                              //       request, and gates the autosave request at the arm's tail
    u8   maPadE5[3];          // +0xE5..+0xE7 tail padding to the attested 232
};
static_assert(sizeof(ShowModeResultsAction) == 232,
              "X360 ModeManager::ShowModeResults posts action 37 with size 232");
static_assert(offsetof(ShowModeResultsAction, maBlock58) == 0x58 &&
              offsetof(ShowModeResultsAction, mu64FieldC8) == 0xC8 &&
              offsetof(ShowModeResultsAction, mbIsOnlinePostEvent) == 0xE4,
              "action-37 offsets are the @0x823EA984 consumer loads");
}
}
