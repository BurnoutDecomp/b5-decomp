// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_Prepare2.cpp
//   BrnGameState::StreetManager::Prepare2  @ 0x823509D8
//   (the DWARF :222 3-param `Prepare`; kept under its X360 symbol name)
//
// ⭐ THE STREET LEG OF GameStateModule::Prepare2 case 2. Two things and nothing else:
//     if (LoadStreetData(this, out, rq)) { SetupParRivals(this, tqm); return 1; }
//     return 0;
//
// SPLIT OUT of BrnGameStateStreetManager.cpp (which also carries the two score-entry
// factories CreateHighScoreEntryFromDown @0x82324A28 and CreateUserScoreEntryFromDown) ON
// PURPOSE, and it is MEASURED (cl /c with the build's own flags + dumpbin /SYMBOLS against the
// defined-symbol set of build\game\obj): mounting that whole TU costs SIX unresolved externals
// -- BrnStreetData::operator++(ScoreType&, int), ChallengeHighScoreEntry::Construct,
// ChallengePlayerScoreEntry::Construct, ChallengeData::SetScore and the two ScoreList
// KAI_MIN_SCORES / KAI_MAX_SCORES tables -- every one of them pulled in by those two factories'
// score-type loops. Prepare2 touches none of it, so this split costs ZERO. Established repo
// pattern (BrnGameStateStreetManager_Prepare.cpp, BrnTriggerQueryManager_Prepare.cpp,
// BrnCarSelectManager_CarChange.cpp). Fold back in when the ChallengeData score family lands.
//
// The body below is MOVED, not copied -- BrnGameStateStreetManager.cpp no longer defines it.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGameState
{

// ---------------------------------------------------------------------------
// ⚠️ [FLAG PC bring-up] NOT A CONSOLE FUNCTION -- the named two-store subset of
// StreetManager::Construct @0x82335978 that this leg cannot run without. The full contract,
// the console attestation, and what it deliberately omits are documented at its declaration
// (BrnGameStateStreetManager.h). It lives in THIS TU because Prepare2 is the only leg that
// needs it; it goes away with the same DELETE-WHEN.
//
// The two asserts are the console's own (BrnGameStateStreetManager.cpp:151 and :153), in the
// console's order -- assert, then store, per pointer.
// ---------------------------------------------------------------------------
void StreetManager::WireOwnerPointers( GameStateModule* lpGameStateModule,
                                       BrnProgression::ProgressionManager* lpProgression )
{
    CGS_ASSERT( lpProgression, "lpProgression" );
    mpProgressionManager = lpProgression;

    CGS_ASSERT( lpGameStateModule, "lpGameStateModule" );
    mpGameStateModule = lpGameStateModule;
}

// @ 0x823509D8. Pure forwarder: the two entry asserts (BrnGameStateStreetManager.cpp:281/:282
// in the console's baked literals), the gated LoadStreetData pump, and -- only once it reports
// complete -- the one-shot SetupParRivals pass. Returns false while the street data is still
// streaming, which is what makes the caller pump it once per frame.
bool StreetManager::Prepare2( GameStateModuleIO::OutputBuffer* lpOutput,
                              CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue,
                              const TriggerQueryManager* lpTriggerQueryManager )
{
    CGS_ASSERT( lpOutput != NULL, "lpOutput" );
    CGS_ASSERT( lpReceiverQueue != NULL, "lpReceiverQueue" );

    if ( !LoadStreetData( lpOutput, lpReceiverQueue ) )
    {
        return false;
    }

    SetupParRivals( lpTriggerQueryManager );
    return true;
}

} // namespace BrnGameState
