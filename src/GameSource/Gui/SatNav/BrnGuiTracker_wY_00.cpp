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
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"    // BrnGui::CalculateRoute
#include "GameSource/GameState/BrnGameStateTypes.h"      // BrnGameState::LandmarkIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"


namespace BrnGui
{
    // GuiTracker::Update (45 insns). Only while mbRouteDataPending: the leg number is the
    // count of route records already received, the leg runs from tracker record [leg] to
    // record [leg + 1] (each record's whole 16-byte position lane), each end is filled by
    // ContructRouteNodeFromTrackedItem, the record is published, and the pending flag
    // drops (`stb 0, 1(this)`).
    //
    // The console leaves the section-index halves as stack residue.
    void GuiTracker::Update(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        if (!mbRouteDataPending)
        {
            return;
        }

        CalculateRoute lCalculateRoute;

        const s32 liLeg = miNumRouteInfoReceived;
        lCalculateRoute.mu16EventID    = static_cast<u16>(liLeg);           // sth -> +0x44
        lCalculateRoute.maPositions[0] = maTrackerRecords[liLeg].mv3Position;
        lCalculateRoute.maPositions[1] = maTrackerRecords[liLeg + 1].mv3Position;

        ContructRouteNodeFromTrackedItem(&maTrackerRecords[liLeg], &lCalculateRoute, 0);
        ContructRouteNodeFromTrackedItem(&maTrackerRecords[liLeg + 1], &lCalculateRoute, 1);

        lpOutput->AddGuiOutEvent(lCalculateRoute);

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
