// =================================================================================================
// GameSource/Gui/BrnGuiCache_wMap.cpp -- the MAP-ARM cache surface (map arm 2026-08-27).
//
// The GuiCache half of the pre-race fly-by / crash-nav MAP arm: the landmark-info fills, the
// active-landmark latch, and the three tracker-refresh producers that publish the id-232
// SetTracker event to the GuiTracker. Landing these retired five of the ten stand-ins in
// BrnMainMapLinkGates.cpp (GetLandmarkInfoFromIndex / GetLandmarkInfoFromID /
// GetEventDestinationLandmarkIndex / RefreshMapState / HACK_...SetActiveLandmarksByEventID).
//
// Sources: X360 ARTIST asm/pseudocode per-function (addresses below); the SetTracker record
// geometry is decoded in the BrnGuiTracker.h banner. GetEventDestinationLandmarkIndex is the
// finished body MOVED from the unmounted BrnGuiCache_wB_res.cpp (the gates file's own
// prescription -- mounting wB_res whole was measured net-negative).
// =================================================================================================

#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/BurnoutConstants.h"                 // E_ACTIVE_RACE_CAR_INDEX_INVALID
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"          // GuiEventUpdateSatNav::SatNavIconInfo
#include "GameSource/Gui/BrnGuiWorldDataController.h"    // WorldDataController (landmark/event lookups)
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"         // GuiTracker (the 232 SetTracker publish)
#include "GameSource/GameState/BrnGameStateTypes.h"      // BrnGameState::LandmarkIndex
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"        // (map-space vocabulary; comments only)
#include "SharedClasses/Progression/BrnRaceEventData.h"  // RaceEventData (checkpoint walk)
#include "SharedClasses/Trigger/BrnLandmark.h"           // BrnTrigger::Landmark
#include "SharedClasses/World/BrnWorldRegion.h"          // WorldRegion::DistrictToCounty
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

namespace
{
    // Shared field-fill of a caller's SatNavIconInfo from a resolved landmark -- the common
    // tail of GetLandmarkInfoFromIndex @0x82506688 and GetLandmarkInfoFromID @0x825067E0
    // (identical store runs; only the landmark-index source differs, so it is a parameter).
    // Store map (console record offsets in the trailing comments):
    void FillIconInfoFromLandmark(const BrnTrigger::Landmark* lpLandmark,
                                  s16 liLandmarkIndex,
                                  BrnGui::GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo)
    {
        // The position lane {x, y, z, 0} (lfs x3 + stvx128 -> out+0x00).
        Vector4 lv4Position;
        const Vector3 lv3Landmark = lpLandmark->GetBoxRegion()->GetPosition();
        lv4Position.x = lv3Landmark.x;
        lv4Position.y = lv3Landmark.y;
        lv4Position.z = lv3Landmark.z;
        lv4Position.w = 0.0f;
        lpOutIconInfo->SetPositionLane(lv4Position);

        lpOutIconInfo->SetRotation(0.0f);      // stfs 0 -> +0x18
        lpOutIconInfo->SetSpeedMph(0.0f);      // stfs 0 -> +0x1C

        // The id: `lwz lm+0x24 / extsw / std -> +0x10` == the sign-extended TriggerRegion id,
        // exactly what the committed GetId() accessor produces (s32 -> CgsID).
        lpOutIconInfo->SetCgsId(lpLandmark->GetId());

        // District first (stb lm+0x32 -> +0x25; SetDistrict carries the console's own
        // `leDistrict >= 0` assert), then county = DistrictToCounty(district) (its own
        // `leCounty >= 0` assert rides SetCounty).
        lpOutIconInfo->SetDistrict(static_cast<BrnWorld::EDistrict>(lpLandmark->GetDistrict()));
        lpOutIconInfo->SetCounty(
            BrnWorld::WorldRegion::DistrictToCounty(lpOutIconInfo->GetDistrict()));

        lpOutIconInfo->SetLandmarkIndexHalf(liLandmarkIndex);                     // sth -> +0x20
        lpOutIconInfo->SetIconType(
            BrnGui::GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK); // stb 4 -> +0x28
        lpOutIconInfo->SetDesignIndex(lpLandmark->GetDesignIndex());              // stb lm+0x31 -> +0x22
        lpOutIconInfo->SetActiveRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_INVALID);    // stb -1 -> +0x26
    }

    // FLAG: sentinel value inferred from the committed LandmarkIndex convention. LandmarkIndex
    // is a signed-16-bit wrapper (BrnGameStateTypes.h); that header documents K_INVALID_LANDMARK
    // as a sentinel but does not yet define it, and no other TU homes it. The X360 compares
    // mEventDestinationLandmarkIndex against the data word word_82F25440, whose stored value is
    // not dumped, so 0xFFFF (-1) is used per the s16 invalid-index convention -- not a novel
    // magic number. Internal linkage (const at namespace scope); no ODR reach.
    // (Moved with GetEventDestinationLandmarkIndex from BrnGuiCache_wB_res.cpp.)
    const u16 K_INVALID_LANDMARK = 0xFFFF;
}

namespace BrnGui
{
    // ---------------------------------------------------------------------------------------------
    // @ 0x82506688 -- fill the caller's SatNavIconInfo for the landmark at REGION INDEX
    // `lLandmarkIndex` and return the out pointer. The landmark-index halfword the record
    // carries is the CALLER'S index (sth r27 -- the argument), not the landmark's own.
    // ---------------------------------------------------------------------------------------------
    GuiEventUpdateSatNav::SatNavIconInfo*
        GuiCache::GetLandmarkInfoFromIndex(BrnGameState::LandmarkIndex lLandmarkIndex,
                                           GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const
    {
        CGS_ASSERT(mpWorldDataController != 0, "mpWorldDataController");   // cpp:3620

        const BrnTrigger::Landmark* lpLandmark =
            mpWorldDataController->GetLandmarkInfoFromIndex(lLandmarkIndex);
        CGS_ASSERT(lpLandmark != 0, "lpLandmark");                         // cpp:3623

        FillIconInfoFromLandmark(lpLandmark,
                                 static_cast<s16>(static_cast<s32>(lLandmarkIndex)),
                                 lpOutIconInfo);
        return lpOutIconInfo;
    }

    // ---------------------------------------------------------------------------------------------
    // @ 0x825067E0 -- the id sibling: resolve the landmark by CgsID, fill the caller's record;
    // the landmark-index halfword is the LANDMARK'S OWN region index (lhz lm+0x28). `void`
    // return per DWARF -- r3 at the console return is the DistrictToCounty leftover, NOT a
    // result (the header note pinned this before the body landed; do not resurrect it).
    // ---------------------------------------------------------------------------------------------
    void GuiCache::GetLandmarkInfoFromID(CgsID lLandmarkID,
                                         GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const
    {
        CGS_ASSERT(mpWorldDataController != 0, "mpWorldDataController");   // cpp:3654

        const BrnTrigger::Landmark* lpLandmark =
            mpWorldDataController->GetLandmarkInfoFromID(lLandmarkID);
        CGS_ASSERT(lpLandmark != 0, "lpLandmark");                         // cpp:3657

        FillIconInfoFromLandmark(lpLandmark,
                                 static_cast<s16>(lpLandmark->GetRegionIndex()),
                                 lpOutIconInfo);
    }

    // ---------------------------------------------------------------------------------------------
    // @ 0x824EE7D0 -- the active-landmark latch (see the header declaration's note; the count
    // store is a BYTE store on console -- a > 255 list truncates, and that is shipped
    // behaviour, not a bug to widen away).
    // ---------------------------------------------------------------------------------------------
    void GuiCache::HandleSetActiveLandmarksEvent(const SetActiveLandmarksEvent* lpEvent)
    {
        CGS_ASSERT(lpEvent->muNumLandmarks <= 512u,
                   "lpActiveLandmarksEvent->muNumLandmarks <= static_cast<uint32_t>( KI_MAX_LANDMARKS_IN_GAME )");   // cpp:4067

        for (u32 luLandmark = 0; luLandmark < lpEvent->muNumLandmarks; ++luLandmark)
            maActiveLandmarks[luLandmark] = lpEvent->maLandmarkIndices[luLandmark];

        muNumActiveLandmarks = static_cast<u8>(lpEvent->muNumLandmarks);   // the console stb
    }

    // ---------------------------------------------------------------------------------------------
    // @ 0x82506F28 -- resolve `liNumLandmarks` landmark INDICES to their icon records and
    // publish the set to the GuiTracker as one id-232 SetTracker event (currently-tracked
    // index 0, flag 1). The record geometry is the BrnGuiTracker.h SetTrackerEvent.
    // ---------------------------------------------------------------------------------------------
    void GuiCache::UpdateTrackerInfo(const u16* lpLandmarkIndices, s32 liNumLandmarks)
    {
        CGS_ASSERT(lpLandmarkIndices != 0, "lpLandmarkIndices");   // cpp:3913

        // The console's stack record leaves the entry interiors as stack residue outside the
        // three fields it writes (position lane / type byte / landmark index); zero-init here
        // -- no recovered consumer reads the residue bytes, and UB-free is the PC constraint.
        GuiTracker::SetTrackerEvent lSetTrackerEvent = {};
        lSetTrackerEvent.miCurrentlyTrackedIndex = 0;
        lSetTrackerEvent.mu8Flag = 1;

        for (s32 liLandmark = 0; liLandmark < liNumLandmarks; ++liLandmark)
        {
            GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
            GetLandmarkInfoFromIndex(
                BrnGameState::LandmarkIndex(lpLandmarkIndices[liLandmark]), &lLandmarkInfo);

            GuiTracker::TrackerInformation& lrEntry = lSetTrackerEvent.maEntries[liLandmark];
            const Vector4& lv4Position = lLandmarkInfo.GetPositionLane();
            lrEntry.mv3Position.x   = lv4Position.x;    // stvx -> entry+0x10
            lrEntry.mv3Position.y   = lv4Position.y;
            lrEntry.mv3Position.z   = lv4Position.z;
            lrEntry.mv3Position.w   = lv4Position.w;
            lrEntry.miLandmarkIndex = lLandmarkInfo.GetLandmarkIndexHalf();   // sth -> entry+0x28
            // (the console's icon-type-4 poke at BE entry byte +0x1E is dropped -- see the
            // TrackerInformation banner in BrnGuiTracker.h)
        }
        lSetTrackerEvent.miNumTrackedItems = liNumLandmarks;

        CGS_ASSERT(mpGuiTracker != 0, "Invalid tracker pointer");   // cpp:3932
        mpGuiTracker->RecEvent(&lSetTrackerEvent, 232);
    }

    namespace
    {
        // Minimal boundary over one row of the online game-mode options mirror
        // (cache maOnlineGameModeOptionsStorage @0xA800, stride 44): the u16 landmark-index
        // array at the row head and its count at +0x24 -- exactly the two faces the X360
        // SpecificGameModeEventInterface accessor pair reads (Even[t]LandmarkAtIndex
        // @0x8240E7E0: bounds-assert vs +0x24 then `*out = *(row + 2*index)`;
        // BrnGameStateSharedIO.h:1929/:1930). FLAG: minimal-slice boundary -- the row's
        // full DWARF type (a sibling slice of BrnGui::PresetEvent) is uncommitted.
        struct OnlineGameModeOptionsRow
        {
            u16 maLandmarks[18];   // +0x00..+0x23
            s32 miNumLandmarks;    // +0x24
            u32 muTail;            // +0x28
        };
        static_assert(sizeof(OnlineGameModeOptionsRow) == 0x2C, "online options row stride");
    }

    // ---------------------------------------------------------------------------------------------
    // @ 0x82507070 (unnamed sub_ in the export set) -- the ONLINE arm of RefreshMapState:
    // publish the current online round's landmark set to the tracker. File-local free function
    // on console too (its `this` is passed explicitly); the row's landmark indices resolve
    // through the same GetLandmarkInfoFromIndex fill.
    // ---------------------------------------------------------------------------------------------
    namespace
    {
        void RefreshTrackerFromOnlineRound(GuiCache* lpCache,
                                           const OnlineGameModeOptionsRow* lpRow)
        {
            CGS_ASSERT(lpRow != 0, "lpEvent");   // cpp:3947

            GuiTracker::SetTrackerEvent lSetTrackerEvent = {};
            lSetTrackerEvent.miCurrentlyTrackedIndex = 0;
            lSetTrackerEvent.mu8Flag = 1;

            const u32 luNumLandmarks = static_cast<u32>(lpRow->miNumLandmarks);
            for (u32 luLandmark = 0; luLandmark < luNumLandmarks; ++luLandmark)
            {
                // The console goes through SpecificGameModeEventInterface's bounds-asserted
                // accessor (@0x8240E7E0); the walk bound here IS that accessor's bound, so
                // the assert cannot fire on this path and the direct row read is the same
                // value it returns.
                GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
                lpCache->GetLandmarkInfoFromIndex(
                    BrnGameState::LandmarkIndex(lpRow->maLandmarks[luLandmark]),
                    &lLandmarkInfo);

                GuiTracker::TrackerInformation& lrEntry =
                    lSetTrackerEvent.maEntries[luLandmark];
                const Vector4& lv4Position = lLandmarkInfo.GetPositionLane();
                lrEntry.mv3Position.x   = lv4Position.x;
                lrEntry.mv3Position.y   = lv4Position.y;
                lrEntry.mv3Position.z   = lv4Position.z;
                lrEntry.mv3Position.w   = lv4Position.w;
                lrEntry.miLandmarkIndex = lLandmarkInfo.GetLandmarkIndexHalf();
            }
            lSetTrackerEvent.miNumTrackedItems = static_cast<s32>(luNumLandmarks);

            CGS_ASSERT(lpCache->GetGuiTracker() != 0, "Invalid tracker pointer");   // cpp:3967
            lpCache->GetGuiTracker()->RecEvent(&lSetTrackerEvent, 232);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // @ 0x82510F40 -- re-publish the map/tracker state: the ONLINE arm (online start in
    // progress) republishes the current online round's landmark set; the OFFLINE arm
    // republishes the current event's checkpoint-landmark list.
    // ---------------------------------------------------------------------------------------------
    void GuiCache::RefreshMapState()
    {
        if (mbOnlineStartInProgress)   // lbz +0x4B4C
        {
            const OnlineGameModeOptionsRow* lpRow =
                reinterpret_cast<const OnlineGameModeOptionsRow*>(
                    &maOnlineGameModeOptionsStorage[44 * miOnlineRoundIndex]);
            RefreshTrackerFromOnlineRound(this, lpRow);
        }
        else
        {
            UpdateTrackerInfo(maCheckpointLandmarks,
                              static_cast<s32>(GetCheckpointsInEvent()));   // lbzx +0x9FB8
        }
    }

    // ---------------------------------------------------------------------------------------------
    // @ 0x825071C8 -- re-latch the active-landmark set for one event at animation parameter
    // lfT (the fly-by grows the set as the map animation plays: liNumActive = count * t), and
    // publish the same set to the tracker. Returns the active count, or -1 when the event id
    // resolves to no event. lbFlag == true seeds the tracker's currently-tracked index from
    // the live checkpoint-reached counter (the crash-nav resume path) instead of 0.
    // ---------------------------------------------------------------------------------------------
    s32 GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID(u32 luEventID, f32 lfT,
                                                                        bool lbFlag)
    {
        CGS_ASSERT(mpWorldDataController != 0, "mpWorldDataController");   // BrnGuiCache.h:2324

        const BrnProgression::RaceEventData* lpRaceEventData =
            mpWorldDataController->GetEventInfoFromEventId(luEventID);
        if (lpRaceEventData == 0)
            return -1;

        GuiTracker::SetTrackerEvent lSetTrackerEvent = {};
        lSetTrackerEvent.mu8Flag = 1;

        if (lbFlag)
        {
            lSetTrackerEvent.miCurrentlyTrackedIndex = GetCheckpointReached();
            CGS_ASSERT(lSetTrackerEvent.miCurrentlyTrackedIndex >= 0,
                       "lSetTrackerEvent.miCurrentlyTrackedIndex >= 0");   // cpp:4204
        }
        else
        {
            lSetTrackerEvent.miCurrentlyTrackedIndex = 0;
        }

        // The animated fraction of the event's checkpoints (fctiwz truncation).
        const s32 liNumActive =
            static_cast<s32>(static_cast<f32>(lpRaceEventData->GetCheckpointCount()) * lfT);

        SetActiveLandmarksEvent lActiveLandmarks = {};
        if (liNumActive != 0)
        {
            CGS_ASSERT(liNumActive <= GuiTracker::KI_TRACKER_STACK_SIZE,
                       "lSetTrackerEvent.miNumTrackedItems <= GuiTracker::KI_TRACKER_STACK_SIZE");   // cpp:4227

            for (s32 liCheckpoint = 0; liCheckpoint < liNumActive; ++liCheckpoint)
            {
                GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
                GetLandmarkInfoFromID(
                    lpRaceEventData->GetCheckpointData(liCheckpoint)->GetLandmarkId(),
                    &lLandmarkInfo);

                GuiTracker::TrackerInformation& lrEntry =
                    lSetTrackerEvent.maEntries[liCheckpoint];
                const Vector4& lv4Position = lLandmarkInfo.GetPositionLane();
                lrEntry.mv3Position.x   = lv4Position.x;
                lrEntry.mv3Position.y   = lv4Position.y;
                lrEntry.mv3Position.z   = lv4Position.z;
                lrEntry.mv3Position.w   = lv4Position.w;
                lrEntry.miLandmarkIndex = lLandmarkInfo.GetLandmarkIndexHalf();

                lActiveLandmarks.maLandmarkIndices[liCheckpoint] =
                    static_cast<u16>(lLandmarkInfo.GetLandmarkIndexHalf());
            }
        }
        lActiveLandmarks.muNumLandmarks    = static_cast<u32>(liNumActive);
        lSetTrackerEvent.miNumTrackedItems = liNumActive;

        HandleSetActiveLandmarksEvent(&lActiveLandmarks);
        mpGuiTracker->RecEvent(&lSetTrackerEvent, 232);

        return liNumActive;
    }

    // ---------------------------------------------------------------------------------------------
    // @ 0x8240FA88 -- the event's destination landmark index. Valid only in the race-style game
    // modes (same mode gate as GetEventDestinationDistrict; the X360 skips the assert for
    // meGameModeType in {0,1,10,6,8,5}); then asserts the stored index is not the invalid-
    // landmark sentinel. Returns mEventDestinationLandmarkIndex (@0x9F4C) wrapped as a
    // LandmarkIndex (the X360 copies the raw u16 into the caller's out slot).
    // (MOVED VERBATIM from the unmounted BrnGuiCache_wB_res.cpp -- the gates file's own
    // retirement prescription; wB_res keeps its other bodies.)
    // ---------------------------------------------------------------------------------------------
    BrnGameState::LandmarkIndex GuiCache::GetEventDestinationLandmarkIndex() const
    {
        CGS_ASSERT(
            (meGameModeType == 0) || (meGameModeType == 1) || (meGameModeType == 10)
                || (meGameModeType == 6) || (meGameModeType == 8) || (meGameModeType == 5),
            "race-style game mode required for GetEventDestinationLandmarkIndex");
        CGS_ASSERT(mEventDestinationLandmarkIndex != K_INVALID_LANDMARK,
                   "mEventDestinationLandmarkIndex != BrnGameState::K_INVALID_LANDMARK");
        return BrnGameState::LandmarkIndex(mEventDestinationLandmarkIndex);
    }
}
