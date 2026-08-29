// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_06.cpp
//   (wave C partfile -- group 6 "road-rule beaten predicates")
//
// [pause-stats wave 2026-08-29] The two predicates the three roads-ruled tallies in
// _wC_04 / _wC_05 count with. They were the last unresolved externals between the pause
// screen's stat panel and a green link:
//
//   BrnGameState::StreetManager::HasPlayerBeatenParScore    @ 0x823361D0  (52 insns)
//   BrnGameState::StreetManager::HasPlayerBeatenFriendScore @ 0x823362A0  (48 insns)
//
// Both are the same five-step shape and both return the three-valued
// StreetManager::ERoadRuleCompletionStatus, NOT a bool:
//   NO_DATA(0) / BEATEN(1) / NOT_BEATEN(2).
//
// ⚠️ THE TWO "MISSING DATA" ARMS ANSWER DIFFERENTLY, AND IT IS NOT SYMMETRY-BREAKING NOISE:
//   * no USER score      -> NOT_BEATEN in both (`li r3, 2` @0x82336228 / @0x823362F0)
//   * no PAR score       -> NO_DATA        (`li r3, 0` @0x82336294)
//   * no FRIEND score    -> BEATEN         (`li r3, 1` @0x82336354)
// i.e. an unset par score means "this road has no rule to beat", while an unset friend score
// means "you are ahead of your friends by default". Both are the binary's.
//
// ⚠️ CompareScores IS STATIC AND ITS FIRST ARGUMENT IS THE SCORE TYPE. The console loads
// `mr r3, r31` (the ScoreType parameter) into the `this` slot -- BrnChallengeData.h:144
// already documents and declares it static for exactly this reason. `> 0` means the user's
// score is worse, hence NOT_BEATEN; `<= 0` (equal counts) is BEATEN.
//
// ⚠️ THE PAR RECORD IS FETCHED THROUGH THE STREET DATA, NOT THROUGH
// StreetManager::GetChallengeParScore @0x82336168 -- the console INLINES that accessor here
// (`addi r3, this, 0x1CC8` + ResourcePtr operator-> + StreetData::GetChallengeParScore, then
// ChallengeParScoresEntry::Copy onto a stack local), which is why no call to it appears.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "SharedClasses/StreetData/BrnStreetData.h"                 // StreetData::GetChallengeParScore
#include "SharedClasses/StreetData/BrnChallengeData.h"              // ChallengeData / ChallengePlayerScoreEntry / ChallengeParScoresEntry
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h" // ChallengeHighScoreEntry
#include "BrnCommonTypes.h"                                         // CgsID (the par record's rival-id out-param)
#include "GameSource/GameState/BrnCgsPlayerName.h"                  // CgsNetwork::PlayerName (the friend record's out-param)

namespace BrnGameState
{

// @ 0x823361D0. Has the local player beaten the authored PAR score for this road/score type?
ERoadRuleCompletionStatus
StreetManager::HasPlayerBeatenParScore( BrnStreetData::ChallengeIndex liIndex,
                                        BrnStreetData::ScoreType leScoreType )
{
    // `li r6, 0` -- the console asks by CHALLENGE index, not by road index.
    BrnStreetData::ChallengePlayerScoreEntry lUserScore;
    GetChallengeUserScore( liIndex, &lUserScore, false );

    // The inlined StreetManager::GetChallengeParScore (see the banner).
    BrnStreetData::ChallengeParScoresEntry lParScore;
    lParScore.Copy( GetStreetData()->GetChallengeParScore( liIndex ) );

    if ( !lUserScore.ContainsData( leScoreType ) )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_NOT_BEATEN;
    }

    if ( !lParScore.ContainsData( leScoreType ) )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_NO_DATA;
    }

    const s32 liPlayerScore = lUserScore.GetScore( leScoreType );

    s32   liParScore = 0;
    CgsID lParRivalId = 0;
    lParScore.GetScore( leScoreType, &liParScore, &lParRivalId );

    // `cmpwi r3, 0 / bgt` -- strictly greater means the player has NOT beaten it.
    if ( BrnStreetData::ChallengeData::CompareScores( leScoreType, liPlayerScore, liParScore ) > 0 )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_NOT_BEATEN;
    }

    return E_ROAD_RULE_COMPLETION_STATUS_BEATEN;
}

// @ 0x823362A0. The friend-table twin: has the local player beaten his friends' best?
ERoadRuleCompletionStatus
StreetManager::HasPlayerBeatenFriendScore( BrnStreetData::ChallengeIndex liIndex,
                                           BrnStreetData::ScoreType leScoreType )
{
    BrnStreetData::ChallengePlayerScoreEntry lUserScore;
    GetChallengeUserScore( liIndex, &lUserScore, false );

    BrnStreetData::ChallengeHighScoreEntry lFriendScore;
    GetChallengeFriendHighScore( liIndex, &lFriendScore, false );

    if ( !lUserScore.ContainsData( leScoreType ) )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_NOT_BEATEN;
    }

    // No friend entry -> BEATEN, unlike the par version's NO_DATA. See the banner.
    if ( !lFriendScore.ContainsData( leScoreType ) )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_BEATEN;
    }

    const s32 liPlayerScore = lUserScore.GetScore( leScoreType );

    // ⓘ the friend record's second out-param is the friend's NAME, not a CgsID -- the
    // ChallengeHighScoreEntry twin of the par record's rival id (BrnChallengeHighScoreEntry.h:40).
    s32                     liFriendScore = 0;
    CgsNetwork::PlayerName  lFriendName;
    lFriendScore.GetScore( leScoreType, &liFriendScore, &lFriendName );

    if ( BrnStreetData::ChallengeData::CompareScores( leScoreType, liPlayerScore, liFriendScore ) > 0 )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_NOT_BEATEN;
    }

    return E_ROAD_RULE_COMPLETION_STATUS_BEATEN;
}

}
