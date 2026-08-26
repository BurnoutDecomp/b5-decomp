#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"
#include "lobbyname.h"   // ::LobbyNameCmp (vendor/dirtysdk/include, an extern "C" declaration)

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
// CGS_ASSERT arrives through the header (BrnChallengeData.h -> CgsAssert.h).
//
// [stuntrace waveB MOUNT-CLOSURE round, 2026-08-26] LOBBYNAMECMP DECLARATION CORRECTED -- this
// was a latent LNK2019 waiting for whoever mounted this TU. What stood here was a local
// declaration INSIDE `namespace BrnStreetData` and WITHOUT extern "C", i.e. the symbol
// `int __cdecl BrnStreetData::LobbyNameCmp(char const*, char const*)`, which is not the
// function anything defines: it is a different name from the one the DirtySDK body exports.
// Mounting this TU against that declaration would have traded one unresolved external for
// another. The comment it carried ("real home ... is not available yet, so it is forward-declared
// here per the 'no reconstructable reference' exception") was simply out of date -- the canonical
// home landed 2026-08-26 as vendor/dirtysdk/include/lobbyname.h with extern "C" linkage, its body
// is vendor/dirtysdk/src/lobbyname.cpp (@0x82B10050), and that .cpp is already mounted
// (tools/build/build_game_exe.bat:113). The header is included above and the call below is
// explicitly ::-qualified so no future namespace-scope declaration can shadow it again.

namespace BrnStreetData
{

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
           ::LobbyNameCmp(maPlayerNames[liScoreType].macName,
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
