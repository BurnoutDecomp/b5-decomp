#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/StreetData/BrnChallengeData.h"   // BrnStreetData::ScoreType / ScoreList / ChallengeData (shared home)
#include "BrnCommonTypes.h"                               // ::CgsID (u64)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnStreetData::ChallengeData::Construct             @ 0x8267D7C0
//   BrnStreetData::ChallengeData::Copy                  @ 0x82676610
//   BrnStreetData::ChallengeParScoresEntry::Copy        @ 0x82676700
//   BrnStreetData::ChallengeParScoresEntry::GetScore    @ 0x8231C7E0
//   BrnStreetData::ChallengePlayerScoreEntry::Construct @ 0x8267D7E8
//   BrnStreetData::ChallengePlayerScoreEntry::Copy      @ 0x82676668
//   BrnStreetData::ChallengeData::GetScore              @ 0x8231B760
//   BrnStreetData::ChallengeData::CompareScores         @ 0x82676640
//
// Score-record initialisation, copy, and the par-scores accessor. The score-record types
// (ScoreType / ScoreList / ChallengeData) now come from the single shared home
// SharedClasses/StreetData/BrnChallengeData.h (no more file-local fork). The derived
// ChallengePlayerScoreEntry / ChallengeParScoresEntry are local to this TU.

namespace BrnStreetData
{
    // The X360 initialises a fresh record's per-entry ids to the all-ones "no owner" sentinel.
    static const ::CgsID KU_INVALID_ID = 0xFFFFFFFF00000000ULL;

    // Per-score-type [min,max] range tables backing the SetScore guard. PINNED (wave-C
    // StreetManager keystone) from the X360 .rdata bytes: dword_820A764C = {0, 0} and
    // dword_820A7654 = {600000, 1000000000} (BE dump 2026-07-16; see
    // scratchpad/waveB/streetmgr_wc_rodata_dump.txt). TIME max = 600000 (a 10-minute
    // millisecond cap), CRASH max = 1000000000.
    const int32_t ScoreList::KAI_MIN_SCORES[ E_SCORE_TYPE_COUNT ] = { 0, 0 };
    const int32_t ScoreList::KAI_MAX_SCORES[ E_SCORE_TYPE_COUNT ] = { 600000, 1000000000 };

    // The X360 initialises a fresh score list to the all-ones "no score" sentinel (-1 per entry);
    // ChallengeData::Construct calls this after clearing the dirty/valid bit arrays.
    void ScoreList::Construct()
    {
        maScores[ E_SCORE_TYPE_TIME ]  = -1;
        maScores[ E_SCORE_TYPE_CRASH ] = -1;
    }

    // X360 SetScore @ 0x8230EC78: maScores[leScoreType] = liScore, guarded by the bounds assert.
    void ScoreList::SetScore( ScoreType leScoreType, int32_t liScore )
    {
        CGS_ASSERT( leScoreType >= 0 && leScoreType < E_SCORE_TYPE_COUNT,
                    "leScoreType >=0 && leScoreType < E_SCORE_TYPE_COUNT" );
        maScores[ leScoreType ] = liScore;
    }

    // The X360 ChallengeData::Construct (DWARF BrnChallengeData.cpp:65) clears both bit arrays
    // then Constructs the score list. UnSetAll zeroes the words (the prior raw
    // 0xFFFFFFFF00000000 write was a reconstruction artifact -- bits 0/1 are 0 either way, so
    // the behaviour is identical and now matches the binary's call sequence).
    void ChallengeData::Construct()
    {
        mDirty.UnSetAll();
        mValidScores.UnSetAll();
        mScoreList.Construct();
    }

    void ChallengeData::Copy(const ChallengeData* lpSource)
    {
        mDirty = lpSource->mDirty;
        mValidScores = lpSource->mValidScores;
        mScoreList = lpSource->mScoreList;
    }

    // X360 BrnStreetData::ChallengeData::SetClean @ 0x8231B6D0. Clears the dirty flag(s)
    // on mDirty (BitArray<2u> @+0). E_SCORE_TYPE_COUNT clears every dirty bit (the X360
    // single 64-bit `std 0` over mDirty's one field == UnSetAll); a specific ScoreType
    // clears just that bit (the `andc` clear-bit == UnSetBit). The X360 emits the
    // CgsBitArray.h:241 bounds assert; message "luIndex < NUMBITS" is verbatim rodata
    // (aLuindexNumbits, no trailing newline); baked file/line dropped per project convention.
    void ChallengeData::SetClean( ScoreType leScoreType )
    {
        if ( leScoreType == E_SCORE_TYPE_COUNT )
        {
            mDirty.UnSetAll();
            return;
        }

        CGS_ASSERT( leScoreType < E_SCORE_TYPE_COUNT, "luIndex < NUMBITS" );
        mDirty.UnSetBit( static_cast<u32>( leScoreType ) );
    }

    // X360 BrnStreetData::ChallengePlayerScoreEntry::SetCarID @ 0x8230EC18.
    // Stores the owning car id for one score type: maCarIDs[leScoreType] = lCarID,
    // guarded by the score-type bounds assert. The X360 store is a full 64-bit stdx
    // (r29 = the CgsID) at this + 24 + leScoreType*8 == maCarIDs[leScoreType] (base @+24,
    // 8-byte CgsID stride). Assert message rodata-VERBATIM; the baked file/line is dropped
    // per project convention (no trailing newline). The PPC r3 return is an ABI artifact of
    // the inlined assert path and is dropped (source return is void).
    void ChallengePlayerScoreEntry::SetCarID( ScoreType leScoreType, ::CgsID lCarID )
    {
        CGS_ASSERT( leScoreType >= 0 && leScoreType < E_SCORE_TYPE_COUNT,
                    "leScoreType >=0 && leScoreType < E_SCORE_TYPE_COUNT" );
        maCarIDs[ leScoreType ] = lCarID;
    }

    // ChallengePlayerScoreEntry is now defined in the shared BrnChallengeData.h home (above);
    // only its method bodies live here.
    void ChallengePlayerScoreEntry::Construct()
    {
        ChallengeData::Construct();
        maCarIDs[0] = KU_INVALID_ID;
        maCarIDs[1] = KU_INVALID_ID;
    }

    void ChallengePlayerScoreEntry::Copy(const ChallengePlayerScoreEntry* lpSource)
    {
        CGS_ASSERT(lpSource != nullptr, "lpPlayerScoreEntry");
        ChallengeData::Copy(lpSource);
        maCarIDs[0] = lpSource->maCarIDs[0];
        maCarIDs[1] = lpSource->maCarIDs[1];
    }

    // ChallengeParScoresEntry is now defined in the shared BrnChallengeData.h home (wave-C
    // StreetManager keystone promotion); only its method bodies live here.
    void ChallengeParScoresEntry::Copy(const ChallengeParScoresEntry* lpSource)
    {
        CGS_ASSERT(lpSource != nullptr, "lpData");
        ChallengeData::Copy(lpSource);
        mRivals[0] = lpSource->mRivals[0];
        mRivals[1] = lpSource->mRivals[1];
    }

    // X360 BrnStreetData::ChallengeParScoresEntry::GetScore @ 0x8231C7E0.
    // Returns the base score for leScoreType plus the rival id stored in mRivals.
    // The X360 build baked the assert file/line as
    // ..\GameSource\GameState\StreetData\BrnChallengeHighScoreEntry.h:204/205 (this
    // accessor was emitted from that header alongside ChallengeHighScoreEntry); the
    // baked path/line are discarded per project convention. `*a4 = *(8*(a2+3)+a1)`
    // resolves to mRivals[leScoreType] (this + 24 + 8*idx). The PPC r3 `return result`
    // is an ABI artifact of the inlined base call and is dropped (source return is void).
    void ChallengeParScoresEntry::GetScore( ScoreType leScoreType,
                                            int32_t*  lpiScore,
                                            ::CgsID*  lpRivalId ) const
    {
        CGS_ASSERT( lpiScore,  "lpiScore != NULL" );
        CGS_ASSERT( lpRivalId, "lpRivalId != NULL" );

        *lpiScore  = ChallengeData::GetScore( leScoreType );
        *lpRivalId = mRivals[ leScoreType ];
    }

    // X360 BrnStreetData::operator++ @ 0x8230EBB8 -- post-increment on the ScoreType enum
    // iterator (DWARF BrnChallengeData.h:56, `extern BrnStreetData::ScoreType
    // operator++(BrnStreetData::ScoreType&, int)`). Reads the old value (lwz r31,0(r3)),
    // advances the referenced enum by one (addi r11,r31,1 / stw r11,0(r3)), asserts the NEW
    // index has not run past E_SCORE_TYPE_COUNT (cmpwi r11,2 / ble skips unless v2>2 -- the
    // loop's one-past-the-end terminator == COUNT is legal, > COUNT is the error), and returns
    // the OLD value (mr r3,r31). Assert message VERBATIM from rodata aLeenumindexESc; the baked
    // file/line (BrnChallengeData.h:56) is dropped per project convention; no trailing newline.
    ScoreType operator++( ScoreType& leEnumIndex, int )
    {
        ScoreType leOld = leEnumIndex;
        leEnumIndex = static_cast<ScoreType>( leEnumIndex + 1 );
        CGS_ASSERT( leEnumIndex <= E_SCORE_TYPE_COUNT, "leEnumIndex <= E_SCORE_TYPE_COUNT" );
        return leOld;
    }

    // ========================================================================
    // ACCESSOR CLOSURE (2026-08-26). The two ChallengeData score accessors the whole
    // road-rule / challenge surface calls and that had no body anywhere in the tree --
    // both are named in the measured link residue of the scoring mount
    // (scratch/stuntrace_scout/datafeed/objs/undef_demangled.txt). Both are real
    // out-of-line X360 functions, so these are transcriptions.
    // ========================================================================

    // ------------------------------------------------------------------------
    // The per-score-type comparator table (DWARF BrnChallengeData.h:191
    // `mapComparisonFunctions`, X360 off_820A765C). Kept TU-LOCAL rather than grown onto
    // the shared home as a static data member: only CompareScores indexes it, and the home
    // header already states the table's definition + values belong to this TU. Its two
    // entries are the X360 pointers 0x82674CF8 (TIME) and 0x82674D30 (CRASH), read straight
    // out of the decrypted ARTIST basefile at 0x820A765C / 0x820A7660 -- the same rodata run
    // whose next-door neighbours 0x820A764C / 0x820A7654 are the already-pinned
    // KAI_MIN_SCORES / KAI_MAX_SCORES tables above, which corroborates the base address.
    //
    // BOTH comparators return the SAME ranking convention: negative when liScore0 is the
    // BETTER score, positive when liScore1 is, 0 on a tie. That is why the callers spell
    // their tests as `CompareScores(...) < 0` == "the first score wins".
    // ------------------------------------------------------------------------

    namespace
    {
        // X360 0x82674CF8 -- E_SCORE_TYPE_TIME. LOWER is better, and a stored 0 is treated as
        // the best possible time (it short-circuits before the ordinary compare):
        //     cmpwi r3, 0 ; bne -> ; li r3, -1 ; blr        ; score0 == 0  -> score0 wins
        //     cmpwi r4, 0 ; bne -> ; li r3,  1 ; blr        ; score1 == 0  -> score1 wins
        //     cmpw  r3, r4 ; blt -> (li r3,-1) ; li r3, 1 ; bgtlr ; li r3, 0 ; blr
        int32_t CompareTimeScores( int32_t liScore0, int32_t liScore1 )
        {
            if ( liScore0 == 0 ) { return -1; }
            if ( liScore1 == 0 ) { return  1; }
            if ( liScore0 <  liScore1 ) { return -1; }
            if ( liScore0 >  liScore1 ) { return  1; }
            return 0;
        }

        // X360 0x82674D30 -- E_SCORE_TYPE_CRASH. HIGHER is better; no zero special case:
        //     cmpw  r3, r4 ; ble -> ; li r3, -1 ; blr       ; score0 > score1 -> score0 wins
        //     li r3, 1 ; bltlr                              ; score0 < score1 -> score1 wins
        //     li r3, 0 ; blr
        int32_t CompareCrashScores( int32_t liScore0, int32_t liScore1 )
        {
            if ( liScore0 > liScore1 ) { return -1; }
            if ( liScore0 < liScore1 ) { return  1; }
            return 0;
        }

        // off_820A765C -- indexed by ScoreType, in enumerator order.
        ChallengeData::ComparisonFunction
        gapComparisonFunctions[ E_SCORE_TYPE_COUNT ] = { CompareTimeScores,    // 0x82674CF8
                                                         CompareCrashScores }; // 0x82674D30
    }

    // ------------------------------------------------------------------------
    // ChallengeData::CompareScores  (X360 0x82676640)
    // ------------------------------------------------------------------------
    // A pure dispatch into the per-score-type comparator table -- no `this`, no asserts:
    //     mr   r11, r3                     ; r3 IS the ScoreType, not a `this` pointer
    //     lis/addi r10, off_820A765C       ; the table base
    //     slwi r11, r11, 2                 ; * sizeof(ComparisonFunction)
    //     mr   r3, r4 ; mr r4, r5          ; shuffle the two scores down into arg 0 / arg 1
    //     lwzx r11, r11, r10 ; mtctr r11 ; bctr    ; TAIL call -- the comparator's return
    //                                             ; value is CompareScores' return value
    // The register shuffle is what proves the method is STATIC (already recorded in the home
    // header): a non-static member would have kept `this` in r3 and read the scores from
    // r4/r5 without moving them.
    //
    // NOTE the index is NOT bounds-checked on the console -- an out-of-range ScoreType reads
    // past the two-entry table and jumps to whatever follows. Reproduced without a guard.
    int32_t ChallengeData::CompareScores( ScoreType leScoreType, int32_t liScore0, int32_t liScore1 )
    {
        return gapComparisonFunctions[ leScoreType ]( liScore0, liScore1 );
    }

    // ------------------------------------------------------------------------
    // ChallengeData::GetScore  (X360 0x8231B760)
    // ------------------------------------------------------------------------
    // Read one score type out of the embedded ScoreList. Two guards, in the console's own
    // order, then the indexed read:
    //     * mValidScores.IsBitSet( leScoreType ) -- asserted, NOT gating; the read happens
    //       either way. (The inlined IsBitSet brings its own CgsBitArray.h:203 index assert
    //       with it, which is where the "invalid index : N < 2" message in the asm comes
    //       from -- it belongs to BitArray, not to this method, so it is not restated here:
    //       the named IsBitSet call below re-emits it.)
    //     * leScoreType in [0, E_SCORE_TYPE_COUNT) -- the method's own assert
    //       (baked file/line BrnChallengeData.h:592; message VERBATIM from rodata).
    //     result = *(4 * (leScoreType + 4) + this) == this + 16 + 4 * leScoreType, and +16 is
    //     mScoreList (mDirty @+0, mValidScores @+8 -- the committed layout), so the read is
    //     mScoreList.maScores[ leScoreType ].
    // ------------------------------------------------------------------------
    int32_t ChallengeData::GetScore( ScoreType leScoreType ) const
    {
        CGS_ASSERT( mValidScores.IsBitSet( static_cast<u32>( leScoreType ) ),
                    "mValidScores.IsBitSet( leScoreType )" );
        CGS_ASSERT( leScoreType >= 0 && leScoreType < E_SCORE_TYPE_COUNT,
                    "leScoreType >=0 && leScoreType < E_SCORE_TYPE_COUNT" );

        return mScoreList.maScores[ leScoreType ];   // this + 16 + 4 * leScoreType
    }
}
