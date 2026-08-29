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
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // [FIX1] GuiCache::GetWorldCameraPosition

#include <cstring>   // [FIX1] std::memcpy (the three register/record copies)

namespace BrnGui
{
    namespace
    {
        // -----------------------------------------------------------------------------
        // ⭐ FIX1 (2026-08-29). The five ids RecEvent's switch arms on, exactly as the
        // X360 `cmpwi`/jump table gives them. Named for what each arm does with the
        // record; FLAG consumer-named -- none of the five has a recovered event TYPE in
        // the tree except 232 (GuiEventSetTracker, which does have one).
        // -----------------------------------------------------------------------------
        const s32 KI_EVENT_CACHE_POINTER     = 64;    // the GuiCache handoff
        const s32 KI_EVENT_LANDMARK_REACHED  = 165;   // one landmark reached
        const s32 KI_EVENT_ROUTE_INFORMATION = 211;   // one 5136-byte route record
        const s32 KI_EVENT_SET_TRACKER       = 232;   // GuiEventSetTracker (0xE8/0xC10)
        const s32 KI_EVENT_PLAYER_SECTION    = 233;   // the player's new section id

        // BrnWorld::KI_INVALID_SECTION_INDEX, named verbatim by RegenerateRouteData's
        // assert literal; the value is the tree's committed 0x7FFF (BrnVehicleManager.h)
        // and the X360 comparison is `cmpwi 0x7FFF`.
        const u16 KI_INVALID_SECTION_INDEX = 0x7FFF;

        // ---- the three payloads the arms above reinterpret -----------------------------
        // Each is the MINIMAL shape its arm actually reads; the console hands RecEvent a
        // bare record pointer (there is no GuiEvent<N> header on this path -- see the
        // GuiEventSetTracker banner in the header for the same finding). FLAG
        // consumer-named, sizes not independently attested beyond the fields read.

        // case 64: one pointer, asserted non-null twice ("lpcacheEvent->mpCachePointer"
        // then "lpPlayerInfo", the second being the null test on cache+0x4AE0).
        struct GuiEventCachePointer
        {
            GuiCache* mpCachePointer;   // +0x00
        };

        // case 165: the arm reads a single u16 at +0x00 (`*a2`, a2 is `unsigned __int16*`)
        // and compares it against TrackerInformation::mTargetLandmarkIndex, which is itself
        // a u16 -- so the field is a landmark index.
        struct GuiEventLandmarkReached
        {
            u16 muLandmarkIndex;   // +0x00
        };

        // case 233: the arm reads one value at +0x00 and stores it into
        // mPlayersTrackerInfo.muTargetSectionId, a u16 section index.
        struct GuiEventPlayerSection
        {
            u16 muSectionId;   // +0x00
        };
    }

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

        // [FIX1] The word at +0x65050 is mRoutePoints' own CgsArray count -- the assert
        // string here IS CgsArray.h:336's, which is what identified it. GetCount() is the
        // unchecked reader, so the -1 sentinel test still reads exactly as the console's.
        CGS_ASSERT(mRoutePoints.GetCount() != -1,
                   "Array used before Construct/Clear was called");

        return static_cast<u32>(mRoutePoints.GetCount())
                   >= static_cast<u32>(KI_ROUTE_MIN_POINTS);
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
    // Notes on the two spans this has to reach through, both held opaque by the owning
    // header (BrnGuiTracker.h), which recovered only the offsets its three accessors read:
    //
    //  * The three `stb 0` at +0x00/+0x01/+0x02 are three separate byte flags, not a vtable
    //    pointer: the X360 writes single bytes into them, and IsRouteInfoAvailable reads a
    //    fourth byte flag (mbHasRoute) immediately after at +0x03. ClearTracker
    //    deliberately does NOT clear mbHasRoute -- the has-route flag survives the clear.
    //  * ⭐ FIX1 2026-08-29: the two members this used to reach through reserved carves are
    //    NAMED now. The old note "no X360 reader of +0x65060 is recovered, so it is left
    //    unnamed" is superseded -- RecEvent's 165/232 arms and RegenerateRouteData all read
    //    and write it, and RegenerateRouteData's own assert spells it
    //    "miCurrentlyTrackedIndex != -1", which is the very -1 this writes. The `stwx 0,
    //    r3, 0x65050` is likewise mRoutePoints' CgsArray count, so it goes through Clear().
    void GuiTracker::ClearTracker()
    {
        mfRouteDistance = 0.0f;    // stfsx flt_82001CC0 (0.0f), r3, 0x65068
        mRoutePoints.Clear();      // stwx  0,  r3, 0x65050  (the Array<> count word)

        mbTrackingActive   = false;   // stb 0, 0(r3)
        mbRouteDataPending = false;   // stb 0, 1(r3)
        mbIsEntireRoute    = false;   // stb 0, 2(r3)

        miTrackerCount = 0;        // stw   0,  4(r3)

        miCurrentlyTrackedIndex = -1;   // stwx -1, r3, 0x65060
    }

    // =================================================================================
    //  ⭐ FIX1 (2026-08-29): the RecEvent slice. Three functions, no invented arms.
    // =================================================================================

    // @ 0x824FA008 -- flatten the received route records into the drawable point line.
    //
    // Store-for-store: clear the point array's count first (`stw 0, this+0x65050`), then
    // for each of the miNumRouteInfoReceived records walk its own miNumPoints entries
    // (cursor `v6` starts at record+0 and strides 16 bytes) and Append each 16-byte VMX
    // register into mRoutePoints. Finish by clearing mbRouteDataPending and RAISING
    // mbHasRoute -- this is the ONLY writer of mbHasRoute in the whole class, which is why
    // IsRouteInfoAvailable could never return true before this body existed.
    //
    // ⚠️ The record loop is driven by the RECEIVE COUNT, not by event id: the console walks
    // maRouteInformation[0 .. miNumRouteInfoReceived), so records land in arrival order but
    // are indexed by miEventId when written (case 211). That is a console behaviour, not a
    // reconstruction choice -- reproduced as-is.
    void GuiTracker::GenerateRouteData()
    {
        mRoutePoints.Clear();   // stw 0, this+0x65050

        const s32 liRecordsReceived = miNumRouteInfoReceived;
        for (s32 liRecord = 0; liRecord < liRecordsReceived; ++liRecord)
        {
            const RouteInformation& lrRoute = maRouteInformation[liRecord];

            const s32 liNumPoints = lrRoute.miNumPoints;
            for (s32 liPoint = 0; liPoint < liNumPoints; ++liPoint)
            {
                mRoutePoints.Append(lrRoute.mav3Points[liPoint]);
            }
        }

        mbRouteDataPending = false;   // stb 0, 1(this)
        mbHasRoute         = true;    // stb 1, 3(this)
    }

    // @ 0x824F41E0 -- re-base the tracker set on the player's own record.
    //
    // The console: memcpy the whole 0xC00 record block onto the stack; store the latched
    // player section id into mPlayersTrackerInfo.muTargetSectionId (asserting it is not the
    // invalid-section sentinel 0x7FFF); copy mPlayersTrackerInfo into maTrackerRecords[0]
    // (six 8-byte moves == the 48-byte record); assert a tracker is selected; then set
    // miTrackerCount = 1 and copy the SNAPSHOT's records [miCurrentlyTrackedIndex ..
    // old miTrackerCount) into slots 1.. , bumping miTrackerCount once per copy. Finally
    // miCurrentlyTrackedIndex = 1, mbRouteDataPending = 1, miNumRouteInfoReceived = 0.
    //
    // The stack snapshot is load-bearing, not a Hex-Rays artefact: the source slots overlap
    // the destination slots (source index >= 1 whenever the tracked index is 1), so copying
    // in place would read records this pass has already overwritten.
    void GuiTracker::RegenerateRouteData()
    {
        TrackerInformation laSnapshot[KI_TRACKER_RECORD_CAPACITY];
        std::memcpy(laSnapshot, maTrackerRecords, sizeof(laSnapshot));   // memcpy(v15, a1+0x10, 0xC00)

        mPlayersTrackerInfo.muTargetSectionId =
            static_cast<u16>(muPlayerTargetSectionId);                   // sth -> this+0xC30
        CGS_ASSERT(mPlayersTrackerInfo.muTargetSectionId != KI_INVALID_SECTION_INDEX,
                   "mPlayersTrackerInfo.muTargetSectionId != BrnWorld::KI_INVALID_SECTION_INDEX");

        maTrackerRecords[0] = mPlayersTrackerInfo;                       // the six 8-byte moves

        CGS_ASSERT(miCurrentlyTrackedIndex != -1, "miCurrentlyTrackedIndex != -1");

        const s32 liFirstKept = miCurrentlyTrackedIndex;
        const s32 liNumKept   = miTrackerCount - liFirstKept;            // v10

        miTrackerCount = 1;                                              // stw 1, 4(this)
        for (s32 liKept = 0; liKept < liNumKept; ++liKept)
        {
            maTrackerRecords[1 + liKept] = laSnapshot[liFirstKept + liKept];
            ++miTrackerCount;                                            // ++*(a1 + 4)
        }

        miCurrentlyTrackedIndex  = 1;       // stw 1, this+0x65060
        mbRouteDataPending       = true;    // stb 1, 1(this)
        miNumRouteInfoReceived   = 0;       // stw 0, this+0x51000
    }

    // @ 0x82501D28 -- the tracker's event sink.
    //
    // NAME: the X360 symbol and the DWARF row both spell it `RecEvent` (no 'v'); that is
    // the spelling kept here and at the CrashNavMapEvent call site.
    //
    // The console asserts the event pointer through the streamed-message path
    // (StrStream "Invalid event pointer", BrnGuiTracker.cpp:104) and then falls into a
    // five-arm switch; unknown ids do nothing at all (`default: break`). liEventSizeBytes
    // is accepted and unused by the console body -- every arm knows its own record size --
    // so it is deliberately not consulted here either.
    void GuiTracker::RecEvent(const CgsModule::Event* lpEvent, s32 liEventId,
                              s32 liEventSizeBytes)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event pointer");   // cpp:104

        (void)liEventSizeBytes;   // the X360 body never reads it (see above)

        switch (liEventId)
        {
            // -------------------------------------------------------------------------
            // 64 -- "here is the GuiCache". Refresh the player's own tracker record from
            // the cache's world-camera lane, and latch the cache pointer the FIRST time
            // one arrives (`if (!*(this + 0x650EC)) *(this + 0x650EC) = *a2`).
            // -------------------------------------------------------------------------
            case KI_EVENT_CACHE_POINTER:
            {
                const GuiEventCachePointer* lpCacheEvent =
                    reinterpret_cast<const GuiEventCachePointer*>(lpEvent);

                CGS_ASSERT(lpCacheEvent->mpCachePointer != 0,
                           "lpcacheEvent->mpCachePointer");                  // cpp:189
                CGS_ASSERT(lpCacheEvent->mpCachePointer != 0, "lpPlayerInfo"); // cpp:192

                // `_R30 = *a2 + 19168` then `lvx v0, r0, r30` / `stvx v0, this, 0xC20`:
                // 19168 == 0x4AE0 == GuiCache::mv4WorldCameraPosition, the very member
                // GuiCache::GetWorldCameraPosition() exposes. The console's second assert
                // ("lpPlayerInfo") is the null test on that same interior address, which is
                // why it renders as the nonsense `*a2 == -19168` in the export.
                mPlayersTrackerInfo.meIconType =
                    GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR;  // stw 0, 0xC10

                // The console moves the WHOLE 16-byte register (lvx/stvx), w lane and all,
                // so the copy is byte-exact rather than a three-component assignment --
                // Vector4 and Vector3 are both the 16-byte VPU type here.
                const Vector4& lrv4Camera =
                    lpCacheEvent->mpCachePointer->GetWorldCameraPosition();
                std::memcpy(&mPlayersTrackerInfo.mv3Position, &lrv4Camera, 0x10);

                if (mpGuiCache == 0)
                {
                    mpGuiCache = lpCacheEvent->mpCachePointer;
                }
                break;
            }

            // -------------------------------------------------------------------------
            // 165 -- "a landmark was reached". Only acts while a set is being tracked and
            // only when the reached landmark is the one currently being headed for; then
            // advance to the next tracker. Advancing off the end tears the whole set down;
            // otherwise the route is regenerated for the new leg (the console literally
            // branches into case 211's GenerateRouteData tail).
            // -------------------------------------------------------------------------
            case KI_EVENT_LANDMARK_REACHED:
            {
                if (!mbTrackingActive)
                {
                    break;
                }

                const GuiEventLandmarkReached* lpReached =
                    reinterpret_cast<const GuiEventLandmarkReached*>(lpEvent);

                const s32 liTracked = miCurrentlyTrackedIndex;
                if (lpReached->muLandmarkIndex
                        != maTrackerRecords[liTracked].mTargetLandmarkIndex)
                {
                    break;   // `*a2 == *(48*v19 + this + 56)` -- +0x10 + +0x28
                }

                const s32 liNext = liTracked + 1;
                miCurrentlyTrackedIndex = liNext;

                if (liNext != miTrackerCount)
                {
                    GenerateRouteData();   // the shared LABEL_19 tail
                    break;
                }

                // The last tracker was consumed: drop everything.
                mbTrackingActive        = false;   // stb 0, 0(this)
                mbRouteDataPending      = false;   // stb 0, 1(this)
                mbIsEntireRoute         = false;   // stb 0, 2(this)
                miTrackerCount          = 0;       // stw 0, 4(this)
                mRoutePoints.Clear();              // stw 0, this+0x65050
                miCurrentlyTrackedIndex = -1;      // stw -1, this+0x65060
                mfRouteDistance         = 0.0f;    // stfs 0.0f, this+0x65068
                break;
            }

            // -------------------------------------------------------------------------
            // 211 -- one route record has arrived. File it by its own event id, add its
            // distance to the running total, and generate once the expected number have
            // landed. The expected number is `miTrackerCount - 1` (there is no leg into
            // the first tracker -- the player is already at it).
            // -------------------------------------------------------------------------
            case KI_EVENT_ROUTE_INFORMATION:
            {
                const RouteInformation* lpRoute =
                    reinterpret_cast<const RouteInformation*>(lpEvent);

                CGS_ASSERT(static_cast<u32>(lpRoute->miEventId)
                               < static_cast<u32>(KI_TRACKER_STACK_SIZE),
                           "lpRouteInformaton->miEventId >= 0 && "
                           "lpRouteInformaton->miEventId < KI_TRACKER_STACK_SIZE");  // cpp:166

                std::memcpy(&maRouteInformation[lpRoute->miEventId], lpRoute,
                            sizeof(RouteInformation));   // memcpy(.., .., 5136)

                const s32 liExpected = miTrackerCount - 1;          // v22
                const s32 liReceived = miNumRouteInfoReceived + 1;  // v23

                miNumRouteInfoReceived = liReceived;
                mfRouteDistance       += lpRoute->mfRouteDistance;

                if (liReceived >= liExpected)
                {
                    GenerateRouteData();
                }
                else
                {
                    mbRouteDataPending = true;   // stb 1, 1(this)
                }
                break;
            }

            // -------------------------------------------------------------------------
            // 232 -- "here is the whole tracked set" (GuiEventSetTracker, 3088 bytes).
            // An empty set is a teardown; a non-empty one adopts the records and arms the
            // route receive. NOTE mbHasRoute is CLEARED here: the set is not drawable
            // until GenerateRouteData raises it again.
            // -------------------------------------------------------------------------
            case KI_EVENT_SET_TRACKER:
            {
                const GuiEventSetTracker* lpSet =
                    reinterpret_cast<const GuiEventSetTracker*>(lpEvent);

                // ⚠️ The console's test is `cmplwi count, 0x40 / bge -> assert`, i.e. it
                // rejects a count of exactly 64 even though the record array holds 64.
                // That off-by-one is the CONSOLE's; reproduced rather than "corrected".
                // (The same shape appears in the case-211 event-id bound below.)
                CGS_ASSERT(static_cast<u32>(lpSet->miNumTrackedItems)
                               < static_cast<u32>(KI_TRACKER_STACK_SIZE),
                           "Invalid number of trackers");   // cpp:112 (streamed on the console)

                const s32 liNumTracked = lpSet->miNumTrackedItems;
                if (liNumTracked != 0)
                {
                    miTrackerCount          = liNumTracked;
                    miNumRouteInfoReceived  = 0;
                    mbTrackingActive        = true;                      // stb 1, 0(this)
                    mbIsEntireRoute         = lpSet->mbIsEntireRoute;    // from record +0xC08
                    mbHasRoute              = false;                     // stb 0, 3(this)
                    mbRouteDataPending      = (liNumTracked > 1);        // stb (count>1), 1(this)
                    mfRouteDistance         = 0.0f;
                    miCurrentlyTrackedIndex = lpSet->miCurrentlyTrackedIndex;  // record +0xC04

                    // The console's 48-byte-per-record copy loop (six 8-byte moves each).
                    for (s32 liRecord = 0; liRecord < liNumTracked; ++liRecord)
                    {
                        maTrackerRecords[liRecord] = lpSet->mTrackedDataInfo[liRecord];
                    }
                }
                else
                {
                    mbTrackingActive        = false;
                    mbRouteDataPending      = false;
                    mbIsEntireRoute         = false;
                    miTrackerCount          = 0;
                    mRoutePoints.Clear();              // stw 0, this+0x65050
                    mfRouteDistance         = 0.0f;
                    miCurrentlyTrackedIndex = -1;
                }
                break;
            }

            // -------------------------------------------------------------------------
            // 233 -- "the player moved to a new section". Latch the section id and
            // re-base the set around the player.
            // -------------------------------------------------------------------------
            case KI_EVENT_PLAYER_SECTION:
            {
                const GuiEventPlayerSection* lpSection =
                    reinterpret_cast<const GuiEventPlayerSection*>(lpEvent);

                muPlayerTargetSectionId = lpSection->muSectionId;   // stw -> this+0x65064
                RegenerateRouteData();
                break;
            }

            default:
                break;   // the console's `default: break` -- unknown ids are ignored
        }
    }
}
