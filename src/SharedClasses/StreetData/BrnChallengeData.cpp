#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnStreetData::ChallengeData::Construct             @ 0x8267D7C0
//   BrnStreetData::ChallengeData::Copy                  @ 0x82676610
//   BrnStreetData::ChallengeParScoresEntry::Copy        @ 0x82676700
//   BrnStreetData::ChallengePlayerScoreEntry::Construct @ 0x8267D7E8
//   BrnStreetData::ChallengePlayerScoreEntry::Copy      @ 0x82676668
//
// Score-record initialisation and copy. A ChallengeData holds two dirty/valid bit
// pairs and a two-entry ScoreList (member layout recovered from the DecFIGS DWARF,
// BrnChallengeData.h). Construct resets every score slot to its "invalid" marker
// (score = -1, owner = 0); ParScores / PlayerScore add further CgsID + time slots.
// Copy duplicates the record word-for-word, matching the X360's 64-bit field moves.

namespace BrnStreetData
{
    // Two parallel scores plus their owning car IDs; the trailing slots hold the
    // invalid/owner markers seen in the constructors. Sizes mirror the X360 stores.
    struct ChallengeData
    {
        s32 maiScores[2];   // +0  : per-type score (init -1)
        s32 maiOwners[2];   // +8  : owner marker    (init 0)
        s32 miScoreA;       // +16 : invalid marker  (init -1)
        s32 miScoreB;       // +20 : invalid marker  (init -1)

        void Construct();
        void Copy(const ChallengeData* lpSource);
    };

    void ChallengeData::Construct()
    {
        maiScores[0] = -1;
        maiOwners[0] = 0;
        maiScores[1] = -1;
        maiOwners[1] = 0;
        miScoreA = -1;
        miScoreB = -1;
    }

    void ChallengeData::Copy(const ChallengeData* lpSource)
    {
        maiScores[0] = lpSource->maiScores[0];
        maiOwners[0] = lpSource->maiOwners[0];
        maiScores[1] = lpSource->maiScores[1];
        maiOwners[1] = lpSource->maiOwners[1];
        miScoreA = lpSource->miScoreA;
        miScoreB = lpSource->miScoreB;
    }

    struct ChallengePlayerScoreEntry : public ChallengeData
    {
        s32 maiExtra[4];    // +24 : two more invalid/owner marker pairs

        void Construct();
        void Copy(const ChallengePlayerScoreEntry* lpSource);
    };

    void ChallengePlayerScoreEntry::Construct()
    {
        ChallengeData::Construct();
        maiExtra[0] = -1;
        maiExtra[1] = 0;
        maiExtra[2] = -1;
        maiExtra[3] = 0;
    }

    void ChallengePlayerScoreEntry::Copy(const ChallengePlayerScoreEntry* lpSource)
    {
        CGS_ASSERT(lpSource != nullptr, "lpPlayerScoreEntry");
        ChallengeData::Copy(lpSource);
        maiExtra[0] = lpSource->maiExtra[0];
        maiExtra[1] = lpSource->maiExtra[1];
        maiExtra[2] = lpSource->maiExtra[2];
        maiExtra[3] = lpSource->maiExtra[3];
    }

    struct ChallengeParScoresEntry : public ChallengeData
    {
        s32 maiExtra[2];    // +24 : par-score time + owner

        void Copy(const ChallengeParScoresEntry* lpSource);
    };

    void ChallengeParScoresEntry::Copy(const ChallengeParScoresEntry* lpSource)
    {
        CGS_ASSERT(lpSource != nullptr, "lpData");
        ChallengeData::Copy(lpSource);
        maiExtra[0] = lpSource->maiExtra[0];
        maiExtra[1] = lpSource->maiExtra[1];
    }
}
