#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gamestate/StreetData/BrnStreetManagerDebugComponent.cpp"
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"  // BrnGameState::StreetManager (SetChallengeUserScore / road-rule accessors)
#include "SharedClasses/StreetData/BrnStreetData.h"                     // BrnStreetData::StreetData::GetRoadCount (Win* road-count bound)
#include "SharedClasses/StreetData/BrnChallengeData.h"                  // ChallengePlayerScoreEntry (Construct / SetScore / SetCarID), ScoreType, ScoreList::KAI_*_SCORES
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // BrnProgression::ProgressionManager (OnTrophyUnlock / CheckForSpecialCarUnlocks / GetAchievementManager)
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"  // AchievementManagerBase::OnSetAllRoadRules
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsID, CgsIDCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CgsDev::Assert
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"
#define CGS_ARTIST_FILE_BRNGAMESTATESTREETMANAGER "..\\..\\..\\GameSource\\Gamestate/StreetData/BrnGameStateStreetManager.cpp"
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
// from BrnGameStateStreetManager.cpp; its two range asserts retain that file's original lines 2807
// and 2808. The tail is represented once here through the X360-attested StreetManager accessors
// (GetNumberOf*RuledByLocalPlayer / UpdateTrophyUnlockOnRoadRuleWin / GetProgressionManager, declared
// on BrnGameStateStreetManager.h) and the now-committed ProgressionManager / AchievementManagerBase.
//
// The eighth, Update @ 0x8234A7E0, is BODIED in BrnStreetManagerDebugComponent_wO_01.cpp (wave O).
// The old park was stale: Profile+0x1CD1C and Profile+0x1CD30 are the two halves the X360 lwzx/
// extldi/add back together -- literally Profile::GetRoadRulesID() (BrnProfile.h:473) -- and the
// third offset was misattributed: "ProgressionManager+0x1D0" is the EMBEDDED Profile's
// muTimeStampOfLastRoadRulesDownload (+0x170 + 0x60). All three are committed named members, so no
// raw-offset poke was needed.
//
// The menu registrations go through the (unnamed) CgsDev::DebugComponent registration helpers,
// which are the base-class methods reconstructed in CgsDebugComponent.h, called by name
// (RegisterVariable / RegisterFunction) -- matching the sibling debug components.

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

    if ( lpData == NULL )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpData",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            477));
        CgsDev::Assert::EndAssert();
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStreetManagerDebugCmpt",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            479));
        CgsDev::Assert::EndAssert();
    }

    lpStreetManagerDebugCmpt->mbOutputRoadRulesScoresToNetwork = true;
}

// @ 0x8233F928
void StreetManagerDebugComponent::PopulateUserChallengeScores( void* lpData )
{
    StreetManagerDebugComponent* lpStreetManagerDebugCmpt =
        static_cast<StreetManagerDebugComponent*>( lpData );

    if ( lpData == NULL )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpData",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            165));
        CgsDev::Assert::EndAssert();
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStreetManagerDebugCmpt",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            167));
        CgsDev::Assert::EndAssert();
    }
    if ( lpStreetManagerDebugCmpt->mpStreetManager == NULL )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStreetManagerDebugCmpt->mpStreetManager",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            168));
        CgsDev::Assert::EndAssert();
    }

    // A plain 64-bit linear-congruential generator (multiplier 0x5851F42D4C957F2D, increment 1
    // -- the well-known Vigna/PCG constant) warmed up eight times. ARTIST 0x8233F9A8 reads the
    // full timer word, then `clrldi r11,r3,32` at 0x8233F9C4 keeps only its low 32 bits as the
    // initial seed. The subsequent LCG operations are 64-bit `mulld`/`addi` steps.
    const u64 KU_LCG_MULTIPLIER = 0x5851F42D4C957F2Dull;

    u64 luRng = static_cast<u32>(CgsSystem::GetSystemTimerBaseTime());
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

        // Score is the advanced RNG high word reduced modulo 991, biased up by 10.
        // MEASURED (0x8233FA50..0x8233FA70): the magic-number division is by 991 --
        // mulhwu with 0x08865437 (== ceil(2^42/991) - 2^32) then `mulli r10, r10, 0x3DF`
        // (0x3DF == 991) for the remainder. (An earlier pass mis-read this as 989.)
        const s32 liScore = static_cast<s32>( static_cast<u32>( luRng >> 32 ) % 991u ) + 10;
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
        // MEASURED (unblock pass, all three inlined tails): every Win* action passes 0
        // (`li r4, 0` at 0x82341E44 / 0x823419C8 / 0x82341BDC), so E_SCORE_TYPE_TIME is
        // correct for ALL callers of this helper -- the earlier "re-verify per-action"
        // FLAG is resolved.
        lpStreetManager->UpdateTrophyUnlockOnRoadRuleWin( BrnStreetData::E_SCORE_TYPE_TIME );

        const int32_t liNumberOfShowTimeRoadsRuled =
            lpStreetManager->GetNumberOfParShowTimeRoadsRuledByLocalPlayer();
        const int32_t liNumberOfTimeTrialRoadsRuled =
            lpStreetManager->GetNumberOfParTimeTrialRoadsRuledByLocalPlayer();
        const int32_t liNumberOfCompleteRoadsRuled =
            lpStreetManager->GetNumberOfCompleteRoadsRuledByLocalPlayer();
        const int32_t liTotalNumberOfRoads = lpStreetManager->GetStreetData()->GetRoadCount();

        if ( liNumberOfShowTimeRoadsRuled > liTotalNumberOfRoads )
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "liNumberOfShowTimeRoadsRuled <= liTotalNumberOfRoads",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE_BRNGAMESTATESTREETMANAGER,
                2807));
            CgsDev::Assert::EndAssert();
        }
        if ( liNumberOfTimeTrialRoadsRuled > liTotalNumberOfRoads )
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "liNumberOfTimeTrailRoadsRuled <= liTotalNumberOfRoads",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE_BRNGAMESTATESTREETMANAGER,
                2808));
            CgsDev::Assert::EndAssert();
        }

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

    if ( lpData == NULL )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpData",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            292));
        CgsDev::Assert::EndAssert();
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStreetManagerDebugCmpt",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            294));
        CgsDev::Assert::EndAssert();
    }
    if ( lpStreetManagerDebugCmpt->mpStreetManager == NULL )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStreetManagerDebugCmpt->mpStreetManager",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            295));
        CgsDev::Assert::EndAssert();
    }

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

    if ( lpData == NULL )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpData",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            207));
        CgsDev::Assert::EndAssert();
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStreetManagerDebugCmpt",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            209));
        CgsDev::Assert::EndAssert();
    }
    if ( lpStreetManagerDebugCmpt->mpStreetManager == NULL )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStreetManagerDebugCmpt->mpStreetManager",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            210));
        CgsDev::Assert::EndAssert();
    }

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

    if ( lpData == NULL )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpData",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            255));
        CgsDev::Assert::EndAssert();
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStreetManagerDebugCmpt",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            257));
        CgsDev::Assert::EndAssert();
    }
    if ( lpStreetManagerDebugCmpt->mpStreetManager == NULL )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStreetManagerDebugCmpt->mpStreetManager",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            258));
        CgsDev::Assert::EndAssert();
    }

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

// FOLDED FROM BrnStreetManagerDebugComponent_wO_01.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnStreetManagerDebugComponent_wO_01.cpp
//   (wave O partfile -- the TU's last un-bodied function)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManagerDebugComponent::Update  @ 0x8234A7E0
//
// This is the per-frame pump StreetManager::Update calls on the embedded debug
// component. It is a pure no-op unless the "Send Scores to Network" debug action
// has armed mbOutputRoadRulesScoresToNetwork; when it has, it republishes the
// local road-rules score summary onto the output buffer's variable game-action
// queue (the same 16-byte type-232 action the three committed StreetManager
// producers post in BrnGameStateStreetManager_wC_03.cpp) and disarms the flag.
//
// The parked note that once blocked this function ("un-named ProgressionManager /
// Profile members Profile+0x1CD1C / Profile+0x1CD30 / ProgressionManager+0x1D0")
// is obsolete: every one of those offsets now has a committed named accessor, and
// the last of the three was MISATTRIBUTED -- 0x1D0 off the ProgressionManager is
// 0x170 (the embedded Profile) + 0x60, i.e. the profile's own
// muTimeStampOfLastRoadRulesDownload (BrnProfile.h:463/:491), which the X360
// folded into a single `lwz r10,0x1D0(r10)`.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"  // StreetManager::GetProgressionManager (:450)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer::GetGameActionQueue / GetGuiOutputQueue
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::GetProfile (:103)
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::GetRoadRulesID / GetNetworkChallengeDownloadTimestamp
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<13312,16>::AddEvent / CgsModule::Event
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnGameState
{
    namespace
    {
        // The 16-byte road-rules score-summary game action (type 232). Copied
        // verbatim from the committed precedent in
        // BrnGameStateStreetManager_wC_03.cpp (an anonymous-namespace type is
        // local to its TU, so this duplicate is a distinct type -- no ODR issue;
        // re-homing both onto the DWARF-named
        // GameStateModuleIO::RoadRulesChallengeScoresAction is a later,
        // both-sides-at-once job). The X360 builds it as three contiguous stack
        // slots -- the profile's road-rules session id (8B), the download
        // timestamp (4B) and the posting StreetManager (4B) -- and hands the
        // leading address to VariableEventQueue<13312,16>::AddEvent(&payload, 232, 16).
        //
        // HOST-vs-CONSOLE (gotcha 1). The X360 immediate is 16 because 32-bit
        // pointers pack mpStreetManager at +12. MEASURED on the x64 gate: the
        // pointer aligns to +16 and the struct is sizeof == 24. Posting the
        // console 16 here would copy the u64, the u32 and 4 bytes of padding and
        // DROP THE MANAGER POINTER ENTIRELY, so the byte count is sizeof(lSummary)
        // and the console value survives only in this comment. The payload is
        // zero-initialised so the alignment hole is never an indeterminate read.
        // The event id 232 IS verbatim from the asm.
        struct RoadRulesScoreSummaryPayload
        {
            u64            mu64RoadRulesID;       // +0
            u32            muDownloadTimestamp;   // +8
            StreetManager* mpStreetManager;       // +12 (X360)
        };
    }

    // -----------------------------------------------------------------------
    // @ 0x8234A7E0. Per-frame pump. The whole body sits under the armed flag
    // (`lbz r11,0x14(r30); beq -> exit`), and the flag is cleared INSIDE the
    // guard (`stb 0 -> 0x14(r30)` after the AddEvent), so the summary is posted
    // exactly once per press of the debug action.
    //
    // MEASURED: the two queue fetches are two separate `bl` to the SAME X360
    // symbol (0x8231D4B8 -- OutputBuffer::GetGameActionQueue, whose exported name
    // is clipped mid-`OutputBuffer` to `BrnGameState::GameStateModuleIO::Ou` (35
    // chars). Identified by its this+4 return and its "Not locked for writing"
    // assert at BrnGameStateModuleIO.h:266; already homed at
    // BrnGameStateModuleIO.h:468.) The first is the assert subject and is
    // spelled with the name its assert string carries; the second feeds AddEvent
    // and is spelled GetGuiOutputQueue(), the concrete-typed twin of that same
    // accessor (BrnGameStateModuleIO.h:492) -- the forward-declared incomplete
    // GameActionQueue cannot be called through. Same function, two spellings,
    // exactly as the wC_03 producers do it.
    // -----------------------------------------------------------------------
    void StreetManagerDebugComponent::Update( GameStateModuleIO::OutputBuffer* lpOutput )
    {
        if ( mbOutputRoadRulesScoresToNetwork )
        {
            CGS_ASSERT_AT( lpOutput != NULL, "lpOutput", CGS_ARTIST_FILE, 111 );
            CGS_ASSERT_AT( lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()", CGS_ARTIST_FILE, 112 );
            CGS_ASSERT_AT( mpStreetManager->GetProgressionManager(),
                        "mpStreetManager->mpProgressionManager", CGS_ARTIST_FILE, 113 );
            // The X360 spells this null-check `addic. r11,pm,0x170` -- the folded
            // `&pm->mProfile != NULL` of the embedded-by-value profile.
            CGS_ASSERT_AT( mpStreetManager->GetProgressionManager()->GetProfile(),
                        "mpStreetManager->mpProgressionManager->GetProfile()", CGS_ARTIST_FILE, 114 );

            BrnProgression::Profile* lpProfile = mpStreetManager->GetProgressionManager()->GetProfile();

            RoadRulesScoreSummaryPayload lSummary = {};
            // `lwzx` the high half (profile+0x1CD30) and the low half
            // (profile+0x1CD1C), then `extldi/add` -- literally GetRoadRulesID().
            lSummary.mu64RoadRulesID     = lpProfile->GetRoadRulesID();
            lSummary.muDownloadTimestamp = lpProfile->GetNetworkChallengeDownloadTimestamp();
            lSummary.mpStreetManager     = mpStreetManager;

            lpOutput->GetGuiOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lSummary ), 232, sizeof( lSummary ) );

            mbOutputRoadRulesScoresToNetwork = false;
        }
    }
}

#undef CGS_ARTIST_FILE
#undef CGS_ARTIST_FILE_BRNGAMESTATESTREETMANAGER
