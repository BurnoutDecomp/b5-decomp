#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ---------------------------------------------------------------------------
// Minimal owning-type slice for BrnStreetData::ChallengeHighScoreEntry::GetScore
// (X360 0x8231C450). Only the members + accessors this one inline accessor
// touches are declared here; everything else in the real header
// (Construct/UpdateEntry/SetScore/Copy/AreAnyScoresEqual/...) is left for its
// own TU. Layout is pinned so maPlayerNames lands at the X360 +24 offset
// (memcpy(a4, 16*a2 + a1 + 24, 16) in the pseudocode).
// ---------------------------------------------------------------------------

namespace CgsNetwork
{
    // CgsNetwork::PlayerName. PS3 DecFIGS DWARF (CgsPlayerName.h) declares
    // `char macName[20]`, but the X360 build this function was lifted from
    // copies / strides 16 bytes (memcpy(...,16) with a 16*index stride), so the
    // X360 record is 16 bytes wide. Modelled file-local at the X360 width because
    // CgsNetwork::PlayerName has no committed home yet; replace with the real
    // header (#include "Network/Players/CgsPlayerName.h") once it is reconstructed.
    // Plain POD: the X360 emits the maPlayerNames[i] -> *lpPlayerName store as a
    // 16-byte memcpy, which the implicit copy-assignment reproduces.
    struct PlayerName
    {
        char macName[16];
    };
}

namespace BrnStreetData
{
    // BrnChallengeData.h:47 (DWARF). Sizes maPlayerNames[E_SCORE_TYPE_COUNT].
    enum ScoreType
    {
        E_SCORE_TYPE_START = 0,
        E_SCORE_TYPE_TIME  = 0,
        E_SCORE_TYPE_CRASH = 1,
        E_SCORE_TYPE_COUNT = 2,
    };

    // Minimal base mirroring the committed SharedClasses/StreetData/BrnChallengeData.cpp
    // layout (mDirty:8 + mValidScores:8 + mScoreList:8 = 24 bytes) purely so the
    // derived maPlayerNames starts at the X360 +24 offset. GetScore is the only
    // base member this accessor calls; it is declared-only (defined in its own TU).
    struct ChallengeData
    {
        u64 mDirty;                 // +0  CgsContainers::BitArray<2> (one u64 word)
        u64 mValidScores;           // +8  CgsContainers::BitArray<2> (one u64 word)
        s32 mScoreList_maScores[2]; // +16 ScoreList { s32 maScores[2] }

        int32_t GetScore( ScoreType leScoreType ) const;
    };

    // BrnChallengeHighScoreEntry.h:49 (DWARF). Only maPlayerNames + GetScore are
    // materialised; the remaining methods are declared-only.
    class ChallengeHighScoreEntry : public ChallengeData
    {
    public:
        void GetScore( ScoreType leScoreType,
                       int32_t* lpiScore,
                       CgsNetwork::PlayerName* lpPlayerName ) const;

    private:
        CgsNetwork::PlayerName maPlayerNames[ E_SCORE_TYPE_COUNT ]; // +24
    };

    inline void
    ChallengeHighScoreEntry::GetScore(
        ScoreType               leScoreType,
        int32_t*                lpiScore,
        CgsNetwork::PlayerName*  lpPlayerName ) const
    {
        if ( !lpiScore )
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpiScore != NULL",
                "..\\..\\..\\GameSource\\GameState/StreetData/BrnChallengeHighScoreEntry.h",
                133 );
            CgsDev::Assert::EndAssert();
        }
        if ( !lpPlayerName )
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpPlayerName != NULL",
                "..\\..\\..\\GameSource\\GameState/StreetData/BrnChallengeHighScoreEntry.h",
                134 );
            CgsDev::Assert::EndAssert();
        }

        *lpiScore = ChallengeData::GetScore( leScoreType );
        *lpPlayerName = maPlayerNames[ leScoreType ];
    }
}
