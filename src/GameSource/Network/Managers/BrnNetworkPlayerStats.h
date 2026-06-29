// ===================================================================================
// BrnNetwork::NetworkPlayerStats  -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkPlayerStats.h
//
// One player's cached online stats record (12 stat values + their display types,
// a timestamp, the player name, the network player id, and an age/calc/local-player
// status block). Layout authoritative from the DecFIGS DWARF
// (BrnNetworkPlayerStats.h:53). Total size 136 bytes, confirmed independently by the
// X360 binary: BrnNetworkModuleIO::InGamePlayerStatusData embeds this object as a
// 136-byte slot (committed stub `u8 mPad0[136]` in BrnNetworkModuleInGamePlayerStatusInterface.cpp)
// and NetworkPlayerStatsResults stores a NetworkPlayerStats[32] cache.
//
// Only operator= is binary-recovered in this TU (@0x82355C50); it is a plain
// member-wise copy of every data member. All other methods are declared-only here
// (their bodies live in BrnNetworkPlayerStats.cpp as separate TUs).
//
// NOTE on member type: the DWARF spells mNetworkPlayerID with a drifting nested
// scope (RoadRulesRecvData::NetworkPlayerID / GuiEventNetworkLaunching::NetworkPlayerID /
// AggressiveMoveData::NetworkPlayerID across DIE copies). On X360 this is the single
// committed top-level typedef BrnNetwork::NetworkPlayerID (== s32, 4 bytes) from
// BrnNetworkSharedIO.h; we use that rather than re-forking a nested alias.
#pragma once

#include "BrnCommonTypes.h"                                   // s32 / f32
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/System/Timer/CgsTime.h"      // CgsSystem::Time

namespace BrnNetwork
{
    typedef CgsSystem::Time Time;   // the DWARF/source spell CgsSystem::Time unqualified as `Time`

    class NetworkPlayerStats
    {
    public:

        // DWARF BrnNetworkPlayerStats.h:57
        enum EStatsStatus
        {
            E_STATS_AGE_UNPREPARED = 0,
            E_STATS_AGE_ERROR      = 1,
            E_STATS_AGE_INVALID    = 2,
            E_STATS_AGE_CURRENT    = 3,
            E_STATS_AGE_UPDATING   = 4,
        };

        // DWARF BrnNetworkPlayerStats.h:67 (note START==TOTAL_GAMES_COMPLETED==0)
        enum EStatsValue
        {
            E_STATS_VALUE_START                   = 0,
            E_STATS_VALUE_TOTAL_GAMES_COMPLETED   = 0,
            E_STATS_VALUE_RANKED_GAMES_COMPLETED  = 1,
            E_STATS_VALUE_UNRANKED_GAMES_COMPLETED= 2,
            E_STATS_VALUE_WIN_PERCENT             = 3,
            E_STATS_VALUE_TAKEDOWNS               = 4,
            E_STATS_VALUE_NUMBER_OF_RIVALS        = 5,
            E_STATS_VALUE_ACHIEVEMENTS_EARNT      = 6,
            E_STATS_VALUE_CHALLENGES_COMPLETED    = 7,
            E_STATS_VALUE_RANK                    = 8,
            E_STATS_VALUE_RANKED_GAMES_STARTED    = 9,
            E_STATS_VALUE_UNRANKED_GAMES_STARTED  = 10,
            E_STATS_VALUE_WINS                    = 11,
            E_STATS_VALUE_COUNT                   = 12,
        };

        // DWARF BrnNetworkPlayerStats.h:87
        enum EStatType
        {
            E_STAT_TYPE_NUMBER  = 0,
            E_STAT_TYPE_STRING  = 1,
            E_STAT_TYPE_MONEY   = 2,
            E_STAT_TYPE_TIME    = 3,
            E_STAT_TYPE_PERCENT = 4,
            E_STAT_TYPE_COUNT   = 5,
        };

        // ---- lifecycle / API (declared-only; bodies are separate TUs) ----------
        void      Construct();
        bool      Prepare(const char* lpcName, Time lTimeStamp, bool lbIsLocalPlayer, NetworkPlayerID lPlayerID);
        bool      Release();
        void      Destruct();
        void      Clear();

        const s32 GetStatAsInt(EStatsValue leValue) const;

        // Address of one raw stat value slot (maiValues[liIndex]). The X360 stats debug component
        // (BrnNetworkStatsDebugComponent::AddStats/RemoveStats) registers each slot as an editable
        // s32* debug-menu variable, indexing the maiValues[] array directly (it is the first member,
        // at object offset 0). Inline accessor over the named member so callers do not offset-hack.
        s32* GetStatValuePtr(s32 liIndex) { return &maiValues[liIndex]; }
        void      GetStatAsString(EStatsValue leValue, char* lpcBuffer, s32 liSize) const;
        void      SetStat(EStatsValue leValue, const char* lpcStat, EStatType leType);
        void      SetStatAsInt(EStatsValue leValue, s32 liValue, EStatType leType);

        void      SetPlayerID(NetworkPlayerID lPlayerID);
        const char*     GetName() const;
        NetworkPlayerID GetPlayerID() const;
        EStatsStatus    GetStatus() const;
        void      SetStatus(EStatsStatus leStatus);
        void      SetTimeStamp(Time lTimeStamp);
        Time      GetTimeStamp() const;
        EStatType GetStatType(EStatsValue leValue) const;
        bool      IsCalculated() const;
        void      SetCalculated(bool lbCalculated);
        bool      IsLocalPlayer() const;

        // ---- owned by THIS TU --------------------------------------------------
        // Compiler-generated-style member-wise copy assignment (@0x82355C50).
        NetworkPlayerStats& operator=(const NetworkPlayerStats& lOther);

    public:
        // DWARF BrnNetworkPlayerStats.h:196 / :197 (defined in the .cpp TU)
        static const s32 KI_NO_RANK;
        static const s32 KI_MININUM_CHAR_BUFFER_SIZE = 11;

    private:
        // ---- data layout (DWARF BrnNetworkPlayerStats.h:201..211; total 136 B) --
        s32             maiValues[12];      // +0   [0..48)   stat values
        EStatType       maeStatType[12];    // +48  [48..96)  per-stat display type
        Time            mTimeStamp;         // +96  [96..104) CgsSystem::Time (s32+f32)
        char            macName[20];        // +104 [104..124)
        NetworkPlayerID mNetworkPlayerID;   // +124 [124..128) (s32)
        EStatsStatus    meStatsStatus;      // +128 [128..132)
        bool            mbIsCalulated;      // +132 (sic: spelling matches DWARF)
        bool            mbIsLocalPlayer;    // +133
        // +134..136 trailing pad to 4-byte alignment (struct size == 136)
    };

    // DWARF BrnNetworkPlayerStats.h:218 -- post-increment over the stat-value enum.
    NetworkPlayerStats::EStatsValue operator++(NetworkPlayerStats::EStatsValue& leValue, int);
}
