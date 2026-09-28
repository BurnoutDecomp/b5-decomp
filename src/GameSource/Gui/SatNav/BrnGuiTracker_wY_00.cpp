// ===================================================================================
// BrnGui::GuiTracker -- the route request (GUI out 494, CalculateRoute)
//   class:BrnGui::GuiTracker, partfile of BrnGuiTracker.cpp
//
// GuiTracker::Update asks the game for one route leg at a time; the answer comes back
// as a 211 route record into RecEvent (BrnGuiTracker.cpp), which re-arms the request
// for the next leg until the whole tracked set is routed.
// ===================================================================================
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"
#include "GameSource/Gui/BrnGuiCache.h"                  // GetLandmarkInfoFromIndex / AtPositionInList
#include "GameSource/Gui/BrnGuiWorldDataController.h"   // the test hook's trigger-data gate
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"    // BrnGui::CalculateRoute
#include "GameSource/GameState/BrnGameStateTypes.h"      // BrnGameState::LandmarkIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the BRN_SATNAV_DIAG witness

#include <cstdlib>   // getenv, std::strtol
#include <cstring>   // std::memset

namespace BrnGui
{
    namespace
    {
        // The console publish is CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<CalculateRoute>,
        // whose whole body is the write-lock assert plus
        //     mOutEvents.AddEvent(&event, T::GetEventType() /*494*/, sizeof(T) /*80*/);
        // FLAG PC-ABI adapter: the tracker is handed that buffer's mOutEvents stand-in
        // (GuiModule::mGuiOutQueue) directly, so this helper IS AddGuiOutEvent<T>'s body minus
        // the lock assert the raw queue has no bit for (the ColourCalibrationScreen precedent).
        // DELETE-WHEN the GUI module owns a real CgsGuiModuleIO::OutputBuffer.
        template <class T>
        void AddGuiOutEvent(CgsModule::VariableEventQueue<18432, 16>* lpQueue, const T& lrEvent)
        {
            CGS_ASSERT(lpQueue != NULL, "lpQueue");
            if (lpQueue != NULL)
            {
                lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrEvent),
                                  lrEvent.GetEventType(), static_cast<s32>(sizeof(T)));
            }
        }

        // [FLAG PC witness -- opt-in BRN_SATNAV_DIAG, first 32] one line per route leg
        // requested: `[satnav] route requested leg <n> types <t0>,<t1> landmarks <id0>,<id1>
        // junctions <j0>,<j1>`.
        void LogRouteRequestWitness(const CalculateRoute& lrRoute)
        {
            static const bool sbDiag = (getenv("BRN_SATNAV_DIAG") != 0);
            static s32 siLinesLeft = 32;

            if (!sbDiag || siLinesLeft <= 0 || CgsDev::Log::gpDebugPrint == 0)
            {
                return;
            }
            --siLinesLeft;

            *CgsDev::Log::gpDebugPrint
                << "[satnav] route requested leg " << static_cast<s32>(lrRoute.mu16EventID)
                << " types " << static_cast<s32>(lrRoute.mePointTypes[0])
                << "," << static_cast<s32>(lrRoute.mePointTypes[1])
                << " landmarks " << static_cast<u64>(lrRoute.maLandmarkIDs[0])
                << "," << static_cast<u64>(lrRoute.maLandmarkIDs[1])
                << " junctions " << static_cast<u32>(lrRoute.muJunctionIDs[0])
                << "," << static_cast<u32>(lrRoute.muJunctionIDs[1]) << "\n";
        }

        // [FLAG PC bring-up TEST HOOK -- OFF BY DEFAULT] BRN_SATNAV_ROUTE_TEST=<a>:<b> (two
        // positions in the trigger data's landmark list, 0 .. landmark count - 1). Every tracked
        // set the offline game publishes holds at most one landmark (all 120 offline events
        // carry 0 or 1 checkpoints), so no offline scenario ever owes a route leg;
        // multi-landmark sets come from the online route screens. Once the trigger data is
        // loaded and nothing is tracked, the hook publishes the two landmarks through the real
        // GuiCache::UpdateTrackerInfo (the offline RefreshMapState publisher), which arms the
        // leg this Update then requests. Fires once per run.
        // UpdateTrackerInfo takes LandmarkIndex values, which are trigger-REGION indices (the
        // lookup matches the landmark's region index, not its list position; the retail data
        // numbers the landmarks' regions from 4670). Each list position is therefore turned
        // into its landmark's region index the way the map's landmark filter builds its list:
        // GuiCache::GetLandmarkInfoAtPositionInList, then the record's landmark half.
        // DELETE-WHEN a harness scenario reaches an online route screen.
        void RunRouteTestHook(GuiCache* lpGuiCache, bool lbTrackingActive)
        {
            static s32 siState = -1;   // -1 unread, 0 off / done, 1 armed
            static s32 saiListPositions[2] = { 0, 0 };
            if (siState == -1)
            {
                siState = 0;
                const char* lpcEnv = getenv("BRN_SATNAV_ROUTE_TEST");
                if (lpcEnv != 0 && lpcEnv[0] != '\0')
                {
                    char* lpcEnd = 0;
                    const long liFirst = std::strtol(lpcEnv, &lpcEnd, 10);
                    if (lpcEnd != lpcEnv && *lpcEnd == ':')
                    {
                        const char* lpcSecond = lpcEnd + 1;
                        const long liSecond = std::strtol(lpcSecond, &lpcEnd, 10);
                        if (lpcEnd != lpcSecond && liFirst >= 0 && liSecond >= 0)
                        {
                            saiListPositions[0] = static_cast<s32>(liFirst);
                            saiListPositions[1] = static_cast<s32>(liSecond);
                            siState = 1;
                        }
                    }
                }
            }

            if (siState != 1 || lbTrackingActive || lpGuiCache == 0)
            {
                return;
            }
            const WorldDataController* lpWorldData = lpGuiCache->GetWorldDataController();
            if (lpWorldData == 0 || !lpWorldData->HasTriggerData())
            {
                return;
            }

            siState = 0;
            const s32 liLandmarkCount = lpWorldData->GetTotalNumberOfLandmarks();
            if (saiListPositions[0] >= liLandmarkCount || saiListPositions[1] >= liLandmarkCount)
            {
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[satnav] TEST HOOK list positions " << saiListPositions[0] << ","
                        << saiListPositions[1] << " outside the landmark list (count "
                        << liLandmarkCount << "), nothing published\n";
                }
                return;
            }

            u16 lau16Landmarks[2];
            for (s32 liEnd = 0; liEnd < 2; ++liEnd)
            {
                GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
                lpGuiCache->GetLandmarkInfoAtPositionInList(saiListPositions[liEnd], &lLandmarkInfo);
                lau16Landmarks[liEnd] = static_cast<u16>(lLandmarkInfo.GetLandmarkIndexHalf());
            }

            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[satnav] TEST HOOK list positions " << saiListPositions[0] << ","
                    << saiListPositions[1] << " publishes landmarks "
                    << static_cast<s32>(lau16Landmarks[0]) << ","
                    << static_cast<s32>(lau16Landmarks[1]) << "\n";
            }
            lpGuiCache->UpdateTrackerInfo(lau16Landmarks, 2);
        }
    }

    // GuiTracker::Update (45 insns). Only while mbRouteDataPending: the leg number is the
    // count of route records already received, the leg runs from tracker record [leg] to
    // record [leg + 1] (each record's whole 16-byte position lane), each end is filled by
    // ContructRouteNodeFromTrackedItem, the record is published, and the pending flag
    // drops (`stb 0, 1(this)`).
    //
    // The console builds the 80-byte record on its stack and never writes
    // maSectionIndices (+0x40..+0x43), so those two halves are residue there. FLAG host
    // deviation: the record is zeroed first, the SendRouteRequestAction precedent.
    void GuiTracker::Update(CgsModule::VariableEventQueue<18432, 16>* lpGuiOutEvents)
    {
        RunRouteTestHook(mpGuiCache, mbTrackingActive);   // [FLAG PC TEST HOOK] see above

        if (!mbRouteDataPending)
        {
            return;
        }

        CalculateRoute lCalculateRoute;
        std::memset(&lCalculateRoute, 0, sizeof(lCalculateRoute));

        const s32 liLeg = miNumRouteInfoReceived;
        lCalculateRoute.mu16EventID    = static_cast<u16>(liLeg);           // sth -> +0x44
        lCalculateRoute.maPositions[0] = maTrackerRecords[liLeg].mv3Position;
        lCalculateRoute.maPositions[1] = maTrackerRecords[liLeg + 1].mv3Position;

        ContructRouteNodeFromTrackedItem(&maTrackerRecords[liLeg], &lCalculateRoute, 0);
        ContructRouteNodeFromTrackedItem(&maTrackerRecords[liLeg + 1], &lCalculateRoute, 1);

        AddGuiOutEvent(lpGuiOutEvents, lCalculateRoute);
        LogRouteRequestWitness(lCalculateRoute);

        mbRouteDataPending = false;
    }

    // GuiTracker::ContructRouteNodeFromTrackedItem (121 insns). Three non-gating asserts,
    // then by the item's icon type:
    //   LANDMARK  -> end type LANDMARK, junction 0, the landmark's CgsID (the `ld` of the
    //                GuiCache::GetLandmarkInfoFromIndex record's +0x10 identity word);
    //   JUNCTION  -> end type JUNCTION, the item's junction id, landmark id 0;
    //   otherwise -> the streamed "can't deal with items of type <n>" assert, nothing written.
    void GuiTracker::ContructRouteNodeFromTrackedItem(const TrackerInfo* lpInTrackedItem,
                                                      CalculateRoute* lpOutCalculateRouteEvent,
                                                      s32 liNodeIndex)
    {
        CGS_ASSERT(lpInTrackedItem != 0, "lpInTrackedItem");
        CGS_ASSERT(lpOutCalculateRouteEvent != 0, "lpOutCalculateRouteEvent");
        CGS_ASSERT((0 <= liNodeIndex) && (CalculateRoute::KI_MAX_POINTS > liNodeIndex),
                   "(0 <= liNodeIndex) && (CalculateRoute::KI_MAX_POINTS > liNodeIndex)");

        switch (lpInTrackedItem->meIconType)
        {
            case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK:
            {
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

                GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
                mpGuiCache->GetLandmarkInfoFromIndex(
                    BrnGameState::LandmarkIndex(lpInTrackedItem->mTargetLandmarkIndex),
                    &lLandmarkInfo);

                lpOutCalculateRouteEvent->mePointTypes[liNodeIndex] =
                    CalculateRoute::E_ROUTE_END_POINT_TYPE_LANDMARK;
                lpOutCalculateRouteEvent->muJunctionIDs[liNodeIndex] = 0;
                lpOutCalculateRouteEvent->maLandmarkIDs[liNodeIndex] = lLandmarkInfo.GetCgsId();
                break;
            }

            case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNCTION:
                lpOutCalculateRouteEvent->mePointTypes[liNodeIndex] =
                    CalculateRoute::E_ROUTE_END_POINT_TYPE_JUNCTION;
                lpOutCalculateRouteEvent->muJunctionIDs[liNodeIndex] =
                    lpInTrackedItem->mTargetJunctionId;
                lpOutCalculateRouteEvent->maLandmarkIDs[liNodeIndex] = 0;
                break;

            default:
                // The console streams the icon type after the text into the assert buffer;
                // the static head is passed through (the house convention for streamed asserts).
                CGS_ASSERT(false, "Route request mechanism can't deal with items of type ");
                break;
        }
    }
}
