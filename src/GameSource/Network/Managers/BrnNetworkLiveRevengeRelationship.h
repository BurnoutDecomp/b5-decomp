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
#include "GameShared/GameClasses/Network/Players/X360/CgsUniquePlayerIDX360.h" // CgsNetwork::UniquePlayerIDX360 (mUniqueID, 24B)
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h" // CgsSystem::DateAndTime (committed, 12B; mLastTimeChanged @+72)

namespace BrnNetwork
{

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

        // Header inline: every clear site zeroes the nine words in place.
        void Clear()
        {
            miTakedowns               = 0;
            miScalps                  = 0;
            miLongestStreak           = 0;
            miWins                    = 0;
            miMarks                   = 0;
            miScoresSettled           = 0;
            miEventsSinceLastTakedown = 0;
            miPaybacksScored          = 0;
            miPaybacksDealt           = 0;
        }
    };

    // BrnNetworkLiveRevengeRelationship.h:74 (DWARF) -- two stat blocks == 72 bytes.
    struct CommonRelationship
    {
        CommonRelationshipStats mPlayerStats; // +0
        CommonRelationshipStats mRivalStats;  // +36

        void Clear()
        {
            mPlayerStats.Clear();
            mRivalStats.Clear();
        }
    };

    // BrnNetworkLiveRevengeRelationship.h:94 (DWARF). Total object size 120 bytes
    // (X360-AUTHORITATIVE). The X360 InGamePlayerStatusData copy/ctor
    // (BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData::operator= @ 0x823628C8) block-copies
    // 120 bytes for mLiveRevengeRelationship spanning object +136..+256, which places
    //   InGamePlayerStatusData::mPlayerName     at +256  (NetworkPlayerStats[136] + this[120] == 256)
    //   InGamePlayerStatusData::mNetworkPlayerID at +272  (256 + PlayerName[16])
    // matching the console stores.
    // Forward declaration for the friend grant below (the debug component publishes this
    // relationship's private stat members into the debug menu by pointer).
    class LiveRevengeDebugComponent;

    struct LiveRevengeRelationship
    {
        // The Live Revenge debug-menu component registers writable pointers to this relationship's
        // private stat members (mOverallStats sub-fields, miCurrentScoreForPlayersPointOfView,
        // miTotalEvents) so they can be inspected/edited in-game. It therefore needs access to those
        // private members -- the X360 debug component (BrnNetworkLiveRevengeDebugComponent) reads them
        // by raw offset off the relationship pointer, which in clean C++ is friendship, not an
        // accessor (the const GetOverallStats() accessor cannot back a writable menu variable).
        friend class BrnNetwork::LiveRevengeDebugComponent;

        // The rival's identity: the player name plus the 64-bit XUID. The reference build spells it
        // through a platform typedef; this build's platform type is the Xbox identity.
        typedef CgsNetwork::UniquePlayerIDX360 UniquePlayerID;

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
        // Header inline (the console inlines it into InGamePlayerStatusData::Clear and
        // LiveRevengeManager::AddNewTableEntry).
        void Construct() { Clear(); }
        bool Prepare(const UniquePlayerID* lpUniquePlayerID); // own TU
        bool Release();                                     // own TU
        void Destruct();                                    // own TU

        // Header inline: every clear site (Release, Destruct, DEBUGClearRelationship, the sync
        // message's size query) emits the same store set. mLastTimeChanged keeps its local flag.
        void Clear()
        {
            // The identity: empty name and zero XUID (the UniquePlayerID clear, inlined).
            mUniqueID.macName[0] = '\0';
            mUniqueID.mqXuid     = 0;
            mLastTimeChanged.Clear();
            mOverallStats.Clear();
            miCurrentScoreForPlayersPointOfView = 0;
            miTotalEvents                       = 0;
        }

        void FlipPointOfView();                             // own TU
        void AddTakedownByLocalPlayer(bool lbMarkedMan);    // own TU
        void AddTakedownByRival(bool lbMarkedMan);          // own TU
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

        // The rival's 64-bit XUID (asm reads ld 0x68(rel) == +104 == mUniqueID.mqXuid).
        // Consumed by GameSearchParams::AreRivalsInSameGame (X360 @ 0x82590FC0).
        u64 GetRivalXUID() const { return mUniqueID.mqXuid; }

        // The rival's name is the PlayerName base of mUniqueID (relationship +88).
        const CgsNetwork::PlayerName* GetRivalName() const { return &mUniqueID; }

        // Header inlines: the console reads the +112 score word directly at every call site.
        s32  GetCurrentScoreForLocalPlayer() const { return miCurrentScoreForPlayersPointOfView; }
        s32  GetCurrentScoreForRival() const       { return -miCurrentScoreForPlayersPointOfView; }
        ERelationshipStatus GetRelationshipStatus() const;  // own TU
        s32  GetTotalEvents() const { return static_cast<s32>(miTotalEvents); }
        const CommonRelationship* GetLastGameStats() const; // own TU
        const CommonRelationship* GetOverallStats() const { return &mOverallStats; }
        void SetLastTimeChanged(CgsSystem::DateAndTime lDateAndTime) { mLastTimeChanged = lDateAndTime; }
        void SetCurrentScoreForPlayer(s32 liScore)  { miCurrentScoreForPlayersPointOfView = liScore; }
        void SetTotalNumberOfEvents(s32 liNumberOfEvents) { miTotalEvents = static_cast<u32>(liNumberOfEvents); }
        void SetOverallStats(CommonRelationship* lpOverallStats) { mOverallStats = *lpOverallStats; }
        void Merge(LiveRevengeRelationship* lpRemoteRelationship); // own TU
        bool IsRivalAheadInCurrentRelationship() const  { return miCurrentScoreForPlayersPointOfView < 0; }
        bool IsPlayerAheadInCurrentRelationship() const { return miCurrentScoreForPlayersPointOfView > 0; }
        bool IsCurrentRelationshipEqual() const         { return miCurrentScoreForPlayersPointOfView == 0; }
        ERelationshipType GetRelationshipType() const;      // own TU
        CgsSystem::DateAndTime GetLastChangedTime() const { return mLastTimeChanged; }
        void OnRoundFinish();                               // own TU
        void OnRoundStart();                                // own TU
        bool Validate() const;                              // own TU
        // DEBUG callbacks are static per leak + DWARF (cast the void* into a local
        // LiveRevengeRelationship* rather than using `this`).
        static void DEBUGResetTimeStamp(void* lpParameter);    // own TU
        static void DEBUGSetTimeStampOld(void* lpParameter);   // own TU
        static void DEBUGClearRelationship(void* lpParameter); // own TU

    private:
        void ScoreSettled();                                // own TU
        void FlipCommonRelationship(CommonRelationship* lpRelationship); // own TU
        void ValidateStat(s32 liLocalPlayerStat, s32 liRemotePlayerStat, s32 liLocalRivalStat,
                          s32 liRemoteRivalStat, const char* lpcName, bool lbShouldWeAssert); // own TU

    private:
        // Reference member order; console offsets:
        CommonRelationship     mOverallStats;                        // +0   (72 bytes)
        CgsSystem::DateAndTime mLastTimeChanged;                     // +72  (12 bytes)
        UniquePlayerID         mUniqueID;                            // +88  (24 bytes, 8-aligned)
        s32                    miCurrentScoreForPlayersPointOfView;  // +112
        u32                    miTotalEvents;                        // +116
    };
} // namespace BrnNetwork
// static_assert(sizeof(BrnNetwork::LiveRevengeRelationship) == 120, "X360 layout");
