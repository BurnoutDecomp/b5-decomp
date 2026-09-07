#include "GameSource/GameState/ModeManager/Debug/BrnModeManagerDebugComponent.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"   // [stuntrace waveB] moved here out
                                                               // of the component header to break the
                                                               // embed cycle; this TU needs the real type.
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, u32 luColour, bool lbJustify);

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The mode-manager debug menu registers a handful of
// mode tunables (its own + the mode manager's + some global marked-man tweaks) and an "end event"
// action with the real CgsDev::DebugComponent debug-menu API (RegisterVariable / SetRange /
// RegisterFunction). The X360 issues these through the (unnamed) DebugComponent registration
// helpers; they are the base-class methods reconstructed in CgsDebugComponent.h, called by name.

namespace BrnGameState
{
    // ARTIST dword_82CDB7BC..82CDB7C8. These are the live marked-man debug
    // tweakables registered by OnActivate, with their exact image initialisers.
    s32 giMarkedManMinOpponentCount = 2;
    s32 giMarkedManMaxOpponentCount = 4;
    f32 gfMarkedManMinRampTime = 5.0f;
    f32 gfMarkedManMaxRampTime = 5.0f;

    void ModeManagerDebugComponent::Construct(ModeManager* lpModeManager)
    {
        CGS_ASSERT(lpModeManager != nullptr, "lpModeManager");
        mpModeManager = lpModeManager;
        mbShowModeInfo = false;
        mbInfiniteLives = false;
        miFinishPosition = 1;
    }

    const char* ModeManagerDebugComponent::GetName() const
    {
        return "Mode Manager";
    }

    void ModeManagerDebugComponent::OnActivate()
    {
        RegisterVariable(mpModeManager->GetScoringSystem()->GetStuntScorer()->GetEndlessStuntRunFlag(),
                         "Endless Stunt Run");
        RegisterVariable(&mbInfiniteLives, "Infinite lives");
        RegisterVariable(&mbShowModeInfo, "Show mode info");
        RegisterVariable(&mpModeManager->mbWinIfSecond, "Win if second");
        RegisterVariable(&giMarkedManMinOpponentCount, "Marked man tweaks", "Min opponent count");
        RegisterVariable(&giMarkedManMaxOpponentCount, "Marked man tweaks", "Max opponent count");
        RegisterVariable(&gfMarkedManMinRampTime, "Marked man tweaks", "Min ramp time");
        RegisterVariable(&gfMarkedManMaxRampTime, "Marked man tweaks", "Max ramp time");
        RegisterVariable(&miFinishPosition, "Finish Position");
        SetRange(&miFinishPosition, 1, 8);
        RegisterFunction(&ModeManagerDebugComponent::FinshMode, this, "End Current Event");
    }

    // X360 0x8231EC30. This deliberately reads the live mode/scorer objects rather than
    // caching values: the original HUD is a direct diagnostic view of the current frame.
    void ModeManagerDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        static const f32 KF_X = 50.0f;
        static const f32 KF_TEXT_SCALE = 16.0f;
        static const u32 KU_TEXT_COLOUR = 0xFFFFFFFFu;

        if (mbShowModeInfo)
        {
            CgsDev::SimpleStrStream lStream;
            const GameMode* lpCurrentMode = mpModeManager->mpCurrentGameMode;

            if (lpCurrentMode != nullptr)
            {
                lStream << lpCurrentMode->GetName() << " state: " << lpCurrentMode->GetCurrentState() << "\n";
                MaybeDrawText(lpRender, lStream.GetBuffer(), KF_X, 100.0f,
                              KF_TEXT_SCALE, KU_TEXT_COLOUR, false);

                lStream.Reset();
                lStream << "Traffic level: " << mpModeManager->mCurrentGameModeParams.mfTrafficDensityScale;
                MaybeDrawText(lpRender, lStream.GetBuffer(), KF_X, 120.0f,
                              KF_TEXT_SCALE, KU_TEXT_COLOUR, false);

                lStream.Reset();
                lStream << "Rank ratio: " << mpModeManager->mCurrentGameModeParams.mfProgressionRankAsRatio;
            }
            else
            {
                lStream << "No mode";
            }

            MaybeDrawText(lpRender, lStream.GetBuffer(), KF_X, 140.0f,
                          KF_TEXT_SCALE, KU_TEXT_COLOUR, false);

            const GameStateModuleIO::EGameModeType leMode = mpModeManager->meCurrentGameModeType;
            bool lbDrawModeLine = false;
            lStream.Reset();

            if (leMode == GameStateModuleIO::E_MODE_ROAD_RAGE)
            {
                lStream << "Madness: " << mpModeManager->mRoadRage.GetRoadRageMadness()
                        << ", madness ratio: " << mpModeManager->mRoadRage.GetRoadRageMadnessRatio();
                lbDrawModeLine = true;
            }
            else if (leMode == GameStateModuleIO::E_MODE_MARKED_MAN)
            {
                lStream << "Opponent count: " << mpModeManager->mSurvivor.GetBroadcastOpponentCount()
                        << ", max ramp timer: " << mpModeManager->mSurvivor.GetMaxRampTimer()
                        << ", ramp timer: " << mpModeManager->mSurvivor.GetRampTimer();
                lbDrawModeLine = true;
            }
            else if (leMode == GameStateModuleIO::E_MODE_STUNT_ATTACK ||
                     leMode == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE ||
                     leMode == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN ||
                     leMode == GameStateModuleIO::E_MODE_ONLINE_MODE_END)
            {
                const StuntModeScoring* lpStuntScoring = mpModeManager->mScoringSystem.GetStuntScorer();
                lStream << "Combo time: " << lpStuntScoring->GetComboActiveTimer()
                        << ", billboards: " << lpStuntScoring->GetBillboardStuntCount();
                lbDrawModeLine = true;
            }

            if (lbDrawModeLine)
            {
                MaybeDrawText(lpRender, lStream.GetBuffer(), KF_X, 160.0f,
                              KF_TEXT_SCALE, KU_TEXT_COLOUR, false);
            }

            if (leMode == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE ||
                leMode == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN ||
                leMode == GameStateModuleIO::E_MODE_ONLINE_MODE_END)
            {
                lStream.Reset();
                const u32 luCarCount = mpModeManager->mScoringSystem.GetNumberOfActiveCars();
                for (u32 luCar = 0; luCar < luCarCount; ++luCar)
                {
                    const CarData* lpCarData = mpModeManager->mScoringSystem.GetCarData(
                        static_cast<EActiveRaceCarIndex>(luCar));
                    if (lpCarData != nullptr)
                    {
                        lStream << lpCarData->GetScoreData()->GetOnlineStuntScore() << " ";
                    }
                }
                MaybeDrawText(lpRender, lStream.GetBuffer(), KF_X, 160.0f,
                              KF_TEXT_SCALE, KU_TEXT_COLOUR, false);
            }
        }

        if (mbInfiniteLives)
        {
            mpModeManager->mScoringSystem.ResetRoadRageCrashesForPlayer();
        }
    }

    // Force the current event to finish at the menu-selected finishing position.
    void ModeManagerDebugComponent::FinshMode(void* lpContext)
    {
        ModeManagerDebugComponent* lpThis = static_cast<ModeManagerDebugComponent*>(lpContext);
        lpThis->mpModeManager->mbFinishCurrentModeNextUpdate = true;
        lpThis->mpModeManager->miDebugFinishPosition = lpThis->miFinishPosition;
    }
}
