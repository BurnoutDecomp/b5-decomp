#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"  // CgsContainers::FastBitArray<2> (UpdateEntry's out-param)
#include "lobbyname.h"   // ::LobbyNameCmp (vendor/dirtysdk/include, an extern "C" declaration)

// Reconstructed from the console build.
//
//   Construct -- base ChallengeData::Construct, then blank each score-type owner
//                            name (PlayerName::Construct("")).
//   Copy -- assert the source, base ChallengeData::Copy, then copy both
//                            score-type owner names (the console does two 16-byte memcpys).
//   IsWholeChallengeOwnedBySamePlayer -- true iff the first owner name is non-empty
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
// is vendor/dirtysdk/src/lobbyname.cpp, and that .cpp is already mounted
// (tools/build/build_game_exe.bat:113). The header is included above and the call below is
// explicitly ::-qualified so no future namespace-scope declaration can shadow it again.

namespace BrnStreetData
{

namespace
{
    // The owner name the score-import Construct stamps on every score it copies in. Same
    // spelling as the StreetManager TU-scope constant of the same name (the two are separate
    // TU-scope constants in the original, not one shared symbol).
    const char KAC_LOCAL_PLAYER_NAME_TEXT[12] = "LCL_PLAY_NM";
}

//
void ChallengeHighScoreEntry::Construct()
{
    ChallengeData::Construct();

    for (s32 liScoreType = 0; liScoreType < E_SCORE_TYPE_COUNT; ++liScoreType)
    {
        maPlayerNames[liScoreType].Construct("");
    }
}

//
void ChallengeHighScoreEntry::Copy(const ChallengeHighScoreEntry* lpData)
{
    CGS_ASSERT(lpData != NULL, "lpData");

    ChallengeData::Copy(lpData);

    for (s32 liScoreType = 0; liScoreType < E_SCORE_TYPE_COUNT; ++liScoreType)
    {
        maPlayerNames[liScoreType] = lpData->maPlayerNames[liScoreType];
    }
}

//
bool ChallengeHighScoreEntry::IsWholeChallengeOwnedBySamePlayer()
{
    s32 liScoreType = 0;

    // Walk the score-type owner names while the current one is non-empty and matches the next;
    // the console returns true as soon as the first pair matches (E_SCORE_TYPE_COUNT == 2).
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

// The score-import Construct: reset to an empty entry, then copy every score the source
// ChallengeData actually carries into this entry, attributing each one to the local-player
// owner name. Unlike the no-arg Construct this one does NOT blank the owner-name array --
// a score type the source does not carry keeps whatever the base Construct left, exactly as
// the console body does.
void ChallengeHighScoreEntry::Construct(ChallengeData* lpData)
{
    ChallengeData::Construct();

    CgsNetwork::PlayerName lLocalPlayerName;
    lLocalPlayerName.Construct(KAC_LOCAL_PLAYER_NAME_TEXT);

    for (s32 liScoreType = 0; liScoreType < E_SCORE_TYPE_COUNT; ++liScoreType)
    {
        const ScoreType leScoreType = static_cast<ScoreType>(liScoreType);

        if (lpData->ContainsData(leScoreType))
        {
            SetScore(leScoreType, lpData->GetScore(leScoreType), &lLocalPlayerName);
        }

        CGS_ASSERT(liScoreType + 1 <= E_SCORE_TYPE_COUNT, "leEnumIndex <= E_SCORE_TYPE_COUNT");
    }
}

// Merge lpEntry's scores into this entry. For every score type the incoming entry carries:
// if this entry has no score of that type, take the incoming one outright; if it has one,
// take the incoming one only when the score type's comparator ranks the stored score WORSE
// than the incoming score (CompareScores(stored, incoming) > 0). Each taken score brings its
// owner name with it. When lpUpdateScoresBitArray is supplied it is cleared up front and one
// bit is set per taken score type. Returns true if anything was taken.
bool ChallengeHighScoreEntry::UpdateEntry(const ChallengeHighScoreEntry* lpEntry,
                                          CgsContainers::FastBitArray<2>* lpUpdateScoresBitArray)
{
    CGS_ASSERT(lpEntry != NULL, "lpEntry");

    if (lpUpdateScoresBitArray)
    {
        lpUpdateScoresBitArray->UnSetAll();
    }

    bool lbUpdated = false;

    for (s32 liScoreType = 0; liScoreType < E_SCORE_TYPE_COUNT; ++liScoreType)
    {
        const ScoreType leScoreType = static_cast<ScoreType>(liScoreType);

        if (lpEntry->ContainsData(leScoreType))
        {
            int32_t                liIncomingScore = 0;
            CgsNetwork::PlayerName lIncomingName;
            lpEntry->GetScore(leScoreType, &liIncomingScore, &lIncomingName);

            bool lbTakeIncoming = true;
            if (ContainsData(leScoreType))
            {
                lbTakeIncoming = ChallengeData::CompareScores(leScoreType,
                                                              ChallengeData::GetScore(leScoreType),
                                                              liIncomingScore) > 0;
            }

            if (lbTakeIncoming)
            {
                SetScore(leScoreType, liIncomingScore, &lIncomingName);

                if (lpUpdateScoresBitArray)
                {
                    CGS_ASSERT(static_cast<u32>(liScoreType) < E_SCORE_TYPE_COUNT,
                               "luIndex < NUMBITS");
                    lpUpdateScoresBitArray->SetBit(static_cast<u32>(liScoreType));
                }

                lbUpdated = true;
            }
        }

        CGS_ASSERT(liScoreType + 1 <= E_SCORE_TYPE_COUNT, "leEnumIndex <= E_SCORE_TYPE_COUNT");
    }

    return lbUpdated;
}

} // namespace BrnStreetData
