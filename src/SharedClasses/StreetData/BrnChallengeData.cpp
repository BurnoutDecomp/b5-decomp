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
//
// Score-record initialisation, copy, and the par-scores accessor. The score-record types
// (ScoreType / ScoreList / ChallengeData) now come from the single shared home
// SharedClasses/StreetData/BrnChallengeData.h (no more file-local fork). The derived
// ChallengePlayerScoreEntry / ChallengeParScoresEntry are local to this TU.

namespace BrnStreetData
{
    // The X360 initialises a fresh record's per-entry ids to the all-ones "no owner" sentinel.
    static const ::CgsID KU_INVALID_ID = 0xFFFFFFFF00000000ULL;

    // Per-score-type [min,max] range tables backing the SetScore guard. FLAGGED placeholders --
    // the real bytes live in X360 .data @ 0x820A764C (min) / 0x820A7654 (max). The {0,0} /
    // {INT32_MAX,INT32_MAX} pair accepts any non-negative score (the safe over-permissive
    // direction); pin the literals when a .data dump is available.
    const int32_t ScoreList::KAI_MIN_SCORES[ E_SCORE_TYPE_COUNT ] = { 0, 0 };
    const int32_t ScoreList::KAI_MAX_SCORES[ E_SCORE_TYPE_COUNT ] = { 0x7FFFFFFF, 0x7FFFFFFF };

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

    struct ChallengeParScoresEntry : public ChallengeData
    {
        ::CgsID mRivals[2];    // +24

        // BrnChallengeData.h:263 (DWARF). X360 0x8231C7E0. Reads the base score and
        // the per-score rival id; returns void (X360 r3 return is an ABI artifact).
        void GetScore( ScoreType leScoreType, int32_t* lpiScore, ::CgsID* lpRivalId ) const;
        void Copy(const ChallengeParScoresEntry* lpSource);
    };

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
}
