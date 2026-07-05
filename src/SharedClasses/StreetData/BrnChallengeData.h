#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // ::CgsID (u64) for ChallengePlayerScoreEntry::maCarIDs
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<2u> (canonical generic)
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT (ContainsData bounds guard)

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
        // BrnChallengeData.h:179 (DWARF). Per-score-type comparator dispatch. CompareScores
        // indexes this table by ScoreType. Declared static here to match the DWARF shape
        // (BrnChallengeData.h:191 `mapComparisonFunctions`); the table's definition + initial
        // values live with the CompareScores body in the BrnChallengeData.cpp TU (X360
        // table region near 0x820A765C) and are NOT emitted here.
        typedef int32_t (*ComparisonFunction)( int32_t liScore0, int32_t liScore1 );

        CgsContainers::BitArray<2u> mDirty;        // +0
        CgsContainers::BitArray<2u> mValidScores;  // +8
        ScoreList                   mScoreList;    // +16

        void    Construct();
        // BrnChallengeData.h:159 (DWARF). Defined in its own TU (different X360 address);
        // declared-only here so derived GetScore/SetScore resolve under cl /c.
        int32_t GetScore( ScoreType leScoreType ) const;
        void    SetScore( ScoreType leScoreType, int32_t liScore );
        void    Copy( const ChallengeData* lpSource );

        // BrnChallengeData.h:144 (DWARF) -- 0x82325F08. Reports whether mValidScores carries a
        // recorded score. Passing E_SCORE_TYPE_COUNT is the X360 "any score?" probe: it returns
        // true iff ANY valid-score bit is set. A specific ScoreType tests just that bit. The
        // bounds assert the X360 emits (CgsBitArray.h:203) is owned by the BitArray call below.
        // Header-inline: the X360 build folds this whole body into every caller
        // (e.g. ScoringSystem::GetHighestLobbyRoadRuleScore 0x8232B280).
        bool ContainsData( ScoreType leScoreType ) const
        {
            if ( leScoreType == E_SCORE_TYPE_COUNT )
            {
                return mValidScores.GetFirstNonZeroBit()
                       != CgsContainers::BitArray<2u>::KI_INVALID_BITINDEX;
            }

            CGS_ASSERT( leScoreType < E_SCORE_TYPE_COUNT, "leScoreType < E_SCORE_TYPE_COUNT" );
            return mValidScores.IsBitSet( static_cast<u32>( leScoreType ) );
        }

        // BrnChallengeData.h (DWARF). X360 0x82558410. The dirty-bit twin of ContainsData:
        // reports whether a score type is marked dirty (needs re-upload). E_SCORE_TYPE_COUNT
        // is the "any dirty?" probe (true iff ANY mDirty bit is set); a specific ScoreType
        // tests just that bit. Header-inline (X360 folds the whole body into its caller,
        // NetworkRoadRulesManager::GetRoadRulesDataToUpload). The CgsBitArray.h:203 de-inlined
        // bounds assert collapses to the same semantic condition ContainsData uses.
        bool IsDirty( ScoreType leScoreType ) const
        {
            if ( leScoreType == E_SCORE_TYPE_COUNT )
            {
                return mDirty.GetFirstNonZeroBit()
                       != CgsContainers::BitArray<2u>::KI_INVALID_BITINDEX;
            }

            CGS_ASSERT( leScoreType < E_SCORE_TYPE_COUNT, "leScoreType < E_SCORE_TYPE_COUNT" );
            return mDirty.IsBitSet( static_cast<u32>( leScoreType ) );
        }

        // BrnChallengeData.h (DWARF). X360 0x8231B6D0. Clears the dirty flag on mDirty:
        // E_SCORE_TYPE_COUNT clears every dirty bit (UnSetAll), a specific ScoreType clears
        // one (UnSetBit). Body in BrnChallengeData.cpp.
        void SetClean( ScoreType leScoreType );

        // BrnChallengeData.h:179 (DWARF). Three-way comparator: dispatches through
        // mapComparisonFunctions[leScoreType] to rank liScore0 vs liScore1 for that score type
        // (negative => liScore0 is the better score, the result GetHighestLobbyRoadRuleScore
        // tests with `< 0`). DECLARE-ONLY: its body + the comparator table live in the
        // BrnChallengeData.cpp TU (it appears only as [external/unknown] from every caller), so
        // it resolves under cl /c without forcing that TU's reconstruction here.
        int32_t CompareScores( ScoreType leScoreType, int32_t liScore0, int32_t liScore1 );
    };

    // BrnChallengeData.h:225 (DWARF). The local-player challenge record: a ChallengeData plus the
    // pair of car ids that scored it. Promoted out of the former file-local definition in
    // BrnChallengeData.cpp so callers outside that TU (e.g. the network road-rules debug component,
    // which builds one on the stack to inject a fake personal best) compile against the one shared
    // definition. Method bodies stay in BrnChallengeData.cpp.
    struct ChallengePlayerScoreEntry : public ChallengeData
    {
        ::CgsID maCarIDs[2];   // +24

        void Construct();
        void Copy( const ChallengePlayerScoreEntry* lpSource );

        // BrnChallengeData.h:225 (DWARF). X360 0x8230EC18. Stores the owning car id for one
        // score type into maCarIDs[leScoreType], bounds-checked against E_SCORE_TYPE_COUNT.
        // Full 64-bit CgsID store (stdx). Body in BrnChallengeData.cpp.
        void SetCarID( ScoreType leScoreType, ::CgsID lCarID );
    };

    // BrnChallengeData.h:56 (DWARF). Post-increment ScoreType iterator: advances the referenced
    // enum, asserts it has not exceeded E_SCORE_TYPE_COUNT, returns the prior value by value.
    // X360 0x8230EBB8. Body in BrnChallengeData.cpp.
    ScoreType operator++( ScoreType& leEnumIndex, int );
}
