#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"  // ChallengeHighScoreEntry::Construct/SetScore
#include "SharedClasses/StreetData/BrnChallengeData.h"                    // ChallengePlayerScoreEntry, ChallengeData::SetScore, ScoreType, operator++
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The three members recovered here are the
// StreetManager methods that DON'T depend on the (still-deferred) StreetManager member
// layout: Prepare2 is a pure forwarder, and the two Create* factories operate entirely
// on the score record handed in. The remaining ledger methods (CanContinueWalking,
// PushSectionIndex, GetParRivalId, GetRoadIndexFromAISectionIndex,
// SetChallengeFriendHighScore) reach fixed StreetManager member offsets (miNextFreeSection
// Slot @+0x1D90, the section-walk short array @+0x1D94, the par-rival u64 table @+0x1800,
// the AISectionsData/StreetData ResourcePtrs @+0x1CE8/+0x1CC8, and the net-challenge array
// @+0), which the canonical header intentionally does not commit yet; they are left for
// when the full StreetManager layout lands.

namespace BrnGameState
{

// @ 0x823509D8
bool StreetManager::Prepare2( void* lpOutput, void* lpReceiverQueue, void* lpParRivalData )
{
    CGS_ASSERT( lpOutput != NULL, "lpOutput" );
    CGS_ASSERT( lpReceiverQueue != NULL, "lpReceiverQueue" );

    if ( !LoadStreetData( lpOutput, lpReceiverQueue ) )
    {
        return false;
    }

    SetupParRivals( lpParRivalData );
    return true;
}

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
