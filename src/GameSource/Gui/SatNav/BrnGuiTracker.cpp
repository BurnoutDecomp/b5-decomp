// ===================================================================================
// BrnGui::GuiTracker  -- implementation
//   class:BrnGui::GuiTracker
//
// Reconstructed store-for-store from the X360 ARTIST build. Member-by-name access; the
// tracker-information record interior and the route-info array body are held opaque in
// the owning header (their full element layout is not recovered by these accessors), so
// the accessor returns a pointer to the named record element.
// ===================================================================================
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"
#include "GameSource/Gui/BrnGuiCache.h"              // [map arm] GuiCache::GetWorldCameraPosition (RecEvent case 64)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // @ 0x82443EC0 - bounds-checked pointer to tracker record `liIndex`.
    // The X360 tests both ends (liIndex < 0 || liIndex >= miTrackerCount), streaming the
    // dynamic "Invalid tracker index - out of range: <i> :: <n>" message into the assert
    // buffer (BrnGuiTracker.h:279) on failure; the house assert forwards the static text.
    // Then returns this + 48*liIndex + 0x10 (slwi/add/slwi address arithmetic) ==
    // &maTrackerRecords[liIndex].
    GuiTracker::TrackerInformation* GuiTracker::GetTrackerInformation(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < miTrackerCount,
                   "Invalid tracker index - out of range");

        return &maTrackerRecords[liIndex];
    }

    // @ 0x82488ED0 - route-info availability. Returns false when the has-route flag at
    // +0x03 is clear (early `lbz 3` / `beq`). Otherwise reads the route-info array's
    // live-count word at +0x65050, asserting it was Construct/Clear'd (count != -1,
    // CgsArray.h:336), and returns true only when the count is >= 2 (the `cmplwi 2` /
    // `bge` -> 1, else 0). The X360 returns the 0/1 in the low byte (clrlwi 24).
    bool GuiTracker::IsRouteInfoAvailable()
    {
        if (!mbHasRoute)
        {
            return false;
        }

        CGS_ASSERT(miRouteInfoCount != -1,
                   "Array used before Construct/Clear was called");

        return static_cast<u32>(miRouteInfoCount) >= static_cast<u32>(KI_ROUTE_MIN_POINTS);
    }

    // @ 0x82488F58 - route distance. Asserts IsRouteInfoAvailable() (BrnGuiTracker.h:371)
    // then loads the f32 at +0x65068 (lfsx) and returns it in f1 (widened to double by
    // the float-return convention).
    double GuiTracker::GetRouteDistance()
    {
        CGS_ASSERT(IsRouteInfoAvailable(), "IsRouteInfoAvailable()");

        return mfRouteDistance;
    }

    // @ 0x824FA0A8 - drop the current route / tracker state. A straight-line, branch-free,
    // call-free store sequence (blr right after the last store); reproduced store-for-store
    // in the X360 order:
    //
    //   lfs   f0, flt_82001CC0   /  stfsx f0, r3, 0x65068  -> mfRouteDistance    = 0.0f
    //   li    r11, 0             /  stwx  r11, r3, 0x65050 -> miRouteInfoCount   = 0
    //                               stb   r11, 0(r3)       -> head byte 0        = 0
    //                               stb   r11, 1(r3)       -> head byte 1        = 0
    //                               stb   r11, 2(r3)       -> head byte 2        = 0
    //                               stw   r11, 4(r3)       -> miTrackerCount     = 0
    //   li    r10, -1            /  stwx  r10, r3, 0x65060 -> word @+0x65060     = -1
    //
    // [map arm 2026-08-27] the head byte trio and the +0x65060 word are NAMED now (the
    // RecEvent decode identified them -- see the header banner): the trio is
    // mbTrackingActive / mbRouteDataPending / mu8SetTrackerFlag, and +0x65060 is
    // miCurrentlyTrackedIndex (-1 == none, exactly this clear's sentinel). ClearTracker
    // deliberately does NOT clear mbHasRoute -- the has-route flag survives the clear.
    void GuiTracker::ClearTracker()
    {
        mfRouteDistance  = 0.0f;   // stfsx flt_82001CC0 (0.0f), r3, 0x65068
        miRouteInfoCount = 0;      // stwx  0,  r3, 0x65050

        mbTrackingActive   = 0;    // stb   0,  0(r3)
        mbRouteDataPending = 0;    // stb   0,  1(r3)
        mu8SetTrackerFlag  = 0;    // stb   0,  2(r3)

        miTrackerCount = 0;        // stw   0,  4(r3)

        miCurrentlyTrackedIndex = -1;   // stwx -1, r3, 0x65060
    }

    // =============================================================================================
    // [map arm 2026-08-27] the tracker's event consumer + the two route builders, decompiled
    // from the X360 bodies (RecEvent @0x82501D28, GenerateRouteData @0x824FA008,
    // RegenerateRouteData @0x824F41E0). The 232 producers live in BrnGuiCache_wMap.cpp.
    // =============================================================================================

    // @ 0x82501D28. The console streams its null-event diagnostic through the global assert
    // buffer; lowered to the house static-text sequence per the project convention.
    void GuiTracker::RecEvent(const void* lpEvent, s32 liEventId)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event pointer");   // BrnGuiTracker.cpp:104

        switch (liEventId)
        {
        case 64:
        {
            // The per-frame GuiCache pointer publish: refresh the player's tracker record
            // from the cache's world-camera lane, and latch the cache pointer once.
            const GuiCache* const* lppCache =
                reinterpret_cast<const GuiCache* const*>(lpEvent);
            CGS_ASSERT(*lppCache != 0, "lpcacheEvent->mpCachePointer");   // :189
            const GuiCache* lpCache = *lppCache;
            CGS_ASSERT(lpCache != 0, "lpPlayerInfo");                     // :192 (the +19168 view)

            // X360: stw 0 @+0xC10 (the player record's head word), then stvx the cache's
            // world-camera lane (cache+0x4AE0) into the record's position lane @+0xC20.
            mPlayersTrackerInfo.maHeadStorage[0] = 0;
            mPlayersTrackerInfo.maHeadStorage[1] = 0;
            mPlayersTrackerInfo.maHeadStorage[2] = 0;
            mPlayersTrackerInfo.maHeadStorage[3] = 0;
            {
                const Vector4& lv4Camera = lpCache->GetWorldCameraPosition();
                mPlayersTrackerInfo.mv3Position.x = lv4Camera.x;
                mPlayersTrackerInfo.mv3Position.y = lv4Camera.y;
                mPlayersTrackerInfo.mv3Position.z = lv4Camera.z;
                mPlayersTrackerInfo.mv3Position.w = lv4Camera.w;
            }
            if (mpGuiCache == 0)
                mpGuiCache = const_cast<GuiCache*>(lpCache);
            break;
        }

        case 165:
        {
            // Checkpoint reached: only while tracking is live, and only when the event's
            // landmark index matches the CURRENTLY tracked record's.
            if (mbTrackingActive == 0)
                break;
            const u16 luEventLandmarkIndex = *reinterpret_cast<const u16*>(lpEvent);
            const s32 liTracked = miCurrentlyTrackedIndex;
            if (luEventLandmarkIndex !=
                static_cast<u16>(maTrackerRecords[liTracked].miLandmarkIndex))
                break;

            miCurrentlyTrackedIndex = liTracked + 1;
            if (miCurrentlyTrackedIndex != miTrackerCount)
            {
                GenerateRouteData();   // the shared mid-route rebuild (X360 LABEL_19)
            }
            else
            {
                // The FINAL checkpoint: drop the whole tracked state.
                mbTrackingActive        = 0;
                mbRouteDataPending      = 0;
                mu8SetTrackerFlag       = 0;
                miTrackerCount          = 0;
                miRouteInfoCount        = 0;
                miCurrentlyTrackedIndex = -1;
                mfRouteDistance         = 0.0f;
            }
            break;
        }

        case 211:
        {
            // One route-information leg: copy it whole into its tracker-stack slot,
            // accumulate the distance, and build the route once every leg is in.
            const RouteInformation* lpLeg =
                reinterpret_cast<const RouteInformation*>(lpEvent);
            CGS_ASSERT(lpLeg->miEventId >= 0 && lpLeg->miEventId < KI_TRACKER_STACK_SIZE,
                       "lpRouteInformaton->miEventId >= 0 && lpRouteInformaton->miEventId < KI_TRACKER_STACK_SIZE");   // :166
            maRouteLegs[lpLeg->miEventId] = *lpLeg;   // the console's 5136-byte memcpy
            ++miNumRouteLegsReceived;
            mfRouteDistance += lpLeg->mfDistance;
            if (miNumRouteLegsReceived >= miTrackerCount - 1)
                GenerateRouteData();
            else
                mbRouteDataPending = 1;
            break;
        }

        case 232:
        {
            // The SetTracker publish: adopt the whole tracked-record stack.
            const SetTrackerEvent* lpSet = reinterpret_cast<const SetTrackerEvent*>(lpEvent);
            if (static_cast<u32>(lpSet->miNumTrackedItems) >=
                static_cast<u32>(KI_TRACKER_STACK_SIZE))
            {
                CGS_ASSERT(false, "Invalid number of trackers");   // :112 (streamed on console)
            }
            if (lpSet->miNumTrackedItems != 0)
            {
                miTrackerCount          = lpSet->miNumTrackedItems;
                miNumRouteLegsReceived  = 0;
                mbTrackingActive        = 1;
                mu8SetTrackerFlag       = lpSet->mu8Flag;
                mbRouteDataPending      = (lpSet->miNumTrackedItems > 1) ? 1 : 0;
                mbHasRoute              = 0;
                mfRouteDistance         = 0.0f;
                miCurrentlyTrackedIndex = lpSet->miCurrentlyTrackedIndex;
                for (s32 liRecord = 0; liRecord < lpSet->miNumTrackedItems; ++liRecord)
                    maTrackerRecords[liRecord] = lpSet->maEntries[liRecord];   // 48B copies
            }
            else
            {
                mbTrackingActive        = 0;
                mbRouteDataPending      = 0;
                mu8SetTrackerFlag       = 0;
                miTrackerCount          = 0;
                miRouteInfoCount        = 0;
                mfRouteDistance         = 0.0f;
                miCurrentlyTrackedIndex = -1;
            }
            break;
        }

        case 233:
            muPendingTargetSectionId = *reinterpret_cast<const s32*>(lpEvent);
            RegenerateRouteData();
            break;

        default:
            break;
        }
    }

    // @ 0x824FA008. Flatten every received leg's polyline into the route-point array.
    // The console appends through Array<rw::math::vpu::Vector3, 5120>::Append; the raw
    // carve here reproduces its store + count bump, with the capacity assert the Array
    // family fires (CgsArray.h precedent).
    void GuiTracker::GenerateRouteData()
    {
        const s32 liNumLegs = miNumRouteLegsReceived;
        miRouteInfoCount = 0;
        for (s32 liLeg = 0; liLeg < liNumLegs; ++liLeg)
        {
            const RouteInformation& lrLeg = maRouteLegs[liLeg];
            for (s32 liPoint = 0; liPoint < lrLeg.miNumPoints; ++liPoint)
            {
                CGS_ASSERT(miRouteInfoCount < KI_ROUTE_POINT_CAPACITY,
                           "miRouteInfoCount < KI_ROUTE_POINT_CAPACITY");
                maRouteInfoPoints[miRouteInfoCount] = lrLeg.maPoints[liPoint];
                ++miRouteInfoCount;
            }
        }
        mbRouteDataPending = 0;
        mbHasRoute         = 1;
    }

    // @ 0x824F41E0. Rebuild the tracked stack from the CURRENT position: stamp the player
    // record with the pending target section id, make it record 0, follow it with the
    // records not yet reached, and reset the leg counter for the fresh route build.
    void GuiTracker::RegenerateRouteData()
    {
        // The console snapshots the whole 64-record stack first (the in-place compaction
        // below reads from the snapshot).
        TrackerInformation laRecordsCopy[KI_TRACKER_STACK_SIZE];
        for (s32 liRecord = 0; liRecord < KI_TRACKER_STACK_SIZE; ++liRecord)
            laRecordsCopy[liRecord] = maTrackerRecords[liRecord];

        mPlayersTrackerInfo.muTargetSectionId = static_cast<u32>(muPendingTargetSectionId);
        CGS_ASSERT(mPlayersTrackerInfo.muTargetSectionId != 0x7FFFu,
                   "mPlayersTrackerInfo.muTargetSectionId != BrnWorld::KI_INVALID_SECTION_INDEX");   // :280

        maTrackerRecords[0] = mPlayersTrackerInfo;

        CGS_ASSERT(miCurrentlyTrackedIndex != -1, "miCurrentlyTrackedIndex != -1");   // :284

        const s32 liRemaining = miTrackerCount - miCurrentlyTrackedIndex;
        miTrackerCount = 1;
        for (s32 liRecord = 0; liRecord < liRemaining; ++liRecord)
        {
            maTrackerRecords[1 + liRecord] = laRecordsCopy[miCurrentlyTrackedIndex + liRecord];
            ++miTrackerCount;
        }

        miCurrentlyTrackedIndex = 1;
        mbRouteDataPending      = 1;
        miNumRouteLegsReceived  = 0;
    }
}
