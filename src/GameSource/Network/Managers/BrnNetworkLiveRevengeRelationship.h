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

    // BrnNetworkLiveRevengeRelationship.h:94 (DWARF). Total object size 120 bytes
    // (X360-AUTHORITATIVE). The X360 InGamePlayerStatusData copy/ctor
    // (BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData::operator= @ 0x823628C8) block-copies
    // 120 bytes for mLiveRevengeRelationship spanning object +136..+256, which places
    //   InGamePlayerStatusData::mPlayerName     at +256  (NetworkPlayerStats[136] + this[120] == 256)
    //   InGamePlayerStatusData::mNetworkPlayerID at +272  (256 + PlayerName[16])
    // matching the X360 stores. The PS3 DecFIGS DWARF spells mUniqueID as UniquePlayerIDPS3, whose
    // combined size with DateAndTime is 4 bytes smaller than the X360 build's (DWARF would imply a
    // 116-byte object placing PlayerName at +252 -- PS3 drift); the X360 binary wins, so the un-homed
    // mLastTimeChanged(DateAndTime)+mUniqueID(MugshotInfo::UniquePlayerID) pad is 40 bytes here.
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
        // first two are modelled as an opaque 40-byte pad to keep the object at the
        // X360-proven 120-byte size (see header comment: the InGamePl copy memcpy's 120B
        // for this object so PlayerName lands at +256) without fabricating their internal
        // layouts. The owning TUs of those accessors must replace this pad with the real
        // members, keeping the combined DateAndTime+UniquePlayerID span at 40 bytes.
        u8  mPad_LastTimeChanged_UniqueID[40];     // +72 (DateAndTime + UniquePlayerID)
        s32 miCurrentScoreForPlayersPointOfView;   // +112
        u32 miTotalEvents;                         // +116
    };
} // namespace BrnNetwork
// static_assert(sizeof(BrnNetwork::LiveRevengeRelationship) == 120, "X360 layout");
