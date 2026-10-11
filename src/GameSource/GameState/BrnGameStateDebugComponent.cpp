#include "GameSource/GameState/BrnGameStateDebugComponent.h"

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/Offences/BrnStuntManager.h"
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{
    // X360 0x823574A0. The console calls the folded base-component reset, then asserts/stores the
    // owner, clears bytes +0x10..+0x16, and clears the 32-bit training-message selector at +0x18.
    // Byte +0x17 and the text buffer are intentionally left untouched, as in ARTIST.
    void GameStateDebugComponent::Construct(GameStateModule* lpGameStateModule)
    {
        DebugComponent::Construct();
        CGS_ASSERT(lpGameStateModule != nullptr, "lpGameStateModule != NULL");

        mpGameStateModule = lpGameStateModule;
        mbShowUpComingRoadRules = false;
        mbDebugThisLandmark = false;
        mbDebugNextLandmark = false;
        mbDebugRenderFinishLines = false;
        mbDebugCheckpointTimes = false;
        mbDrawStuntBreakdown = false;
        mbDrawMiscInfo = false;
        meTrainingMessage = 0;
    }

    const char* GameStateDebugComponent::GetName() const
    {
        return "Game State"; // X360 0x82357528
    }

    // X360 OnActivate 0x82376AA8. The Hex-Rays int return is the last registration helper's r3;
    // the vtable declaration is void, and the assembly takes only this. Keep all 25 original
    // registrations, their storage owners and their ordering, including rows with no confirmed
    // original consumer below the current image-wide reader evidence.
    void GameStateDebugComponent::OnActivate()
    {
        RegisterVariable(&mbShowUpComingRoadRules, "Show upcoming roads");
        RegisterVariable(&mpGameStateModule->GetCarSelectManager()->mbDEBUG_DisableUnlock,
                         "Disable car unlock movies");
        RegisterVariable(&mpGameStateModule->GetCarSelectManager()->mbDEBUG_UnlockTrophyCarsForTesting,
                         "Unlock trophy cars on each Junkyard visit");
        RegisterVariable(&mpGameStateModule->GetCarSelectManager()->mbDEBUG_UnlockShutdownCarsForTesting,
                         "Unlock shutdown cars on each Junkyard visit");
        RegisterVariable(&mpGameStateModule->GetStuntManager()->mbAlwaysToJumpCameras,
                         "Always do jump cameras");
        RegisterVariable(&mbDrawStuntBreakdown, "Show stunt breakdown");
        RegisterVariable(&mbDebugRenderFinishLines, "Draw finish lines");
        RegisterVariable(&mbDebugNextLandmark, "Draw arrow to next landmark");
        RegisterVariable(&mbDebugThisLandmark, "Debug current landmark");
        RegisterVariable(&mpGameStateModule->mbAllowSnapRaceMode, "Snap race enabled");
        RegisterVariable(&mpGameStateModule->mbDebugLimitSnapRaceStartConditions,
                         "Limit snap race start conditions");
        RegisterVariable(&mpGameStateModule->mbDebugMakeAllTakedownsSignature,
                         "Make all TDs signature");
        RegisterVariable(&mpGameStateModule->muDebugSigTDRegionIndex, "Sig TD region index");
        RegisterVariable(&mpGameStateModule->mbDrawCrashDebugText, "Show crashing debug");
        RegisterVariable(&mpGameStateModule->mbDrawSignatureTakedownRegionText,
                         "Show signature TD region");
        RegisterVariable(&mbDebugCheckpointTimes, "Debug checkpoint times");
        RegisterVariable(&mbDrawMiscInfo, "Show misc info");
        RegisterFunction(&GameStateDebugComponent::ClearTrainingFlagsCallback, this,
                         "Reset training states");
        RegisterFunction(&GameStateDebugComponent::ToggleShowtimeCallback, this,
                         "Toggle Showtime control");

        StreetManager* lpStreetManager = mpGameStateModule->GetStreetManager();
        RegisterVariable(&lpStreetManager->miCurrentPlayerRoadIndex, "Current road index");
        RegisterVariable(&lpStreetManager->miNewestLeftPlayerRoadIndex, "LEFT road index");
        RegisterVariable(&lpStreetManager->miNewestForwardPlayerRoadIndex, "FORWARD road index");
        RegisterVariable(&lpStreetManager->miNewestRightPlayerRoadIndex, "RIGHT road index");
        RegisterVariable(&meTrainingMessage, "Training Message");
        SetRange(&meTrainingMessage, 0, 76);
        RegisterFunction(&GameStateDebugComponent::PlayTrainingMessageCallback, this, "Play Message");
    }

    // X360 0x8236CAB8 tail-calls the owning TrainingManager's debug clear function.
    void GameStateDebugComponent::ClearTrainingFlagsCallback(void* lpData)
    {
        GameStateDebugComponent* lpThis = static_cast<GameStateDebugComponent*>(lpData);
        lpThis->mpGameStateModule->GetTrainingManager()->DEBUG_ClearTrainingFlags();
    }

    // X360 0x823578F8 stores true in GameStateModule::mbToggleShowtimeBehaviour.
    void GameStateDebugComponent::ToggleShowtimeCallback(void* lpData)
    {
        GameStateDebugComponent* lpThis = static_cast<GameStateDebugComponent*>(lpData);
        lpThis->mpGameStateModule->ToggleShowtimeBehaviour();
    }

    // X360 0x82357910 stores the selected type first, then latches the training state to pending.
    void GameStateDebugComponent::PlayTrainingMessageCallback(void* lpData)
    {
        GameStateDebugComponent* lpThis = static_cast<GameStateDebugComponent*>(lpData);
        TrainingManager* lpTrainingManager = lpThis->mpGameStateModule->GetTrainingManager();
        lpTrainingManager->meCurrentTrainingType =
            static_cast<BrnProgression::ETrainingType>(lpThis->meTrainingMessage);
        lpTrainingManager->meTrainingState = E_TRAINING_STATE_PENDING_MESSAGE;
    }
}
