#pragma once

#include "types.hpp"
#include "SharedClasses/StreetData/BrnChallengeData.h"   // BrnStreetData::ScoreType / ScoreList / ChallengeData (shared home)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameSource/GameState/BrnCgsPlayerName.h"        // CgsNetwork::PlayerName (shared 16-byte home)

// ChallengeHighScoreEntry : ChallengeData. ChallengeData/ScoreType/ScoreList now come
// from the shared home (BrnChallengeData.h); they are no longer redefined here.
// X360: GetScore 0x8231C450, SetScore 0x8231C4F0, ClearScore 0x8231C710 (all inline).

namespace BrnStreetData
{
    class ChallengeHighScoreEntry : public ChallengeData
    {
    public:
        void Construct();
        void Construct( ChallengeData* lpData );
        bool UpdateEntry( const ChallengeHighScoreEntry* lpEntry,
                          CgsContainers::FastBitArray<2>* lpUpdateScoresBitArray );
        bool AreAnyScoresEqual( const ChallengeHighScoreEntry* lpEntry,
                                CgsContainers::FastBitArray<2>* lpEqualScoresBitArray );

        inline void GetScore( ScoreType leScoreType,
                              int32_t* lpiScore,
                              CgsNetwork::PlayerName* lpPlayerName ) const;
        inline void SetScore( ScoreType leScoreType,
                              int32_t liScore,
                              const CgsNetwork::PlayerName* lpPlayerName );
        inline void ClearScore( ScoreType leScoreType );

        void Copy( const ChallengeHighScoreEntry* lpData );
        bool IsWholeChallengeOwnedBySamePlayer();

    private:
        CgsNetwork::PlayerName maPlayerNames[ E_SCORE_TYPE_COUNT ]; // +24
    };

    inline void
    ChallengeHighScoreEntry::GetScore(
        ScoreType               leScoreType,
        int32_t*                lpiScore,
        CgsNetwork::PlayerName*  lpPlayerName ) const
    {
        CGS_ASSERT( lpiScore, "lpiScore != NULL" );
        CGS_ASSERT( lpPlayerName, "lpPlayerName != NULL" );
        *lpiScore = ChallengeData::GetScore( leScoreType );
        *lpPlayerName = maPlayerNames[ leScoreType ];
    }

    // X360 0x8231C4F0. Feb-2007 leak (BrnChallengeHighScoreEntry.h) is the body
    // ground truth: SetBit dirty + valid, copy the player name, then chain to the
    // base ChallengeData::SetScore. BINARY-vs-LEAK DELTA: the X360 build wraps the
    // whole write in a per-score-type [min,max] range guard
    // (a3 >= dword_820A764C[a2] && a3 <= dword_820A7654[a2] in the pseudocode ==
    // ScoreList::KAI_MIN_SCORES[leScoreType] / KAI_MAX_SCORES[leScoreType]); an
    // out-of-range score is silently rejected (no bit set, no name stored, no base
    // call). The leak omits the guard, so this is a behavioural difference, not a
    // debug-only assert -- it is reconstructed here per the X360 binary (authoritative).
    // The X360 also inlines the CgsContainers::BitArray<2u>::SetBit luIndex<NUMBITS
    // StrStream asserts (CgsBitArray.h:222); those bounds checks now live inside
    // BitArray<2u>::SetBit in the canonical CgsBitArray.h, so they are not re-emitted
    // here. The leading leScoreType<E_SCORE_TYPE_COUNT assert is the X360 line-155 guard.
    inline void
    ChallengeHighScoreEntry::SetScore(
        ScoreType                     leScoreType,
        int32_t                       liScore,
        const CgsNetwork::PlayerName* lpPlayerName )
    {
        CGS_ASSERT( leScoreType >= 0 && leScoreType < E_SCORE_TYPE_COUNT,
                    "leScoreType >=0 && leScoreType < E_SCORE_TYPE_COUNT" );

        if ( liScore >= ScoreList::KAI_MIN_SCORES[ leScoreType ] &&
             liScore <= ScoreList::KAI_MAX_SCORES[ leScoreType ] )
        {
            mDirty.SetBit( leScoreType );
            mValidScores.SetBit( leScoreType );
            maPlayerNames[ leScoreType ] = *lpPlayerName;
            ChallengeData::SetScore( leScoreType, liScore );
        }
    }

    // X360 0x8231C710 (no leak body; reconstructed op-for-op from the pseudocode).
    // Blank the player name for this score type, zero the stored score (via the base
    // setter, which also sets the dirty+valid bits), then clear the dirty+valid bits.
    // The X360 inlines CgsContainers::BitArray<2u>::UnSetBit twice (mDirty @+0 then
    // mValidScores @+8); its luIndex<NUMBITS bounds assert (CgsBitArray.h:241) now
    // lives inside BitArray<2u>::UnSetBit. The pseudocode's `*v4 = 0` after
    // Construct("") is the already-empty macName[0]=0 that Construct("") leaves --
    // not a separate store -- so it is folded into the Construct call.
    inline void
    ChallengeHighScoreEntry::ClearScore( ScoreType leScoreType )
    {
        maPlayerNames[ leScoreType ].Construct( "" );
        ChallengeData::SetScore( leScoreType, 0 );
        mDirty.UnSetBit( leScoreType );
        mValidScores.UnSetBit( leScoreType );
    }
}
