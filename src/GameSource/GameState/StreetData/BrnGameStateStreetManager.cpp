#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"  // ChallengeHighScoreEntry::Construct/SetScore
#include "SharedClasses/StreetData/BrnChallengeData.h"                    // ChallengePlayerScoreEntry, ChallengeData::SetScore, ScoreType, operator++
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The three members recovered here are the
// StreetManager methods that DON'T depend on the StreetManager member layout: Prepare2
// is a pure forwarder, and the two Create* factories operate entirely on the score
// record handed in. The full member layout is now committed in the frozen header
// (StreetManager keystone, wave B); the remaining 36 ledger methods land as
// BrnGameStateStreetManager_wB_* partfiles.

namespace BrnGameState
{

// @ 0x823509D8. Prepare2 WAS DEFINED HERE; it was SPLIT OUT 2026-08-11 into the sibling
// BrnGameStateStreetManager_Prepare2.cpp so that GameStateModule::Prepare2 case 2 could make the
// console's single call without dragging this TU's two score-entry factories. MEASURED
// (cl /c + dumpbin /SYMBOLS vs the defined-symbol set of build\game\obj): mounting this whole TU
// costs SIX unresolved externals (BrnStreetData::operator++, ChallengeHighScoreEntry::Construct,
// ChallengePlayerScoreEntry::Construct, ChallengeData::SetScore, ScoreList::KAI_MIN_SCORES /
// KAI_MAX_SCORES) and every one belongs to the two factories below. Fold the split file back in
// when the ChallengeData score family lands. Do NOT re-add the body: two definitions is LNK2005.

// @ 0x82324A28
BrnStreetData::ChallengeHighScoreEntry*
StreetManager::CreateHighScoreEntryFromDown(
    BrnStreetData::ChallengeHighScoreEntry* lpEntry,
    int                                     liUnused,
    const CgsNetwork::PlayerName*           lpPlayerNames,
    const int32_t*                          lpScores )
{
    (void)liUnused;   // reserved by the ABI, unreferenced by the X360 body

    lpEntry->Construct();

    // BrnStreetData::operator++ (out-of-line here) carries the inlined
    // "leEnumIndex <= E_SCORE_TYPE_COUNT" bounds assert the X360 fired each pass.
    for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        if ( lpPlayerNames[leScoreType].macName[0] != '\0' && lpScores[leScoreType] != 0 )
        {
            lpEntry->SetScore( leScoreType, lpScores[leScoreType], &lpPlayerNames[leScoreType] );
        }
    }

    return lpEntry;
}

// @ 0x82324AC0
BrnStreetData::ChallengePlayerScoreEntry*
StreetManager::CreateUserChallengeScoreFr(
    BrnStreetData::ChallengePlayerScoreEntry* lpEntry,
    int                                       liUnused,
    const int32_t*                            lpScores )
{
    (void)liUnused;   // reserved by the ABI, unreferenced by the X360 body

    lpEntry->Construct();

    for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        if ( lpScores[leScoreType] != 0 )
        {
            lpEntry->SetScore( leScoreType, lpScores[leScoreType] );
        }
    }

    return lpEntry;
}

} // namespace BrnGameState
