// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_00.cpp
//   (wave C partfile -- group 0 "new road score")
//
// Faithful de-optimisation of BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::ProcessNewRoadScore  @ 0x823496C8
//
// Every asm store / branch / early-out / assert has a counterpart here, and all
// state is reached through the frozen-header named members
// (BrnGameStateStreetManager.h) -- never a raw-offset cast:
//   * `mpStreetData->` == the asm's `addi rX, this, 0x1CC8` +
//     BrnStreetData::StreetData_::oper (ResourcePtr<StreetData>::operator->).
//   * `mpGameStateModule->GetDeveloperChallengeManager()` == the inlined
//     `mpGameStateModule + 185712` (0x2D570) embedded-subobject adjust that the
//     asm asserts non-null and then calls OnSetRoadRule on.
//   * `mpGameStateModule->GetActivePlayerCarId()` == the `ldx rX, pModule, 0x456D8`
//     64-bit car-id read feeding both SetCarID stores.
//   * `mpProgressionManager->GetProfile()` == pm+0x170; the +612 best-showtime
//     max-update is Get/SetNewHighShowtimeScore.
//   * `mpProgressionManager->GetAchievementManager()` == *(pm+133432).
//   * the two inlined bit pokes are BitArray<64u>::SetBit / ::UnSetBit on a stack
//     BitArray and on maRoadRulesChangedBitArrays[meActiveRoadRuleType]
//     (this+0x1CA8 + type*8); their CgsBitArray.h:222/:241 bounds asserts belong
//     to those methods and are not duplicated here.
//
// TRAPS kept faithful:
//   * the save-slot scan compares FULL 64-bit road CgsIDs against
//     KAA_SAVE_GAME_CHALLENGE_ROAD_IDS; a miss returns without touching anything.
//   * the two result flags come out of `cntlzw`-pair tests, i.e. exactly
//     `ChallengeData::CompareScores(...) < 0` -- reproduced branch for branch.
//   * ChallengeData::GetScore is re-read (not cached) for each comparison, as the
//     X360 code does.
//   * every Get/SetChallenge* call on this path passes the by-road-index mapping
//     flag TRUE.
//   * the assert message at line 2699 spells `lpOutputBuffer->...` even though the
//     frozen parameter is lpOutput -- kept VERBATIM.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"                                    // GetDeveloperChallengeManager / GetActivePlayerCarId
#include "GameSource/GameState/BrnGameStateModuleIO.h"                                  // OutputBuffer::GetGameActionQueue / GetGuiOutputQueue
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h"// DeveloperChallengeManager::OnSetRoadRule
#include "GameSource/GameState/Progression/BrnProgressionManager.h"                     // ProgressionManager accessors
#include "GameSource/GameState/Progression/BrnProfile.h"                                // Profile::Get/SetNewHighShowtimeScore
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase::OnSetRoadRule
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"                 // ChallengeHighScoreEntry Construct/GetScore/IsWholeChallengeOwnedBySamePlayer
#include "SharedClasses/StreetData/BrnChallengeData.h"                                  // ChallengeData / ChallengePlayerScoreEntry / ChallengeParScoresEntry
#include "SharedClasses/StreetData/BrnStreetData.h"                                     // StreetData::GetRoad / GetChallengeParScore, Road::GetId
#include "GameSource/GameState/BrnCgsPlayerName.h"                                      // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                              // CgsContainers::BitArray<64u>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                        // CgsModule::Event / VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                                      // CGS_ASSERT

namespace
{
    // The 48-byte record the X360 builds at var_160 and posts with
    // AddEvent(&record, 233, 48): the freshly-recorded score entry, the road's
    // save-game challenge slot, then a zero byte. (X360 stores: Copy @+0,
    // stw slot @+0x28, stb 0 @+0x2C.)
    // FLAG: the field identifiers are ours -- no DWARF name survives for this
    // GUI payload; the offsets/sizes are byte-exact from the image.
    struct NewRoadScoreRecord
    {
        BrnStreetData::ChallengePlayerScoreEntry mScoreEntry;           // +0x00 (40B)
        s32                                      miSaveGameSlotIndex;   // +0x28
        u8                                       mu8Flag;               // +0x2C
    };
    static_assert( sizeof(NewRoadScoreRecord) == 48, "new-road-score record is 0x30" );

    // The 48-byte notification the X360 builds at var_200 and posts with
    // AddEvent(&notification, 281, 48) once the score is a new high score or has
    // beaten the friend's score.
    // FLAG: the field identifiers are ours (no DWARF name for this GUI payload);
    // every offset/size below is byte-exact from the image.
    struct RoadRuleWonNotification
    {
        ::CgsID                mRoadId;                            // +0x00
        u32                    meScoreType;                        // +0x08
        s32                    miReserved;                         // +0x0C (stw 0)
        s32                    miNumberOfRoadsRuled;               // +0x10
        CgsNetwork::PlayerName mPlayerName;                        // +0x14 (16B)
        u8                     mu8One;                             // +0x24 (stb 1)
        u8                     mbWholeChallengeOwnedBySamePlayer;  // +0x25
        u8                     mu8Zero0;                           // +0x26 (stb 0)
        u8                     mu8Zero1;                           // +0x27 (stb 0)
        u8                     mbBeatFriendScore;                  // +0x28
    };
    static_assert( sizeof(RoadRuleWonNotification) == 48, "road-rule-won notification is 0x30" );
}

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// @ 0x823496C8. Apply a freshly-scored local challenge entry to a road's slot.
//
// Resolves the road's persisted save-game challenge slot (a road with no slot
// early-outs), pulls the road's user / par / friend records, notifies the
// developer-challenge manager, keeps the profile's best show-time score, ranks the
// new score against the stored user score, par and the friend high score, and posts
// the resulting GUI actions (55/233/281/285/149). A genuine new high score also
// runs the trophy / achievement fan-out.
// ----------------------------------------------------------------------------
void StreetManager::ProcessNewRoadScore( GameStateModuleIO::OutputBuffer* lpOutput,
                                         BrnStreetData::ChallengePlayerScoreEntry lNewScoreEntry,
                                         BrnStreetData::ScoreType leScoreType,
                                         BrnStreetData::ChallengeIndex liChallengeIndex,
                                         bool lbCheckFriendScore )
{
    bool lbBeatFriendScore = false;

    const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liChallengeIndex );

    BrnStreetData::ChallengeIndex liSaveGameSlotIndex = 0;
    const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
    while ( *lpSlotRoadId != lpRoad->GetId() )
    {
        ++lpSlotRoadId;
        ++liSaveGameSlotIndex;
        if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
        {
            return;
        }
    }

    if ( liSaveGameSlotIndex >= 0 )
    {
        BrnStreetData::ChallengePlayerScoreEntry lUserScore;
        GetChallengeUserScore( liChallengeIndex, &lUserScore, true );

        BrnStreetData::ChallengeParScoresEntry lParScores;
        lParScores.Copy( mpStreetData->GetChallengeParScore( liChallengeIndex ) );

        BrnStreetData::ChallengeHighScoreEntry lFriendHighScore;
        GetChallengeFriendHighScore( liChallengeIndex, &lFriendHighScore, true );

        BrnStreetData::ChallengePlayerScoreEntry lNewEntry;
        lNewEntry.Construct();

        bool lbNewHighScore = false;

        if ( lNewScoreEntry.ContainsData( leScoreType ) )
        {
            const s32 liNewScore = lNewScoreEntry.GetScore( leScoreType );

            bool lbUserScoreBeatsPar = false;

            CGS_ASSERT( mpGameStateModule, "mpGameStateModule" );
            CGS_ASSERT( mpGameStateModule->GetDeveloperChallengeManager(),
                        "mpGameStateModule->GetDeveloperChallengeManager()" );

            // The X360 hands the FULL 64-bit road CgsID in r6; the committed
            // DeveloperChallengeManager::OnSetRoadRule models the 32-bit low half its
            // body compares against a road-id literal, so the narrowing cast IS the
            // faithful spelling of this call, not a convenience.
            mpGameStateModule->GetDeveloperChallengeManager()->OnSetRoadRule(
                leScoreType,
                liNewScore,
                static_cast<s32>( mpStreetData->GetRoad( liChallengeIndex )->GetId() ) );

            if ( leScoreType == BrnStreetData::E_SCORE_TYPE_CRASH )
            {
                BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
                CGS_ASSERT( lpProfile != NULL, "lpProfile != NULL" );

                if ( liNewScore > lpProfile->GetNewHighShowtimeScore() )
                {
                    lpProfile->SetNewHighShowtimeScore( liNewScore );
                }
            }

            s32     liParScore;
            ::CgsID lParRivalId;
            lParScores.GetScore( leScoreType, &liParScore, &lParRivalId );

            bool lbBeatsStoredUserScore;
            if ( lUserScore.ContainsData( leScoreType ) )
            {
                lbBeatsStoredUserScore =
                    lUserScore.CompareScores( leScoreType, liNewScore, lUserScore.GetScore( leScoreType ) ) < 0;
                lbUserScoreBeatsPar =
                    lParScores.CompareScores( leScoreType, lUserScore.GetScore( leScoreType ), liParScore ) < 0;
            }
            else
            {
                lbBeatsStoredUserScore = true;
            }

            if ( lbBeatsStoredUserScore )
            {
                lUserScore.SetScore( leScoreType, liNewScore );

                CGS_ASSERT( mpProgressionManager, "mpProgressionManager" );
                CGS_ASSERT( mpProgressionManager->GetProfile(), "mpProgressionManager->GetProfile()" );

                const ::CgsID lActivePlayerCarId = mpGameStateModule->GetActivePlayerCarId();
                lUserScore.SetCarID( leScoreType, lActivePlayerCarId );

                if ( lParScores.CompareScores( leScoreType, liNewScore, liParScore ) < 0 )
                {
                    lNewEntry.SetScore( leScoreType, liNewScore );
                    lNewEntry.SetCarID( leScoreType, lActivePlayerCarId );

                    if ( lbCheckFriendScore && lFriendHighScore.ContainsData( leScoreType ) )
                    {
                        s32                    liFriendScore;
                        CgsNetwork::PlayerName lFriendHolderName;
                        lFriendHighScore.GetScore( leScoreType, &liFriendScore, &lFriendHolderName );

                        if ( lFriendHighScore.CompareScores( leScoreType, liNewScore, liFriendScore ) < 0 )
                        {
                            lbNewHighScore = true;
                        }
                        else
                        {
                            lbBeatFriendScore = !lbUserScoreBeatsPar;
                        }
                    }
                    else
                    {
                        lbNewHighScore = !lbUserScoreBeatsPar;
                    }

                    const u8 lu8ScoreRecorded = 0;
                    lpOutput->GetGuiOutputQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>( &lu8ScoreRecorded ), 55, 1 );
                }
            }
        }

        CGS_ASSERT( lpOutput->GetGameActionQueue(), "lpOutputBuffer->GetGameActionQueue()" );

        if ( lNewEntry.ContainsData( BrnStreetData::E_SCORE_TYPE_COUNT ) )
        {
            NewRoadScoreRecord lRecord;
            lRecord.mScoreEntry.Copy( &lNewEntry );
            lRecord.miSaveGameSlotIndex = liSaveGameSlotIndex;
            lRecord.mu8Flag             = 0;

            lpOutput->GetGuiOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lRecord ), 233, 48 );
        }

        if ( lbNewHighScore || lbBeatFriendScore )
        {
            BrnStreetData::ChallengeHighScoreEntry lHighScore;
            lHighScore.Construct();
            GetHighScoreEntry( liChallengeIndex, &lHighScore, true );

            RoadRuleWonNotification lNotification;
            lNotification.mPlayerName.Construct( "" );
            lNotification.mRoadId     = mpStreetData->GetRoad( liChallengeIndex )->GetId();
            lNotification.mu8One      = 1;
            lNotification.meScoreType = static_cast<u32>( leScoreType );
            lNotification.mbWholeChallengeOwnedBySamePlayer = lHighScore.IsWholeChallengeOwnedBySamePlayer();
            lNotification.mu8Zero0    = 0;
            lNotification.mu8Zero1    = 0;
            lNotification.miReserved  = 0;
            lNotification.miNumberOfRoadsRuled = GetNumberOfRoadsRulesByLocalPlayer();
            lNotification.mbBeatFriendScore    = lbBeatFriendScore;

            lpOutput->GetGuiOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lNotification ), 281, 48 );

            if ( leScoreType == meActiveRoadRuleType )
            {
                CgsContainers::BitArray<64u> lChangedRoads;
                lChangedRoads.UnSetAll();
                lChangedRoads.SetBit( liSaveGameSlotIndex );

                lpOutput->GetGuiOutputQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>( &lChangedRoads ), 285, 8 );

                maRoadRulesChangedBitArrays[meActiveRoadRuleType].UnSetBit( liSaveGameSlotIndex );
            }

            mNewHighScoreBuffer.EraseMatchingEntries( lNotification.mRoadId, leScoreType );
        }

        SetChallengeUserScore( liChallengeIndex, &lUserScore, true );

        if ( lbNewHighScore )
        {
            UpdateTrophyUnlockOnRoadRuleWin( leScoreType );

            CGS_ASSERT( mpProgressionManager, "mpProgressionManager" );
            CGS_ASSERT( mpProgressionManager->GetAchievementManager(),
                        "mpProgressionManager->GetAchievementManager()" );

            mpProgressionManager->GetAchievementManager()->OnSetRoadRule(
                leScoreType, mpStreetData->GetRoad( liChallengeIndex )->GetId() );

            s32 liTrophyCode;
            if ( HasPlayerBeatenParScore( liChallengeIndex, BrnStreetData::E_SCORE_TYPE_TIME )
                     == E_ROAD_RULE_COMPLETION_STATUS_BEATEN
                 && HasPlayerBeatenParScore( liChallengeIndex, BrnStreetData::E_SCORE_TYPE_CRASH )
                     == E_ROAD_RULE_COMPLETION_STATUS_BEATEN )
            {
                liTrophyCode = 31;
            }
            else
            {
                liTrophyCode = 29 + ( ( leScoreType != BrnStreetData::E_SCORE_TYPE_TIME ) ? 1 : 0 );
            }

            lpOutput->GetGuiOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &liTrophyCode ), 149, 4 );

            mpProgressionManager->CheckForSpecialCarUnlocks();
            mpProgressionManager->SendGameCompletionResults( lpOutput->GetGuiOutputQueue() );
        }
    }
}

} // namespace BrnGameState
