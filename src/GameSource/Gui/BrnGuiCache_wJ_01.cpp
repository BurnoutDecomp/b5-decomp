// =================================================================================================
// GameSource/Gui/BrnGuiCache_wJ_01.cpp -- the GuiCache MAP-SIDE closure (wave J, 2026-08-29).
//
// This partfile retires the FIVE GuiCache stand-ins that GameSource/Gui/SatNav/
// BrnMainMapLinkGates.cpp has been carrying, plus the two GuiCache link holes the wave's
// census measured. Every body here is reconstructed store-for-store from
// .ida-exports/BURNOUT_X360_ARTIST.XEX; the assert texts and their BrnGuiCache.cpp line
// numbers are the console's own.
//
//   GuiCache::AppendExpectedAptComponentList          @0x824EE538  (+ the helper @0x824ED920)
//   GuiCache::ClearExpectedControlledAptComponentList @0x824EE798
//   GuiCache::GetLandmarkInfoFromIndex                @0x82506688
//   GuiCache::GetLandmarkInfoFromID                   @0x825067E0
//   GuiCache::GetEventDestinationLandmarkIndex        @0x8240FA88  (MOVED, see below)
//   GuiCache::HandleSetActiveLandmarksEvent           @0x824EE7D0
//   GuiCache::UpdateTrackerInfo                       @0x82506F28
//   GuiCache::UpdateTrackerInfoFromOnlineEvent        @0x82507070  (un-named on X360)
//   GuiCache::RefreshMapState                         @0x82510F40
//   GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID @0x825071C8
//
// ⭐ MOUNT CONTRACT -- these MUST be one change with the mount of this file, or the link
// breaks with LNK2005 (a gate and its real body cannot coexist):
//   DELETE from GameSource/Gui/SatNav/BrnMainMapLinkGates.cpp:
//     GuiCache::GetLandmarkInfoFromIndex (:275), GuiCache::GetLandmarkInfoFromID (:302),
//     GuiCache::GetEventDestinationLandmarkIndex (:332), GuiCache::RefreshMapState (:354),
//     GuiCache::HACK_..._SetActiveLandmarksByEventID (:378), and the file-local
//     FillInertIconInfo helper (:118), whose only two callers were the first two.
//     ⚠️ The FOUR REMAINING gates in that file (MainMapComponent::Update / ::SetZoom,
//     MapManager::RecvEvent, MapTransform::CalculateZoomFactor) and the BrnProgression
//     block are NOT ours -- leave them and the file's bat line alone.
//   DELETE from GameSource/Gui/BrnGuiCache_wB_res.cpp: GetEventDestinationLandmarkIndex
//     (:69). That TU is UNMOUNTED so there is no link fault today, but leaving the body
//     there re-arms one the moment anybody mounts wB_res -- and the gate's own banner
//     already scheduled this move. (Done in this change; noted for the reviewer.)
//
// ⛔⛔ ONE HONEST HOLE, NAMED RATHER THAN FAKED -- READ THIS BEFORE SCORING THE WAVE.
// Three of the bodies below end by publishing a 3088-byte GuiEventSetTracker to the sat-nav
// tracker: `GuiTracker::RecEvent(tracker, &record, 232, 3088)`. **GuiTracker::RecEvent
// @0x82501D28 has no body anywhere in the tree**, and bodying it is a THREE-function slice
// in a TU this partfile does not own (its 165/211 arms tail into GuiTracker::GenerateRouteData
// @0x824FA008, its 233 arm into GuiTracker::RegenerateRouteData @0x824F41E0, and its 211 arm
// needs the 5136-byte GuiEventRouteInformation record -- none of the three is reconstructed).
// Writing only the case-232 arm would be an invented body for the other four ids.
// So the publish -- and ONLY the publish -- goes through
// GuiCacheTrackerBoundary::PublishSetTrackerEvent below, which LOGS ONCE and drops
// ([[silent-drop-stubs]]: an absence downstream must never read as a silent success).
// EVERYTHING ELSE IS REAL: the console's asserts fire at the console's conditions, the record
// is built store-for-store, HandleSetActiveLandmarksEvent genuinely latches the cache's
// active-landmark table (which is the leg the crash-nav map's icon manager actually reads),
// and the HACK worker returns the console's count. Retiring the boundary is a two-line edit.
// ⚠️ WHAT A TESTER SEES UNTIL THEN: the map's icons and the active-landmark set are live; the
// tracker ROUTE LINE stays empty, and one `[guicache-tracker-boundary]` line appears in the
// log the first time a map screen or fly-by leaves. That is the whole residual.
//
// ⭐ CONSOLE BEHAVIOUR THAT WILL LOOK LIKE A REGRESSION AFTER THE MOUNT, pre-empted:
// GetLandmarkInfoFromIndex / GetLandmarkInfoFromID now FIRE the console's `lpLandmark`
// assert when the lookup misses, where the deleted gate silently filled a zeroed record.
// That is correct: WorldDataController::GetLandmarkInfoFrom{Index,ID} return NULL on a miss
// and already fire their own diagnostic, and the console asserts on top of it. The asserts
// are non-gating in this build; the callers' behaviour on a miss is unchanged (the record is
// left as the caller staged it), so nothing new is dereferenced.
// ⚠️ AND: HACK_..._SetActiveLandmarksByEventID returns **-1**, not 0, when the event id does
// not resolve -- the deleted gate returned 0. -1 is the console's own no-event answer
// (`li r3, -1` @0x82507230). On this build WorldDataController::GetEventInfoFromEventId
// answers NULL while mpProgressionData is an unbound ResourcePtr, so -1 is what the map will
// actually see, and PreRaceFlyByState::UpdateIconManager's `miPreviousIconCount <
// liNumActiveIcons` test stays false on -1 exactly as it did on 0 -- no spurious chirp.
// =================================================================================================

#include <cstring>   // std::memset (the GetOnlineFinishPoint link gate)

#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the tracker-publish boundary print
#include "GameSource/Gui/BrnGuiWorldDataController.h"        // WorldDataController (the three forwardees)
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"             // GuiTracker + GuiEventSetTracker
#include "GameSource/GameState/BrnGameStateTypes.h"          // BrnGameState::LandmarkIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // SpecificGameModeEventInterface::Event
#include "SharedClasses/Trigger/BrnLandmark.h"               // BrnTrigger::Landmark (COMPLETE: field reads)
#include "SharedClasses/Progression/BrnRaceEventData.h"      // RaceEventData / CheckpointData
#include "SharedClasses/World/BrnWorldRegion.h"              // BrnWorld::WorldRegion::DistrictToCounty

namespace
{
    // ⛔ LINK BOUNDARY, NOT A RECONSTRUCTION -- see the banner's "ONE HONEST HOLE".
    // X360: `lwz r3, 0x4054(cache) ; addi r4, r1, <record> ; li r5, 0xE8 ; li r6, 0xC10 ;
    //        bl BrnGui__GuiTracker__RecEvent`  (@0x82507050, @0x82507378 and the online twin).
    // The three arguments this boundary drops are exactly those four registers.
    // Logs ONCE per process so an empty route line is always attributable.
    // DELETE-WHEN GuiTracker::RecEvent @0x82501D28 lands in
    // GameSource/Gui/SatNav/BrnGuiTracker.cpp: replace the three call sites below with
    //     lpTracker->RecEvent(reinterpret_cast<const CgsModule::Event*>(&lSetTrackerEvent),
    //                         lSetTrackerEvent.GetEventType(), sizeof(lSetTrackerEvent));
    // and delete this namespace. The declaration is already on BrnGuiTracker.h.
    // [FLAG PC bring-up guard, wave J] LOG-ONCE report that the landmark table is not
    // resident. WorldDataController::GetLandmarkInfoFrom{Index,ID} go STRAIGHT through
    // `mpTriggerData->...` with no null path of their own (their owning header says so at
    // BrnGuiWorldDataController.h:140-149), and stage 2/3's "TriggerData" acquire is ANSWERED
    // with a null memory pointer when Triggers.dat is not yet resident -- so an unbound
    // ResourcePtr faults inside the container's operator->, not at a `== 0` test. The two
    // fills below therefore ask HasTriggerData() first. The console has no such test because
    // on the console the resource is always there.
    // ⚠️ The test is HasMemoryResource()-shaped ON PURPOSE: `mpTriggerData == 0` is NOT the
    // same question and would pass on an unbound-but-non-null ResourcePtr.
    // DELETE-WHEN the TriggerData acquire reports a miss as a miss (the same DELETE-WHEN the
    // sibling guard in WorldDataController::GetEventInfoFromEventId already carries).
    void LogAbsentTriggerDataOnce(const char* lpacCaller)
    {
        static bool sbLogged = false;
        if (sbLogged)
        {
            return;
        }
        sbLogged = true;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[guicache-landmark-fill] " << lpacCaller
                << ": WorldDataController has no resident TriggerData -- the landmark record is "
                   "left as the caller staged it [FLAG PC bring-up]\n";
        }
    }

    void PublishSetTrackerEvent(BrnGui::GuiTracker* lpTracker,
                                const BrnGui::GuiEventSetTracker& lrSetTrackerEvent)
    {
        (void)lpTracker;
        (void)lrSetTrackerEvent;

        static bool sbLogged = false;
        if (sbLogged)
        {
            return;
        }
        sbLogged = true;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[guicache-tracker-boundary] BrnGui::GuiTracker::RecEvent(232): no body in "
                   "the tree, the GuiEventSetTracker record is built and dropped -- the sat-nav "
                   "ROUTE LINE will stay empty [FLAG link boundary]\n";
        }
    }
}

namespace BrnGui
{
    // =============================================================================================
    //  1. The apt-component watcher pair (the two GuiCache link holes the census measured)
    // =============================================================================================

    // X360 BrnGui::StateLoadingHelper::AppendExpectedAptComponentList @0x824ED920.
    // Store-for-store. The console's three asserts, in its order, all non-gating:
    //   * `cmplwi flow, 2 ; bls` -> "Invalid GuiFlow of " << flow   (BrnGuiCache.cpp:755)
    //   * `cmplwi count, 0xC0 ; blt` -> the list-full text          (BrnGuiCache.cpp:760)
    //   * `add existing,count ; cmplwi 0xC0 ; ble` -> the total text (BrnGuiCache.cpp:762)
    // The append loop is the X360's `v14[++*v14] = *v19++` -- the ids array starts one word
    // after the count, so writing at the PRE-increment count is an append at the old length.
    // ⚠️ The streamed flow value is dropped from the first message: CGS_ASSERT takes a plain
    // `const char*`, which is this tree's standing lowering for the StrStream asserts.
    void StateLoadingHelper::AppendExpectedAptComponentList(GuiFlow leFlow,
                                                            const u32* lpauComponentNameHashes,
                                                            u32 luCount)
    {
        CGS_ASSERT(static_cast<u32>(leFlow) <= 2u, "Invalid GuiFlow of ");   // cpp:755

        ComponentsToWatch& lrWatch = maComponentsToWatch[leFlow];

        CGS_ASSERT(luCount < ComponentsToWatch::KU_MAX_COMPONENTS_TO_WATCH,
                   "Component list is full. Consider increasing the size, or are we doing "
                   "something silly?");                                       // cpp:760
        CGS_ASSERT(lrWatch.muNumberOfComponentsToWatch + luCount
                       <= ComponentsToWatch::KU_MAX_COMPONENTS_TO_WATCH,
                   "Too many components to watch Consider increasing the size, or are we doing "
                   "something silly?");                                       // cpp:762

        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        {
            lrWatch.mauComponentsToWatchIds[lrWatch.muNumberOfComponentsToWatch] =
                lpauComponentNameHashes[luIndex];
            ++lrWatch.muNumberOfComponentsToWatch;
        }
    }

    // X360 BrnGui::GuiCache::AppendExpectedAptComponentList @0x824EE538 -- a pure
    // `addi r3, r3, 8` + tail-branch into the helper, exactly like the Set / Clear faces
    // already in BrnGuiCache.cpp.
    void GuiCache::AppendExpectedAptComponentList(GuiFlow leFlow,
                                                  const u32* lpauComponentNameHashes,
                                                  u32 luCount)
    {
        mStateLoadingHelper.AppendExpectedAptComponentList(leFlow, lpauComponentNameHashes,
                                                           luCount);
    }

    // X360 BrnGui::GuiCache::ClearExpectedControlledAptComponentList @0x824EE798 -- the whole
    // body is `li r11, 0 ; stw r11, 0x404C(r3) ; blr`. cache+0x404C is the embedded helper's
    // muControlledComponentCount (helper base +0x8, its tail is count @+0x4044 /
    // pending-unload @+0x4048), so on the host it goes through the named face.
    // ⚠️ It clears the COUNT ONLY -- the mpaControlledComponents / muControlledComponentNameHash
    // slots keep their contents, to be overwritten by the next AppendExpectedControlledObject.
    // That is the console's behaviour, not an omission.
    void GuiCache::ClearExpectedControlledAptComponentList()
    {
        mStateLoadingHelper.ClearControlledComponentList();
    }

    // =============================================================================================
    //  2. The two landmark-info fills
    // =============================================================================================

    // X360 BrnGui::GuiCache::GetLandmarkInfoFromIndex @0x82506688. Store-for-store.
    //
    // The out-record fill, in the console's own order (offsets are SatNavIconInfo's):
    //   +0x00  the 16-byte lane  = { landmark position x, y, z, 0 }  (three `lfs` off the
    //          landmark's BoxRegion + a `stw 0` for the w lane, then one `stvx128`)
    //   +0x18  mfRotation  = 0.0f   } both from flt_82001CC0
    //   +0x1C  mfSpeedMph  = 0.0f   }
    //   +0x10  mCgsId      = sign-extended `lwz 0x24(landmark)`, i.e. TriggerRegion::GetId()
    //          stored WHOLE with an `std` (the Xenon ABI's 64-bit CgsID)
    //   +0x25  mu8District = `lbz 0x32(landmark)`, i.e. Landmark::GetDistrict()
    //   +0x24  mu8County   = DistrictToCounty(GetDistrict())   -- read back off the record
    //   +0x20  the landmark half = THE CALLER'S INDEX (`sth r27`), not the landmark's own
    //   +0x28  mi8IconType = 4 == E_SATNAVICON_LANDMARK
    //   +0x26  mi8ActiveRaceCarIndex = -1
    //   +0x22  mu8DesignIndex = `lbz 0x31(landmark)`
    // The two range asserts (`leDistrict >= 0` BrnGuiEventTypeDefs.h:1873, `leCounty >= 0`
    // :1857) are inside the SetDistrict / SetCounty faces, which is where the console inlines
    // them -- so they are reproduced by calling those setters rather than by hand.
    //
    // ⚠️ THE COUNTY IS DERIVED FROM THE RECORD, NOT FROM THE LOCAL. The console calls
    // SatNavIconInfo::GetDistrict(icon) AFTER storing the district byte and feeds THAT to
    // DistrictToCounty. Same value either way today, but the round-trip is the console's and
    // it is what makes the `leDistrict >= 0` assert load-bearing; kept verbatim.
    //
    // Returns lpOutIconInfo. ⚠️ The X360's r3 at return is the DistrictToCounty leftover -- a
    // decompiler artifact, not a result; the DWARF return is the out pointer.
    GuiEventUpdateSatNav::SatNavIconInfo*
        GuiCache::GetLandmarkInfoFromIndex(BrnGameState::LandmarkIndex lLandmarkIndex,
                                           GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const
    {
        CGS_ASSERT(mpWorldDataController != 0, "mpWorldDataController");   // cpp:3620

        // [FLAG PC bring-up guard] see LogAbsentTriggerDataOnce above.
        if (mpWorldDataController == 0 || !mpWorldDataController->HasTriggerData())
        {
            LogAbsentTriggerDataOnce("GuiCache::GetLandmarkInfoFromIndex");
            return lpOutIconInfo;
        }

        const BrnTrigger::Landmark* lpLandmark =
            mpWorldDataController->GetLandmarkInfoFromIndex(lLandmarkIndex);
        CGS_ASSERT(lpLandmark != 0, "lpLandmark");                         // cpp:3623

        // [FLAG PC bring-up guard, wave J] The console derefs lpLandmark unconditionally --
        // its assert is a report, not a gate, and this build's asserts are non-gating. The
        // lookup CAN answer NULL here (WorldDataController::GetLandmarkInfoFromIndex returns
        // NULL on a miss, and its own diagnostic has already fired), so a null deref would
        // turn a reported miss into a crash. The caller's record is left exactly as it staged
        // it, which is what the console's non-fatal path effectively leaves too.
        // DELETE-WHEN the trigger-data landmark table is populated on this build.
        if (lpLandmark == 0)
        {
            return lpOutIconInfo;
        }

        // The three `lfs` off the landmark's BoxRegion position + `stw 0` for the w lane,
        // then one `stvx128` -- the whole 16-byte lane in one store.
        const Vector3 lv3LandmarkPosition = lpLandmark->GetBoxRegion()->GetPosition();
        const Vector4 lv4PositionLane = { lv3LandmarkPosition.x, lv3LandmarkPosition.y,
                                          lv3LandmarkPosition.z, 0.0f };
        lpOutIconInfo->SetPositionLane(lv4PositionLane);                    // stvx128 -> +0x00
        lpOutIconInfo->SetRotation(0.0f);                                   // stfs    -> +0x18
        lpOutIconInfo->SetSpeedMph(0.0f);                                   // stfs    -> +0x1C
        lpOutIconInfo->SetCgsId(lpLandmark->GetId());                       // std     -> +0x10
        lpOutIconInfo->SetDistrict(
            static_cast<BrnWorld::EDistrict>(lpLandmark->GetDistrict()));   // stb     -> +0x25
        lpOutIconInfo->SetCounty(
            BrnWorld::WorldRegion::DistrictToCounty(lpOutIconInfo->GetDistrict()));  // -> +0x24
        lpOutIconInfo->SetLandmarkIndexHalf(
            static_cast<s16>(static_cast<s32>(lLandmarkIndex)));            // sth     -> +0x20
        lpOutIconInfo->SetIconType(
            GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK);   // stb 4   -> +0x28
        lpOutIconInfo->SetActiveRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_INVALID); // stb -1 -> +0x26
        lpOutIconInfo->SetDesignIndex(lpLandmark->GetDesignIndex());        // stb     -> +0x22

        return lpOutIconInfo;
    }

    // X360 BrnGui::GuiCache::GetLandmarkInfoFromID @0x825067E0. The id sibling of the above:
    // byte-identical except for the two differences the asm shows --
    //   * the forwardee is WorldDataController::GetLandmarkInfoFromID (asserts at
    //     BrnGuiCache.cpp:3654 / :3657 instead of :3620 / :3623), and
    //   * the +0x20 half-word is `lhz 0x28(landmark)` -- the RESOLVED landmark's own
    //     TriggerRegion region index -- because the caller supplied an id, not an index.
    // Return type is `void` per DWARF (BrnGuiCache.h:798); r3 at return is the
    // DistrictToCounty leftover, NOT a result. Do not resurrect a return value.
    void GuiCache::GetLandmarkInfoFromID(CgsID lLandmarkID,
                                         GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const
    {
        CGS_ASSERT(mpWorldDataController != 0, "mpWorldDataController");   // cpp:3654

        // [FLAG PC bring-up guard] see LogAbsentTriggerDataOnce above.
        if (mpWorldDataController == 0 || !mpWorldDataController->HasTriggerData())
        {
            LogAbsentTriggerDataOnce("GuiCache::GetLandmarkInfoFromID");
            return;
        }

        const BrnTrigger::Landmark* lpLandmark =
            mpWorldDataController->GetLandmarkInfoFromID(lLandmarkID);
        CGS_ASSERT(lpLandmark != 0, "lpLandmark");                         // cpp:3657

        // [FLAG PC bring-up guard, wave J] -- same guard, same reason, as the index sibling.
        if (lpLandmark == 0)
        {
            return;
        }

        const Vector3 lv3LandmarkPosition = lpLandmark->GetBoxRegion()->GetPosition();
        const Vector4 lv4PositionLane = { lv3LandmarkPosition.x, lv3LandmarkPosition.y,
                                          lv3LandmarkPosition.z, 0.0f };
        lpOutIconInfo->SetPositionLane(lv4PositionLane);
        lpOutIconInfo->SetRotation(0.0f);
        lpOutIconInfo->SetSpeedMph(0.0f);
        lpOutIconInfo->SetCgsId(lpLandmark->GetId());
        lpOutIconInfo->SetDistrict(
            static_cast<BrnWorld::EDistrict>(lpLandmark->GetDistrict()));
        lpOutIconInfo->SetCounty(
            BrnWorld::WorldRegion::DistrictToCounty(lpOutIconInfo->GetDistrict()));
        lpOutIconInfo->SetIconType(
            GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK);
        lpOutIconInfo->SetLandmarkIndexHalf(
            static_cast<s16>(lpLandmark->GetRegionIndex()));               // lhz 0x28(lm) -> +0x20
        lpOutIconInfo->SetActiveRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_INVALID);
        lpOutIconInfo->SetDesignIndex(lpLandmark->GetDesignIndex());
    }

    // =============================================================================================
    //  3. The event-destination accessor (MOVED here from the unmounted BrnGuiCache_wB_res.cpp:69)
    // =============================================================================================

    // X360 BrnGui::GuiCache::GetEventDestinationLandmarkIndex @0x8240FA88. Two non-gating
    // asserts, both sited in the HEADER on the console (BrnGuiCache.h:4055 / :4057):
    //   * the race-style game-mode gate. The asm's test is `mode >= 2 && mode != 10 && mode != 6
    //     && mode != 8 && mode != 5` -> fire; i.e. the PASSING set is {0, 1, 5, 6, 8, 10}, which
    //     is the assert text's OFFLINE_RACE / FACE_OFF / ONLINE_RACE / ELIMINATOR / MARKED_MAN /
    //     BURNING_ROUTE. Spelled as the positive membership test here.
    //   * the sentinel gate: the stored index must not be K_INVALID_LANDMARK (word_82F25440,
    //     the image's 0xFFFF).
    // Then `*out = mEventDestinationLandmarkIndex` (@0x9F4C, a raw u16 copy).
    BrnGameState::LandmarkIndex GuiCache::GetEventDestinationLandmarkIndex() const
    {
        CGS_ASSERT(
            (meGameModeType == 0) || (meGameModeType == 1) || (meGameModeType == 10)
                || (meGameModeType == 6) || (meGameModeType == 8) || (meGameModeType == 5),
            "( meGameModeType == GsmIO::E_MODE_OFFLINE_RACE ) || ( meGameModeType == "
            "GsmIO::E_MODE_FACE_OFF ) || ( meGameModeType == GsmIO::E_MODE_ONLINE_RACE ) || "
            "( meGameModeType == GsmIO::E_MODE_ELIMINATOR ) || ( meGameModeType == "
            "GsmIO::E_MODE_MARKED_MAN ) || ( meGameModeType == GsmIO::E_MODE_BURNING_ROUTE )");
        CGS_ASSERT(mEventDestinationLandmarkIndex != 0xFFFFu,
                   "mEventDestinationLandmarkIndex != BrnGameState::K_INVALID_LANDMARK");
        return BrnGameState::LandmarkIndex(mEventDestinationLandmarkIndex);
    }

    // =============================================================================================
    //  4. The active-landmark latch
    // =============================================================================================

    // X360 BrnGui::GuiCache::HandleSetActiveLandmarksEvent @0x824EE7D0. Store-for-store:
    // one non-gating bound assert (BrnGuiCache.cpp:4067), the copy loop into the +0x5288
    // table, then the count with a BYTE store into +0x5286.
    //
    // ⭐ THE COUNT STORE IS A BYTE ON PURPOSE (`stb r11, 0x5286`). A list longer than 255 is
    // TRUNCATED in the count while all its entries are written -- shipped console behaviour,
    // preserved deliberately (see the muNumActiveLandmarks note on BrnGuiCache.h). The copy
    // loop is `do { ... } while (++i < count)`, so a zero count copies nothing.
    void GuiCache::HandleSetActiveLandmarksEvent(
        const GuiEventSetActiveLandmarks* lpActiveLandmarksEvent)
    {
        CGS_ASSERT(lpActiveLandmarksEvent->muNumLandmarks
                       <= GuiEventSetActiveLandmarks::KU_MAX_LANDMARKS_IN_GAME,
                   "lpActiveLandmarksEvent->muNumLandmarks <= static_cast<uint32_t>( "
                   "KI_MAX_LANDMARKS_IN_GAME )");                           // cpp:4067

        for (u32 luIndex = 0; luIndex < lpActiveLandmarksEvent->muNumLandmarks; ++luIndex)
        {
            mau16ActiveLandmarks[luIndex] = static_cast<u16>(
                static_cast<s32>(lpActiveLandmarksEvent->maLandmarkIndices[luIndex]));
        }

        muNumActiveLandmarks =
            static_cast<u8>(lpActiveLandmarksEvent->muNumLandmarks);        // stb 0x5286
    }

    // =============================================================================================
    //  5. The two tracker publishers + RefreshMapState
    // =============================================================================================

    // X360 BrnGui::GuiCache::UpdateTrackerInfo @0x82506F28. Store-for-store.
    //
    // Builds a GuiEventSetTracker on the stack:
    //   mbIsEntireRoute       = true        (`stb 1` BEFORE the loop)
    //   miCurrentlyTrackedIndex = 0         (`stw 0`, likewise before the loop)
    //   per element i in [0, liCount):
    //       GetLandmarkInfoFromIndex(lpLandmarkIndices[i], &lIconInfo)
    //       item.meIconType           = 4   (`stw r27` where r27 == 4)
    //       item.mv3Position          = the icon record's 16-byte lane (lvx128/stvx128)
    //       item.mTargetLandmarkIndex = the icon record's +0x20 half-word
    //   miNumTrackedItems     = liCount     (`stw r25` AFTER the loop)
    // then asserts the tracker pointer and publishes with RecEvent(&record, 232, 3088).
    //
    // ⚠️ THE RECORD IS NOT ZEROED FIRST -- neither is the console's stack copy. Items beyond
    // miNumTrackedItems carry whatever the frame held, and that is safe because RecEvent's
    // case-232 arm copies EXACTLY miNumTrackedItems records. Faithfully reproduced (a
    // defensive memset here would be a divergence, not a fix); the local is left
    // default-initialised for the same reason the console leaves its frame alone.
    void GuiCache::UpdateTrackerInfo(const u16* lpLandmarkIndices, s32 liCount)
    {
        CGS_ASSERT(lpLandmarkIndices != 0, "lpLandmarkIndices");            // cpp:3913

        GuiEventSetTracker lSetTrackerEvent;
        lSetTrackerEvent.miCurrentlyTrackedIndex = 0;
        lSetTrackerEvent.mbIsEntireRoute         = true;

        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
            GetLandmarkInfoFromIndex(BrnGameState::LandmarkIndex(lpLandmarkIndices[liIndex]),
                                     &lLandmarkInfo);

            GuiTracker::TrackerInformation& lrItem = lSetTrackerEvent.mTrackedDataInfo[liIndex];
            lrItem.meIconType = GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK;
            // `lvx128 v0, r0, <icon> ; stvx128 v0, <item>, -0x18` -- the WHOLE 16-byte
            // lane, w included, not a three-component narrow.
            const Vector4& lrv4Lane = lLandmarkInfo.GetPositionLane();
            lrItem.mv3Position.x = lrv4Lane.x;
            lrItem.mv3Position.y = lrv4Lane.y;
            lrItem.mv3Position.z = lrv4Lane.z;
            lrItem.mv3Position.w = lrv4Lane.w;
            lrItem.mTargetLandmarkIndex =
                static_cast<u16>(lLandmarkInfo.GetLandmarkIndexHalf());
        }

        lSetTrackerEvent.miNumTrackedItems = liCount;

        CGS_ASSERT(mpGuiTracker != 0, "Invalid tracker pointer");           // cpp:3932
        PublishSetTrackerEvent(mpGuiTracker, lSetTrackerEvent);            // [FLAG link boundary]
    }

    // X360 sub_82507070 (NO SYMBOL -- the name below is ours, see the header note). The ONLINE
    // twin of UpdateTrackerInfo: identical record, identical publish, but the landmark indices
    // come from a round's SpecificGameModeEventInterface::Event -- count from the event's
    // `lwz +0x24` (miNumLandmarks) and each index from Event::GetLandmark(i) @0x8240E7E0.
    // Asserts lpEvent (cpp:3947) and the tracker pointer (cpp:3967).
    void GuiCache::UpdateTrackerInfoFromOnlineEvent(
        const BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");                                // cpp:3947

        // [FLAG PC bring-up guard, wave J] the console derefs immediately; the assert is
        // non-gating here. Only RefreshMapState's online arm reaches this, and it hands a
        // pointer into the cache's own +0xA800 mirror, so a null is not expected -- the guard
        // exists only so a non-gating assert cannot become a crash. DELETE-WHEN asserts gate.
        if (lpEvent == 0)
        {
            return;
        }

        const u32 luNumLandmarks = static_cast<u32>(lpEvent->GetNumLandmarks());

        GuiEventSetTracker lSetTrackerEvent;
        lSetTrackerEvent.miCurrentlyTrackedIndex = 0;
        lSetTrackerEvent.mbIsEntireRoute         = true;

        for (u32 luIndex = 0; luIndex < luNumLandmarks; ++luIndex)
        {
            GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
            GetLandmarkInfoFromIndex(lpEvent->GetLandmark(static_cast<s32>(luIndex)),
                                     &lLandmarkInfo);

            GuiTracker::TrackerInformation& lrItem = lSetTrackerEvent.mTrackedDataInfo[luIndex];
            lrItem.meIconType = GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK;
            // `lvx128 v0, r0, <icon> ; stvx128 v0, <item>, -0x18` -- the WHOLE 16-byte
            // lane, w included, not a three-component narrow.
            const Vector4& lrv4Lane = lLandmarkInfo.GetPositionLane();
            lrItem.mv3Position.x = lrv4Lane.x;
            lrItem.mv3Position.y = lrv4Lane.y;
            lrItem.mv3Position.z = lrv4Lane.z;
            lrItem.mv3Position.w = lrv4Lane.w;
            lrItem.mTargetLandmarkIndex =
                static_cast<u16>(lLandmarkInfo.GetLandmarkIndexHalf());
        }

        lSetTrackerEvent.miNumTrackedItems = static_cast<s32>(luNumLandmarks);

        CGS_ASSERT(mpGuiTracker != 0, "Invalid tracker pointer");           // cpp:3967
        PublishSetTrackerEvent(mpGuiTracker, lSetTrackerEvent);            // [FLAG link boundary]
    }

    // X360 BrnGui::GuiCache::RefreshMapState @0x82510F40. The whole body is the two-way
    // dispatch below -- eight pseudocode lines, three functions deep:
    //   if (mbOnlineStartInProgress)   // `lwz 0x4B4C`
    //       sub_82507070(this, &maOnlineGameModeOptions[miOnlineRoundIndex]);
    //                                  // `44 * *(this+0xA7FC) + this + 0xA800`
    //   else
    //       UpdateTrackerInfo(this, &maCheckpointLandmarks[0], muCheckpointsInEvent);
    //                                  // `this+0x9F54`, count `this+0x9FB8`
    // ⚠️ NO ASSERTS OF ITS OWN, and no gate on the count -- an event with zero checkpoints
    // legitimately publishes an EMPTY tracker record, which is how the console clears the
    // route line. Do not add an early-out.
    // ⚠️ The online arm's stride 44 is sizeof(SpecificGameModeEventInterface::Event); the
    // +0xA800 mirror is that Event array (see GetOnlineLandmarkIndex @0x8240FB50, which
    // indexes it identically).
    void GuiCache::RefreshMapState()
    {
        if (mbOnlineStartInProgress)
        {
            typedef BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event
                OnlineModeEvent;
            const OnlineModeEvent* lpOnlineGameModeOptions =
                reinterpret_cast<const OnlineModeEvent*>(maOnlineGameModeOptionsStorage);
            UpdateTrackerInfoFromOnlineEvent(&lpOnlineGameModeOptions[miOnlineRoundIndex]);
        }
        else
        {
            UpdateTrackerInfo(maCheckpointLandmarks,
                              static_cast<s32>(muCheckpointsInEvent));
        }
    }

    // =============================================================================================
    //  6. The HACK worker -- the map's active-landmark re-latch
    // =============================================================================================

    // X360 BrnGui::GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID @0x825071C8.
    // Store-for-store, in the console's order.
    //
    // ⭐ THE PPC FLOAT-ARG GPR SKIP IS WHY HEX-RAYS HIDES THE THIRD ARGUMENT (THE recurring
    // campaign bug, already flagged on the header): at the call site r4 = the event id,
    // f1 = the clamped animation t, r6 = the bool -- r5 is DEAD because the float skips its
    // GPR slot. The signature is `s32 (u32, f32, bool)` (DWARF BrnGuiCache.h:1476) and the
    // asm confirms it: `fmr f31, f1` @0x825071E0 and `mr r30, r6` @0x825071E8.
    //
    // Shape:
    //   assert mpWorldDataController                                (BrnGuiCache.h:2324)
    //   lpEventData = WorldDataController::GetEventInfoFromEventId(luEventID)
    //   if (!lpEventData) return -1;                                (`li r3, -1`)
    //   lSetTrackerEvent.mbIsEntireRoute = true
    //   miCurrentlyTrackedIndex = (lbFlag == 1) ? GetCheckpointReached() : 0
    //       with the console's `>= 0` assert on the former            (cpp:4204)
    //   liNumTracked = (s32)((f32)lpEventData->GetCheckpointCount() * lfT)
    //       -- fcfid/frsp/fmuls/fctiwz: a truncating conversion, and `t` is the fraction of
    //          the event's checkpoints to reveal
    //   if (liNumTracked == 0) both counts = 0
    //   else { both counts = liNumTracked; assert <= 64               (cpp:4227)
    //          for i: GetLandmarkInfoFromID(GetCheckpointData(i)->GetLandmarkId(), &info)
    //                 item.meIconType = 4; item.mv3Position = info lane
    //                 lActiveLandmarks.maLandmarkIndices[i] = info +0x20 half
    //                 item.mTargetLandmarkIndex             = the SAME half }
    //   HandleSetActiveLandmarksEvent(&lActiveLandmarks)
    //   mpGuiTracker->RecEvent(&lSetTrackerEvent, 232, 3088)
    //   return liNumTracked
    //
    // ⚠️ THE `<= 64` ASSERT IS NON-GATING AND THE WRITE THAT FOLLOWS IS NOT BOUNDED BY IT ON
    // THE CONSOLE. lSetTrackerEvent.mTrackedDataInfo holds KI_TRACKER_STACK_SIZE == 64 items
    // and liNumTracked is `checkpointCount * t` -- a >64-checkpoint event would run off the
    // stack record on the console too. This build's asserts do not halt, so the loop below is
    // clamped to the array bound with an explicit FLAG rather than reproducing a stack smash:
    // that is the one deliberate divergence in this function, and it changes nothing for any
    // shipped event (the checkpoint tables are far shorter than 64).
    s32 GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID(u32 luEventID, f32 lfT,
                                                                        bool lbFlag)
    {
        CGS_ASSERT(mpWorldDataController != 0, "mpWorldDataController");    // BrnGuiCache.h:2324

        // [FLAG PC bring-up guard] the console derefs straight through; the assert above is
        // non-gating here. -1 is the console's own no-event answer, so this returns exactly
        // what a missing event returns rather than inventing a third outcome.
        if (mpWorldDataController == 0)
        {
            return -1;
        }

        const BrnProgression::RaceEventData* lpEventData =
            mpWorldDataController->GetEventInfoFromEventId(luEventID);
        if (lpEventData == 0)
        {
            // `li r3, -1` @0x82507230 -- the console's own "no such event" answer. NOT 0:
            // 0 would read as "an event with no active landmarks", which is a different fact.
            return -1;
        }

        GuiEventSetTracker lSetTrackerEvent;
        lSetTrackerEvent.mbIsEntireRoute = true;                            // stb 1 (before all)

        if (lbFlag)
        {
            lSetTrackerEvent.miCurrentlyTrackedIndex = GetCheckpointReached();
            CGS_ASSERT(lSetTrackerEvent.miCurrentlyTrackedIndex >= 0,
                       "lSetTrackerEvent.miCurrentlyTrackedIndex >= 0");    // cpp:4204
        }
        else
        {
            lSetTrackerEvent.miCurrentlyTrackedIndex = 0;
        }

        // fcfid / frsp / fmuls / fctiwz -- the count scaled by the animation parameter and
        // TRUNCATED toward zero. Reproduced as the same widen-multiply-truncate chain.
        const s32 liNumTracked = static_cast<s32>(
            static_cast<f32>(lpEventData->GetCheckpointCount()) * lfT);

        GuiEventSetActiveLandmarks lActiveLandmarksEvent;
        lActiveLandmarksEvent.muNumLandmarks = 0;
        lSetTrackerEvent.miNumTrackedItems   = 0;

        if (liNumTracked != 0)
        {
            lActiveLandmarksEvent.muNumLandmarks = static_cast<u32>(liNumTracked);
            lSetTrackerEvent.miNumTrackedItems   = liNumTracked;

            CGS_ASSERT(liNumTracked <= GuiTracker::KI_TRACKER_STACK_SIZE,
                       "lSetTrackerEvent.miNumTrackedItems <= "
                       "GuiTracker::KI_TRACKER_STACK_SIZE");                // cpp:4227

            // [FLAG deliberate divergence -- see the banner above] the console walks to
            // liNumTracked regardless; we stop at the record's own capacity so a
            // non-gating assert cannot become a stack overrun.
            s32 liWriteLimit = liNumTracked;
            if (liWriteLimit > GuiTracker::KI_TRACKER_STACK_SIZE)
            {
                liWriteLimit = GuiTracker::KI_TRACKER_STACK_SIZE;
            }

            for (s32 liIndex = 0; liIndex < liWriteLimit; ++liIndex)
            {
                const BrnProgression::CheckpointData* lpCheckpoint =
                    lpEventData->GetCheckpointData(liIndex);

                GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
                GetLandmarkInfoFromID(static_cast<CgsID>(lpCheckpoint->GetLandmarkId()),
                                      &lLandmarkInfo);

                GuiTracker::TrackerInformation& lrItem =
                    lSetTrackerEvent.mTrackedDataInfo[liIndex];
                lrItem.meIconType = GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK;
                // The whole 16-byte lane (lvx128/stvx128), w included.
                const Vector4& lrv4Lane = lLandmarkInfo.GetPositionLane();
                lrItem.mv3Position.x = lrv4Lane.x;
                lrItem.mv3Position.y = lrv4Lane.y;
                lrItem.mv3Position.z = lrv4Lane.z;
                lrItem.mv3Position.w = lrv4Lane.w;

                // The SAME half-word lands in both records (`sth r11, 0(r29)` into the
                // active-landmark list and `sth r11, 0(r31)` into the tracker item).
                const s16 li16LandmarkIndex = lLandmarkInfo.GetLandmarkIndexHalf();
                lActiveLandmarksEvent.maLandmarkIndices[liIndex] =
                    BrnGameState::LandmarkIndex(li16LandmarkIndex);
                lrItem.mTargetLandmarkIndex = static_cast<u16>(li16LandmarkIndex);
            }
        }

        HandleSetActiveLandmarksEvent(&lActiveLandmarksEvent);

        CGS_ASSERT(mpGuiTracker != 0, "Invalid tracker pointer");
        PublishSetTrackerEvent(mpGuiTracker, lSetTrackerEvent);            // [FLAG link boundary]

        return liNumTracked;
    }

    // =============================================================================================
    // @0x8241E4C8 -- GuiCache::GetNumEventStarts. The live count of registered event-start
    // records, read by CrashNavIconRenderer::GetNumIcons @0x82456C68 and by
    // OnlineSelectRoute::UpdateAfterToggleChange @0x8249E558.
    //
    // ⭐ ADDRESS + SHAPE CORRECTION (this wave). BrnGuiCache_GetNumEventStarts.cpp (UNMOUNTED)
    // carried this as a tail-FORWARDER to SetUpAllEventStartsInterface::GetNumEventStarts, on
    // the strength of a 0x824F8830 attribution the header has since retired. The real export
    // at 0x8241E4C8 forwards to nothing at all -- it is four instructions plus the array
    // guard:
    //     addi r31, r3, 0x5690      ; &mSetUpAllEventStartsInterface
    //     lwz  r11, 0x20D0(r31)     ; interface + 0x20D0 == cache + 0x7760 == miEventStartsCount
    //     cmpwi r11, -1 / bne       ; the CgsArray "not constructed" sentinel
    //     <BeginAssert / FireAssert("Array used before Construct/Clear was called",
    //                               "..\..\..\GameShared\GameClasses\Containers/CgsArray.h", 336)
    //      / EndAssert>
    //     lwz  r3, 0x20D0(r31)      ; and return it (RE-LOADED after the assert, not cached)
    // i.e. the count member is read BY NAME here, exactly as the already-inline
    // GetEventStart(index) twin next to it in the header reads it. That also removes the
    // include clash the forwarder was split into its own TU for: this body needs no
    // BrnGameStateSharedIO.h type at all, so it belongs in this partfile.
    //
    // The assert is non-gating in this build (CGS_ASSERT), and the console likewise falls
    // through and returns the sentinel -- reproduced rather than "fixed": a -1 answer is the
    // console's own report that nothing ever called Construct on the array.
    // =============================================================================================
    u32 GuiCache::GetNumEventStarts() const
    {
        CGS_ASSERT(miEventStartsCount != -1,
                   "Array used before Construct/Clear was called");   // CgsArray.h:336
        return static_cast<u32>(miEventStartsCount);
    }

    // =============================================================================================
    // @0x8241E7D8 -- GuiCache::GetNumOnlineFinishPoints. Total set bits across the 256-bit
    // online finish-point bitmask (maOnlineFinishPointsMask @+0x7770, four doublewords loaded
    // by the console at 0x7770 / 0x7778 / 0x7780 / 0x7788). Each word gets the classic 5-step
    // 64-bit SWAR population count (`srdi 1 / and / subf`, `srdi 2 / and / and / add`,
    // `srdi 4 / add / and`, `mulld 0x0101010101010101 / srdi 56`) and the four counts are
    // summed. No assert, no branch -- the whole function is straight-line.
    //
    // The step-1 and step-2 masks the X360 materialises carry redundant HIGH bits set
    // (0xD555555555555555 and 0xF333333333333333 rather than 0x5555... / 0x3333...); those
    // extra bits only ever meet bits the shift has already zeroed, so each step is the
    // canonical popcount step exactly. Kept as the console's constants rather than tidied,
    // so the store-for-store reading is checkable against the asm.
    //
    // MOVED HERE this wave from the UNMOUNTED BrnGuiCache_wB_res.cpp (see the note left in
    // that file). The two cannot coexist -- LNK2005.
    // =============================================================================================
    u32 GuiCache::GetNumOnlineFinishPoints() const
    {
        u32 luFinishPointCount = 0;
        for (s32 liWord = 0; liWord < 4; ++liWord)
        {
            u64 luBits = maOnlineFinishPointsMask[liWord];
            luBits = luBits - ((luBits >> 1) & 0xD555555555555555ULL);
            luBits = ((luBits >> 2) & 0xF333333333333333ULL) + (luBits & 0x3333333333333333ULL);
            luBits = (luBits + (luBits >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
            luFinishPointCount += static_cast<u32>((luBits * 0x0101010101010101ULL) >> 56);
        }
        return luFinishPointCount;
    }
    // =============================================================================================
    // X360 BrnGui::GuiCache::GetOnlineFinishPoint @0x82506940 (json name field verified).
    // ⛔ LINK GATE, NOT A RECONSTRUCTION -- relocated here 2026-08-29 when
    // BrnMainMapLinkGates.cpp (its old home) died with its last real-body retirement, matching
    // the flyby wave's deletion of that file. The real body asserts mpWorldDataController,
    // popcount-walks the 256-bit maOnlineFinishPointsMask @+0x7770 to the liIndex-th set bit,
    // resolves it through the events-with-unique-finish list (cache+0x8040) and
    // WorldDataController::GetLandmarkInfoFromIndex, and fills the out record. Its ONLY caller
    // (CrashNavIconRenderer::GetIconInformation's ONLINE_FINISH_POINTS arm,
    // BrnCrashNavIconRenderer_wK_01.cpp) iterates CountSetBits() of that same mask -- ZERO
    // offline -- so the gate is unreachable in the offline milestone; it exists to close the
    // link. RETURN VALUE = lpOutIconInfo, record ZEROED (deterministic; no reachable caller
    // consumes the zeros). [FLAG link scaffold]
    // DELETE-WHEN the real body lands (the popcount walk + the 0x8040-list element lookup).
    // =============================================================================================
    GuiEventUpdateSatNav::SatNavIconInfo*
    GuiCache::GetOnlineFinishPoint(s32 /*liIndex*/,
                                   GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const
    {
        static bool sbLoggedGate = false;
        if (!sbLoggedGate)
        {
            sbLoggedGate = true;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[gui-cache-gate] BrnGui::GuiCache::GetOnlineFinishPoint: inert"
                       " stand-in, no body anywhere in the tree [FLAG link scaffold]\n";
            }
        }
        if (lpOutIconInfo != 0)
        {
            std::memset(lpOutIconInfo, 0, sizeof(*lpOutIconInfo));
        }
        return lpOutIconInfo;
    }
}
