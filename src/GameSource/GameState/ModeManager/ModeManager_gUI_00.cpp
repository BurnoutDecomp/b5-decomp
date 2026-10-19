// b5-decomp/src/GameSource/GameState/ModeManager/ModeManager_gUI_00.cpp
//
// ModeManager support bodies (owning header BrnModeManager.h).
//
// This mounted partfile retains the two GetScoringSystem accessors and the extracted
// PreWorldUpdateClocksBringUp helper. The partial ConstructInterModeStateBringUp route was
// removed when GameStateModule::Construct switched to the full ModeManager::Construct body in
// BrnModeManager_Lifecycle.cpp.
//
// ModeManager::Prepare is separately selected by GameStateModule::Prepare. The full
// ModeManager::PreWorldUpdate body is selected through the current GameState EMM pre-world path.
// No current caller was found for PreWorldUpdateClocksBringUp in this source tree; retain it for
// separate cleanup and do not add it alongside the full pre-world route.
//
// GetScoringSystem is this repository's de-inlining of the console's ScoringSystem member adjust.
// Keep these accessors while mode/scoring bodies call them.
// ============================================================================================
#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // ScoringSystem (embedded by value)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"      // [tut-ticker] GameMode::IsOnline (the clock leg)
#include "GameSource/GameState/BrnGameStateModule.h"                     // [tut-ticker] GameStateModule accessors (the clock leg)
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // [tut-ticker] TrainingManager::IsInPictureParadise
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // [tut-ticker] CarSelectManager::GetJunkyardId
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h" // NetworkRoundManager (mpNetworkRoundManager)
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
// [tut-ticker] PreWorldUpdateClocksBringUp -- the extracted CLOCK leg of
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
// (*(a4+32) * *(a4+28) == multiplier * base); the caller hands in the same game timestep the
// sibling PreWorldUpdate legs already use. [!] Note that the full PreWorldUpdate reads that product
// off its own mTimerStatusInterface member (X360 +28092 * +28096 -- the SIM sub-status), which is
// the same number by a different route.
// [FLAG PC bring-up] the picture-paradise byte is read through the heap TrainingManager the PC
// allocates (see mpTrainingManager's FLAG in BrnGameStateModule.h); same value, named read.
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
