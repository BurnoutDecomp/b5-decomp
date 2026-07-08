#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"  // BrnGameState::StreetManager::SetChallengeUserScore
#include "SharedClasses/StreetData/BrnChallengeData.h"                  // ChallengePlayerScoreEntry (Construct / SetScore / SetCarID), ScoreType
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsID, CgsIDCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The in-game debug menu for the road-rules
// StreetManager. Four of this TU's eight X360-attested functions are bodied here -- the ones
// that touch only this component's own members, the committed challenge-score record types, and
// declared StreetManager methods:
//   * GetName                     (0x823175F0) -- the menu label.
//   * OnActivate                  (0x8234A718) -- registers the debug actions + the
//                                                 "number of road rules to win" selector.
//   * SendRoadRulesScoresToNetwork(0x82317600) -- arms the network-upload toggle.
//   * PopulateUserChallengeScores (0x8233F928) -- fills every road with a randomised score.
//
// The remaining four (Update @ 0x8234A7E0 and the three Win* actions @ 0x82341CD8 / 0x82341AC0
// / 0x823418B8) reach the still-deferred StreetManager mpProgressionManager (+0x1D10) /
// Profile / AchievementManager (+133432) member layouts and the game-action output queue, none
// of which have an attested accessor yet; they are declared in the header and left for the pass
// that commits those types (reconstructing them now would require raw-offset pokes into opaque
// classes, which the faithfulness rules forbid).
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

} // namespace BrnGameState
