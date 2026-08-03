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
        //
        // STATIC -- MEASURED, and it used to be declared non-static here. The whole X360 body
        // (@0x82676640, dumped with headless IDA into scratchpad/waveJ/comparescores.txt) is
        //     mr r11,r3 / slwi r11,r11,2 / lwzx r11, r11, off_820A765C
        //     mr r3,r4  / mr r4,r5       / mtctr r11 / bctr
        // i.e. r3 is used as the comparator-table INDEX (the ScoreType), not as a `this`
        // pointer, and the two scores arrive in r4/r5 -- three arguments, no object. Every
        // call site agrees: CrashNavMap::UpdateRoadRule @0x824B6EA8 (`mr r3, r28` where r28 is
        // the loop's ScoreType) and StreetManager::ProcessNewRoadScore @0x823498F4 (`mr r3,
        // r27`, the same register it feeds GetScore's ScoreType argument). Declaring it static
        // is source-compatible with the committed callers that already spell it through an
        // object (`lEntry.CompareScores(...)` stays legal), and it lets the call sites that
        // have no ChallengeData in hand -- like UpdateRoadRule -- spell it as the console does.
        static int32_t CompareScores( ScoreType leScoreType, int32_t liScore0, int32_t liScore1 );
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

    // BrnChallengeData.h:244 (DWARF). The compiled par-score record for one challenge road: a
    // ChallengeData plus the pair of per-score-type rival ids. Promoted out of the former
    // file-local definition in BrnChallengeData.cpp (wave-C StreetManager keystone) so the
    // StreetManager callers -- GetChallengeParScore @0x82336168, ProcessScoreRequestEvent
    // @0x8234A240, ProcessNewRoadScore @0x823496C8, SendUpcomingRoadMessage @0x82348798 --
    // can spell Copy/GetScore by name. SharedClasses/StreetData/BrnStreetData.h (whose
    // GetChallengeParScore rows stride over this type) now includes this definition instead
    // of its former byte-identical POD model. Layout: 24-byte ChallengeData base + 16 = 40.
    // The DWARF also declares Construct(Time, CgsID, Time, CgsID, int32_t, CgsID) at :255
    // (the StreetData-compiler filler); its BrnStreetData::Time param type has no committed
    // home yet, so that one declaration is deferred with its TU. Method bodies live in
    // BrnChallengeData.cpp.
    struct ChallengeParScoresEntry : public ChallengeData
    {
        ::CgsID mRivals[2];   // +24

        // BrnChallengeData.h:263 (DWARF). X360 0x8231C7E0. Reads the base score and the
        // per-score rival id; returns void (the X360 r3 return is an ABI artifact).
        void GetScore( ScoreType leScoreType, int32_t* lpiScore, ::CgsID* lpRivalId ) const;

        // BrnChallengeData.h:266 (DWARF). X360 0x82676700.
        void Copy( const ChallengeParScoresEntry* lpSource );
    };

    // BrnChallengeData.h:56 (DWARF). Post-increment ScoreType iterator: advances the referenced
    // enum, asserts it has not exceeded E_SCORE_TYPE_COUNT, returns the prior value by value.
    // X360 0x8230EBB8. Body in BrnChallengeData.cpp.
    ScoreType operator++( ScoreType& leEnumIndex, int );
}
