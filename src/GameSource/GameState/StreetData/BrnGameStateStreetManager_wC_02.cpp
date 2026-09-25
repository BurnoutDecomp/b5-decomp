// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_02.cpp
//   (wave C partfile -- group 2)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::ProcessScoreRequestEvent  @ 0x8234A240
//   (BrnGameState::StreetManager::SetupParRivals @0x8233F560 was split out 2026-08-11 into
//    the sibling BrnGameStateStreetManager_SetupParRivals.cpp -- see the note at its former
//    place below. Its two vector immediates and its LCG seed went with it.)
//
// Every store / branch / early-out / assert has an asm counterpart; members are
// the frozen-header named fields (never raw-offset casts).
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameEvents.h"                        // GameStateModuleIO::RoadRulesScoreRequestEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                 // OutputBuffer (GetGameActionQueue / GetGuiOutputQueue)
#include "GameSource/GameState/BrnGameActions.h"                       // RoadRulesRoadScoreAction (action 284)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // CgsModule::Event / VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"           // CgsDev::StrStream (streamed dev assert)
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT + KI_MESSAGEBUFFERSIZE

#include <string.h>

// Case-insensitive bounded compare (console strnicmp -> MSVC _strnicmp).
// Committed precedent: GameSource/Game/X360/BrnSystemHWX360.cpp:11.
#if defined(_MSC_VER)
#  define strnicmp _strnicmp
#endif

namespace BrnGameState
{

// ---------------------------------------------------------------------------
// @ 0x8233F560. SetupParRivals WAS DEFINED HERE; it was SPLIT OUT 2026-08-11 into the sibling
// BrnGameStateStreetManager_SetupParRivals.cpp so that GameStateModule::Prepare2 case 2 could
// stop parking on it. MEASURED (cl /c + dumpbin /SYMBOLS vs the defined-symbol set of
// build\game\obj): mounting this whole partfile costs THIRTEEN unresolved externals, and every
// one of them belongs to ProcessScoreRequestEvent below (the score-entry family, PlayerName,
// SPrintf and the StrStream dev-assert chain). SetupParRivals touches none of them, so the split
// costs ZERO. Fold the split file back in when the score-entry family lands. Do NOT re-add the
// body: two definitions is LNK2005.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// @ 0x8234A240. Answers a GUI road-score request: formats the road's user and
// high scores against the compiled par score (falling back to the par rival's
// numeric id as the displayed name) and posts the 48-byte response (type 284).
// EVERY score access uses meActiveRoadRuleType -- never a parameter.
// ---------------------------------------------------------------------------
void StreetManager::ProcessScoreRequestEvent(
        GameStateModuleIO::OutputBuffer* lpOutput,
        const GameStateModuleIO::RoadRulesScoreRequestEvent* lpScoreRequestEvent )
{
    if ( meActiveRoadRuleType == BrnStreetData::E_SCORE_TYPE_COUNT )
    {
        return;
    }

    CGS_ASSERT( lpOutput, "lpOutput" );
    CGS_ASSERT( lpScoreRequestEvent, "lpRequestEvent" );

    GameStateModuleIO::RoadRulesRoadScoreAction lResponse;
    lResponse.mPlayerName.Construct( "" );

    const BrnStreetData::ChallengeIndex liChallengeIndex = lpScoreRequestEvent->mRoadChallengeIndex;

    lResponse.mChallengeIndex        = liChallengeIndex;
    lResponse.mbAIRulesRoadOffline   = false;
    lResponse.mbAIRulesRoadOnline    = false;
    lResponse.mbLocalPlayerRulesRoad = false;
    lResponse.mRoadID               = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liChallengeIndex];

    if ( meActiveRoadRuleType == BrnStreetData::E_SCORE_TYPE_TIME )
    {
        lResponse.meActiveRoadRule = E_ACTIVE_ROAD_RULE_OFFLINE_TIME;
    }
    else if ( meActiveRoadRuleType == BrnStreetData::E_SCORE_TYPE_CRASH )
    {
        lResponse.meActiveRoadRule = E_ACTIVE_ROAD_RULE_OFFLINE_CRASH;
    }
    else
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::Assert::BeginAssert();
        CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Unhandled road rule type ";
        lStrStream << static_cast<s32>( meActiveRoadRuleType );
        lStrStream << " in StreetManager::ProcessScoreRequestEvent\n";
        // 0x8234A34C-58: r4 = the recovered path literal, r5 = 0xC54 (line 3156). Passed
        // verbatim (CgsID.cpp:80 precedent) so the recovered X360 site is not discarded.
        CgsDev::Assert::FireAssert(
            lacMessage,
            "..\\..\\..\\GameSource\\Gamestate/StreetData/BrnGameStateStreetManager.cpp",
            3156 );
        CgsDev::Assert::EndAssert();
    }

    BrnStreetData::ChallengeHighScoreEntry lHighScoreEntry;
    GetHighScoreEntry( liChallengeIndex, &lHighScoreEntry, false );

    BrnStreetData::ChallengePlayerScoreEntry lUserScoreEntry;
    GetChallengeUserScore( liChallengeIndex, &lUserScoreEntry, false );

    BrnStreetData::ChallengeParScoresEntry lParScoresEntry;
    lParScoresEntry.Copy( mpStreetData->GetChallengeParScore( liChallengeIndex ) );

    s32     liParScore   = 0;
    ::CgsID lParRivalId  = 0;
    lParScoresEntry.GetScore( meActiveRoadRuleType, &liParScore, &lParRivalId );

    // ---- user score ------------------------------------------------------
    bool lbUserScoreBeatsPar = false;
    s32  liUserScore         = 0;
    if ( lUserScoreEntry.ContainsData( meActiveRoadRuleType ) )
    {
        liUserScore = lUserScoreEntry.GetScore( meActiveRoadRuleType );
        lbUserScoreBeatsPar =
            ( lUserScoreEntry.CompareScores( meActiveRoadRuleType, liUserScore, liParScore ) < 0 );
    }

    if ( lbUserScoreBeatsPar )
    {
        lResponse.miOfflineScore = liUserScore;
    }
    else
    {
        char lacRivalName[13];
        CgsCore::SPrintf( lacRivalName, 12, "%llu", GetParRivalId( liChallengeIndex, meActiveRoadRuleType ) );
        lacRivalName[12] = 0;
        lResponse.mPlayerName.Construct( lacRivalName );

        lResponse.miOfflineScore       = liParScore;
        lResponse.mbAIRulesRoadOffline = true;
    }

    // ---- high score ------------------------------------------------------
    CgsNetwork::PlayerName lHighScoreHolderName;
    s32  liHighScore          = 0;
    bool lbHighScoreBeatsPar  = false;
    if ( lHighScoreEntry.ContainsData( meActiveRoadRuleType ) )
    {
        lHighScoreEntry.GetScore( meActiveRoadRuleType, &liHighScore, &lHighScoreHolderName );
        lbHighScoreBeatsPar =
            ( lHighScoreEntry.CompareScores( meActiveRoadRuleType, liHighScore, liParScore ) < 0 );
    }

    if ( lbHighScoreBeatsPar )
    {
        if ( strnicmp( lHighScoreHolderName.macName, KAC_LOCAL_PLAYER_NAME_TEXT, 16 ) != 0 )
        {
            lResponse.mPlayerName.Construct( lHighScoreHolderName.macName );
        }
        else
        {
            lResponse.mbLocalPlayerRulesRoad = true;
        }
        lResponse.miOnlineScore = liHighScore;
    }
    else
    {
        char lacRivalName[13];
        CgsCore::SPrintf( lacRivalName, 12, "%llu", GetParRivalId( liChallengeIndex, meActiveRoadRuleType ) );
        lacRivalName[12] = 0;
        lResponse.mPlayerName.Construct( lacRivalName );

        lResponse.miOnlineScore       = liParScore;
        lResponse.mbAIRulesRoadOnline = true;
    }

    CGS_ASSERT( lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()" );

    CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
    lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lResponse ),
                       GameStateModuleIO::E_ACTION_ROAD_RULES_ROAD_SCORE, sizeof( lResponse ) );
}

} // namespace BrnGameState
