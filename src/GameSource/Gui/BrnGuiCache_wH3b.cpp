// BrnGuiCache_wH3b.cpp -- the sat-nav minimap slice's GuiCache leg (HUD H3b, 2026-08-25).
// The "bodies link from the GuiCache TU" rows the SatNavRenderer / MapIconManager mounts
// pulled onto the link closure:
//   GetPresetEventDisplayInfo        @ 0x824F8838   GetProfileEventDisplayInfo @ 0x824F8AF0
//   GetDriveThrough                  (X360-inlined; offsets from GetDriveThroughOrJunkyard-
//   GetNumberOfDriveThroughs          AtIndex @0x824FAC10: entries cache+0x7790 stride 0x30,
//                                     count cache+0x8030, bound assert BrnGuiCache.h:5164)
//   GetOnlineLandmarkInfoAtPositionInList
//   PresetEvent::GetPositionLookupId / GetEventId (word +0x20 / +0x28 of the 0x2C record)
//
// Recon: scratch h3b_dump8/9/10.txt (decomp + asm; the maEventStarts stride-48 indexer
// @0x824F65E0 pins the display-record stride, the mEvents 7700-byte storage / 175 cap
// pins the preset stride 0x2C).

#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameSource/Gui/BrnGuiWorldDataController.h"       // WorldDataController (the online-landmark forward)
#include "SharedClasses/Trigger/BrnLandmark.h"                 // BrnTrigger::Landmark (COMPLETE: field reads)
#include "SharedClasses/World/BrnWorldRegion.h"                // BrnWorld::WorldRegion::DistrictToCounty

namespace BrnGui
{

// @ 0x824F8838 -- resolve a PRESET event id to its display record: walk the embedded
// maEventStarts array (count == miEventStartsCount, the word the X360 reads at
// interface+0x20D0) matching the +0x10 light-trigger id. The X360's failure path
// builds "Unable to find event start with light trigger id: 0x%X" through the
// StrStream; lowered to the static-message assert per convention.
const SatNavEventDisplayInfo* GuiCache::GetPresetEventDisplayInfo(u32 luEventId) const
{
    CGS_ASSERT(miEventStartsCount != -1,
               "Array used before Construct/Clear was called");   // CgsArray.h:336 (non-gating)
    for (s32 liIndex = 0; liIndex < miEventStartsCount; ++liIndex)
    {
        if (maEventStarts[liIndex].muLightTriggerId == luEventId)
            return &maEventStarts[liIndex];
    }
    CGS_ASSERT(false, "Unable to find event start with light trigger id: ");   // BrnGuiCache.cpp:3772 (non-gating)
    return 0;
}

// @ 0x824F8AF0 -- the PROFILE flavour: same walk, matching the +0x18 event-instance id
// ("Unable to find event start with event id: " on the X360 failure path).
//
// ⭐⭐ THIS ASSERT FIRES ON THIS BUILD, AND IT IS **FAITHFUL** -- SETTLED 2026-08-27, NOT SILENCED.
// The X360 body @0x824F8AF0 is structurally identical: the same `!= -1` constructed-guard, the
// same linear walk over the count at interface+0x20D0, and the same fall-through that opens an
// assert and streams "Unable to find event start with event id: " before AppendFormat'ing the id.
// (Ours drops the id only because CGS_ASSERT takes a plain `const char*`; that is this tree's
// standing lowering convention, applied here as everywhere else.) So the assert is the console's,
// at the console's site, on the console's condition.
//
// ⛔ WHY IT FIRES HERE AND NOT ON THE CONSOLE, named precisely rather than guessed:
// `maEventStarts` is NEVER POPULATED on this build. Its only producer is
// SetUpAllEventStartsInterface::AddEventStart @0x82361398 (Interface_SetUpAllEventStarts.cpp),
// whose only caller is the console's GameStateModule::SendSetUpAllEventStartsMessage -- and that
// function is UNRECONSTRUCTED (flagged at BrnGameStateModule.cpp, in ProcessGameEvents' latch
// tail). The array is Construct'd, so the `!= -1` guard above passes and stays silent; the count
// is simply 0, so EVERY profile lookup walks an empty array and falls through. The assert is
// therefore reporting exactly what is true: this build cannot resolve any profile event id.
// ⇒ There is nothing to fix IN THIS FUNCTION. Silencing it here -- a null-return without the
// assert, an early `if (miEventStartsCount == 0) return 0;`, anything -- would delete the only
// runtime report that a real producer is missing, which is the silent-drop-stub class.
// The fix is to reconstruct SendSetUpAllEventStartsMessage (and the WDC progression binding its
// consumer also needs); that is the progression/WDC wave's work, not the GuiCache's.
//
// ⚠️ NON-GATING, and checked rather than assumed: the sole live caller,
// SatNavRenderer::RefreshSatNavIconInfo, already carries a FLAG'd PC bring-up guard
// (BrnSatNavRenderer.cpp: `if (lpDisplay == 0 || lpRaceEventData == 0 || ...) return;`) with its
// own DELETE-WHEN. No null is dereferenced; the run completes with the HUD up.
// ⚠️ REACHABILITY CHANGED 2026-08-26 WITHOUT THIS CODE CHANGING: once the HUD began surviving
// crashes, FBurnMain's sat-nav pre-pass kept running for the rest of a drive instead of stopping
// at the first crash, so the lookup is now attempted far more often. Un-gating a consumer makes a
// pre-existing fault reachable; it does not create it. (Control-run proven, endcrash wave §06.)
//
// ⭐⭐ THE PER-TICK STORM WAS A SEPARATE, CALLER-SIDE DEFECT -- FOUND AND FIXED 2026-08-27.
// A run measured 3,178 fires of the assert below (12,712 across the four-site chain). That was
// NOT this function repeating a legitimate report: SatNavRenderer::RefreshSatNavIconInfo's PC
// bring-up guard was returning BEFORE the console's unconditional slot claim + count increment,
// so its own "already cached" scan could never hit and every repost of the same event id redid
// the lookup. The console's producer reposts action 201 -> GUI 311 EVERY SIM TICK while the
// player car sits in a traffic-light trigger region, so one unresolvable id became an unbounded
// storm. With the console's stores restored this assert fires ONCE PER DISTINCT EVENT ID, which
// is the console's own shape. See BrnSatNavRenderer.cpp for the full measurement.
const SatNavEventDisplayInfo* GuiCache::GetProfileEventDisplayInfo(u32 luEventId) const
{
    CGS_ASSERT(miEventStartsCount != -1,
               "Array used before Construct/Clear was called");   // CgsArray.h:336 (non-gating)
    for (s32 liIndex = 0; liIndex < miEventStartsCount; ++liIndex)
    {
        if (maEventStarts[liIndex].muEventInstanceId == luEventId)
            return &maEventStarts[liIndex];
    }
    CGS_ASSERT(false, "Unable to find event start with event id: ");   // BrnGuiCache.cpp (non-gating)
    return 0;
}

// (X360-inlined at GetDriveThroughOrJunkyardAtIndex @0x824FAC10.) The drive-through /
// junkyard icon list the map selection walks.
const GuiEventUpdateSatNav::SatNavIconInfo* GuiCache::GetDriveThrough(s32 liIndex) const
{
    CGS_ASSERT(liIndex < miNumDriveThroughs, "liIndex < miNumDriveThroughs");   // BrnGuiCache.h:5164 (non-gating)
    return &maDriveThroughInfo[liIndex];
}

s32 GuiCache::GetNumberOfDriveThroughs() const
{
    return miNumDriveThroughs;
}

// Fill lpOutIconInfo with the online-landmark record at a position-in-list slot -- the
// ONLINE_CHECKPOINTS (display type 2) source for the sat-nav and crash-nav icon renderers.
// Forwards to WorldDataController::GetOnlineLandmarkInfoAtPositionInList (the trigger data's
// online-landmark table) and then packs the landmark into the icon record with exactly the
// same store sequence as GetLandmarkInfoAtPositionInList / GetLandmarkInfoFromIndex in
// BrnGuiCache_wJ_01.cpp: position lane, 0.0f rotation and speed, the whole 64-bit landmark
// id, district then county (county read back OFF the record), type 4, -1 in the
// active-race-car slot, design index, and the landmark's own region index @+0x20.
void GuiCache::GetOnlineLandmarkInfoAtPositionInList(
         s32 liIndex,
         GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const
{
    CGS_ASSERT(mpWorldDataController != 0, "mpWorldDataController");   // cpp:3586

    // [FLAG PC bring-up guard] the trigger-data resource can be acquired-but-unbound on this
    // build, and both asserts here are non-gating, so the two derefs below are guarded rather
    // than turned into a crash; the caller's record is left exactly as it staged it.
    // DELETE-WHEN asserts gate / the trigger-data landmark table is populated on this build.
    if (mpWorldDataController == 0 || !mpWorldDataController->HasTriggerData())
    {
        return;
    }

    const BrnTrigger::Landmark* lpLandmark =
        mpWorldDataController->GetOnlineLandmarkInfoAtPositionInList(liIndex);
    CGS_ASSERT(lpLandmark != 0, "lpLandmark");                         // cpp:3589
    if (lpLandmark == 0)
    {
        return;
    }

    const Vector3 lv3LandmarkPosition = lpLandmark->GetBoxRegion()->GetPosition();
    const Vector4 lv4PositionLane = { lv3LandmarkPosition.x, lv3LandmarkPosition.y,
                                      lv3LandmarkPosition.z, 0.0f };
    lpOutIconInfo->SetPositionLane(lv4PositionLane);                    // -> +0x00
    lpOutIconInfo->SetRotation(0.0f);                                   // -> +0x18
    lpOutIconInfo->SetSpeedMph(0.0f);                                   // -> +0x1C
    lpOutIconInfo->SetCgsId(lpLandmark->GetId());                       // -> +0x10
    lpOutIconInfo->SetDistrict(
        static_cast<BrnWorld::EDistrict>(lpLandmark->GetDistrict()));   // -> +0x25
    lpOutIconInfo->SetCounty(
        BrnWorld::WorldRegion::DistrictToCounty(lpOutIconInfo->GetDistrict()));  // -> +0x24
    lpOutIconInfo->SetIconType(
        GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK);   // -> +0x28
    lpOutIconInfo->SetLandmarkIndexHalf(
        static_cast<s16>(lpLandmark->GetRegionIndex()));                // -> +0x20
    lpOutIconInfo->SetActiveRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_INVALID); // -> +0x26
    lpOutIconInfo->SetDesignIndex(lpLandmark->GetDesignIndex());        // -> +0x22
}

// @ 0x8241E520 (the GuiCache face over the mEvents CgsArray element accessor
// @0x8241E430 -> CgsContainers::Arr). Stride 0x2C (the 7700-byte storage / 175 cap).
// NOTE: BrnGuiCache_wB_res.cpp carries a declared-only twin behind a link-time helper
// (GetPresetEventAtIndex) with two further unreconstructed deps; that TU stays
// unmounted and THIS is the single mounted definition.
const PresetEvent* GuiCache::GetPresetEvent(s32 liIndex) const
{
    CGS_ASSERT(mEventsCtorSentinel != -1,
               "Array used before Construct/Clear was called");   // CgsArray.h:336 (non-gating)
    CGS_ASSERT(liIndex >= 0 && liIndex < mEventsCtorSentinel,
               "luEventIndex < maEvents.GetLength()");            // BrnGameStateSharedIO.h:2014 (non-gating)
    return reinterpret_cast<const PresetEvent*>(&maEventsStorage[0x2C * liIndex]);
}

// The two preset-event record reads (X360 words +0x20 / +0x28 of the 0x2C-stride mEvents
// element; offsets proven by the renderer's GetIconInformation preset branch).
u32 PresetEvent::GetPositionLookupId() const
{
    return muPositionLookupId;
}

u32 PresetEvent::GetEventId() const
{
    return muEventId;
}

} // namespace BrnGui
