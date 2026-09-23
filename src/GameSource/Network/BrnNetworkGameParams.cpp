#include "GameSource/Network/BrnNetworkGameParams.h"
#include "GameSource/Network/Parameters/BrnNetworkParameterData.h"   // AmendGameModeContexts / MatchmakingContext
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT (range/round-trip tripwires)

#include <cstring>   // std::memcpy

// =============================================================================
// BrnNetwork::GameParams -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnNetworkGameParams.h for the
// corrected platform base + the leaf layout
// (mGameData @ +0x150, seven PlayerParams maPlayerParams @ +0x170).
//
// This TU SHIPS:
//   ~GameParams  (the `scalar deleting destructor' @ 0x82567338 forwards to it)
//   operator=    @ 0x82566A00
// =============================================================================

namespace BrnNetwork
{

// X360 @ 0x82567338 (`scalar deleting destructor'). As the object tears down,
// the Xenon codegen reinstalls the shared ServerInterfaceStructureInterface base
// vtable (off_8207C88C) into each of the seven embedded 0xA0-stride sub-objects
// (this+0x170 .. this+0x530) and then the primary vtable at this+0, before the
// delete-expression's conditional operator delete. That whole vtable walk + the
// conditional free are compiler-synthesised from this trivial out-of-line virtual
// destructor; only the empty body is hand-written, matching the committed
// convention (BrnNetwork::GameResults::~GameResults @ 0x827DFB60). Defining it
// out-of-line here also anchors this class's vtable to this TU.
GameParams::~GameParams()
{
}

// Member-wise copy: chain to the platform base operator=, then copy the game leaf's own
// storage in the console order -- the 0x20-byte GameData block (eight words), then each
// of the seven PlayerParams records over its member span [+4 .. +0xA0) (a 0x40-byte run,
// seven words, a 0x24-byte run, seven words: exactly PlayerParams' member-wise copy,
// which leaves the vtable slot alone).
GameParams& GameParams::operator=(const GameParams& lrOther)
{
    CgsNetwork::ServerInterfaceGameParamsX360::operator=(lrOther);

    mGameData = lrOther.mGameData;

    for (s32 liSlot = 0; liSlot < KI_GAMEPARAMS_PLAYER_SLOTS; ++liSlot)
    {
        maPlayerParams[liSlot] = lrOther.maPlayerParams[liSlot];
    }

    return *this;
}

// =============================================================================
// Game-surface packed-flag accessors. The search-data word is muCustomFlags (+0xE8),
// the leaf game-params word is mGameData.muUser1 (+0x15C). Each base-bit/num-bit
// placement is read off the extrwi/insrwi operands of the accessor asm.
// =============================================================================

// Boost-type discriminant, bits 24..26 of the game-search word.
// The console reads only the word's most significant byte (lbz +0xE8 on the
// big-endian word) and keeps its low 3 bits: (muCustomFlags >> 24) & 7.
s32 GameParams::BoostType() const
{
    return static_cast<s32>((muCustomFlags >> 24) & 7u);   // base bit 24, 3 bits
}

// X360 @ 0x82584098. Game-mode discriminant: bits 8..12 of the game-search word,
// biased by +10 (the E_GAME_MODE_* enum base).
s32 GameParams::GameMode() const
{
    return static_cast<s32>((muCustomFlags >> KU_BRN_GAMESEARCHDATA_GAMEMODE_BASE_BIT)
                            & ((1u << KU_BRN_GAMESEARCHDATA_GAMEMODE_NUM_BITS) - 1u))
         + KI_GAME_PARAMS_GAME_MODE_BIAS;
}

// X360 @ 0x82583DB0. Replicated-payload accessor: hands back the 0x20-byte leaf data
// block at +0x150 (mGameData). The base declares GetData() and GetData() const as
// SEPARATE pure-virtual slots, so both must be defined; the const overload shares this
// trivial body.
void* GameParams::GetData()
{
    return &mGameData;
}

const void* GameParams::GetData() const
{
    return &mGameData;
}

// X360 @ 0x82583DA8. Size of the replicated payload GetData() returns.
u32 GameParams::GetDataSize() const
{
    return sizeof(mGameData);   // 0x20
}

// X360 @ 0x82584D28. This leaf's serialisation pattern string: "l*" == one long field.
const char* GameParams::GetPattern() const
{
    return "l*";
}

// X360 @ 0x82584058. lwz r11,0x15C(r3); extrwi r3,r11,1,24 -> (word >> 7) & 1.
bool GameParams::InfiniteBoost() const
{
    return (mGameData.muUser1 >> 7) & 1;
}

// X360 @ 0x825840B8. lwz r11,0xE8(r3); extrwi r3,r11,1,8 -> (word >> 23) & 1.
bool GameParams::IsTrafficCheckingOn() const
{
    return (muCustomFlags >> 23) & 1;
}

// X360 @ 0x825840A8. lwz r11,0xE8(r3); extrwi r3,r11,1,14 -> (word >> 17) & 1.
bool GameParams::IsTrafficOn() const
{
    return (muCustomFlags >> 17) & 1;
}

// X360 @ 0x825840E8. Runner-crash count packed into muCustomFlags bits 27..29.
//   lwz r11,0xE8(r3); extrwi r3,r11,3,2  ->  (muCustomFlags >> 27) & 7
s32 GameParams::NumRunnerCrashes() const
{
    return static_cast<s32>((muCustomFlags >> KU_BRN_GAMESEARCHDATA_RUNNER_CRASHES_BASE_BIT)
                            & ((1u << KU_BRN_GAMESEARCHDATA_RUNNER_CRASHES_NUM_BITS) - 1u));
}

// Rounds count packed into the leaf game-params word, bits 11..14.
s32 GameParams::NumberRounds() const
{
    return static_cast<s32>((mGameData.muUser1 >> KI_GAME_PARAMS_ROUNDS_BASE_BIT)
                            & ((1u << KI_GAME_PARAMS_ROUNDS_NUM_BITS) - 1u));
}

// X360 @ 0x82584088. Security level = low 2 bits of muCustomFlags.
//   lwz r11,0xE8(r3); clrlwi r3,r11,30  ->  muCustomFlags & 3
s32 GameParams::Security() const
{
    return static_cast<s32>(muCustomFlags
                            & ((1u << KU_BRN_GAMESEARCHDATA_SECURITY_NUM_BITS) - 1u));
}

// X360 @ 0x82584018. Pack the 3-bit boost-type field (base bit 24) into the game-search
// word at +0xE8 (insrwi r11,r4,3,5 -> mask 0x07000000).
void GameParams::SetBoostType(s32 liBoostType)
{
    muCustomFlags = ((static_cast<u32>(liBoostType) << 24) & 0x07000000u) | (muCustomFlags & 0xF8FFFFFFu);
}

// X360 @ 0x8258A790. Pack the 5-bit game-mode field (base bit 8, stored biased by -10)
// into the game-search word at +0xE8, then (re)publish the game-mode LIVE context via
// AmendGameModeContexts over the base's context array (maRankedContexts @ +0xF8, count
// @ +0x148). AmendGameModeContexts is handed the raw game-mode value. Then
// assert the field round-trips.
void GameParams::SetGameMode(s32 liGameMode)
{
    muCustomFlags = ((static_cast<u32>(liGameMode - KI_GAME_PARAMS_GAME_MODE_BIAS) << 8) & 0x1F00u)
                  | (muCustomFlags & 0xFFFFE0FFu);

    AmendGameModeContexts(liGameMode,
                          &miContextCount,
                          reinterpret_cast<MatchmakingContext*>(maRankedContexts));

    CGS_ASSERT(GameMode() == liGameMode, "GameMode() == leGameMode");
}

// X360 @ 0x82583EA8. Pack the 1-bit infinite-boost flag (base bit 7) into the leaf
// game-params word (mGameData.muUser1).
void GameParams::SetInfiniteBoost(bool lbInfiniteBoost)
{
    mGameData.muUser1 = ((static_cast<u32>(lbInfiniteBoost) << 7) & 0x80u) | (mGameData.muUser1 & 0xFFFFFF7Fu);
}

// X360 @ 0x82584038. Pack the runner-crash count into muCustomFlags bits 27..29
// (insrwi r11,r4,3,2 -> mask 0x38000000).
void GameParams::SetNumRunnerCrashes(s32 liNumRunnerCrashes)
{
    muCustomFlags = ((static_cast<u32>(liNumRunnerCrashes) << 27) & 0x38000000u)
                  | (muCustomFlags & 0xC7FFFFFFu);
}

// X360 @ 0x82583F48. Pack the round count into the leaf game-params word bits 11..14
// (insrwi r11,r30,4,17 -> mask 0x7800). Two range asserts.
void GameParams::SetNumberRounds(s32 liNumRounds)
{
    CGS_ASSERT(liNumRounds > 0, "liNumRounds > 0");
    CGS_ASSERT(liNumRounds < 16, "liNumRounds < (1<<KI_GAME_PARAMS_ROUNDS_NUM_BITS)");

    mGameData.muUser1 = ((static_cast<u32>(liNumRounds) << 11) & 0x7800u)
                      | (mGameData.muUser1 & 0xFFFF87FFu);
}

// X360 @ 0x82583FD0. Pack (game-mode - 10) into muCustomFlags bits 18..22
// (insrwi r11,(r4-10),5,9).
void GameParams::SetPreviousGameMode(s32 liGameMode)
{
    muCustomFlags = ((static_cast<u32>(liGameMode - KI_GAME_PARAMS_GAME_MODE_BIAS) << 18) & 0x7C0000u)
                  | (muCustomFlags & 0xFF83FFFFu);
}

// X360 @ 0x82583E30. Pack the vehicle level limit into the leaf game-params word bits 3..6
// (insrwi r11,r30,4,25 == (8 * liLevelLimit) & 0x78). Two range asserts.
void GameParams::SetRagerVehicleLevelLimit(s32 liLevelLimit)
{
    CGS_ASSERT(liLevelLimit >= 0, "liLevelLimit >= 0");
    CGS_ASSERT(liLevelLimit < 16, "liLevelLimit < (1<<KI_GAME_PARAMS_VEHICLE_LEVEL_NUM_BITS)");

    mGameData.muUser1 = ((static_cast<u32>(liLevelLimit) << 3) & 0x78u)
                      | (mGameData.muUser1 & 0xFFFFFF87u);
}

// Store bit 30 (vehicle-choice) of the game-search word.
void GameParams::SetVehicleChoice(s32 liVehicleChoice)
{
    const u32 luMask = ((1u << KU_BRN_GAMESEARCHDATA_VEHICLE_CHOICE_NUM_BITS) - 1)
                       << KU_BRN_GAMESEARCHDATA_VEHICLE_CHOICE_BASE_BIT;
    muCustomFlags = ((static_cast<u32>(liVehicleChoice) << KU_BRN_GAMESEARCHDATA_VEHICLE_CHOICE_BASE_BIT) & luMask)
                  | (muCustomFlags & ~luMask);
}

// X360 @ 0x82583DB8. Range-check the vehicle-level limit, then store it in the 4-bit
// VEC_LEVEL field (bits 13..16) of the game-search word.
void GameParams::SetVehicleLevelLimit(s32 liLevelLimit)
{
    CGS_ASSERT(liLevelLimit >= 0, "liLevelLimit >= 0");
    CGS_ASSERT(liLevelLimit < (1 << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_NUM_BITS),
               "liLevelLimit < (1<<KU_BRN_GAMESEARCHDATA_VEC_LEVEL_NUM_BITS)");

    const u32 luMask = ((1u << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_NUM_BITS) - 1)
                       << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_BASE_BIT;
    muCustomFlags = ((static_cast<u32>(liLevelLimit) << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_BASE_BIT) & luMask)
                  | (muCustomFlags & ~luMask);
}

// Pack the 6-bit skill level into muCustomFlags bits 2..7 (mask 0xFC).
void GameParams::SetSkillLevel(u32 luSkillLevel)
{
    muCustomFlags = ((luSkillLevel << 2) & 0xFCu) | (muCustomFlags & 0xFFFFFF03u);
}

// Store the 2-bit security level in the low bits of muCustomFlags.
void GameParams::SetSecurity(s32 leSecurity)
{
    muCustomFlags = (muCustomFlags & ~((1u << KU_BRN_GAMESEARCHDATA_SECURITY_NUM_BITS) - 1u))
                  | (static_cast<u32>(leSecurity) & ((1u << KU_BRN_GAMESEARCHDATA_SECURITY_NUM_BITS) - 1u));
}

// Store the traffic-on flag in muCustomFlags bit 17.
void GameParams::SetTrafficOn(bool lbTrafficOn)
{
    const u32 luMask = 1u << KU_BRN_GAMESEARCHDATA_TRAFFIC_ON_BASE_BIT;
    muCustomFlags = ((static_cast<u32>(lbTrafficOn) << KU_BRN_GAMESEARCHDATA_TRAFFIC_ON_BASE_BIT) & luMask)
                  | (muCustomFlags & ~luMask);
}

// Store the traffic-checking flag in muCustomFlags bit 23.
void GameParams::SetTrafficCheckingOn(bool lbTrafficCheckingOn)
{
    const u32 luMask = 1u << KU_BRN_GAMESEARCHDATA_TRAFFIC_CHECKING_ON_BASE_BIT;
    muCustomFlags = ((static_cast<u32>(lbTrafficCheckingOn) << KU_BRN_GAMESEARCHDATA_TRAFFIC_CHECKING_ON_BASE_BIT) & luMask)
                  | (muCustomFlags & ~luMask);
}

// Range-check the time limit, then pack it into the leaf game-params word bits 15..19
// (mask 0xF8000).
void GameParams::SetTimeLimit(s32 liTimeLimit)
{
    CGS_ASSERT(liTimeLimit >= 0, "liTimeLimit >= 0");
    CGS_ASSERT(liTimeLimit < (1 << KI_GAME_PARAMS_TIME_LIMIT_NUM_BITS),
               "liTimeLimit < (1<<KI_GAME_PARAMS_TIME_LIMIT_NUM_BITS)");
    mGameData.muUser1 = ((static_cast<u32>(liTimeLimit) << KI_GAME_PARAMS_TIME_LIMIT_BASE_BIT) & 0xF8000u)
                      | (mGameData.muUser1 & 0xFFF07FFFu);
}

// X360 @ 0x82584068. Read the 5-bit TIME_LIMIT field (bits 15..19) of the leaf
// game-params word.
s32 GameParams::TimeLimit() const
{
    const u32 luMask = (1u << KI_GAME_PARAMS_TIME_LIMIT_NUM_BITS) - 1;
    return static_cast<s32>((mGameData.muUser1 >> KI_GAME_PARAMS_TIME_LIMIT_BASE_BIT) & luMask);
}

// X360 @ 0x825840D8. Vehicle-choice bit (bit 30) of muCustomFlags (+0xE8).
s32 GameParams::VehicleChoice() const
{
    return (muCustomFlags >> 30) & 0x1;
}

// X360 @ 0x82584048. 4-bit vehicle-level-limit field (bits 13..16) of muCustomFlags (+0xE8).
s32 GameParams::VehicleLevelLimit() const
{
    return (muCustomFlags >> 13) & 0xF;
}

// Store the locality word (+0x164). The console body is one store, shared by identical
// code folding with an unrelated setter of the same shape.
void GameParams::SetLocality(u32 luLocality)
{
    mGameData.muUser3 = luLocality;
}


// The defaults a freshly prepared game advertises: this build's network version, a
// two-to-eight player game over three laps with traffic (and traffic checking) on, no vehicle
// level limit, three runner crashes, previous-game-mode field 8 and the unset-locality marker.
static const s32 KI_GAME_PARAMS_DEFAULT_NETWORK_VERSION = 2;
static const s32 KI_GAME_PARAMS_DEFAULT_LAPS            = 3;
static const s32 KI_GAME_PARAMS_DEFAULT_MIN_PLAYERS     = 2;
static const s32 KI_GAME_PARAMS_DEFAULT_MAX_PLAYERS     = 8;
static const s32 KI_GAME_PARAMS_DEFAULT_VEHICLE_LEVEL   = 0;
static const u32 KU_GAME_PARAMS_DEFAULT_PREVIOUS_GAMEMODE_FIELD = 8;   // packed (bias-subtracted) value
static const s32 KI_GAME_PARAMS_DEFAULT_RUNNER_CRASHES  = 3;
static const u32 KU_GAME_PARAMS_UNSET_LOCALITY          = 0x7A7A5A5Au;

// The lobby game record SerialiseFromGame reads is the network library's own structure: its
// per-player entries start at +0x294 and are 0xA4 bytes apart (console layout).
static const s32 KI_LOBBY_GAME_PLAYER_RECORDS_OFFSET = 0x294;
static const s32 KI_LOBBY_GAME_PLAYER_RECORD_SIZE    = 0xA4;

// Reset to the default game: the platform defaults first, then an empty name and password,
// the default slot counts and packed settings, an unranked game in the first game mode, and
// every player record prepared.
bool GameParams::Prepare()
{
    if (!CgsNetwork::ServerInterfaceGameParamsX360::Prepare())
    {
        return false;
    }

    SetName("");
    SetPassword("");

    mGameData.muUser1 &= ~((((1u << KI_GAME_PARAMS_VEHICLE_LEVEL_NUM_BITS) - 1u) << KI_GAME_PARAMS_VEHICLE_LEVEL_BASE_BIT)
                           | (((1u << KI_GAME_PARAMS_INFINITE_BOOST_NUM_BITS) - 1u) << KI_GAME_PARAMS_INFINITE_BOOST_BASE_BIT));
    SetMinPlayers(KI_GAME_PARAMS_DEFAULT_MIN_PLAYERS);
    SetMaxPlayers(KI_GAME_PARAMS_DEFAULT_MAX_PLAYERS);
    SetTotalSlots(KI_GAME_PARAMS_DEFAULT_MAX_PLAYERS, 0);

    const u32 luNetworkVersionMask =
        (((1u << KI_GAME_PARAMS_NETWORK_VERSION_NUM_BITS) - 1u) << KI_GAME_PARAMS_NETWORK_VERSION_BASE_BIT);
    const u32 luLapsMask      = (((1u << KI_GAME_PARAMS_LAPS_NUM_BITS) - 1u) << KI_GAME_PARAMS_LAPS_BASE_BIT);
    const u32 luTimeLimitMask = (((1u << KI_GAME_PARAMS_TIME_LIMIT_NUM_BITS) - 1u) << KI_GAME_PARAMS_TIME_LIMIT_BASE_BIT);
    mGameData.muUser1 = (mGameData.muUser1 & ~(luNetworkVersionMask | luLapsMask | luTimeLimitMask))
                      | (static_cast<u32>(KI_GAME_PARAMS_DEFAULT_NETWORK_VERSION) << KI_GAME_PARAMS_NETWORK_VERSION_BASE_BIT)
                      | (static_cast<u32>(KI_GAME_PARAMS_DEFAULT_LAPS) << KI_GAME_PARAMS_LAPS_BASE_BIT);

    const u32 luSecurityMask =
        (((1u << KU_BRN_GAMESEARCHDATA_SECURITY_NUM_BITS) - 1u) << KU_BRN_GAMESEARCHDATA_SECURITY_BASE_BIT);
    const u32 luSkillMask = (((1u << KU_BRN_GAMESEARCHDATA_SKILL_NUM_BITS) - 1u) << KU_BRN_GAMESEARCHDATA_SKILL_BASE_BIT);
    const u32 luTrafficCheckingMask = (((1u << KU_BRN_GAMESEARCHDATA_TRAFFIC_CHECKING_ON_NUM_BITS) - 1u) << KU_BRN_GAMESEARCHDATA_TRAFFIC_CHECKING_ON_BASE_BIT);
    const u32 luBoostTypeMask =
        (((1u << KU_BRN_GAMESEARCHDATA_BOOST_TYPE_NUM_BITS) - 1u) << KU_BRN_GAMESEARCHDATA_BOOST_TYPE_BASE_BIT);
    const u32 luRunnerCrashesMask =
        (((1u << KU_BRN_GAMESEARCHDATA_RUNNER_CRASHES_NUM_BITS) - 1u) << KU_BRN_GAMESEARCHDATA_RUNNER_CRASHES_BASE_BIT);
    muCustomFlags = (muCustomFlags & ~(luSecurityMask | luSkillMask | luTrafficCheckingMask | luBoostTypeMask | luRunnerCrashesMask))
                  | (1u << KU_BRN_GAMESEARCHDATA_TRAFFIC_CHECKING_ON_BASE_BIT)
                  | (static_cast<u32>(KI_GAME_PARAMS_DEFAULT_RUNNER_CRASHES) << KU_BRN_GAMESEARCHDATA_RUNNER_CRASHES_BASE_BIT);

    miRoomID = -1;
    SetRankedGame(false);
    SetGameMode(KI_GAME_PARAMS_GAME_MODE_BIAS);

    const u32 luVehicleLevelMask =
        (((1u << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_NUM_BITS) - 1u) << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_BASE_BIT);
    const u32 luTrafficOnMask =
        (((1u << KU_BRN_GAMESEARCHDATA_TRAFFIC_ON_NUM_BITS) - 1u) << KU_BRN_GAMESEARCHDATA_TRAFFIC_ON_BASE_BIT);
    const u32 luPreviousGameModeMask = (((1u << KU_BRN_GAMESEARCHDATA_PREVIOUS_GAMEMODE_NUM_BITS) - 1u) << KU_BRN_GAMESEARCHDATA_PREVIOUS_GAMEMODE_BASE_BIT);
    muCustomFlags = (muCustomFlags & ~(luVehicleLevelMask | luTrafficOnMask | luPreviousGameModeMask))
                  | (static_cast<u32>(KI_GAME_PARAMS_DEFAULT_VEHICLE_LEVEL) << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_BASE_BIT)
                  | (1u << KU_BRN_GAMESEARCHDATA_TRAFFIC_ON_BASE_BIT)
                  | (KU_GAME_PARAMS_DEFAULT_PREVIOUS_GAMEMODE_FIELD << KU_BRN_GAMESEARCHDATA_PREVIOUS_GAMEMODE_BASE_BIT);
    mbJoinUserset = false;

    mGameData.muUser3 = KU_GAME_PARAMS_UNSET_LOCALITY;

    for (s32 liSlot = 0; liSlot < KI_GAMEPARAMS_PLAYER_SLOTS; ++liSlot)
    {
        maPlayerParams[liSlot].Prepare();
    }
    return true;
}

// Serialise the platform game record, then let each of the seven player-param records read
// its own player entry of the lobby record.
void GameParams::SerialiseFromGame(const void* lpGame)
{
    CgsNetwork::ServerInterfaceGameParamsX360::SerialiseFromGame(lpGame);

    const u8* lpu8PlayerRecords = static_cast<const u8*>(lpGame) + KI_LOBBY_GAME_PLAYER_RECORDS_OFFSET;
    for (s32 liSlot = 0; liSlot < KI_GAMEPARAMS_PLAYER_SLOTS; ++liSlot)
    {
        maPlayerParams[liSlot].SerialiseFromPlayer(lpu8PlayerRecords + liSlot * KI_LOBBY_GAME_PLAYER_RECORD_SIZE);
    }
}

} // namespace BrnNetwork
