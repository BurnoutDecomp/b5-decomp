#include "GameSource/GameState/BrnGameStateDebugComponent.h"

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/Offences/BrnStuntManager.h"
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "SharedClasses/World/BrnWorldRegion.h"
#include "SharedClasses/Trigger/BrnLandmark.h"
#include "SharedClasses/Trigger/BrnRegion.h"
#include "SharedClasses/Trigger/BrnTriggerBase.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"
#include "pc/math/VmxFloat.h"
#include "SDKs/XboxMath/XMVectorACos.h"

#include <cmath>

namespace BrnGameState
{
namespace
{
    // X360 data recovered from the exact bytes at 0x8202A1A0. The matcher reads the first 66
    // words in this order; the final all-ones word at +0x210 is present in the image but is not
    // examined. The lookup fallback is row 66 of the independent name table below.
    const CgsID KAA_DEBUG_ROAD_IDS[67] =
    {
        0x0000000000060F4EULL, 0x0000000000060442ULL, 0x000000000006028CULL, 0x000000000005E3EBULL,
        0x00000000000609BAULL, 0x000000000005FF66ULL, 0x0000000000060B65ULL, 0x00000000000604EAULL,
        0x000000000005E4A7ULL, 0x00000000000606D5ULL, 0x000000000005FDF0ULL, 0x000000000005FFCEULL,
        0x0000000000061061ULL, 0x0000000000060BC4ULL, 0x000000000005E344ULL, 0x000000000005FD54ULL,
        0x000000000005FC83ULL, 0x0000000000060DA2ULL, 0x0000000000060A8DULL, 0x000000000005FF25ULL,
        0x000000000005FCB5ULL, 0x0000000000060284ULL, 0x0000000000060EBCULL, 0x0000000000060B9CULL,
        0x0000000000060F6DULL, 0x00000000000608E2ULL, 0x000000000005E851ULL, 0x000000000006021CULL,
        0x000000000005FDECULL, 0x0000000000060665ULL, 0x0000000000060421ULL, 0x0000000000060B52ULL,
        0x0000000000060AB0ULL, 0x0000000000060CA8ULL, 0x0000000000060220ULL, 0x000000000005FD44ULL,
        0x0000000000060F91ULL, 0x0000000000060DC6ULL, 0x000000000005F643ULL, 0x000000000006108FULL,
        0x0000000000060867ULL, 0x000000000005E2C7ULL, 0x0000000000060CACULL, 0x000000000005DA6BULL,
        0x0000000000060DAEULL, 0x00000000000608A1ULL, 0x000000000005E87EULL, 0x000000000005E2C9ULL,
        0x0000000000061121ULL, 0x000000000005DCA5ULL, 0x0000000000060988ULL, 0x000000000005E6AEULL,
        0x0000000000061186ULL, 0x0000000000060222ULL, 0x00000000000607BDULL, 0x000000000005EAA3ULL,
        0x0000000000060950ULL, 0x000000000005FD8DULL, 0x00000000000602B7ULL, 0x00000000000604F7ULL,
        0x00000000000609D0ULL, 0x0000000000061024ULL, 0x000000000005FFEEULL, 0x00000000000602ACULL,
        0x000000000005FFA0ULL, 0x000000000005E806ULL, 0xFFFFFFFFFFFFFFFFULL
    };

    // X360 char[67][30] at 0x8202A3B8. SHA256 of the recovered 2010 bytes:
    // 592c8050b62dc87035a95632925ed619fa80dbd146a6eaf30647c43c50f1b3fa
    const char KAA_DEBUG_ROAD_NAMES[67][30] =
    {
        "S. BAY EXPRESSWAY",
        "N. MOUNTAIN DRIVE",
        "CROSS PASS",
        "DOYLE ST",
        "8TH STREET",
        "YOUNG AVE",
        "SULLIVAN AVE",
        "RACK WAY",
        "3RD ST",
        "HANS WAY",
        "SHEPHERD AVE",
        "FRANKE AVE",
        "HEALY AVENUE S.",
        "W. CRAWFORD DRIVE",
        "4TH ST",
        "FRY AVE",
        "5TH ST",
        "ROBERTS PKWY",
        "7TH STREET S.",
        "WARREN AVE",
        "6TH ST",
        "NELSON WAY",
        "WEBSTER AVE",
        "HAWLEY AVE",
        "I-88 SECTION 3",
        "S. ROUSE DRIVE",
        "GLANCEY AVE",
        "READ LANE",
        "GABRIEL AVE",
        "S. MOUNTAIN DRIVE",
        "PINE ROAD",
        "NAKAMURA AVE",
        "9TH STREET",
        "MOORE AVE",
        "BAERPASS",
        "EVANS BLVD",
        "I-88 SECTION 4",
        "HEALY AVE N.",
        "1ST ST",
        "I-88 SECTION 1",
        "N. LAKE DRIVE",
        "SPLIT POINT",
        "HUDSON AVE",
        "ANGUS WHARF",
        "PARR AVE",
        "S. LAKE DRIVE",
        "RIVERSIDE AVE",
        "WATT ST",
        "E. CRAWFORD DRIVE",
        "SCHELL WAY",
        "HALL AVE",
        "2ND ST",
        "I-88 SECTION 2",
        "N. ROUSE DRIVE",
        "LUCAS WAY",
        "ROOT AVE",
        "NEWTON DRIVE",
        "PARADISE AVE",
        "CHUBB LANE",
        "SCHEMBRI PASS",
        "7TH STREET N.",
        "HARBER STREET",
        "HAMILTON AVE",
        "UPHILL DRIVE",
        "KING AVE",
        "PATTERSON AVE",
        "_"
    };

    // X360 char[5][20] at 0x8202AB98. This fixed local array is not the separately declared
    // KAPC_COUNTY_NAMES_ALT pointer table. SHA256 of the recovered 100 bytes:
    // f8de432db027bfcf1d12659fbc0450518646aa847bb38ddff9f8da6cbd2d6d6b
    const char KAA_DEBUG_COUNTY_NAMES[5][20] =
    {
        "PALM BAY",
        "SILVER LAKE",
        "HARBOUR TOWN",
        "WHITE MOUNTAIN",
        "DOWNTOWN"
    };

    const char* const KAC_DEBUG_STUNT_COUNT_FORMATS[3] =
    {
        "JUMPS Total = %d, Num completed = %d",
        "SMASHES Total = %d, Num completed = %d",
        "STUNTS Total = %d, Num completed = %d",
    };
}
    // X360 0x823574A0. The console calls the folded base-component reset, then asserts/stores the
    // owner, clears bytes +0x10..+0x16, and clears the 32-bit training-message selector at +0x18.
    // Byte +0x17 and the text buffer are intentionally left untouched, as in ARTIST.
    void GameStateDebugComponent::Construct(GameStateModule* lpGameStateModule)
    {
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

    // ARTIST 0x82375464..0x82375468, inlined in GameStateModule::Destruct.
    void GameStateDebugComponent::Destruct()
    {
        mpGameStateModule = nullptr;
        DebugComponent::Destruct();
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
    // X360 0x82357860 compares a full CgsID (PPC ld/cmpld) against the first 66 table words;
    // the 67th word is not a candidate. A miss selects the final display label at row 66.
    s32 GameStateDebugComponent::FindRoadNameIndexFromId(CgsID lRoadId)
    {
        for (s32 liRoadNameIndex = 0; liRoadNameIndex < 66; ++liRoadNameIndex)
        {
            if (KAA_DEBUG_ROAD_IDS[liRoadNameIndex] == lRoadId)
            {
                return liRoadNameIndex;
            }
        }

        return 66;
    }

    // X360 0x8236C950. It draws the StreetManager's left and right road branches; invalid road
    // indices use CgsID zero, which resolves through FindRoadNameIndexFromId to the '_' row.
    void GameStateDebugComponent::DrawUpcomingRoads(CgsDev::Debug2DImmediateRender* lpRender)
    {
        StreetManager* lpStreetManager = mpGameStateModule->GetStreetManager();
        const BrnStreetData::RoadIndex liLeftRoadIndex = lpStreetManager->mLeftRoadIndex;
        CgsID lLeftRoadId = 0;
        if (liLeftRoadIndex != -1)
        {
            const BrnStreetData::StreetData* lpStreetData = lpStreetManager->GetStreetData();
            lLeftRoadId = lpStreetData->GetRoad(liLeftRoadIndex)->GetId();
        }
        const s32 liLeftRoadNameIndex = FindRoadNameIndexFromId(lLeftRoadId);
        lpRender->DrawText(KAA_DEBUG_ROAD_NAMES[liLeftRoadNameIndex],
                           370.0f, 70.0f, 30.0f, 0xFF14B414u);

        const BrnStreetData::RoadIndex liRightRoadIndex = lpStreetManager->mRightRoadIndex;
        CgsID lRightRoadId = 0;
        if (liRightRoadIndex != -1)
        {
            const BrnStreetData::StreetData* lpStreetData = lpStreetManager->GetStreetData();
            lRightRoadId = lpStreetData->GetRoad(liRightRoadIndex)->GetId();
        }
        const s32 liRightRoadNameIndex = FindRoadNameIndexFromId(lRightRoadId);
        lpRender->DrawText(KAA_DEBUG_ROAD_NAMES[liRightRoadNameIndex],
                           870.0f, 70.0f, 30.0f, 0xFF14B414u);
    }

    // X360 0x82357538. The profile's per-county counts are completed elements; the StuntManager
    // table supplies the total for each of the three original element categories.
    void GameStateDebugComponent::DrawStuntBreakdown(CgsDev::Debug2DImmediateRender* lpRender)
    {
        BrnProgression::Profile* lpProfile =
            mpGameStateModule->GetProgressionManager()->GetProfile();
        StuntManager* lpStuntManager = mpGameStateModule->GetStuntManager();
        char lacCountText[136];

        for (s32 liCounty = 0; liCounty < 5; ++liCounty)
        {
            lpRender->DrawText(KAA_DEBUG_COUNTY_NAMES[liCounty],
                               80.0f, 50.0f + 100.0f * static_cast<f32>(liCounty),
                               16.0f, 0xFFFFFFFFu);

            for (s32 liElementType = 0; liElementType < 3; ++liElementType)
            {
                const BrnGameState::StuntElementType leElementType =
                    static_cast<BrnGameState::StuntElementType>(liElementType);
                const BrnWorld::ECounty leCounty =
                    static_cast<BrnWorld::ECounty>(liCounty);
                const s32 liCompletedCount =
                    lpProfile->GetStuntElementCountByCounty(leElementType, leCounty);

                // The X360 inline getter asserts this bound before reading its signed s16 cell.
                CGS_ASSERT(leCounty < BrnWorld::E_COUNTY_VALID_COUNT,
                           "leCounty < BrnWorld::E_COUNTY_VALID_COUNT");
                const s32 liTotalCount =
                    lpStuntManager->GetTotalStuntElementCountByCounty(leElementType, liCounty);
                CgsCore::SPrintf(lacCountText, 90, KAC_DEBUG_STUNT_COUNT_FORMATS[liElementType],
                                 liTotalCount, liCompletedCount);
                lpRender->DrawText(lacCountText,
                                   80.0f, 70.0f + 100.0f * static_cast<f32>(liCounty) +
                                              20.0f * static_cast<f32>(liElementType),
                                   16.0f, 0xFFFFFFFFu);
            }
        }
    }

    // X360 0x82376D68. Render the mode timer/takedown readouts, then the two opt-in debug panels.
    void GameStateDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        if (mbDrawMiscInfo)
        {
            ModeManager* lpModeManager = mpGameStateModule->GetModeManager();
            ScoringSystem* lpScoringSystem = lpModeManager->GetScoringSystem();
            const CgsSystem::Time lCurrentTime =
                lpModeManager->mTimerStatusInterface.GetSimTimerStatus()->GetTime();

            if (lpScoringSystem->HasModeTimeExpired(lCurrentTime))
            {
                lpRender->DrawText("Come in number 5, your time is up!",
                                   120.0f, 80.0f, 20.0f, 0xFF0000FFu);
            }
            else if (lpScoringSystem->IsTimeLimitActive())
            {
                const CgsSystem::Time lModuleTime =
                    mpGameStateModule->mTimerStatusInterface.GetSimTimerStatus()->GetTime();
                const CgsSystem::Time lTimeRemaining =
                    lpScoringSystem->GetModeTimeRemaining(lModuleTime);
                const s32 liRemainingSeconds = lTimeRemaining.GetSeconds();
                CgsCore::SPrintf(macScoreText, KI_SCORE_TEXT_LENGTH,
                                 "Time remaining %d.%d",
                                 liRemainingSeconds / 60, liRemainingSeconds % 60);
                lpRender->DrawText(macScoreText, 120.0f, 80.0f, 20.0f, 0xFF0000FFu);
            }

            if (lpModeManager->GetCurrentGameModeType() == GameStateModuleIO::E_MODE_ROAD_RAGE)
            {
                const RoadRageModeScoring* lpRoadRageScoring =
                    lpScoringSystem->GetRoadRageScoring();
                CgsCore::SPrintf(macScoreText, KI_SCORE_TEXT_LENGTH,
                                 "Takedowns %d of %d",
                                 lpRoadRageScoring->GetNumTakedownsAchieved(),
                                 lpRoadRageScoring->GetTargetNumTakedowns());
                lpRender->DrawText(macScoreText, 120.0f, 50.0f, 20.0f, 0xFF0000FFu);
            }
        }

        if (mbShowUpComingRoadRules)
        {
            DrawUpcomingRoads(lpRender);
        }
        if (mbDrawStuntBreakdown)
        {
            DrawStuntBreakdown(lpRender);
        }
    }

    // X360 0x8236C840. The player-car crash byte comes from the last active-race-car interface;
    // its inline IsPlayerCarCrashing() reproduces the interface sentinel and state-array read.
    // The streaming wait and both component flags gate the automatic DebugInterface acquisition.
    void GameStateDebugComponent::RenderLandmarkDebug() const
    {
        const bool lbPlayerCarIsCrashing =
            mpGameStateModule->mLastActiveRaceCarInterface.IsPlayerCarCrashing();
        if (lbPlayerCarIsCrashing || mpGameStateModule->mbWaitingForStreaming ||
            (!mbDebugThisLandmark && !mbDebugNextLandmark))
        {
            return;
        }

        CgsDev::DebugInterface lDebugInterface;
        CgsDev::DebugRender& lrRender = lDebugInterface.Get2dRender();
        const CgsDev::DebugUI::Metrics& lrMetrics = lDebugInterface.GetUI().GetMetrics();
        if (mbDebugThisLandmark)
        {
            RenderCurrentLandmarkInfo(lrRender, lrMetrics);
        }
        if (mbDebugNextLandmark)
        {
            RenderNextLandmarkInfo(lrRender, lrMetrics);
        }
    }

    // X360 0x8236C040. The 0xFFFF and 0xFFFE sentinels display None/Multi; otherwise the
    // landmark's design index and the current region's 32-bit ID are shown.
    void GameStateDebugComponent::RenderCurrentLandmarkInfo(
        CgsDev::DebugRender& lrRender,
        const CgsDev::DebugUI::Metrics& lrMetrics) const
    {
        const s32 liRegionIndex =
            static_cast<s32>(mpGameStateModule->GetModeManager()->GetPlayerCurrentLandmark());
        const BrnTrigger::TriggerData* lpTriggerData = mpGameStateModule->GetTrackTriggerData();
        char lacLandmarkText[KI_LANDMARK_TEXT_LENGTH];
        CgsCore::StrCpy(lacLandmarkText, KI_LANDMARK_TEXT_LENGTH, "Landmark: ");

        if (liRegionIndex == -1)
        {
            CgsCore::StrCat(lacLandmarkText, KI_LANDMARK_TEXT_LENGTH, "None");
        }
        else if (liRegionIndex == -2)
        {
            CgsCore::StrCat(lacLandmarkText, KI_LANDMARK_TEXT_LENGTH, "Multi");
        }
        else
        {
            const BrnTrigger::Landmark* lpLandmark =
                lpTriggerData->GetLandmarkFromRegionIndex(liRegionIndex);
            CgsCore::SPrintf(lacLandmarkText + 10, KI_LANDMARK_TEXT_LENGTH - 10,
                             "%d", static_cast<s32>(lpLandmark->GetDesignIndex()));
        }

        const Vector2 lv2Position = { lrMetrics.mfScreenWidth - 30.0f, 145.0f, 0.0f, 0.0f };
        lrRender.Draw2DTextJustified(lacLandmarkText, lv2Position,
                                     CgsDev::DebugRender::E_JUSTIFY_RIGHT,
                                     18.0f, 0xFF0080FFu);
        if (liRegionIndex != -1 && liRegionIndex != -2)
        {
            CgsCore::SPrintf(lacLandmarkText, KI_LANDMARK_TEXT_LENGTH,
                             "ID: %d", liRegionIndex);
            const Vector2 lv2RegionIndexPosition =
                { lrMetrics.mfScreenWidth - 30.0f, 165.0f, 0.0f, 0.0f };
            lrRender.Draw2DTextJustified(lacLandmarkText, lv2RegionIndexPosition,
                                         CgsDev::DebugRender::E_JUSTIFY_RIGHT,
                                         18.0f, 0xFF0080FFu);

            const BrnTrigger::TriggerRegion* lpRegion = lpTriggerData->GetRegion(liRegionIndex);
            CgsCore::SPrintf(lacLandmarkText, KI_LANDMARK_TEXT_LENGTH,
                             "CGSID: %u", static_cast<u32>(lpRegion->GetId()));
            const Vector2 lv2RegionIdPosition =
                { lrMetrics.mfScreenWidth - 30.0f, 185.0f, 0.0f, 0.0f };
            lrRender.Draw2DTextJustified(lacLandmarkText, lv2RegionIdPosition,
                                         CgsDev::DebugRender::E_JUSTIFY_RIGHT,
                                         18.0f, 0xFF0080FFu);
        }
    }

    // X360 0x8236C3B0. The next landmark is a region-table index in ModeManager's published
    // mode route. ARTIST uses zAxis/wAxis from mPlayerCarTransform and a signed xAxis dot product
    // to place a 60-pixel arrow around (screenWidth-80, 60); its 30-pixel half-length is exact.
    void GameStateDebugComponent::RenderNextLandmarkInfo(
        CgsDev::DebugRender& lrRender,
        const CgsDev::DebugUI::Metrics& lrMetrics) const
    {
        ModeManager* lpModeManager = mpGameStateModule->GetModeManager();
        if (lpModeManager->GetCurrentGameMode() == nullptr)
        {
            return;
        }

        const EGlobalRaceCarIndex lePlayerGlobalRaceCarIndex =
            static_cast<EGlobalRaceCarIndex>(mpGameStateModule->GetPlayerGlobalRaceCarIndex());
        const s32 liNextLandmarkIndex =
            lpModeManager->GetNextLandmarkIndex(lePlayerGlobalRaceCarIndex);
        const u32 luNumLandmarks = lpModeManager->muNumLandmarks;
        if (luNumLandmarks == 0 || static_cast<u32>(liNextLandmarkIndex) >= luNumLandmarks)
        {
            return;
        }

        const s32 liLandmarkRegionIndex = static_cast<s16>(
            lpModeManager->maLandmarkIndices[liNextLandmarkIndex]);
        const BrnTrigger::TriggerData* lpTriggerData = mpGameStateModule->GetTrackTriggerData();
        if (mbDebugThisLandmark)
        {
            const BrnTrigger::Landmark* lpLabelLandmark =
                lpTriggerData->GetLandmarkFromRegionIndex(liLandmarkRegionIndex);
            char lacLandmarkText[336];
            CgsCore::SPrintf(lacLandmarkText, 256, "Next LM: %d (%u/%u)",
                             static_cast<s32>(lpLabelLandmark->GetDesignIndex()),
                             static_cast<u32>(liNextLandmarkIndex + 1), luNumLandmarks);
            const Vector2 lv2LabelPosition =
                { lrMetrics.mfScreenWidth - 30.0f, 125.0f, 0.0f, 0.0f };
            lrRender.Draw2DTextJustified(lacLandmarkText, lv2LabelPosition,
                                         CgsDev::DebugRender::E_JUSTIFY_RIGHT,
                                         18.0f, 0xFF0080FFu);
        }

        const BrnTrigger::Landmark* lpLandmark =
            lpTriggerData->GetLandmarkFromRegionIndex(liLandmarkRegionIndex);
        const Matrix44Affine& lrPlayerCarTransform = mpGameStateModule->mPlayerCarTransform;
        const Vector3 lv3LandmarkPosition = lpLandmark->GetBoxRegion()->GetPosition();
        namespace vmx = CgsPC::VmxFloat;
        Vector3 lv3Displacement = vmx::Subtract(lv3LandmarkPosition, lrPlayerCarTransform.wAxis);
        lv3Displacement.w = 0.0f;

        // ARTIST8236C55C..5C4: one unconditional rsqrt refinement for
        // each normalization, then the original VMX max/min and XDK ACos.
        const Vector3 lv3Forward = vmx::Normalize3Unconditional(lrPlayerCarTransform.zAxis, 1);
        const Vector3 lv3Direction = vmx::Normalize3Unconditional(lv3Displacement, 1);
        const f32 lfLowerClampedDot = vmx::Maximum(vmx::Dot3(lv3Forward, lv3Direction), -1.0f);
        const f32 lfForwardDot = std::isnan(lfLowerClampedDot) || lfLowerClampedDot < 1.0f
            ? lfLowerClampedDot : 1.0f;
        const f32 lfRelativeAngle = XboxMath::XMVectorACos(lfForwardDot);

        // The CRT sin/cos calls return double; ARTIST rounds each result
        // to f32 before the scalar products. The side test uses displacement,
        // not the normalized direction, and vsel has a distinct zero arm.
        const f32 lfSin = static_cast<f32>(std::sin(static_cast<f64>(lfRelativeAngle)));
        const f32 lfCos = static_cast<f32>(std::cos(static_cast<f64>(lfRelativeAngle)));
        const f32 lfRightDot = vmx::Dot3(lrPlayerCarTransform.xAxis, lv3Displacement);
        const f32 lfSide = lfRightDot > 0.0f ? 1.0f : (lfRightDot >= 0.0f ? 0.0f : -1.0f);
        const f32 lfArrowHalfX = (lfSide * lfSin) * 30.0f;
        const f32 lfArrowHalfY = lfCos * 30.0f;
        const Vector2 lv2ArrowCentre =
            { lrMetrics.mfScreenWidth - 80.0f, 60.0f, 0.0f, 0.0f };
        const Vector2 lv2ArrowTip =
        {
            vmx::Subtract(lv2ArrowCentre.x, lfArrowHalfX),
            vmx::Subtract(lv2ArrowCentre.y, lfArrowHalfY),
            0.0f, 0.0f
        };
        const Vector2 lv2ArrowTail =
        {
            vmx::Subtract(vmx::Multiply(lv2ArrowCentre.x, 2.0f), lv2ArrowTip.x),
            vmx::Subtract(vmx::Multiply(lv2ArrowCentre.y, 2.0f), lv2ArrowTip.y),
            0.0f, 0.0f
        };
        lrRender.Draw2DArrow(lv2ArrowTail, lv2ArrowTip, 0xFF00FFFFu);

        // ARTIST8236C710..75C computes the displayed magnitude separately,
        // with two refinements and its final zero selection.
        const f32 lfDistance = vmx::Magnitude3(lv3Displacement);
        char lacDistanceText[336];
        const char* lpcDistancePrefix =
            static_cast<u32>(liNextLandmarkIndex) == luNumLandmarks ? "Finish" : "Next";
        CgsCore::SPrintf(lacDistanceText, 256, "%s: %d M", lpcDistancePrefix,
                         static_cast<s32>(lfDistance));
        const Vector2 lv2DistancePosition =
            { lrMetrics.mfScreenWidth - 50.0f, 105.0f, 0.0f, 0.0f };
        lrRender.Draw2DTextJustified(lacDistanceText, lv2DistancePosition,
                                     CgsDev::DebugRender::E_JUSTIFY_CENTRE,
                                     18.0f, 0xFF0080FFu);
    }

}
