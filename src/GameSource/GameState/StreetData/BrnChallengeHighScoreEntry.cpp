#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   Construct @0x82317680 -- base ChallengeData::Construct, then blank each score-type owner
//                            name (PlayerName::Construct("")).
//   Copy      @0x823176D0 -- assert the source, base ChallengeData::Copy, then copy both
//                            score-type owner names (the X360 does two 16-byte memcpys).
//   IsWholeChallengeOwnedBySamePlayer @0x82317758 -- true iff the first owner name is non-empty
//                            and every score-type owner name matches; with E_SCORE_TYPE_COUNT==2
//                            this is a single name[0]==name[1] compare via LobbyNameCmp.
//
// CGS_ASSERT arrives through the header (BrnChallengeData.h -> CgsAssert.h). LobbyNameCmp's
// real home (the un-reconstructed CgsNetwork lobby header) is not available yet, so it is
// forward-declared here per the "no reconstructable reference" exception -- the per-TU cl /c
// gate needs only the declaration.

namespace BrnStreetData
{

// Returns 0 when the two 16-byte player-name strings name the same player (X360 LobbyNameCmp).
int LobbyNameCmp(const char* lpcNameA, const char* lpcNameB);

// @ 0x82317680
void ChallengeHighScoreEntry::Construct()
{
    ChallengeData::Construct();

    for (s32 liScoreType = 0; liScoreType < E_SCORE_TYPE_COUNT; ++liScoreType)
    {
        maPlayerNames[liScoreType].Construct("");
    }
}

// @ 0x823176D0
void ChallengeHighScoreEntry::Copy(const ChallengeHighScoreEntry* lpData)
{
    CGS_ASSERT(lpData != NULL, "lpData");

    ChallengeData::Copy(lpData);

    for (s32 liScoreType = 0; liScoreType < E_SCORE_TYPE_COUNT; ++liScoreType)
    {
        maPlayerNames[liScoreType] = lpData->maPlayerNames[liScoreType];
    }
}

// @ 0x82317758
bool ChallengeHighScoreEntry::IsWholeChallengeOwnedBySamePlayer()
{
    s32 liScoreType = 0;

    // Walk the score-type owner names while the current one is non-empty and matches the next;
    // the X360 returns true as soon as the first pair matches (E_SCORE_TYPE_COUNT == 2).
    while (maPlayerNames[liScoreType].macName[0] != '\0' &&
           LobbyNameCmp(maPlayerNames[liScoreType].macName,
                        maPlayerNames[liScoreType + 1].macName) == 0)
    {
        ++liScoreType;
        CGS_ASSERT(liScoreType <= E_SCORE_TYPE_COUNT, "leEnumIndex <= E_SCORE_TYPE_COUNT");

        if (liScoreType >= 1)
        {
            return true;
        }
    }

    return false;
}

} // namespace BrnStreetData
