// b5-decomp/src/GameSource/GameState/ModeManager/ModeManager_gUI_00.cpp
//
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
//
// TWO ACCESSOR BODIES, landed for a LINK reason measured in the gateui wave's round-2 pass:
// StuntManager::ProcessStuntElement @0x8239CDB0 asserts `mpModeManager->GetScoringSystem()` and
// then calls ScoringSystem::DealWithStunt through it, so `ModeManager::GetScoringSystem` is an
// UNDEF external in StuntManager_gUI_00.obj. Its own TU (BrnModeManager.cpp) is a ~38 KB-object,
// many-hundred-function reconstruction that is NOT mounted and does not body these two, so the
// whole GameState chain would fail to link on a two-line accessor.
//
// ⓘ THE CONSOLE EMITS NO SYMBOL FOR EITHER -- there is no `ModeManager::GetScoringSystem` in
// progress/identity.json or the export set, because the X360 inlines it everywhere: each game-mode
// body reaches the ScoringSystem the ModeManager embeds BY VALUE at ModeManager+0xDB0 as a direct
// `this + 0xDB0` pointer adjust (e.g. OnlineRaceMode::PreWorldUpdate / GetOutroTimeout). The
// declaration in BrnModeManager.h is this repo's de-inlining of that adjust, added so no
// reconstructed body has to poke the byte offset; these are its bodies, and they are exactly the
// adjust and nothing more.
//
// ⚠️ DUPLICATE-SYMBOL WATCH: BrnModeManager.h's comment says "body + real member land with the
// ModeManager TU". If BrnModeManager.cpp is ever bodied WITH these two, delete this partfile (and
// its mount line) rather than letting both exist.
#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // ScoringSystem (embedded by value)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"      // [tut-ticker] GameMode::IsOnline (the clock leg)
#include "GameSource/GameState/BrnGameStateModule.h"                     // [tut-ticker] GameStateModule accessors (the clock leg)
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // [tut-ticker] TrainingManager::IsInPictureParadise
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // [tut-ticker] CarSelectManager::GetJunkyardId
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT

namespace BrnGameState
{

ScoringSystem* ModeManager::GetScoringSystem()
{
    return &mScoringSystem;
}

const ScoringSystem* ModeManager::GetScoringSystem() const
{
    return &mScoringSystem;
}

// ----------------------------------------------------------------------------
// ⭐ [tut-ticker] ConstructInterModeStateBringUp -- the extracted inter-mode seed stores of
// ModeManager::Construct @0x82340008 (sole caller GameStateModule::Construct):
//     *(a1 + 27992) = a2;    // mpGameStateModule (+0x6D58)
//     *(a1 + 3480)  = 0;     // mpCurrentGameMode
//     *(a1 + 3476)  = -1;    // meCurrentGameModeType = E_MODE_NONE
//     *(a1 + 38172) = 0.0;   // mfTimeInFreeBurn
//     *(a1 + 38176) = 0.0;   // mfTimeInMode
//     *(a1 + 38180) = 0.0;   // mfTimeInOnline
// See the header banner for why the E_MODE_NONE store was load-bearing and missing.
// [FLAG PC bring-up] the EXTRACTION is the deviation: the console Construct also seeds the
// checkpoint/scoring state this slice does not model; those stores land with the full TU.
// ----------------------------------------------------------------------------
void ModeManager::ConstructInterModeStateBringUp(GameStateModule* lpGameStateModule)
{
    mpGameStateModule     = lpGameStateModule;
    mpCurrentGameMode     = nullptr;
    meCurrentGameModeType = static_cast<GameStateModuleIO::EGameModeType>(-1);  // E_MODE_NONE
    mfTimeInFreeBurn      = 0.0f;
    mfTimeInMode          = 0.0f;
    mfTimeInOnline        = 0.0f;
}

// ----------------------------------------------------------------------------
// ⭐ [tut-ticker] PreWorldUpdateClocksBringUp -- the extracted clock leg of
// ModeManager::PreWorldUpdate @0x823537B8. The console, in its own if/else:
//   mode running (mpCurrentGameMode != 0):
//       *(a1+38176) += gameTimestep;                       // mfTimeInMode
//       if (mode->mbIsOnline /*+172*/) *(a1+38180) += dt;  // mfTimeInOnline
//       else                           *(a1+38180) = 0.0;
//   no mode running:
//       *(a1+38180) = 0.0;
//       v59 = gsm->mCarSelectManager.mJunkyardId /*gsm+183744, ld*/;
//       if (v59 || gsm->mTrainingManager.mbInPictureParadise /*gsm+46660*/)
//            *(a1+38172) = 0.0;                            // mfTimeInFreeBurn
//       else *(a1+38172) += gameTimestep;
// The timestep the console adds is the pre-world input buffer's timer product
// (*(a4+32) * *(a4+28) == multiplier * base); the caller hands in the same game timestep
// the sibling PreWorldUpdate legs already use.
// [FLAG PC bring-up] the picture-paradise byte is read through the heap TrainingManager the
// PC allocates (see mpTrainingManager's FLAG in BrnGameStateModule.h); same value, named read.
// ----------------------------------------------------------------------------
void ModeManager::PreWorldUpdateClocksBringUp(f32 lfGameTimestep)
{
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");

    if (mpCurrentGameMode != nullptr)
    {
        mfTimeInMode += lfGameTimestep;
        if (mpCurrentGameMode->IsOnline())
        {
            mfTimeInOnline += lfGameTimestep;
        }
        else
        {
            mfTimeInOnline = 0.0f;
        }
    }
    else
    {
        mfTimeInOnline = 0.0f;

        const bool lbInJunkyard =
            (mpGameStateModule->GetCarSelectManager()->GetJunkyardId() != 0);
        const bool lbInPictureParadise =
            (mpGameStateModule->GetTrainingManager() != nullptr) &&
            mpGameStateModule->GetTrainingManager()->IsInPictureParadise();

        if (lbInJunkyard || lbInPictureParadise)
        {
            mfTimeInFreeBurn = 0.0f;
        }
        else
        {
            mfTimeInFreeBurn += lfGameTimestep;
        }
    }
}

}
