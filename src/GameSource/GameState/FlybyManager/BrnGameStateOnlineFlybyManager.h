#pragma once

#include <cstddef> // offsetof

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h" // CgsContainers::FastBitArray
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h" // CgsSystem::DateAndTime

// ===========================================================================
// BrnGameState::OnlineFlybyManager
//
// The online flyby / rival-rating subsystem (DWARF home
// GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h). It derives from
// BrnGameState::FlybyManager and, for each connected network player, computes a "rivalry
// weighting" used to pick which rivals appear in the pre-race flyby and which flavour of
// rivalry message is shown.
//
// LAYOUT AUTHORITY: all member offsets below are pinned from the X360 ARTIST asm
// (BURNOUT_X360_ARTIST.XEX) of the twelve recovered functions, NOT from the PS3-flavoured
// DecFIGS DWARF. Key offsets, all relative to the OnlineFlybyManager `this`:
//   +0x000 .. +0x29F   FlybyManager base subobject (0x2A0 bytes; see base TU)
//   +0x2A0 (672)       mPlayerStatusInterface: InGamePlayerStatusInterface
//                        -> maPlayerStats[]   : NetworkPlayerStats[]  (record stride 312)
//                        -> miNumPlayers       at this+0x2A0+0x9E4 == this+0xC84 (3204)
//   +0xC88 (3208)      mAvailableNoRivalryMessages: FastBitArray<5>
//
// NOTE: CalculateRivalryRating's "no-relationship" branch advances an LCG whose 32-bit state is
// *(this + 640) -- byte 0x280, which lies INSIDE the base FlybyManager::mRandom (CgsNumeric::Random
// at +0x260). It is reached here through the base mRandom blob, not a derived member.
//
// The X360 build reaches the per-player record as a1 + 672 + 312*index. Within a 312-byte
// NetworkPlayerStats record:
//   +0x100 (256)  mPlayerName            : PlayerName       (GetRivalName returns &record+256)
//   +0x110 (272)  mNetworkPlayerID       : NetworkPlayerID  (lookup key)
//   +0x120 (288)  mMarkedManTargetID     : NetworkPlayerID  (who this player is marking)
//   +0x088 (136)  mLiveRevengeRelationship    : LiveRevengeRelationship
//   +0x0AC (172)  mSquareRevengeRelationship  : LiveRevengeRelationship (+36 from the live one)
//
// Several functions take a FlybyManager::RivalRating* (its mbHasValidRelationship/mPlayerID/
// miRivalWeighting at +0/+4/+8) and accumulate weightings into miRivalWeighting.
// ===========================================================================

template <typename T>
struct Pointer32
{
    u32 muAddress;

    void Set(T* lpPointer)
    {
        muAddress = static_cast<u32>(reinterpret_cast<usize>(lpPointer));
    }

    T* Get() const
    {
        return reinterpret_cast<T*>(static_cast<usize>(muAddress));
    }
};

namespace BrnNetwork
{
struct RoadRulesRecvData
{
    typedef s32 NetworkPlayerID;
};
}

namespace CgsNetwork
{
static const BrnNetwork::RoadRulesRecvData::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

namespace BrnNetwork
{
// ---------------------------------------------------------------------------
// LiveRevengeRelationship -- the per-pair revenge/takedown record embedded in every
// NetworkPlayerStats. Layout pinned from the rating-calculator asm (offsets relative to the
// relationship base, which sits at NetworkPlayerStats+136):
//   +0x00 (DWORD 0)  miPlayerTakedowns      (GetOngoingStat case 0 reads [1] == +4; see below)
// Only the members the twelve functions touch are named; the rest of the 36-byte common-stats
// block + trailing fields are reserved as a sized blob so the embedded size is correct.
//
// The common-stats sub-block ("mOverallStats") begins at the relationship base. GetOngoingStat
// reads it by DWORD index: [1]=mMugshotsCollected, [4]=mMarks, [2]=mLongestStreak, [3]=mWins,
// [7]=mPaybacksSuccessful, [8]=mPaybacksTotal (escaped = total-successful). The live takedown
// counters used by CalculateOnlineRivals are mPlayerStats.miTakedowns (+0) and
// mRivalStats.miTakedowns (DWORD [9] == +36 -> the second 36-byte stats block).
// The embedded "last activity" DateAndTime sits at +72 (DWORD [18..20]).
// ---------------------------------------------------------------------------
struct LiveRevengeRelationship
{
    // --- mOverallStats.mPlayerStats : the player-side CommonStats sub-block (base +0) ---
    s32 miTakedowns;              // +0x00  CalculateOnlineRivals: player-side takedowns
    s32 miMugshotsCollected;      // +0x04  GetOngoingStat [1]
    s32 miLongestStreak;          // +0x08  GetOngoingStat [2] / GetNewRivalry +8
    s32 miWins;                   // +0x0C  GetOngoingStat [3]
    s32 miMarks;                  // +0x10  GetOngoingStat [4]
    s32 miScoreSettled;           // +0x14  GetNewRivalry +20 (per-side "score settled")
    s32 maReserved_18;            // +0x18
    s32 miPaybacksSuccessful;     // +0x1C  GetOngoingStat [7]
    s32 miPaybacksTotal;          // +0x20  GetOngoingStat [8]

    // --- mOverallStats.mRivalStats : the rival-side CommonStats sub-block (base +0x24) ---
    // GetNewRivalryStat reaches a per-side view as (this + 36) so the same field names line up.
    s32 miRivalTakedowns;         // +0x24  CalculateOnlineRivals v24[9]
    s32 miRivalMugshotsCollected; // +0x28
    s32 miRivalLongestStreak;     // +0x2C
    s32 miRivalWins;              // +0x30
    s32 miRivalMarks;             // +0x34
    s32 miRivalScoreSettled;      // +0x38
    s32 maRivalReserved_3C[3];    // +0x3C

    // mLastActivity : CgsSystem::DateAndTime (DWORD [18..20] == +0x48, 12 bytes)
    CgsSystem::DateAndTime mLastActivity; // +0x48

    s32 maReserved_54[7];         // +0x54 .. +0x70  (pad up to the net-stat fields)
    s32 miNetTakedownDelta;       // +0x70 (112)  GetNewRivalry "current status" net delta
    s32 miEventsPlayedTogether;   // +0x74 (116)  GetNewRivalry "events" (side-independent)

    bool Validate() const;                 // external (other-TU body)
    s32  GetTotalTakedowns() const;        // external (other-TU body)
};
}

namespace BrnGameState
{
class GameStateModule;
class ScoringSystem;

namespace GameStateModuleIO { struct FlybyData; }

// ---------------------------------------------------------------------------
// FlybyManager base slice. The full base home lives in BrnGameStateFlybyManager.cpp; here we
// re-declare the same byte-exact 0x2A0 layout this derived TU stands on, so the derived members
// land at the X360-proven offsets and `this`-relative offset math resolves by name. Members are
// identical to the base TU (do NOT reorder/retype/resize).
// ---------------------------------------------------------------------------
class FlybyManager
{
public:
    GameStateModule* GetGameStateModule();   // asserts non-null, returns mpGameStateModule

    struct RivalRating
    {
        bool mbHasValidRelationship;                                   // +0x00
        u8   mPad1[3];
        BrnNetwork::RoadRulesRecvData::NetworkPlayerID mPlayerID;      // +0x04
        s32  miRivalWeighting;                                        // +0x08

        static s32 SortRivalsCallback(const void* lpData0, const void* lpData1);
    };

    GameStateModuleIO::FlybyData* GetFlybyData();   // returns &mFlybyData (this+0x10)

protected:
    u32                            mVTable;            // +0x000
    u8                             maUnknown_04[0x0C]; // +0x004
    u8                             mFlybyData[0x250];  // +0x010
    u8                             mRandom[0x30];      // +0x260 CgsNumeric::Random
    Pointer32<GameStateModule>     mpGameStateModule;  // +0x290
    Pointer32<ScoringSystem>       mpScoringSystem;    // +0x294
    s32                            miRankLeader;       // +0x298
    s32                            miRankOustider;     // +0x29C
};

static_assert(sizeof(FlybyManager) == 0x2A0, "FlybyManager base layout drift");

// ---------------------------------------------------------------------------
// NetworkPlayerStats -- one 312-byte per-player record held in the player-status interface.
// Only the fields the twelve functions read are named; the surrounding bytes are reserved blobs
// so the record size (and therefore the inter-record stride the asm relies on) stays at 312.
// ---------------------------------------------------------------------------
struct NetworkPlayerStats
{
    u8                                  maHeader[136];                   // +0x000
    BrnNetwork::LiveRevengeRelationship mLiveRevengeRelationship;        // +0x088 (136), 120 bytes
    u8                                  maPlayerName[16];                // +0x100 (256) PlayerName
    BrnNetwork::RoadRulesRecvData::NetworkPlayerID mNetworkPlayerID;     // +0x110 (272)
    u8                                  maPad_114[12];                   // +0x114
    BrnNetwork::RoadRulesRecvData::NetworkPlayerID mMarkedManTargetID;   // +0x120 (288)
    u8                                  maTail[312 - 292];               // +0x124..0x138

    // The 172-offset square relationship overlaps the same storage region; GetOngoingStat reaches
    // it as (&mLiveRevengeRelationship + 36 bytes). Modelled as a typed accessor rather than a
    // second member to keep the single 312-byte footprint unambiguous.
    BrnNetwork::LiveRevengeRelationship* GetLiveRelationship()
    {
        return &mLiveRevengeRelationship;
    }
    BrnNetwork::LiveRevengeRelationship* GetSquareRelationship()
    {
        return reinterpret_cast<BrnNetwork::LiveRevengeRelationship*>(
            reinterpret_cast<u8*>(&mLiveRevengeRelationship) + 36);
    }
    static s32 GetStatAsInt(const NetworkPlayerStats* lpStats, s32 liStatIndex); // external
};

static_assert(sizeof(NetworkPlayerStats) == 312, "NetworkPlayerStats record stride drift");

// ---------------------------------------------------------------------------
// InGamePlayerStatusInterface -- the per-player-stats array + count that the OnlineFlybyManager
// embeds at this+0x2A0. Count sits at +0x9E4 within the interface (== this+0xC84). Only the
// fields the twelve functions touch are named.
// ---------------------------------------------------------------------------
struct InGamePlayerStatusInterface
{
    NetworkPlayerStats maPlayerStats[8];   // +0x000 (8 * 312 == 0x9C0)
    u8                 maPad_9C0[0x24];     // +0x9C0..0x9E4
    s32                miNumPlayers;        // +0x9E4 (2532)
};

static_assert(offsetof(InGamePlayerStatusInterface, miNumPlayers) == 0x9E4,
              "miNumPlayers offset drift");

class GameStateModule
{
public:
    BrnNetwork::RoadRulesRecvData::NetworkPlayerID GetLocalPlayerNetworkID(); // external
    s32 GetPlayerActiveRaceCarIndex();                                        // external
};

// ===========================================================================
// OnlineFlybyManager : public FlybyManager
// ===========================================================================
class OnlineFlybyManager : public FlybyManager
{
public:
    // ---- rivalry-stat comparison enums (DWARF BrnGameStateOnlineFlybyManager.h) ----
    enum ENoRivalryCompare : s32
    {
        E_NO_RIVALRY_START        = 0,
        E_NO_RIVALRY_TAKEDOWNS    = 0,
        E_NO_RIVALRY_TOTAL_RIVALS = 1,
        E_NO_RIVALRY_RANK         = 2,
        E_NO_RIVALRY_EVENTS       = 3,
        E_NO_RIVALRY_WINS         = 4,
        E_NO_RIVALRY_COUNT        = 5,
    };

    enum ENewRivalryCompare : s32
    {
        E_NEW_RIVALRY_START          = 0,
        E_NEW_RIVALRY_CURRENT_STATUS = 0,
        E_NEW_RIVALRY_SCORE_SETTLED  = 1,
        E_NEW_RIVALRY_EVENTS         = 2,
        E_NEW_RIVALRY_LONGEST_STREAK = 3,
        E_NEW_RIVALRY_COUNT          = 4,
    };

    enum EOngoingRivalryCompare : s32
    {
        E_ONGOING_RIVALRY_START               = 0,
        E_ONGOING_RIVALRY_MUGSHOTS_COLLECTED  = 0,
        E_ONGOING_RIVALRY_MARKS               = 1,
        E_ONGOING_RIVALRY_LONGEST_STREAK      = 2,
        E_ONGOING_RIVALRY_WINS                = 3,
        E_ONGOING_RIVALRY_PAYBACKS_SUCCESSFUL = 4,
        E_ONGOING_RIVALRY_PAYBACKS_ESCAPED    = 5,
        E_ONGOING_RIVALRY_COUNT               = 6,
    };

    enum EMessageStyle : s32
    {
        E_MESSAGE_STYLE_POSITIVE = 0,
        E_MESSAGE_STYLE_NEGATIVE = 1,
        E_MESSAGE_STYLE_COUNT    = 2,
    };

    // ---- the twelve functions owned by this TU ----
    s32  CalculateNumberOfCarsInFlyby();                                        // 0x82357F08
    void CalculateOnlineRivals();                                              // 0x823861C0

    void CalculateRivalryRating(FlybyManager::RivalRating* lpRating);          // 0x8236DAE0
    void CalculateNewOrOngoingRivalryRating(FlybyManager::RivalRating* lpRating); // 0x82358028
    void CalculateMarkedManRivalryRating(FlybyManager::RivalRating* lpRating);  // 0x823642F0
    void CalculatePointsLeaderRating(FlybyManager::RivalRating* lpRating);      // 0x82358148

    const u8* GetRivalName(BrnNetwork::RoadRulesRecvData::NetworkPlayerID lPlayerID); // 0x823581F8

    s32  GetOngoingStat(FlybyManager::RivalRating* lpRating,
                        EOngoingRivalryCompare leStatType, EMessageStyle leStyle); // 0x823582A0
    s32  GetNewRivalryStat(FlybyManager::RivalRating* lpRating,
                           ENewRivalryCompare leStatType, EMessageStyle leStyle);  // 0x82358418
    s32  GetNoRivalryStat(FlybyManager::RivalRating* lpRating,
                          ENoRivalryCompare leStatType);                           // 0x82364488

    s32  CountAvailableOngoingRivalryMessages(EOngoingRivalryCompare leStatType,
                                              EMessageStyle leStyle);               // 0x82358570
    s32  CountAvailableNewRivalryMessages(ENewRivalryCompare leStatType,
                                          EMessageStyle leStyle);                   // 0x82358648

    // ---- helpers reached by the twelve (other ledger entries / external); declare-only ----
    BrnNetwork::RoadRulesRecvData::NetworkPlayerID GetLocalPlayerNetworkID();
    NetworkPlayerStats* GetPlayerStats(BrnNetwork::RoadRulesRecvData::NetworkPlayerID lPlayerID);
    BrnNetwork::LiveRevengeRelationship* GetLiveRevengeRelationship(
        BrnNetwork::RoadRulesRecvData::NetworkPlayerID lPlayerID) const;
    void CalculateDisconnectRating(FlybyManager::RivalRating* lpRating);
    void CalculateTeamRating(FlybyManager::RivalRating* lpRating);
    void AddNoRivalryFlybyMessage(FlybyManager::RivalRating* lpRating);
    void AddNewRivalryMessage(FlybyManager::RivalRating* lpRating);
    void AddOngoingRivalryMessage(FlybyManager::RivalRating* lpRating);

private:
    InGamePlayerStatusInterface         mPlayerStatusInterface;       // +0x2A0
    CgsContainers::FastBitArray<5>      mAvailableNoRivalryMessages;  // +0xC88
};

static_assert(sizeof(FlybyManager) + offsetof(InGamePlayerStatusInterface, miNumPlayers)
                  == 3204,
              "miNumPlayers absolute offset drift (must be 3204)");

// ===========================================================================
// Cross-TU boundary shims.
//
// The functions below live in OTHER ledger TUs (ScoringSystem, GameStateModule, the FlybyData
// payload in BrnGameStateSharedIO, and this class's own non-twelve message-builder helpers). The
// twelve reconstructed functions call them, so they are DECLARED here (no body in this TU) at
// their X360 call signatures. They are reached BY NAME -- this header models the call boundary,
// not the callees' internals (those are owned and bodied by their own TUs). The compile gate is
// `cl /c` (no link), so these unresolved externals are expected.
// ===========================================================================
s32 ScoringSystem_GetNumberOfNetworkPlayersStillConnected(ScoringSystem* lpScoringSystem);
BrnNetwork::RoadRulesRecvData::NetworkPlayerID ScoringSystem_GetPointsLeader(
    ScoringSystem* lpScoringSystem);

// The X360 cross-references a network player to its active-race-car index via either the module's
// or the scoring system's player table (sub_8231DD88). Two overloads keep the call sites typed.
s32 ModulePlayerActiveRaceCarIndex(GameStateModule* lpModule,
                                   BrnNetwork::RoadRulesRecvData::NetworkPlayerID lPlayerID);
s32 ModulePlayerActiveRaceCarIndex(ScoringSystem* lpScoringSystem,
                                   BrnNetwork::RoadRulesRecvData::NetworkPlayerID lPlayerID);

void FlybyData_Prepare(GameStateModuleIO::FlybyData* lpFlybyData);
void FlybyData_AddCar(GameStateModuleIO::FlybyData* lpFlybyData, s32 leActiveRaceCarIndex,
                      const u8* lpPlayerName);
void FlybyData_AddMessage(GameStateModuleIO::FlybyData* lpFlybyData, const char* lpMessageId,
                          const u8* lpPlayerName);
s32  FlybyData_GetNumberOfCars(GameStateModuleIO::FlybyData* lpFlybyData);
void FlybyData_TagLastCarStyle(GameStateModuleIO::FlybyData* lpFlybyData, s32 liStyle);

// The pre-race rivalry message tables (DWARF: CombinedStringID[statType][2 styles][KI_MAX_MESSAGES]).
// Modelled as ragged pointer rows here; CountAvailable* walk a row to its null terminator. Each entry
// is a CombinedStringID (two pointer-width words), so a row step is 2 and a per-style row is
// KI_MAX_MESSAGES(5) * 2 == 10 slots wide. The X360 indexes off_82CDBBB4/off_82CDBA74 as
// base[20*statType + 10*style] (asm 0x82358570 / 0x82358648), i.e. statType stride 20, style stride
// 10 -- so the innermost extent MUST be 10 for KA[statType][style] to land on the right row.
// Defined (with their real string-id contents) in the owning data TU; declared extern here.
extern const void* const KA_ONGOING_RIVALRY_MESSAGES
    [OnlineFlybyManager::E_ONGOING_RIVALRY_COUNT][OnlineFlybyManager::E_MESSAGE_STYLE_COUNT][5 * 2];
extern const void* const KA_NEW_RIVALRY_MESSAGES
    [OnlineFlybyManager::E_NEW_RIVALRY_COUNT][OnlineFlybyManager::E_MESSAGE_STYLE_COUNT][5 * 2];

// ---- relationship sub-block offset checks (pinned from the rating-calculator asm) ----
static_assert(offsetof(BrnNetwork::LiveRevengeRelationship, miMugshotsCollected) == 0x04, "rel +4");
static_assert(offsetof(BrnNetwork::LiveRevengeRelationship, miLongestStreak) == 0x08, "rel +8");
static_assert(offsetof(BrnNetwork::LiveRevengeRelationship, miMarks) == 0x10, "rel +16");
static_assert(offsetof(BrnNetwork::LiveRevengeRelationship, miPaybacksSuccessful) == 0x1C, "rel +28");
static_assert(offsetof(BrnNetwork::LiveRevengeRelationship, miPaybacksTotal) == 0x20, "rel +32");
static_assert(offsetof(BrnNetwork::LiveRevengeRelationship, miRivalTakedowns) == 0x24, "rel +36");
static_assert(offsetof(BrnNetwork::LiveRevengeRelationship, mLastActivity) == 0x48, "rel +72");
static_assert(offsetof(BrnNetwork::LiveRevengeRelationship, miNetTakedownDelta) == 0x70, "rel +112");
static_assert(offsetof(BrnNetwork::LiveRevengeRelationship, miEventsPlayedTogether) == 0x74, "rel +116");
static_assert(sizeof(BrnNetwork::LiveRevengeRelationship) == 0x78, "relationship size drift (120)");
static_assert(offsetof(NetworkPlayerStats, maPlayerName) == 0x100, "playerName +256");
static_assert(offsetof(NetworkPlayerStats, mNetworkPlayerID) == 0x110, "playerID +272");
static_assert(offsetof(NetworkPlayerStats, mMarkedManTargetID) == 0x120, "markedMan +288");
}
