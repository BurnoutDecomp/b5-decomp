#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"  // BrnGameState::StreetManager (SetChallengeUserScore / road-rule accessors)
#include "SharedClasses/StreetData/BrnStreetData.h"                     // BrnStreetData::StreetData::GetRoadCount (Win* road-count bound)
#include "SharedClasses/StreetData/BrnChallengeData.h"                  // ChallengePlayerScoreEntry (Construct / SetScore / SetCarID), ScoreType, ScoreList::KAI_*_SCORES
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // BrnProgression::ProgressionManager (OnTrophyUnlock / CheckForSpecialCarUnlocks / GetAchievementManager)
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"  // AchievementManagerBase::OnSetAllRoadRules
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsID, CgsIDCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The in-game debug menu for the road-rules
// StreetManager. Seven of this TU's eight X360-attested functions are bodied here:
//   * GetName                     (0x823175F0) -- the menu label.
//   * OnActivate                  (0x8234A718) -- registers the debug actions + the
//                                                 "number of road rules to win" selector.
//   * SendRoadRulesScoresToNetwork(0x82317600) -- arms the network-upload toggle.
//   * PopulateUserChallengeScores (0x8233F928) -- fills every road with a randomised score.
//   * WinAllRoadRules             (0x82341CD8) -- best-score every road bar one, then award trophies.
//   * WinSpecificNumberOfTimeRoadRules  (0x823418B8) -- best-time the first N roads, then award.
//   * WinSpecificNumberOfCrashRoadRules (0x82341AC0) -- best-crash the first N roads, then award.
//
// The three Win* actions share an inlined "award road-rule trophies" tail that the X360 folds in
// from BrnGameStateStreetManager.cpp (its two range asserts carry that file's line numbers); it is
// reproduced faithfully in each action through the X360-attested StreetManager road-rule accessors
// (GetNumberOf*RuledByLocalPlayer / UpdateTrophyUnlockOnRoadRuleWin / GetProgressionManager, declared
// on BrnGameStateStreetManager.h) and the now-committed ProgressionManager / AchievementManagerBase.
//
// The remaining one, Update @ 0x8234A7E0, packs a road-rules score-summary game-action event from
// deep, still-un-named ProgressionManager/Profile members (Profile+0x1CD1C / Profile+0x1CD30 /
// ProgressionManager+0x1D0) into the output queue; those offsets have no attested member/accessor,
// so reconstructing it now would require raw-offset pokes into opaque classes (which the
// faithfulness rules forbid). It stays declared-only, for the pass that commits those members.
//
// The menu registrations go through the (unnamed) CgsDev::DebugComponent registration helpers,
// which are the base-class methods reconstructed in CgsDebugComponent.h, called by name
// (RegisterVariable / RegisterFunction) -- matching the sibling debug components.

// GetSystemTimerBaseTime has no committed header of its own; the recovered siblings
// (CgsDebugManager.cpp, CgsMoviePlayer.cpp) forward-declare it locally -- do the same.
namespace CgsSystem { u32 GetSystemTimerBaseTime(); }

namespace BrnGameState
{

// @ 0x823175F0
const char* StreetManagerDebugComponent::GetName() const
{
    return "Street Manager";
}

// @ 0x8234A718
void StreetManagerDebugComponent::OnActivate()
{
    // NOTE (faithful to the X360 asm): the "Win all road rules bar one" label is registered
    // against the WinAllRoadRules callback -- the label text and the callback do not match, but
    // that is what the binary does.
    RegisterFunction( &WinAllRoadRules,                    this, "Win all road rules bar one" );
    RegisterFunction( &WinSpecificNumberOfTimeRoadRules,   this, "Win Specific Number Of Time RoadRules" );
    RegisterFunction( &WinSpecificNumberOfCrashRoadRules,  this, "Win Specific Number Of RoadRules" );
    RegisterVariable( &miNumberOfRoadRulesToWin, "Number of road rules to win" );
    RegisterFunction( &PopulateUserChallengeScores,        this, "Populate User Challenge Scores" );
    RegisterFunction( &SendRoadRulesScoresToNetwork,       this, "Send Scores to Network" );
}

// @ 0x82317600
void StreetManagerDebugComponent::SendRoadRulesScoresToNetwork( void* lpData )
{
    StreetManagerDebugComponent* lpStreetManagerDebugCmpt =
        static_cast<StreetManagerDebugComponent*>( lpData );

    CGS_ASSERT( lpData != NULL, "lpData" );
    CGS_ASSERT( lpStreetManagerDebugCmpt != NULL, "lpStreetManagerDebugCmpt" );

    lpStreetManagerDebugCmpt->mbOutputRoadRulesScoresToNetwork = true;
}

// @ 0x8233F928
void StreetManagerDebugComponent::PopulateUserChallengeScores( void* lpData )
{
    StreetManagerDebugComponent* lpStreetManagerDebugCmpt =
        static_cast<StreetManagerDebugComponent*>( lpData );

    CGS_ASSERT( lpData != NULL, "lpData" );
    CGS_ASSERT( lpStreetManagerDebugCmpt != NULL, "lpStreetManagerDebugCmpt" );
    CGS_ASSERT( lpStreetManagerDebugCmpt->mpStreetManager != NULL,
                "lpStreetManagerDebugCmpt->mpStreetManager" );

    // A plain 64-bit linear-congruential generator (multiplier 0x5851F42D4C957F2D, increment 1
    // -- the well-known Vigna/PCG constant) seeded from the system timer and warmed up eight
    // times. It only scatters debug scores, so exact reproducibility is not required, but the
    // full 64-bit multiply is reproduced faithfully (the X360 builds the constant with a
    // lis/ori pair per dword + insrdi and iterates it with mulld).
    const u64 KU_LCG_MULTIPLIER = 0x5851F42D4C957F2Dull;

    u64 luRng = CgsSystem::GetSystemTimerBaseTime();
    for ( s32 liWarmUp = 0; liWarmUp < 8; ++liWarmUp )
    {
        luRng = luRng * KU_LCG_MULTIPLIER + 1;
    }

    // Roads are filled high-to-low (63 down to 0), matching the X360 counted-down loop.
    for ( s32 liRoad = 63; liRoad >= 0; --liRoad )
    {
        // Score type comes from the low bit of the current RNG high word (before advancing).
        const BrnStreetData::ScoreType leScoreType =
            static_cast<BrnStreetData::ScoreType>( static_cast<u32>( luRng >> 32 ) & 1u );
        luRng = luRng * KU_LCG_MULTIPLIER + 1;

        BrnStreetData::ChallengePlayerScoreEntry lEntry;
        lEntry.Construct();

        // Score is the advanced RNG high word reduced modulo 989, biased up by 10.
        const s32 liScore = static_cast<s32>( static_cast<u32>( luRng >> 32 ) % 989u ) + 10;
        luRng = luRng * KU_LCG_MULTIPLIER + 1;

        lEntry.SetScore( leScoreType, liScore );

        const CgsID lCarID = CgsIDCompress( "PUSSC01" );
        lEntry.SetCarID( leScoreType, lCarID );

        lpStreetManagerDebugCmpt->mpStreetManager->SetChallengeUserScore( liRoad, &lEntry, false );
    }
}

// -----------------------------------------------------------------------------------------------
// Shared "award road-rule trophies" tail (X360: inlined into all three Win* actions from
// BrnGameStateStreetManager.cpp -- the two range asserts below carry that file's line numbers
// (2807 / 2808); the baked file/line are dropped per project convention, only the verbatim message
// strings are kept -- note the original "TimeTrail" spelling). Written once here as a file-local
// helper so the three de-inlined callers stay readable; the X360 emitted one folded copy per action.
// It touches no StreetManagerDebugComponent member, only the passed-in StreetManager.
// -----------------------------------------------------------------------------------------------
namespace
{
    void AwardRoadRuleWinTrophies( StreetManager* lpStreetManager )
    {
        // Wave-B header freeze firmed the param up from the old `int32_t
        // liUpdateHighScores` model to its real type (BrnStreetData::ScoreType --
        // UpdateTrophyUnlockOnRoadRuleWin @ 0x82341780 branches on 0==TIME / 1==CRASH).
        // The literal 0 this helper always passed == E_SCORE_TYPE_TIME. FLAG: the X360
        // crash-flavoured Win* actions may pass E_SCORE_TYPE_CRASH at their inlined
        // copies -- re-verify per-action when those inline tails are next visited.
        lpStreetManager->UpdateTrophyUnlockOnRoadRuleWin( BrnStreetData::E_SCORE_TYPE_TIME );

        const int32_t liNumberOfShowTimeRoadsRuled =
            lpStreetManager->GetNumberOfParShowTimeRoadsRuledByLocalPlayer();
        const int32_t liNumberOfTimeTrialRoadsRuled =
            lpStreetManager->GetNumberOfParTimeTrialRoadsRuledByLocalPlayer();
        const int32_t liNumberOfCompleteRoadsRuled =
            lpStreetManager->GetNumberOfCompleteRoadsRuledByLocalPlayer();
        const int32_t liTotalNumberOfRoads = lpStreetManager->GetStreetData()->GetRoadCount();

        CGS_ASSERT( liNumberOfShowTimeRoadsRuled <= liTotalNumberOfRoads,
                    "liNumberOfShowTimeRoadsRuled <= liTotalNumberOfRoads" );
        CGS_ASSERT( liNumberOfTimeTrialRoadsRuled <= liTotalNumberOfRoads,
                    "liNumberOfTimeTrailRoadsRuled <= liTotalNumberOfRoads" );

        BrnProgression::ProgressionManager* lpProgressionManager =
            lpStreetManager->GetProgressionManager();

        // Trophy ids are the bare X360 li-immediates the call site passes (7/25/8/23); no
        // achievement-id enum is recovered, so they stay literal (matching ProgressionManager::
        // OnTrophyUnlock's own s32 single-id model).
        if ( liNumberOfShowTimeRoadsRuled == liTotalNumberOfRoads )
        {
            lpProgressionManager->OnTrophyUnlock( 7 );
            // X360 passes score-type 1 (== E_SCORE_TYPE_CRASH) to OnSetAllRoadRules.
            lpProgressionManager->GetAchievementManager()->OnSetAllRoadRules(
                BrnStreetData::E_SCORE_TYPE_CRASH );
        }

        lpProgressionManager->OnTrophyUnlock( 25 );

        if ( liNumberOfCompleteRoadsRuled == liTotalNumberOfRoads )
        {
            lpProgressionManager->OnTrophyUnlock( 8 );
        }

        lpProgressionManager->OnTrophyUnlock( 23 );
        lpProgressionManager->CheckForSpecialCarUnlocks();
    }
}

// @ 0x82341CD8
void StreetManagerDebugComponent::WinAllRoadRules( void* lpData )
{
    StreetManagerDebugComponent* lpStreetManagerDebugCmpt =
        static_cast<StreetManagerDebugComponent*>( lpData );

    CGS_ASSERT( lpData != NULL, "lpData" );
    CGS_ASSERT( lpStreetManagerDebugCmpt != NULL, "lpStreetManagerDebugCmpt" );
    CGS_ASSERT( lpStreetManagerDebugCmpt->mpStreetManager != NULL,
                "lpStreetManagerDebugCmpt->mpStreetManager" );

    StreetManager* lpStreetManager = lpStreetManagerDebugCmpt->mpStreetManager;

    // "...bar one": every road except the last gets a best-possible score.
    for ( int32_t liRoad = 0;
          liRoad < lpStreetManager->GetStreetData()->GetRoadCount() - 1;
          ++liRoad )
    {
        BrnStreetData::ChallengePlayerScoreEntry lEntry;
        lEntry.Construct();

        // Best-possible crash score == the score table's per-type maximum; best-possible time
        // score == the table minimum + 1 (SetScore range-checks each against those same tables).
        lEntry.SetScore( BrnStreetData::E_SCORE_TYPE_CRASH,
                         BrnStreetData::ScoreList::KAI_MAX_SCORES[ BrnStreetData::E_SCORE_TYPE_CRASH ] );
        lEntry.SetCarID( BrnStreetData::E_SCORE_TYPE_CRASH, CgsIDCompress( "PUSSC01" ) );

        lEntry.SetScore( BrnStreetData::E_SCORE_TYPE_TIME,
                         BrnStreetData::ScoreList::KAI_MIN_SCORES[ BrnStreetData::E_SCORE_TYPE_TIME ] + 1 );
        lEntry.SetCarID( BrnStreetData::E_SCORE_TYPE_TIME, CgsIDCompress( "PUSSC01" ) );

        lpStreetManager->SetChallengeUserScore( liRoad, &lEntry, true );
    }

    AwardRoadRuleWinTrophies( lpStreetManager );
}

// @ 0x823418B8
void StreetManagerDebugComponent::WinSpecificNumberOfTimeRoadRules( void* lpData )
{
    StreetManagerDebugComponent* lpStreetManagerDebugCmpt =
        static_cast<StreetManagerDebugComponent*>( lpData );

    CGS_ASSERT( lpData != NULL, "lpData" );
    CGS_ASSERT( lpStreetManagerDebugCmpt != NULL, "lpStreetManagerDebugCmpt" );
    CGS_ASSERT( lpStreetManagerDebugCmpt->mpStreetManager != NULL,
                "lpStreetManagerDebugCmpt->mpStreetManager" );

    StreetManager* lpStreetManager = lpStreetManagerDebugCmpt->mpStreetManager;

    for ( int32_t liRoad = 0;
          liRoad < lpStreetManagerDebugCmpt->miNumberOfRoadRulesToWin;
          ++liRoad )
    {
        BrnStreetData::ChallengePlayerScoreEntry lEntry;
        lEntry.Construct();

        // Best-possible time score == 0.
        lEntry.SetScore( BrnStreetData::E_SCORE_TYPE_TIME, 0 );
        lEntry.SetCarID( BrnStreetData::E_SCORE_TYPE_TIME, CgsIDCompress( "PUSSC01" ) );

        lpStreetManager->SetChallengeUserScore( liRoad, &lEntry, false );
    }

    AwardRoadRuleWinTrophies( lpStreetManager );
}

// @ 0x82341AC0
void StreetManagerDebugComponent::WinSpecificNumberOfCrashRoadRules( void* lpData )
{
    StreetManagerDebugComponent* lpStreetManagerDebugCmpt =
        static_cast<StreetManagerDebugComponent*>( lpData );

    CGS_ASSERT( lpData != NULL, "lpData" );
    CGS_ASSERT( lpStreetManagerDebugCmpt != NULL, "lpStreetManagerDebugCmpt" );
    CGS_ASSERT( lpStreetManagerDebugCmpt->mpStreetManager != NULL,
                "lpStreetManagerDebugCmpt->mpStreetManager" );

    StreetManager* lpStreetManager = lpStreetManagerDebugCmpt->mpStreetManager;

    for ( int32_t liRoad = 0;
          liRoad < lpStreetManagerDebugCmpt->miNumberOfRoadRulesToWin;
          ++liRoad )
    {
        BrnStreetData::ChallengePlayerScoreEntry lEntry;
        lEntry.Construct();

        // Best-possible crash score == INT32_MAX (the X360 stores 0x7FFFFFFF directly).
        lEntry.SetScore( BrnStreetData::E_SCORE_TYPE_CRASH, 0x7FFFFFFF );
        lEntry.SetCarID( BrnStreetData::E_SCORE_TYPE_CRASH, CgsIDCompress( "PUSSC01" ) );

        lpStreetManager->SetChallengeUserScore( liRoad, &lEntry, false );
    }

    AwardRoadRuleWinTrophies( lpStreetManager );
}

} // namespace BrnGameState
