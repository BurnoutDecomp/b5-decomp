#include "GameSource/GameState/Progression/BrnProgressionDebugComponent.h"

#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/Progression/BrnProfile.h"
#include "GameSource/GameState/Progression/BrnDerivedCars.h"                       // DerivedCarArray (AddSpecialEventWins)
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"                // CarData
#include "GameSource/GameState/Progression/BrnProgressionRivalData.h"              // RivalData
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                       // GetGameStateModule / mpTriggerQueryManager
#include "GameSource/GameState/BrnGameStateModule.h"                               // GetActivePlayerCarId / GetActivePlayerWheelId
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"       // GetTrafficData / traffic-light region
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"             // road-rules tallies / street data
#include "GameSource/GameState/Offences/BrnStuntManager.h"                         // GetTotalStuntElementCount
#include "SharedClasses/Progression/BrnProgressionData.h"
#include "SharedClasses/Progression/BrnProgressionRankData.h"
#include "SharedClasses/Progression/BrnRival.h"
#include "SharedClasses/Progression/BrnOpponentData.h"
#include "SharedClasses/StreetData/BrnStreetData.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"                      // TrafficData::GetJunctionLogicBoxForTrafficLight
#include "SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"
#include "SharedClasses/World/BrnWorldRegion.h"                                    // WorldRegion::DistrictToString
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"                                     // CgsIDUnCompress
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                         // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

namespace BrnProgression
{
    namespace
    {
        // Names of the runtime game modes, in GameStateModuleIO::EGameModeType order.
        const s32 KI_GAME_MODE_NAME_COUNT = 18;
        const char* const KAPC_GAME_MODE_NAMES[KI_GAME_MODE_NAME_COUNT] =
        {
            "Offline Race", "Face Off", "Offline Showtime", "Road Rage", "Pursuit", "Burning Route",
            "Eliminator", "Stunt Race", "Survivor", "Traffic Attack", "Online Race", "Online Road Rage",
            "Online Team Stunt run", "Online Burning Home Run", "Online Free-for-all stunt",
            "Online Free Burn Lobby", "Online Showtime", "Online Coop stunt",
        };

        // The text buffer the HUD panels build into.
        const s32 KI_HUD_TEXT_LENGTH = 10240;

        // The profile event list shows at most this many events, from miEventStartIndex.
        const u32 KU_PROFILE_EVENTS_SHOWN = 30;

        // Every drive-thru in the world.
        const s32 KI_DRIVE_THRU_COUNT = 35;

        // The four rank bars are stacked this far apart.
        const f32 KF_RANK_BAR_SPACING = 50.0f;

        const ProgressionData* GetLoadedProgressionData(const CgsResource::ResourcePtr<ProgressionData>& lrData)
        {
            return lrData.HasMemoryResource() ? lrData.GetMemoryResource() : 0;
        }

        Vector2 MakeVector2(f32 lfX, f32 lfY)
        {
            Vector2 lv2Result;
            lv2Result.x = lfX;
            lv2Result.y = lfY;
            lv2Result.z = 0.0f;
            lv2Result.w = 0.0f;
            return lv2Result;
        }
    }

    const char* GetModeStringForEventData(u8 lu8EventDataMode)
    {
        const u32 luGameMode = static_cast<u32>(ProgressionManager::GetEvent(lu8EventDataMode));
        if (luGameMode < static_cast<u32>(KI_GAME_MODE_NAME_COUNT))
        {
            return KAPC_GAME_MODE_NAMES[luGameMode];
        }
        if (static_cast<s32>(luGameMode) == -1)
        {
            return "<none>";
        }
        return "<invalid>";
    }

    // The profile panel (completion, spawn and current car, rank, per-mode wins and ranks, road
    // rules, stunt elements, rivals, drive-thrus, events), the rank bars, the profile's events and
    // optionally its rivals; then, each behind its toggle, the event junctions, the rival roster
    // and the car opponent sets. "Par Time Trail Road Rules" reads the show-time tally as well.
    void ProgressionDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        if (mbShowProfile)
        {
            const Profile* lpProfile = &mpProgressionManager->mProfile;

            char lacText[KI_HUD_TEXT_LENGTH];
            CgsDev::StrStream lStream(lacText, sizeof(lacText));

            char lacSpawnCar[16];
            char lacSpawnWheels[16];
            char lacCurrentCar[16];
            char lacCurrentWheels[16];
            CgsIDUnCompress(lpProfile->GetSpawnCarId(), lacSpawnCar);
            CgsIDUnCompress(lpProfile->GetSpawnWheelId(), lacSpawnWheels);
            CgsIDUnCompress(mpModeManager->GetGameStateModule()->GetActivePlayerCarId(), lacCurrentCar);
            CgsIDUnCompress(mpModeManager->GetGameStateModule()->GetActivePlayerWheelId(), lacCurrentWheels);

            lStream << "Completion: ";
            lStream << mpProgressionManager->ComputeCompletionPercentage() << "%\n";
            lStream << "Car to spawn in on boot: ";
            lStream << lacSpawnCar;
            lStream << "\n";
            lStream << "Wheels to spawn in on boot: ";
            lStream << lacSpawnWheels;
            lStream << "\n";
            lStream << "Current Car: ";
            lStream << lacCurrentCar;
            lStream << "\n";
            lStream << "Current Wheels: ";
            lStream << lacCurrentWheels;
            lStream << "\n";

            if (mpProgressionManager->mi8ProgressionRank ==
                static_cast<s32>(mpProgressionManager->mpProgressionData->GetProgressionRankCount()))
            {
                lStream << "Final rank complete\n";
            }
            else
            {
                const s32 liRank = static_cast<s8>(mpProgressionManager->GetProgressionRank());
                lStream << "Current Rank: " << liRank << "\n";
            }

            lStream << "\n Race wins: ";
            lStream << lpProfile->GetNumWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE) << "\n";
            lStream << "Road Rage wins: ";
            lStream << lpProfile->GetNumWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE) << "\n";
            lStream << "Stunt Attack wins: ";
            lStream << lpProfile->GetNumWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK) << "\n";
            lStream << "Marked Man wins: ";
            lStream << lpProfile->GetNumWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN) << "\n";
            lStream << "\n Event Rank Race wins: ";
            lStream << lpProfile->GetNumRankWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE) << "\n";
            lStream << "Event Rank Road Rage wins: ";
            lStream << lpProfile->GetNumRankWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE) << "\n";
            lStream << "Event Rank Stunt Attack wins: ";
            lStream << lpProfile->GetNumRankWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK) << "\n";
            lStream << "Event Rank Marked Man wins: ";
            lStream << lpProfile->GetNumRankWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN) << "\n";

            lStream << "\n Race rank: ";
            const s32 liRaceRank = mpProgressionManager->GetProgressionRankForGameMode(
                BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE);
            lStream << liRaceRank << "\n";
            lStream << "Road Rage rank: ";
            const s32 liRoadRageRank = mpProgressionManager->GetProgressionRankForGameMode(
                BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE);
            lStream << liRoadRageRank << "\n";
            const s32 liStuntRank = mpProgressionManager->GetProgressionRankForGameMode(
                BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK);
            lStream << "Stunt Attack rank: ";
            lStream << liStuntRank << "\n";
            const s32 liMarkedManRank = mpProgressionManager->GetProgressionRankForGameMode(
                BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN);
            lStream << "Marked Man rank: ";
            lStream << liMarkedManRank << "\n";

            BrnGameState::StreetManager* lpStreetManager = mpProgressionManager->mpStreetManager;
            lStream << "\nPar Show Time Road Rules: ";
            lStream << lpStreetManager->GetNumberOfParShowTimeRoadsRuledByLocalPlayer() << "\n";
            lStream << "Par Time Trail Road Rules: ";
            lStream << lpStreetManager->GetNumberOfParShowTimeRoadsRuledByLocalPlayer() << "\n";
            const s32 liRoadCount = lpStreetManager->GetStreetData()->GetRoadCount();
            lStream << "Road Count: ";
            lStream << liRoadCount << "\n";

            const BrnGameState::StuntManager* lpStuntManager = mpProgressionManager->mpStuntManager;
            lStream << "\nBillBoards done: ";
            lStream << lpProfile->GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD) << "\n";
            lStream << "BillBoards count: ";
            lStream << static_cast<s32>(lpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD)) << "\n";
            lStream << "Jumps done: ";
            lStream << lpProfile->GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP) << "\n";
            lStream << "Jumps count: ";
            lStream << static_cast<s32>(lpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP)) << "\n";
            lStream << "Smashes done: ";
            lStream << lpProfile->GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH) << "\n";
            lStream << "Smashes count: ";
            lStream << static_cast<s32>(lpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH)) << "\n";

            lStream << "\nTrue Number Of Rivals : ";
            lStream << mpProgressionManager->GetTrueNumberOfRivals() << "\n";
            lStream << "Number Of Beaten Rivals : ";
            lStream << mpProgressionManager->GetNumberOfBeatenRivals() << "\n";
            lStream << "\nDrive Thru Count : ";
            lStream << KI_DRIVE_THRU_COUNT << "\n";
            lStream << "Drive Thrus Found : ";
            lStream << lpProfile->GetDriveThrusFound() << "\n";

            s32 liDiscoveredEvents = 0;
            for (s32 liEvent = 0; liEvent < lpProfile->miEventCount; ++liEvent)
            {
                if (lpProfile->maEvents[liEvent].IsFlagSet(ProfileEvent::E_FLAG_DISCOVERED))
                {
                    ++liDiscoveredEvents;
                }
            }
            lStream << "\nDiscovered Events : ";
            lStream << liDiscoveredEvents << "\n";
            lStream << "Event Count : ";
            lStream << lpProfile->GetEventCount() << "\n";

            DrawTextWithOffsets(lpDisplay, &lStream, 60.0f, 40.0f);

            Vector2 lv2BarPosition = MakeVector2(1000.0f, 250.0f);
            DrawRankAsBar(lpDisplay, BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE, 0xFFFF0000u, lv2BarPosition, "RACE");
            lv2BarPosition.y += KF_RANK_BAR_SPACING;
            DrawRankAsBar(lpDisplay, BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE, 0xFF0000FFu, lv2BarPosition, "ROAD RAGE");
            lv2BarPosition.y += KF_RANK_BAR_SPACING;
            DrawRankAsBar(lpDisplay, BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN, 0xFF00FFFFu, lv2BarPosition, "MARKED MAN");
            lv2BarPosition.y += KF_RANK_BAR_SPACING;
            DrawRankAsBar(lpDisplay, BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK, 0xFF00FF00u, lv2BarPosition, "STUNT RUN");

            RenderProfileEvents(lpDisplay, lpProfile);
            if (mbShowProfileRivalInfo)
            {
                RenderProfileRivals(lpDisplay, lpProfile);
            }
        }

        if (mbShowRaceData)
        {
            RenderRaceEvents(lpDisplay);
        }

        if (mbShowRivalData)
        {
            char lacText[KI_HUD_TEXT_LENGTH];
            CgsDev::StrStream lStream(lacText, sizeof(lacText));
            lStream.Reset();
            lStream << "Rivals:\n";
            MaybeDrawText(lpDisplay, lStream.GetBuffer(), 32.0f, 46.0f, 12.0f, 0xFFFFFFFFu, false);

            for (s32 liRival = 0; liRival < mpProgressionManager->mpProgressionData->GetRivalCount(); ++liRival)
            {
                const Rival* lpRival = mpProgressionManager->mpProgressionData->GetRival(liRival);

                char lacCarId[16];
                CgsIDUnCompress(lpRival->GetCarId(), lacCarId);
                lStream << "   ";
                lStream << lpRival->GetName();
                lStream << ", CarID: ";
                lStream << lacCarId;
                lStream << ", District: ";
                lStream << BrnWorld::WorldRegion::DistrictToString(lpRival->GetDistrict());
                lStream << "\n";
            }

            DrawTextWithOffsets(lpDisplay, &lStream, 60.0f, 40.0f);
        }

        if (mbShowPlayerOpponents)
        {
            for (u32 luSet = 0; luSet < GetLoadedProgressionData(mpProgressionManager->mpProgressionData)->muCarOpponentsCount; ++luSet)
            {
                const CarOpponentSet& lrSet =
                    GetLoadedProgressionData(mpProgressionManager->mpProgressionData)->GetCarOpponentSets()[luSet];

                CgsDev::SimpleStrStream lStream;
                char lacCarId[16];
                CgsIDUnCompress(lrSet.GetPlayerCarId(), lacCarId);
                lStream << lacCarId;
                lStream << " (";
                for (s32 liOpponent = 0; liOpponent < lrSet.GetOpponentCount(); ++liOpponent)
                {
                    CGS_ASSERT(liOpponent >= 0 && liOpponent < lrSet.GetOpponentCount(),
                               "liCarOpponentIndex >= 0 && liCarOpponentIndex < miOpponentCount");
                    CgsIDUnCompress(lrSet.GetCarOpponent(liOpponent)->GetCarId(), lacCarId);
                    lStream << lacCarId;
                    lStream << " ";
                }
                lStream << ")";

                MaybeDrawText(lpDisplay, lStream.GetBuffer(), 100.0f,
                              static_cast<f32>(luSet) * 20.0f + 100.0f, 16.0f, 0xFFFFFFFFu, false);
            }
        }
    }

    // One cheat-awarded special event per call to the count: every discovered special event gets
    // its car family unlocked and its event marked won, until the requested number is spent.
    void ProgressionDebugComponent::AddSpecialEventWins()
    {
        Profile* lpProfile = &mpProgressionManager->mProfile;
        CGS_ASSERT(lpProfile, "lpProfile");

        ++lpProfile->muMedalCountFromTheStart;
        mpProgressionManager->mbPlayerMedalsUpdateRequired = true;
        DefeatAllRivals();

        s32 liWinsToAdd = miNumberOfSpecialEventWinsToAdd;
        s32 liWinsAdded = 0;

        for (u32 luEvent = 0; luEvent < static_cast<u32>(lpProfile->miEventCount); ++luEvent)
        {
            CGS_ASSERT(luEvent < static_cast<u32>(lpProfile->miEventCount), "luIndex < static_cast<uint32_t>(miEventCount)");
            ProfileEvent& lrEvent = lpProfile->maEvents[luEvent];

            const ProgressionData* lpProgressionData = GetLoadedProgressionData(mpProgressionManager->mpProgressionData);
            const RaceEventData* lpRaceEventData = lpProgressionData->FindOfflineEvent(lrEvent.GetID());
            CGS_ASSERT(lpRaceEventData, "lpRaceEventData");

            if (lpRaceEventData->GetSpecialEventCarId() == 0)
            {
                continue;
            }

            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Adding event: " << liWinsAdded << "\n";
            }

            const CgsID lPlayerCarCgsID = lpRaceEventData->GetSpecialEventCarId();
            ++liWinsAdded;
            CGS_ASSERT(lPlayerCarCgsID != 0, "lPlayerCarCgsID != kCGSID_NULL");

            DerivedCarArray lDerivedCars;
            lDerivedCars.ConstructPatternLiveryList(mpProgressionManager->mpVehicleList, lPlayerCarCgsID);
            mpProgressionManager->UnlockDerivedCarCollection(lDerivedCars);

            lrEvent.SetFlags(static_cast<u16>(lrEvent.GetFlags() | ProfileEvent::E_FLAG_DISCOVERED |
                                              ProfileEvent::E_FLAG_RANK_WIN | ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE));

            for (u32 luCar = 0; luCar < lDerivedCars.GetLength(); ++luCar)
            {
                const CgsID lCarId = lDerivedCars.GetItem(luCar);
                for (s32 liProfileCar = 0; liProfileCar < lpProfile->miCarCount; ++liProfileCar)
                {
                    if (lpProfile->maCars[liProfileCar].GetId() == lCarId)
                    {
                        CarData* lpCar = &lpProfile->maCars[liProfileCar];
                        if (lpCar != 0)
                        {
                            lpCar->SetUnlockSequenceAlreadyShown();
                        }
                        break;
                    }
                }
            }

            --liWinsToAdd;
            if (liWinsToAdd == 0)
            {
                break;
            }
        }
    }

    // Every event junction with an offline event (id and mode), then, while the player is in a
    // traffic-light region, the offline and online event of the junction that light belongs to.
    void ProgressionDebugComponent::RenderRaceEvents(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        char lacText[KI_HUD_TEXT_LENGTH];
        CgsDev::StrStream lStream(lacText, sizeof(lacText));
        lStream.Append("Events:\n");

        const ProgressionData* lpProgressionData = GetLoadedProgressionData(mpProgressionManager->mpProgressionData);
        CgsDev::StrStreamBase& lrStream = lStream;

        for (u32 luJunction = 0; luJunction < lpProgressionData->muEventJunctionCount; ++luJunction)
        {
            const RaceEventData* lpOfflineEvent = lpProgressionData->GetEventJunctions()[luJunction].GetOfflineEvent();
            if (lpOfflineEvent == 0)
            {
                continue;
            }

            const char* lpcMode = GetModeStringForEventData(lpOfflineEvent->GetMode());
            const u32 luJunctionId = lpProgressionData->GetEventJunction(luJunction)->GetID();
            lStream << "ID:  ";
            lrStream << luJunctionId;
            lStream << "   ";
            lStream << lpcMode;
            lStream << "\n";
        }

        DrawTextWithOffsets(lpDisplay, &lStream, 50.0f, 50.0f);
        lStream.Reset();

        const BrnGameState::TriggerQueryManager* lpTriggerQueryManager = mpModeManager->GetTriggerQueryManager();
        const BrnTraffic::TrafficData* lpTrafficData = lpTriggerQueryManager->GetTrafficData();
        if (lpTriggerQueryManager != 0 && lpTriggerQueryManager->IsPlayerInTrafficLightRegion())
        {
            const BrnTraffic::JunctionLogicBox* lpJunction =
                lpTrafficData->GetJunctionLogicBoxForTrafficLight(lpTriggerQueryManager->GetPlayerCurrentTrafficLightId());
            lStream << "*** Junction Event Trigger ***\n\n";

            const char* lpcTail;
            if (lpJunction == 0 || lpJunction->GetEventJunctionID() == static_cast<u32>(-1))
            {
                lpcTail = "No Event Data\n";
            }
            else
            {
                const RaceEventData* lpOfflineEvent = 0;
                for (u32 luJunction = 0; luJunction < lpProgressionData->muEventJunctionCount; ++luJunction)
                {
                    if (lpProgressionData->GetEventJunctions()[luJunction].GetID() == lpJunction->GetEventJunctionID())
                    {
                        lpOfflineEvent = lpProgressionData->GetEventJunctions()[luJunction].GetOfflineEvent();
                        break;
                    }
                }
                if (lpOfflineEvent != 0)
                {
                    RenderRaceEventData(lpDisplay, lpOfflineEvent, lStream);
                }
                else
                {
                    lStream << "\n### The offline event could not be found! ###\n";
                }

                const RaceEventData* lpOnlineEvent = 0;
                for (u32 luJunction = 0; luJunction < lpProgressionData->muEventJunctionCount; ++luJunction)
                {
                    if (lpProgressionData->GetEventJunctions()[luJunction].GetID() == lpJunction->GetEventJunctionID())
                    {
                        lpOnlineEvent = lpProgressionData->GetEventJunctions()[luJunction].GetOnlineEvent();
                        break;
                    }
                }
                if (lpOnlineEvent != 0)
                {
                    lStream << "\n";
                    RenderRaceEventData(lpDisplay, lpOnlineEvent, lStream);
                }
                else
                {
                    lStream << "\n### The online event could not be found! ###\n";
                }
                lpcTail = "\n";
            }

            lStream << lpcTail;
        }

        DrawTextWithOffsets(lpDisplay, &lStream, 250.0f, 200.0f);
    }

    // The event's mode and, for a race, its destination landmark, crash-breaker flag and rival
    // counts.
    void ProgressionDebugComponent::RenderRaceEventData(CgsDev::Debug2DImmediateRender* /*lpDisplay*/,
                                                        const RaceEventData* lpRaceEventData,
                                                        CgsDev::StrStream& lrStream)
    {
        const u8 lu8Mode = lpRaceEventData->GetMode();
        lrStream << "Offline Event: ";
        lrStream << GetModeStringForEventData(lu8Mode);
        lrStream << "\n";

        if (lpRaceEventData->GetMode() == RaceEventData::E_MODE_RACE)
        {
            const u32 luLandmarkId = lpRaceEventData->GetCheckpointData(0)->GetLandmarkId();
            lrStream << "Destination Landmark: LM_" << luLandmarkId << "\n";
            if (lpRaceEventData->GetFlag(RaceEventData::E_FLAG_CRASHBREAKER))
            {
                lrStream << "Crash Breaker: True\n";
            }
            else
            {
                lrStream << "Crash Breaker: False\n";
            }
            lrStream << "Start Rivals: " << static_cast<s32>(lpRaceEventData->GetStartRivalCount()) << "\n";
            lrStream << "Add Rivals: " << static_cast<s32>(lpRaceEventData->GetAddRivalCount()) << "\n";
        }
    }

    // Rank, medal totals and, when toggled, a 30-event window of the profile's events with their
    // medal letters: G / S / B for the rank-win, non-rank-win and special-win flags, f finished,
    // d discovered.
    void ProgressionDebugComponent::RenderProfileEvents(CgsDev::Debug2DImmediateRender* lpDisplay, const Profile* lpProfile)
    {
        char lacText[KI_HUD_TEXT_LENGTH];
        CgsDev::StrStream lStream(lacText, sizeof(lacText));
        const u32 luEventCount = static_cast<u32>(lpProfile->miEventCount);
        lStream.Append("-------------------------------------------------\n");

        if (mpProgressionManager->mi8ProgressionRank ==
            static_cast<s32>(mpProgressionManager->mpProgressionData->GetProgressionRankCount()))
        {
            lStream << "Rank progression complete\n\n";
        }
        else
        {
            const s32 liRank = static_cast<s8>(mpProgressionManager->GetProgressionRank());
            const ProgressionData* lpProgressionData = GetLoadedProgressionData(mpProgressionManager->mpProgressionData);
            const u32 luRankCount = lpProgressionData->GetProgressionRankCount();
            lStream << "Current Rank: ";
            lStream << liRank << "/" << luRankCount << "\n";
            const s32 liNextRankMedals =
                lpProgressionData->GetProgressionRankData(static_cast<u32>(liRank))->GetMedalThresholdToNextRank();
            lStream << "Next rank at ";
            lStream << liNextRankMedals << " medals\n";
        }

        u32 luGoldWins;
        u32 luSilverWins;
        u32 luBronzeWins;
        const u32 luTotalWins = lpProfile->GetTotalWinCount(luGoldWins, luSilverWins, luBronzeWins);
        const u32 luMedalsFromTheStart = lpProfile->muMedalCountFromTheStart;
        lStream << "Medals: G: ";
        lStream << luGoldWins << " S: " << luSilverWins << " B: " << luBronzeWins << " ( " << luTotalWins << " )\n";
        lStream << "Medal Wins From The Start: ";
        lStream << luMedalsFromTheStart << "\n";
        lStream << "\nEvents:\n-------------------------------------------------\n";

        if (mbShowProfileEventInfo)
        {
            u32 luFirst;
            if (luEventCount <= KU_PROFILE_EVENTS_SHOWN)
            {
                luFirst = 0;
            }
            else
            {
                s32 liFirst = miEventStartIndex;
                if (!(liFirst > 0))
                {
                    liFirst = 0;
                }
                luFirst = static_cast<u32>(liFirst);
                if (luFirst > luEventCount - KU_PROFILE_EVENTS_SHOWN)
                {
                    luFirst = luEventCount - KU_PROFILE_EVENTS_SHOWN;
                }
            }

            u32 luEnd = luFirst + KU_PROFILE_EVENTS_SHOWN;
            if (luEnd > luEventCount)
            {
                luEnd = luEventCount;
            }

            CgsDev::StrStreamBase& lrStream = lStream;
            for (u32 luEvent = luFirst; luEvent < luEnd; ++luEvent)
            {
                CGS_ASSERT(luEvent < static_cast<u32>(lpProfile->miEventCount), "luIndex < static_cast<uint32_t>(miEventCount)");
                const ProfileEvent* lpEvent = &lpProfile->maEvents[luEvent];
                CGS_ASSERT(lpEvent, "lpEvent");

                char lacMedal[6] = { '.', '.', '.', '.', '.', '\0' };
                if (lpEvent->IsFlagSet(ProfileEvent::E_FLAG_RANK_WIN))
                {
                    lacMedal[0] = 'G';
                }
                if (lpEvent->IsFlagSet(ProfileEvent::E_FLAG_NON_RANK_WIN))
                {
                    lacMedal[1] = 'S';
                }
                if (lpEvent->IsFlagSet(ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE))
                {
                    lacMedal[2] = 'B';
                }
                if (lpEvent->IsFlagSet(ProfileEvent::E_FLAG_FINISHED))
                {
                    lacMedal[3] = 'f';
                }
                if (lpEvent->IsFlagSet(ProfileEvent::E_FLAG_DISCOVERED))
                {
                    lacMedal[4] = 'd';
                }

                lStream << "ID: ";
                lrStream << lpEvent->GetID();
                lStream << "  Medal: ";
                lStream << lacMedal;
                lStream << "\n";
            }
        }

        DrawTextWithOffsets(lpDisplay, &lStream, 250.0f, 30.0f);
    }

    // Every rival the profile has unlocked: id, car, unlock rank, state, district and the
    // takedowns needed to beat it.
    void ProgressionDebugComponent::RenderProfileRivals(CgsDev::Debug2DImmediateRender* lpDisplay, const Profile* lpProfile)
    {
        char lacText[KI_HUD_TEXT_LENGTH];
        CgsDev::StrStream lStream(lacText, sizeof(lacText));
        CgsDev::StrStreamBase& lrStream = lStream;
        lStream.Append("Unlocked Rivals:\n----------------------------------\n");

        for (s32 liRival = 0; liRival < lpProfile->GetRivalCount(); ++liRival)
        {
            const RivalData* lpRivalData =
                (liRival >= 0 && liRival < lpProfile->GetRivalCount()) ? lpProfile->GetRivalData(liRival) : 0;

            const Rival* lpRival = mpProgressionManager->mpProgressionData->FindRival(lpRivalData->mRivalId);
            const RivalData::EState leState = lpRivalData->meState;
            CGS_ASSERT(lpRival, "lpRival");

            if (leState == RivalData::E_STATE_LOCKED)
            {
                continue;
            }

            const char* lpcState;
            switch (leState)
            {
            case RivalData::E_STATE_UNLOCKED: lpcState = "Unlocked"; break;
            case RivalData::E_STATE_FLEEING:  lpcState = "Fleeing";  break;
            case RivalData::E_STATE_BEATEN:   lpcState = "Beaten";   break;
            default:                          lpcState = "ERROR";    break;
            }

            char lacCarId[16];
            CgsIDUnCompress(lpRivalData->mCarId, lacCarId);

            lrStream << static_cast<u64>(lpRivalData->mRivalId);
            lStream << "  ";
            lStream << lacCarId;
            lStream << "  R";
            lrStream << static_cast<s32>(static_cast<u8>(lpRival->GetUnlockRank()));
            lStream << "  ";
            lStream << lpcState;
            lStream << "   -  ";
            lStream << BrnWorld::WorldRegion::DistrictToString(lpRival->GetDistrict());
            lStream << " (";
            lrStream << static_cast<s32>(static_cast<u16>(lpRival->GetPursuitTarget()));
            lStream << " TDs to win)\n";
        }

        DrawTextWithOffsets(lpDisplay, &lStream, 550.0f, 30.0f);
    }
}
