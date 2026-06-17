#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<2u> (canonical generic)

// =============================================================================
// BrnChallengeData.h - the SINGLE shared home for the StreetData challenge score-record
// types: BrnStreetData::ScoreType, ScoreList, and ChallengeData (built on the canonical
// CgsContainers::BitArray<2u>). Promoted out of the former file-local definitions in
// SharedClasses/StreetData/BrnChallengeData.cpp and GameSource/GameState/StreetData/
// BrnChallengeHighScoreEntry.h (which both now #include this header) so there is exactly
// one program-wide definition of each (kills the soft ODR fork).
//
// Layout (DWARF SharedClasses/StreetData/BrnChallengeData.h; version-5 + X360 binary
// authoritative): ChallengeData == 24 bytes (mDirty @+0, mValidScores @+8, mScoreList @+16).
// ScoreType has 2 entries (TIME=0, CRASH=1, COUNT=2) -- X360-authoritative over the
// Feb-2007 leak's 5-entry enum.
// =============================================================================

namespace CgsContainers
{
    // FastBitArray<tuNumBits>: used by-pointer only (ChallengeHighScoreEntry::UpdateEntry /
    // AreAnyScoresEqual take FastBitArray<2>*). Forward-declared here; its real home is the
    // CgsContainers TU. No definition needed for the pointer-only params.
    template <u32 tuNumBits> class FastBitArray;
}

namespace BrnStreetData
{
    // BrnChallengeData.h:47 (DWARF) -- the score-type index. SetScore/GetScore bounds-check
    // against E_SCORE_TYPE_COUNT.
    enum ScoreType
    {
        E_SCORE_TYPE_START = 0,
        E_SCORE_TYPE_TIME  = 0,
        E_SCORE_TYPE_CRASH = 1,
        E_SCORE_TYPE_COUNT = 2,
    };

    // BrnChallengeData.h:68 (DWARF). maScores int32_t[2] @ :92. Complete type so ChallengeData
    // can embed it by value.
    struct ScoreList
    {
        int32_t maScores[E_SCORE_TYPE_COUNT];   // +0

        // Per-score-type [min,max] range tables backing the ChallengeHighScoreEntry::SetScore
        // guard (DWARF BrnChallengeData.h:70/71). Defined in BrnChallengeData.cpp; the literal
        // values are FLAGGED placeholders pending a .data dump of 0x820A764C / 0x820A7654.
        static const int32_t KAI_MIN_SCORES[E_SCORE_TYPE_COUNT];
        static const int32_t KAI_MAX_SCORES[E_SCORE_TYPE_COUNT];

        void Construct();
        void SetScore( ScoreType leScoreType, int32_t liScore );

        // Endian fix-up shims (declared-only; bodies land with a caller TU -- rw::EndianSwap
        // lives in prebuilt rwcore.lib so no body is emitted here).
        void FixUp();
        void FixDown();
    };

    // BrnChallengeData.h:53 (DWARF). Two 2-bit dirty/valid bit arrays + a two-entry ScoreList.
    struct ChallengeData
    {
        CgsContainers::BitArray<2u> mDirty;        // +0
        CgsContainers::BitArray<2u> mValidScores;  // +8
        ScoreList                   mScoreList;    // +16

        void    Construct();
        // BrnChallengeData.h:159 (DWARF). Defined in its own TU (different X360 address);
        // declared-only here so derived GetScore/SetScore resolve under cl /c.
        int32_t GetScore( ScoreType leScoreType ) const;
        void    SetScore( ScoreType leScoreType, int32_t liScore );
        void    Copy( const ChallengeData* lpSource );
    };
}
