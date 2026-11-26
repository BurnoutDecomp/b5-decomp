#include "GameSource/GameState/BrnGameActions.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"   // CgsNetwork::K_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // OnlineGameResults round-index runtime asserts
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // CgsDev::Log::gpDebugPrint / CgsDev::Message::gxMessageFilterFlags
#define CGS_ARTIST_FILE_BRNGAMESTATESHAREDIO "..\\..\\..\\GameSource\\GameState/BrnGameStateSharedIO.h"
#include <cmath>    // std::fabs
#include <cstring>  // (OnlineGameResults helpers)

namespace
{
// SoundTriggerAction::IsEmpty query-position tolerance: the float IsEmpty @0x82355178 loads from
// rodata (`lvlx v12, r0, r11` @0x82355198, r11 = unk_82029BA4), image-read 0x34000000 ==
// 1.1920928955078125e-07f, i.e. FLT_EPSILON. [FX-BRIDGES 2026-09-24] It used to be a 1.0e-4f
// "UNCONFIRMED stand-in", which called a query position empty up to 1e-4 per lane.
const float KF_QUERY_POS_EPSILON = 1.1920928955078125e-07f;
}

// ⛔ RETIRED 2026-08-01 (reset-player-car wave): this TU carried a FILE-LOCAL FORK of the
// debug-print plumbing -- its own `CgsDev::Message::gxMessageFilterFlags`, its own
// `CgsDev::Log::DebugPrint` struct and its own `WriteInt` -- which is an outright ODR clash
// with the real CgsLog.h/CgsStrStream.h definitions this TU already includes transitively.
// It compiled only because NOTHING EVER LINKED THIS FILE: mounting it in build_game_exe.bat
// turned the fork into eight hard compile errors on the first try. The real
// `*CgsDev::Log::gpDebugPrint << ...` chain is used below instead.

namespace BrnGameState
{
namespace GameStateModuleIO
{
// X360 0x822A0250. True iff the requested reset position OR direction is non-zero: DWARF
// (BrnEntityModuleUnity) lists rw::math::vpu::operator!=(Vector3, Vector3) and _asmIsEqualV3 here,
// against the SDK zero register (vspltisw 0, 0x822A0254).
bool ResetPlayerCarAction::HasToChangeLocation() const
{
    return mPosition != rw::math::vpu::GetVector3_Zero() || mDirection != rw::math::vpu::GetVector3_Zero();
}

// X360 0x82355178. Empty iff the query position is (within tolerance) zero AND there is no
// entity, no result type, and no active-trigger bits.
bool SoundTriggerAction::IsEmpty()
{
    if (std::fabs(static_cast<f32>(mQueryPos.GetX())) > KF_QUERY_POS_EPSILON
        || std::fabs(static_cast<f32>(mQueryPos.GetY())) > KF_QUERY_POS_EPSILON
        || std::fabs(static_cast<f32>(mQueryPos.GetZ())) > KF_QUERY_POS_EPSILON)
    {
        return false;
    }
    if (mEntityId.muValue != 0u)
    {
        return false;
    }
    if (meResultType != E_TYPE_INVALID)
    {
        return false;
    }
    if (muActiveTriggers != 0u)
    {
        return false;
    }
    return true;
}

// X360 0x8230FE98. Range-checked setter for the disconnected player's active-race-car slot.
void RemotePlayerDisconnectedAction::SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT_AT((leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
        "( leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 ) && ( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )",
            CGS_ARTIST_FILE, 6360 /* ARTIST 0x8230FECC */);


    meActiveRaceCarIndex = leActiveRaceCarIndex;
}

// X360 0x8230FF00. Store the disconnected player's network id (must not be the invalid sentinel).
void RemotePlayerDisconnectedAction::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lPlayerID)
{
    CGS_ASSERT_AT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
        "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
            CGS_ARTIST_FILE, 6374 /* ARTIST 0x8230FF2C */);


    mPlayerID = lPlayerID;
}

// X360 0x82355088. Initialise a setup-network-car action payload (7 members). The three asserts
// bound the race-car index and reject the null model id (expression/file/line byte-exact).
void SetupNetworkCarAction::Construct(EPlayerScoringIndex lePlayerScoringIndex,
                                      EActiveRaceCarIndex leActiveRaceCarIndex,
                                      Vector3             lPos,
                                      Vector3             lAt,
                                      CgsID               lModelId,
                                      CgsID               lWheelModelId,
                                      f32                 lfBaseDeformationAmount)
{
    CGS_ASSERT_AT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 783);
    CGS_ASSERT_AT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 784);
    CGS_ASSERT_AT(lModelId != 0, "lModelId != kCGSID_NULL", CGS_ARTIST_FILE, 785);

    mfBaseDeformationAmount = lfBaseDeformationAmount;   // stfs f31, 0x38(r31) @0x82355140
    mWorldSpacePosition  = lPos;
    // ARTIST 0x82355148-0x82355158 stores the indices before the model IDs and orientation.
    mePlayerScoringIndex = lePlayerScoringIndex;
    meActiveRaceCarIndex = leActiveRaceCarIndex;
    mModelId             = lModelId;
    mWheelModelId        = lWheelModelId;
    mAt                  = lAt;
}

// X360 0x823551F0. Bounds-checked setter for the added player's scoring slot.
void OnlinePlayerAddedAction::SetPlayerScoringIndex(EPlayerScoringIndex lePlayerScoringIndex)
{
    CGS_ASSERT_AT(
        (lePlayerScoringIndex >= E_PLAYER_SCORING_INDEX_0) &&
        (lePlayerScoringIndex <  E_PLAYER_SCORING_INDEX_COUNT),
        "(lePlayerScoringIndex >= E_PLAYER_SCORING_INDEX_0) && (lePlayerScoringIndex < E_PLAYER_SCORING_INDEX_COUNT)",
            CGS_ARTIST_FILE, 6390 /* ARTIST 0x82355224 */);


    mePlayerScoringIndex = lePlayerScoringIndex;
}

// X360 0x82355258. Range-checked setter for the removed online player's active-race-car slot.
// (The X360 streamed a dynamic "invalid race car index" message via StrStream; reduced here to a
// static CGS_ASSERT-style expression, matching the committed RemotePlayerDisconnectedAction.)
void OnlinePlayerRemovedAction::SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT((leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
               "We have an invalid race car index here");

    meActiveRaceCarIndex = leActiveRaceCarIndex;
}

// X360 0x823554B0. Store the player's overall progression rank + the four per-mode ranks. The two
// asserts bound the rank below the rank count and reject the "finished last rank" sentinel
// (verbatim file/line). liRankCount is assert-only (not stored). Void; return-result dropped.
void RankInfoResponseAction::SetProgressionRanks(s32 liPlayerRank, s32 liRankCount, s32 liOfflineRace,
                                                 s32 liRoadRage, s32 liStuntAttack, s32 liMarkedMan)
{
    CGS_ASSERT_AT(liPlayerRank < liRankCount,
        "liPlayerRank < liRankCount", CGS_ARTIST_FILE, 6879 /* ARTIST 0x823554EC */);

    CGS_ASSERT_AT(liPlayerRank != KI_PLAYER_HAS_FINISHED_LAST_RANK,
        "liPlayerRank != KI_PLAYER_HAS_FINISHED_LAST_RANK", CGS_ARTIST_FILE, 6880 /* ARTIST 0x82355510 */);


    miPlayerRank  = liPlayerRank;
    miOfflineRace = liOfflineRace;
    miRoadRage    = liRoadRage;
    miStuntAttack = liStuntAttack;
    miMarkedMan   = liMarkedMan;
}

// X360 0x82355328. Store the four per-mode rank-win counts, after optionally echoing each through
// the global debug stream when the message filter's bit 0 is set. Void; return-result dropped.
void RankInfoResponseAction::SetProgressionRankEventWins(s32 liOfflineRaceRankWins, s32 liRoadRageRankWins,
                                                         s32 liStuntAttackRankWins, s32 liMarkedManRankWins)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "liOfflineRaceRankWins : " << liOfflineRaceRankWins << "\n";
        *CgsDev::Log::gpDebugPrint << "liRoadRageRankWins : "    << liRoadRageRankWins    << "\n";
        *CgsDev::Log::gpDebugPrint << "liStuntAttackRankWins : " << liStuntAttackRankWins << "\n";
        *CgsDev::Log::gpDebugPrint << "liMarkedManRankWins : "   << liMarkedManRankWins   << "\n";
    }

    miOfflineRaceRankWins = liOfflineRaceRankWins;
    miRoadRageRankWins    = liRoadRageRankWins;
    miStuntAttackRankWins = liStuntAttackRankWins;
    miMarkedManRankWins   = liMarkedManRankWins;
}

// X360 0x8230FDF0. Initialise a prepare-for-mode action payload: stamp the stage to "all in one",
// copy in the per-event GameModeParams, store the current round + "coming from online lobby" flag,
// and reset the per-player scoring slots + the disconnected-player table to their empty markers.
void PrepareForModeAction::Construct(const GameModeParams* lpGameModeParams,
                                     s32                    liCurrentRound,
                                     bool                   lbComingFromOnlineLobbyMode)
{
    CGS_ASSERT_AT(lpGameModeParams,
        "lpGameModeParams", CGS_ARTIST_FILE, 1183 /* ARTIST 0x8230FE1C */);

    mePrepareForModeStage             = E_PFM_STAGE_ALL_IN_ONE;
    mGameModeParams                   = *lpGameModeParams;
    miCurrentRound                    = liCurrentRound;
    mbComingFromOnlineLobbyMode       = lbComingFromOnlineLobbyMode;
    mbFinishedOnlineEvent             = false;
    mbStartingFreeburnDueToPlayerJoin = false;
    miShotGroup                       = -1;

    for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
    {
        maePlayerScoringIndex[liIndex]         = E_PLAYER_SCORING_INDEX_COUNT;
        maDisconnectedNetworkPlayerID[liIndex] = CgsNetwork::K_INVALID_PLAYER_ID;
    }

    miNumPlayersDisconnected = 0;
}

// PrepareForModeAction::GetGameModeParams is header-inline (DWARF BrnGameActions.h:874): the
// console's mGameModeParams sits at action+0x30 (WorldModule::BridgeInputToEntityModules 0x827ADF88
// reads `lwz r11,0x178(r31)` = GameModeParams +0x148, the EGameModeType GameModeParams::Construct
// 0x8231C374 stores, and `ld r11,0x890(r31)` = +0x860, the muFlags GameModeParams::GetFlag 0x821F2C88
// loads).

// X360 0x822A0198. Linear search of the disconnected-player table: true iff lNetworkPlayerID is one
// of the miNumPlayersDisconnected recorded ids. The two asserts bound the count to [0, 8).
bool PrepareForModeAction::GetPlayerDisconnected(BrnNetwork::NetworkPlayerID lNetworkPlayerID) const
{
    CGS_ASSERT_AT(miNumPlayersDisconnected >= 0,
        "miNumPlayersDisconnected >= 0", CGS_ARTIST_FILE, 1136 /* ARTIST 0x822A01C8 */);
    CGS_ASSERT_AT(miNumPlayersDisconnected < KI_MAX_DISCONNECTED_NETWORK_PLAYERS,
        "miNumPlayersDisconnected < BrnWorld::KI_MAX_ACTIVE_RACE_CARS", CGS_ARTIST_FILE, 1137 /* ARTIST 0x822A01F0 */);

    for (s32 liIndex = 0; liIndex < miNumPlayersDisconnected; ++liIndex)
    {
        if (lNetworkPlayerID == maDisconnectedNetworkPlayerID[liIndex])
        {
            return true;
        }
    }
    return false;
}

// X360 0x8230FD60. Append a disconnected player's network id to the fixed-capacity disconnected-player
// list, then bump the count. The two asserts bound the count (>= 0 and < KI_MAX_ACTIVE_RACE_CARS - 1).
void PrepareForModeAction::SetPlayerDisconnected(BrnNetwork::NetworkPlayerID lPlayerID)
{
    CGS_ASSERT_AT(miNumPlayersDisconnected >= 0,
        "miNumPlayersDisconnected >= 0", CGS_ARTIST_FILE, 1160 /* ARTIST 0x8230FD90 */);

    CGS_ASSERT_AT(miNumPlayersDisconnected < 7,
        "miNumPlayersDisconnected < (BrnWorld::KI_MAX_ACTIVE_RACE_CARS - 1)", CGS_ARTIST_FILE, 1161 /* ARTIST 0x8230FDB8 */);


    maDisconnectedNetworkPlayerID[miNumPlayersDisconnected] = lPlayerID;
    ++miNumPlayersDisconnected;
}

// ARTIST SendModeStopMessages 0x8234C2F4..0x8234C310 inlines this constructor:
// initialise all eight position slots to the invalid player and clear disconnects.
void OnlineRoundResults::Construct()
{
    for (s32 liPosition = 0; liPosition < KI_MAX_PLAYERS; ++liPosition)
    {
        maPlayerPosition[liPosition] = CgsNetwork::K_INVALID_PLAYER_ID;
    }
    maDisconnectedPlayers.Clear();
}

// X360 0x8231CA38. Record a player's finishing slot. The valid-id assert rejects the invalid
// sentinel; position -1 means "disconnected" -> append the id to the disconnected list, else
// bounds-check the position and store the id at that slot. (X360 return-result artifact dropped.)
void OnlineRoundResults::SetPosition(BrnNetwork::NetworkPlayerID lNetworkPlayerID, s32 liPosition)
{
    CGS_ASSERT_AT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
        "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
            CGS_ARTIST_FILE, 6641 /* ARTIST 0x8231CA6C */);


    if (liPosition == -1)
    {
        maDisconnectedPlayers.Append(lNetworkPlayerID);
    }
    else
    {
        CGS_ASSERT_AT(liPosition >= 0,
            "liPosition >= 0", CGS_ARTIST_FILE, 6649 /* ARTIST 0x8231CAAC */);
        CGS_ASSERT_AT(liPosition < KI_MAX_PLAYERS,
            "liPosition < KI_MAX_PLAYERS",
                CGS_ARTIST_FILE, 6650 /* ARTIST 0x8231CAD0 */);


        maPlayerPosition[liPosition] = lNetworkPlayerID;
    }
}

// X360 0x82558580. Look up a player's finishing position. Disconnected players report
// KI_POSITION_DISCONNECTED; otherwise linear-scan the position table for the id and return its slot.
// Reaching the end without a match is a logic error (X360 streamed a runtime CgsStrStream message;
// reduced to a static CGS_ASSERT, still returning the -1 sentinel).
s32 OnlineRoundResults::GetPosition(BrnNetwork::NetworkPlayerID lNetworkPlayerID) const
{
    CGS_ASSERT_AT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\BrnGameActions.h", 6670);

    for (s32 liIndex = 0; liIndex < maDisconnectedPlayers.GetCount(); ++liIndex)
    {
        if (maDisconnectedPlayers.GetItem(liIndex) == lNetworkPlayerID)
        {
            return KI_POSITION_DISCONNECTED;
        }
    }

    for (s32 liPosition = 0; liPosition < KI_MAX_PLAYERS; ++liPosition)
    {
        if (maPlayerPosition[liPosition] == lNetworkPlayerID)
        {
            return liPosition;
        }
    }

    CGS_FAIL("Player has no result position.");
    return KI_POSITION_DISCONNECTED;
}

// ===== OnlineGameResults (re-homed here from BrnGameStateSharedIO; DWARF home BrnGameActions.h:5153) =====

// X360 0x8230F178. Reset the whole results record; miEventType -> -1 (invalid). (The X360 zeroes
// only the low word of mCarUsed -- an unused-half micro-quirk -- reconstructed as a full typed
// reset.) X360 `return result` on a void fn dropped.
void OnlineGameResults::Clear()
{
    mCarUsed                     = 0;
    mSecondsInEvent.SetFloatVal(0.0f);
    mfMetersDriven               = 0.0f;
    miTakedownsFor               = 0;
    miTakedownsAgainst           = 0;
    miTraitorousTakedownsFor     = 0;
    miTraitorousTakedownsAgainst = 0;
    miMarkedManTakedownsFor      = 0;
    miNumberOfRounds             = 0;
    miNumberOfRoundsPlayed       = 0;
    miNumberOfPlayers            = 0;
    miEventType                  = -1;
    mbIsRanked                   = false;

    for (s32 liRound = 0; liRound < KI_MAX_ROUNDS; ++liRound)
    {
        maRoundTimes[liRound].SetFloatVal(0.0f);
        mafRoundDistances[liRound]        = 0.0f;
        maiRoundStuntScores[liRound]      = 0;
        maiRoundStuntMultipliers[liRound] = 0;
    }
}

// X360 0x8230F220. Store one online-race round (time {seconds,fraction} + distance). Guards: result
// must be an online-race, and liRoundIndex in range (the out-of-range guard streams a runtime
// "index: N rounds M" message through CgsStrStream, matching the X360).
void OnlineGameResults::SetRaceResults(s32 liRoundIndex, const CgsSystem::Time& lRoundTime, f32 lfRoundDistance)
{
    CGS_ASSERT_AT(E_MODE_ONLINE_RACE == miEventType, "E_MODE_ONLINE_RACE == meEventType", CGS_ARTIST_FILE_BRNGAMESTATESHAREDIO, 2131);

    CGS_ASSERT_STREAM_AT(liRoundIndex < miNumberOfRounds,
                         "index: " << liRoundIndex << " rounds " << miNumberOfRounds << "\n",
                         CGS_ARTIST_FILE_BRNGAMESTATESHAREDIO, 2132);

    maRoundTimes[liRoundIndex]      = lRoundTime;
    mafRoundDistances[liRoundIndex] = lfRoundDistance;
}

// X360 0x82580C60. Read back an online-race round result (same guards as SetRaceResults).
void OnlineGameResults::GetRaceResults(s32 liRoundIndex, CgsSystem::Time* lpRoundTime, f32* lpfRoundDistance) const
{
    CGS_ASSERT_AT(E_MODE_ONLINE_RACE == miEventType, "E_MODE_ONLINE_RACE == meEventType", CGS_ARTIST_FILE_BRNGAMESTATESHAREDIO, 2159);

    CGS_ASSERT_STREAM_AT(liRoundIndex < miNumberOfRounds,
                         "index: " << liRoundIndex << " rounds " << miNumberOfRounds << "\n",
                         CGS_ARTIST_FILE_BRNGAMESTATESHAREDIO, 2160);

    *lpRoundTime      = maRoundTimes[liRoundIndex];
    *lpfRoundDistance = mafRoundDistances[liRoundIndex];
}

// X360 0x8230F370. Store one online-stunt round (score + multiplier). Guards: result must be an
// online-stunt run (the inlined IsOnlineStuntRun predicate: FUGITIVE/FREE_BURN/MODE_END), and
// liRoundIndex in range.
void OnlineGameResults::SetStuntResults(s32 liRoundIndex, s32 liScore, s32 liMultiplier)
{
    CGS_ASSERT_AT(IsOnlineStuntRun(static_cast<EGameModeType>(miEventType)),
               "IsOnlineStuntRun( meEventType )", CGS_ARTIST_FILE_BRNGAMESTATESHAREDIO, 2145);

    CGS_ASSERT_STREAM_AT(liRoundIndex < miNumberOfRounds,
                         "index: " << liRoundIndex << " rounds " << miNumberOfRounds << "\n",
                         CGS_ARTIST_FILE_BRNGAMESTATESHAREDIO, 2146);

    maiRoundStuntScores[liRoundIndex]      = liScore;
    maiRoundStuntMultipliers[liRoundIndex] = liMultiplier;
}

// X360 0x82580DA8. Read back an online-stunt round result (same guards as SetStuntResults).
void OnlineGameResults::GetStuntResults(s32 liRoundIndex, s32* lpiScore, s32* lpiMultiplier) const
{
    CGS_ASSERT_AT(IsOnlineStuntRun(static_cast<EGameModeType>(miEventType)),
               "IsOnlineStuntRun( meEventType )", CGS_ARTIST_FILE_BRNGAMESTATESHAREDIO, 2173);

    CGS_ASSERT_STREAM_AT(liRoundIndex < miNumberOfRounds,
                         "index: " << liRoundIndex << " rounds " << miNumberOfRounds << "\n",
                         CGS_ARTIST_FILE_BRNGAMESTATESHAREDIO, 2174);

    *lpiScore      = maiRoundStuntScores[liRoundIndex];
    *lpiMultiplier = maiRoundStuntMultipliers[liRoundIndex];
}


// =============================================================================
// PrepareForModeAction - the six declared-only SETTERS the mounted event core calls.
//   [stuntrace wave B, MOUNT-CLOSURE round]
//
// None of the six has an out-of-line X360 symbol: the console inlines every one of them into
// ModeManager::PrepareForMode @0x82342930, which builds the action record on its own stack
// (IDA local var_980) and then memcpy's 0x8E0 == 2272 bytes of it into the manager's cache
// (@0x82342E40 addi r4,r1,var_980 ; li r5,0x8E0 ; bl memcpy). That record size is itself the
// confirmation that this is the type BrnGameActions.h models -- 2272 is the console
// PrepareForModeAction, and the header's own note already records the host/console width
// divergence (seam_audit S3b arm 23: symmetric across producer and consumer, so it costs console
// byte-compat and nothing else).
//
// EVERY member below is read out of ONE asm window, at a fixed distance from that var_980 record
// base: record offset == 0x980 - (the var_XX number IDA prints). All six land on members the
// committed header had already named and offset-pinned from the DWARF, so this round CONFIRMS
// that layout rather than proposing it:
//     var_980 -> record +0x000   mePrepareForModeStage             (stw,  enum)
//     (walk)  -> record +0x004   maePlayerScoringIndex[8]          (stw,  enum, see below)
//     var_B8  -> record +0x8C8   mfPlayerBoostEarning              (stfs, f32)
//     var_B4  -> record +0x8CC   miShotGroup                       (stw,  s32)
//     var_AF  -> record +0x8D1   mbFinishedOnlineEvent             (stb,  bool)
//     var_AE  -> record +0x8D2   mbStartingFreeburnDueToPlayerJoin (stb,  bool)
// The two neighbours at +0x8C8 and +0x8CC are disambiguated by INSTRUCTION KIND, not by order:
// PrepareForMode loads their sources as lfs 0x31C(startParams) and lwz 0x320(startParams) on
// consecutive instructions (@0x82342A94 / @0x82342A90) and stores them back as stfs / stw -- a
// swap would have to survive four mismatched types.
//
// The producer call sites for all six are in the mounted BrnModeManager_Prepare.cpp
// (:575, :576, :577, :591, :686, :714, :726, :727), and seam_audit S3b already checked the
// mounted consumer arm (case 23) against them. These bodies are the missing middle of that chain.
//
// Only the six SETTERS are landed here. Of their read twins, GetShotGroup has a live consumer
// now (MainDirector's prepare-for-mode arm, which the console compiles as a bare word load) and
// is a header inline beside the other four; GetPlayerScoringIndex and GetPlayerBoostEarning
// still have no mounted caller, so they stay declared-only -- a body with no live consumer is a
// body whose member mapping nothing can check.
// =============================================================================

// -----------------------------------------------------------------------------
// SetPlayerScoringIndex (record +0x004, maePlayerScoringIndex[liIndex]).
//
// Recovered from the loop tail of ModeManager::PrepareForMode @0x82342D90..0x82342DEC, where the
// console walks the grid positions and stores each player's scoring slot into the record:
//     cmpwi cr6, r20, 0 ; blt -> assert          ; r20 == liIndex (the grid position)
//     cmpwi cr6, r20, 8 ; blt -> ok, else assert
//         FireAssert("(liIndex>=0) && (liIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)",
//                    "..\\..\\..\\GameSource\\GameState/BrnGameActions.h", 0x4D6 == 1238)
//     cmpwi cr6, r28, 0 ; blt -> assert          ; r28 == leScoringIndex
//     cmpwi cr6, r28, 8 ; blt -> ok, else assert
//         FireAssert("(leScoringIndex>=E_PLAYER_SCORING_INDEX_0) && "
//                    "(leScoringIndex<E_PLAYER_SCORING_INDEX_COUNT)", same file, 0x4D7 == 1239)
//     stw r28, 0(r11) ; addi r11, r11, 4         ; maePlayerScoringIndex[liIndex] = leScoringIndex
// The file string is THIS header's (BrnGameActions.h) and the two line numbers are consecutive --
// both marks of an inlined member's own assert pair rather than the caller's. The walk uses a
// running pointer bumped by 4 per iteration, seeded from var_97C == record +0x004, which is
// exactly where the header already places maePlayerScoringIndex[0].
//
// The bound the FIRST assert spells is E_ACTIVE_RACE_CAR_INDEX_COUNT, not KI_MAX_PLAYERS -- the
// console really does mix the two names for the same 8 here (the header's own note records
// KI_MAX_PLAYERS == BrnWorld::KI_MAX_ACTIVE_RACE_CARS == 8). The assert STRINGS are reproduced
// verbatim per the standing rule; the C++ bound is the array's own extent constant, so the check
// and the array can never drift apart silently even though the string says something else.
// Both asserts are non-gating tripwires: the console stores regardless.
// -----------------------------------------------------------------------------
void PrepareForModeAction::SetPlayerScoringIndex(s32 liIndex, EPlayerScoringIndex leIndex)
{
    CGS_ASSERT(liIndex >= 0 && liIndex < KI_MAX_PLAYERS,
               "(liIndex>=0) && (liIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");
    CGS_ASSERT(leIndex >= E_PLAYER_SCORING_INDEX_0 && leIndex < E_PLAYER_SCORING_INDEX_COUNT,
               "(leScoringIndex>=E_PLAYER_SCORING_INDEX_0) && (leScoringIndex<E_PLAYER_SCORING_INDEX_COUNT)");
    maePlayerScoringIndex[liIndex] = leIndex;
}

// -----------------------------------------------------------------------------
// GetPlayerScoringIndex (record +0x04 + 4*liIndex). Inlined at its reader,
// RaceCarEntityModule::AddRaceCarToStartingGridOrFreeburnLobby: the setter's index assert
// (same string, BrnGameActions.h line 0x4DF) then `lwzx` of the slot. Non-gating, as above.
// -----------------------------------------------------------------------------
EPlayerScoringIndex PrepareForModeAction::GetPlayerScoringIndex(s32 liIndex) const
{
    CGS_ASSERT(liIndex >= 0 && liIndex < KI_MAX_PLAYERS,
               "(liIndex>=0) && (liIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");
    return maePlayerScoringIndex[liIndex];
}

// -----------------------------------------------------------------------------
// SetPlayerBoostEarning (record +0x8C8, mfPlayerBoostEarning).
// ModeManager::PrepareForMode @0x82342A94 lfs f0, 0x31C(r31) -> @0x82342A9C stfs f0, var_B8(r1),
// i.e. record +0x8C8. The source is StartGameModeParams::GetBoostEarning(), bodied this same
// round in BrnGameModeParams.cpp, and the committed producer line already carries the matching
// note: BrnModeManager_Prepare.cpp:575 "// action +0x8C8". No assert on the console path.
// -----------------------------------------------------------------------------
void PrepareForModeAction::SetPlayerBoostEarning(f32 lfBoostEarning)
{
    mfPlayerBoostEarning = lfBoostEarning;
}

// -----------------------------------------------------------------------------
// SetShotGroup (record +0x8CC, miShotGroup).
// The paired instruction: @0x82342A90 lwz r9, 0x320(r31) -> @0x82342AAC stw r9, var_B4(r1) ==
// record +0x8CC. Source is StartGameModeParams::GetShotGroup(); producer note at
// BrnModeManager_Prepare.cpp:576 ("// action +0x8CC"). A WORD store next to the float store
// above -- that difference in kind is what keeps these two adjacent slots apart. No assert.
// -----------------------------------------------------------------------------
void PrepareForModeAction::SetShotGroup(s32 liShotGroup)
{
    miShotGroup = liShotGroup;
}

// -----------------------------------------------------------------------------
// SetFinishedOnlineEvent (record +0x8D1, mbFinishedOnlineEvent).
// @0x82342ABC stb r11, var_AF(r1) == record +0x8D1, sourced from the manager's own
// mbFinishedOnlineEvent byte (r27 == this + 0x9507, built by the addis/addi pair at
// @0x82342A88 / @0x82342A98). Producer: BrnModeManager_Prepare.cpp:591 ("// action +0x8D1").
// CROSS-CHECKED FROM THE OTHER END: the read twin GetFinishedOnlineEvent() is already a header
// inline from the [evt-flow E1] round, and it was derived from the CONSUMER side
// (TranslateGameActionsToGuiEvents' case-23 arm @0x823EAD80 loads 0x8D1(r31)). Two independent
// derivations, one byte.
// -----------------------------------------------------------------------------
void PrepareForModeAction::SetFinishedOnlineEvent(bool lbFinished)
{
    mbFinishedOnlineEvent = lbFinished;
}

// -----------------------------------------------------------------------------
// SetStartingFreeburnLobbyDueToPlayerJoin (record +0x8D2, mbStartingFreeburnDueToPlayerJoin).
// @0x82342AB8 stb r10, var_AE(r1) == record +0x8D2, the byte immediately after
// mbFinishedOnlineEvent, sourced at @0x82342AA4 from lbz r10, 0x130(r10) off the module pointer
// at this+0x6D64. Producer: BrnModeManager_Prepare.cpp:577. Same producer/consumer cross-check as
// above -- the case-23 arm reads 0x8D2(r31), which is what the committed header inline
// GetStartingFreeburnLobbyDueToPlayerJoin() was built from.
// -----------------------------------------------------------------------------
void PrepareForModeAction::SetStartingFreeburnLobbyDueToPlayerJoin(bool lbStarting)
{
    mbStartingFreeburnDueToPlayerJoin = lbStarting;
}

// -----------------------------------------------------------------------------
// SetPrepareStage (record +0x000, mePrepareForModeStage).
//
// The split-prepare tail of ModeManager::PrepareForMode @0x82342E34..0x82342E6C is the whole
// derivation, and it is unusually explicit:
//     li   r28, 1
//     stw  r28, var_980(r1)                     ; the OUTGOING record's stage = 1
//     bl   memcpy(this + 0x8C10, &var_980, 0x8E0)  ; snapshot it into the manager's cache
//     stw  r28, var_980(r1)                     ; (re-stamped; the compiler reloaded it)
//     li   r11, 2
//     stw  r11, 0(r31)                          ; the CACHED copy's stage = 2
// var_980 IS the record base, so the member written is at record +0x000 == mePrepareForModeStage,
// and the two values are E_PFM_STAGE_FIRST_OF_TWO(1) and E_PFM_STAGE_SECOND_OF_TWO(2). That is
// exactly the committed producer pair at BrnModeManager_Prepare.cpp:726/:727 (fresh post stamped
// stage 1, cache stamped stage 2), and it is what makes the mounted consumer's
// IsFirstPrepareForMode() gate drop the repost the way the console drops it -- the seam audit's
// S3b arm-23 finding, which until now was resting on an unresolved symbol.
// No assert on the console path (the stage is compiler-generated, never a caller's argument).
// -----------------------------------------------------------------------------
void PrepareForModeAction::SetPrepareStage(EPrepareForModeStage leStage)
{
    mePrepareForModeStage = leStage;
}
}
}

#undef CGS_ARTIST_FILE
#undef CGS_ARTIST_FILE_BRNGAMESTATESHAREDIO
