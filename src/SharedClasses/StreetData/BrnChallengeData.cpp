#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnStreetData::ChallengeData::Construct             @ 0x8267D7C0
//   BrnStreetData::ChallengeData::Copy                  @ 0x82676610
//   BrnStreetData::ChallengeParScoresEntry::Copy        @ 0x82676700
//   BrnStreetData::ChallengePlayerScoreEntry::Construct @ 0x8267D7E8
//   BrnStreetData::ChallengePlayerScoreEntry::Copy      @ 0x82676668
//
// Score-record initialisation and copy. Member layout recovered from the DecFIGS
// DWARF (BrnChallengeData.h / CgsBitArray.h / CgsID.h): a ChallengeData holds two
// 2-bit dirty/valid bit arrays (each backed by one u64 word) and a two-entry
// ScoreList; the player/par entries add a two-entry CgsID array. Construct fills
// every slot with its "invalid" pattern; Copy is a plain member-wise duplicate
// (the X360 build emits it as 64-bit field moves).

namespace CgsContainers
{
    // CgsContainers::BitArray<N>: N bits packed into 64-bit words (one word for N<=64).
    template <u32 N>
    struct BitArray
    {
        u64 maxBits[(N + 63) / 64];
    };
}

namespace BrnStreetData
{
    typedef u64 CgsID;

    struct ScoreList
    {
        s32 maScores[2];
    };

    // The X360 initialises a fresh record's bit words to this pattern and its
    // scores / ids to the all-ones "no score / no owner" sentinel.
    static const u64 KU_EMPTY_BITWORD = 0xFFFFFFFF00000000ULL;
    static const u64 KU_INVALID_ID    = 0xFFFFFFFF00000000ULL;

    struct ChallengeData
    {
        CgsContainers::BitArray<2u> mDirty;        // +0
        CgsContainers::BitArray<2u> mValidScores;  // +8
        ScoreList                   mScoreList;     // +16

        void Construct();
        void Copy(const ChallengeData* lpSource);
    };

    void ChallengeData::Construct()
    {
        mDirty.maxBits[0] = KU_EMPTY_BITWORD;
        mValidScores.maxBits[0] = KU_EMPTY_BITWORD;
        mScoreList.maScores[0] = -1;
        mScoreList.maScores[1] = -1;
    }

    void ChallengeData::Copy(const ChallengeData* lpSource)
    {
        mDirty = lpSource->mDirty;
        mValidScores = lpSource->mValidScores;
        mScoreList = lpSource->mScoreList;
    }

    struct ChallengePlayerScoreEntry : public ChallengeData
    {
        CgsID maCarIDs[2];   // +24

        void Construct();
        void Copy(const ChallengePlayerScoreEntry* lpSource);
    };

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
        CgsID mRivals[2];    // +24

        void Copy(const ChallengeParScoresEntry* lpSource);
    };

    void ChallengeParScoresEntry::Copy(const ChallengeParScoresEntry* lpSource)
    {
        CGS_ASSERT(lpSource != nullptr, "lpData");
        ChallengeData::Copy(lpSource);
        mRivals[0] = lpSource->mRivals[0];
        mRivals[1] = lpSource->mRivals[1];
    }
}
