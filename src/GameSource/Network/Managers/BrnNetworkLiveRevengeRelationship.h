// ---- GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h ----
// All shape (member names, offsets, return types, const-ness, enum values, base)
// recovered from the DecFIGS DWARF for this exact header
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h),
// gated against the X360 binary (BrnNetwork::LiveRevengeRelationship::GetTotalTakedowns @ 0x82355540).
//
// Layout proof for GetTotalTakedowns (X360 reads *a1 + a1[9]):
//   *a1  -> mOverallStats.mPlayerStats.miTakedowns  (object +0)
//   a1[9]-> mOverallStats.mRivalStats.miTakedowns   (object +36; mPlayerStats is
//           9 x int32 = 36 bytes, so mRivalStats begins at +36, its miTakedowns at +36)
// This matches the X360-baked assert text exactly:
//   "mOverallStats.mPlayerStats.miTakedowns + mOverallStats.mRivalStats.miTakedowns >= 0"
#pragma once

#include "types.hpp"
#include "GameSource/GameState/BrnCgsPlayerName.h"   // CgsNetwork::PlayerName (committed, 16B)

namespace BrnNetwork
{
    // Uncommitted dependency types -- forward-declared only (this TU touches them only in
    // declaration-only accessors, never defining/calling them here). Promote to their real
    // homes when those TUs land.
    struct DateAndTime;     // mLastTimeChanged storage (un-homed)

    // BrnNetworkLiveRevengeRelationship.h:47 (DWARF) -- 9 x int32 == 36 bytes.
    struct CommonRelationshipStats
    {
        s32 miTakedowns;              // +0
        s32 miScalps;                 // +4
        s32 miLongestStreak;          // +8
        s32 miWins;                   // +12
        s32 miMarks;                  // +16
        s32 miScoresSettled;          // +20
        s32 miEventsSinceLastTakedown;// +24
        s32 miPaybacksScored;         // +28
        s32 miPaybacksDealt;          // +32

        void Clear();                 // own TU
    };

    // BrnNetworkLiveRevengeRelationship.h:74 (DWARF) -- two stat blocks == 72 bytes.
    struct CommonRelationship
    {
        CommonRelationshipStats mPlayerStats; // +0
        CommonRelationshipStats mRivalStats;  // +36

        void Clear();                         // own TU
    };

    // BrnNetworkLiveRevengeRelationship.h:94 (DWARF). Total object size 116 bytes
    // (proven by InGamePlayerStatusData: NetworkPlayerStats[136] + this[116] places
    //  PlayerName at +252 and mNetworkPlayerID at +272, matching the X360 stores).
    struct LiveRevengeRelationship
    {
        // BrnNetworkLiveRevengeRelationship.h:98 (DWARF)
        enum ERelationshipType
        {
            E_RELATIONSHIP_TYPE_NONE  = 0,
            E_RELATIONSHIP_TYPE_NEW   = 1,
            E_RELATIONSHIP_TYPE_OLD   = 2,
            E_RELATIONSHIP_TYPE_COUNT = 3,
        };

        // BrnNetworkLiveRevengeRelationship.h:108 (DWARF)
        enum ERelationshipStatus
        {
            E_RELATIONSHIP_STATUS_AHEAD  = 0,
            E_RELATIONSHIP_STATUS_EQUAL  = 1,
            E_RELATIONSHIP_STATUS_BEHIND = 2,
            E_RELATIONSHIP_STATUS_COUNT  = 3,
        };

    public:
        void Construct();                                   // own TU
        bool Release();                                     // own TU
        void Destruct();                                    // own TU
        void Clear();                                       // own TU
        void FlipPointOfView();                             // own TU
        void AddTakedownByLocalPlayer(bool);                // own TU
        void AddTakedownByRival(bool);                      // own TU
        void AddPaybackDealtByLocalPlayer();                // own TU
        void AddPaybackDealtByRival();                      // own TU
        void AddPaybackScoredByLocalPlayer();               // own TU
        void AddPaybackScoredByRival();                     // own TU
        void AddWinByLocalPlayer();                         // own TU
        void AddWinByRival();                               // own TU
        void AddMarkByPlayer();                             // own TU
        void AddMarkByRival();                              // own TU

        // === Reconstructed in this TU ===
        s32 GetTotalTakedowns() const;                      // @ 0x82355540

        // GetRivalName returns the committed CgsNetwork::PlayerName (do NOT re-fork PlayerName).
        const CgsNetwork::PlayerName* GetRivalName() const; // own TU
        s32  GetCurrentScoreForLocalPlayer() const;         // own TU
        s32  GetCurrentScoreForRival() const;               // own TU
        ERelationshipStatus GetRelationshipStatus() const;  // own TU
        s32  GetTotalEvents() const;                        // own TU
        const CommonRelationship* GetLastGameStats() const; // own TU
        const CommonRelationship* GetOverallStats() const;  // own TU
        void SetCurrentScoreForPlayer(s32);                 // own TU
        void SetTotalNumberOfEvents(s32);                   // own TU
        void SetOverallStats(CommonRelationship*);          // own TU
        void Merge(LiveRevengeRelationship*);               // own TU
        bool IsRivalAheadInCurrentRelationship() const;     // own TU
        bool IsPlayerAheadInCurrentRelationship() const;    // own TU
        bool IsCurrentRelationshipEqual() const;            // own TU
        ERelationshipType GetRelationshipType() const;      // own TU
        void OnRoundFinish();                               // own TU
        void OnRoundStart();                                // own TU
        bool Validate() const;                              // own TU
        // DEBUG callbacks are static per leak + DWARF (cast the void* into a local
        // LiveRevengeRelationship* rather than using `this`).
        static void DEBUGResetTimeStamp(void*);             // own TU
        static void DEBUGSetTimeStampOld(void*);            // own TU
        static void DEBUGClearRelationship(void*);          // own TU

    private:
        void ScoreSettled();                                // own TU
        void FlipCommonRelationship(CommonRelationship*);   // own TU
        void ValidateStat(s32, s32, s32, s32, const char*, bool); // own TU

    private:
        // BrnNetworkLiveRevengeRelationship.h:320 (DWARF) -- TOUCHED by this TU.
        CommonRelationship mOverallStats;          // +0  (72 bytes)

        // Untouched tail. Real members per DWARF are:
        //   BrnNetworkLiveRevengeRelationship.h:323  DateAndTime                 mLastTimeChanged;
        //   BrnNetworkLiveRevengeRelationship.h:326  MugshotInfo::UniquePlayerID mUniqueID;
        //   BrnNetworkLiveRevengeRelationship.h:329  s32                         miCurrentScoreForPlayersPointOfView;
        //   BrnNetworkLiveRevengeRelationship.h:332  u32                         miTotalEvents;
        // DateAndTime and MugshotInfo::UniquePlayerID are NOT committed yet, so the
        // first two are modelled as an opaque 36-byte pad to keep the object at the
        // proven 116-byte size without fabricating their internal layouts. The
        // owning TUs of those accessors must replace this pad with the real members.
        u8  mPad_LastTimeChanged_UniqueID[36];     // +72 (DateAndTime + UniquePlayerID)
        s32 miCurrentScoreForPlayersPointOfView;   // +108
        u32 miTotalEvents;                         // +112
    };
} // namespace BrnNetwork
// static_assert(sizeof(BrnNetwork::LiveRevengeRelationship) == 116, "X360 layout");
