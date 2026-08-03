// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 06: the "inspect an event" pair.
//
//   CalculateEventZoomFactor @0x824BF4B0
//   UpdateEvent              @0x824CC3F8
//
// Both bodies are landed. They were reconstructed in full from the raw X360 assembly
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824CC3F8.json and /0x824BF4B0.json, field
// `assembly`; copies at scratchpad/waveJ/asm_g06_*.txt) and originally parked because
// seven names they use were declared nowhere in b5-decomp/src. Those declarations have
// since been applied:
//
//   BrnMapIconManager.h  -- `friend struct CrashNavMap;` (miNumUsedIcons was already
//                           committed but private) plus the three DWARF-attested members
//                           muSelectedJunctionID / miSelectedCheckpoint /
//                           mbShowingCrashNavRoute (X360 +0xAA10 / +0xAA14 / +0xAA21).
//   BrnGuiCache.h        -- GuiCache::GetLandmarkInfoFromID(CgsID, SatNavIconInfo*) const;
//                           GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID
//                           (u32, f32, bool); and SatNavEventDisplayInfo's +0x14 junction
//                           id word (split out of the existing 8-byte pad).
//   BrnRaceEventData.h   -- RaceEventData::GetCheckpointCount() and a COMPLETE
//                           RaceEventData::CheckpointData carrying GetLandmarkId().
//
// CORRECTION TO THE MEASUREMENT THIS BANNER USED TO QUOTE. It said the probe against the
// committed headers produced "five C2039, one C2248, one C2027". Re-measured from OUTSIDE
// the probe directory -- MSVC resolves a quoted include relative to the INCLUDING FILE's
// directory before any /I, so a probe.cpp sitting inside scratchpad/waveJ/probe_g06/ binds
// to that directory's own shadow headers and never tests the committed ones at all -- the
// real figure was C2039 x10 across SEVEN distinct names (mbShowingCrashNavRoute,
// miSelectedCheckpoint, muSelectedJunctionID, GetJunctionID, GetCheckpointCount,
// GetLandmarkInfoFromID, HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID) plus one
// C2248 and one C2027. The blocking SET was right; only the count was understated.
//
// Nothing else about these two functions is outstanding: every constant they read was
// dumped from the image with headless IDA this wave (scratchpad/waveJ/g05_consts.txt,
// crashnav_sinit.txt, crashnav_floats2.txt) and every argument list was arbitrated from
// the assembly, including the two PPC float-ABI call sites (MainMapComponent::SetZoom and
// HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID, where the float argument skips
// its gpr slot) and the fsel progress clamp's NaN polarity.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventUpdateSatNav::SatNavIconInfo
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // GetEventInfoFromEventId
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                         // BrnGui::MapTransform
#include "SharedClasses/Progression/BrnRaceEventData.h"                   // BrnProgression::RaceEventData

namespace BrnGui
{
    namespace
    {
        // TU statics, DWARF BrnCrashNavMap.cpp:70 / :71. X360 .data @0x82FB4AC0 and
        // @0x82FB4D10, both written by the same runtime static initialiser shape
        // (0x82C54D20 / 0x82C54D60) from flt_8206B494 == 638.0 and flt_8206B490 ==
        // 349.79999 with lanes 2/3 zeroed. They hold the SAME value in the shipped build.
        const Vector2 K_CRASHNAV_LONG_DISPLAY_RECT = { 638.0f, 349.8f, 0.0f, 0.0f };
        const Vector2 K_CRASHNAV_TALL_DISPLAY_RECT = { 638.0f, 349.8f, 0.0f, 0.0f };
    }

    // ================================================================================
    //  CalculateEventZoomFactor  @ 0x824BF4B0
    //
    //  Frame the whole of the event the player is inspecting: take the bounding box of
    //  the event-start marker plus every checkpoint landmark on the 2D map plane, centre
    //  the main map on it, and return the zoom factor that makes that box fit the
    //  crash-nav display rect.
    // ================================================================================
    f32 CrashNavMap::CalculateEventZoomFactor()
    {
        const SatNavEventDisplayInfo* lpEventStart =
            mpGuiCache->GetProfileEventDisplayInfo(muInspectingEventID);

        // GetWorldDataController() is X360-inlined here and carries its own
        // "mpWorldDataController" assert (BrnGuiCache.h:2324) -- do not repeat it.
        const BrnProgression::RaceEventData* lpRaceEventData =
            mpGuiCache->GetWorldDataController()->GetEventInfoFromEventId(muInspectingEventID);

        // cpp:2120 / cpp:2121 -- both non-fatal on the console; both pointers are used
        // regardless, exactly as below.
        CGS_ASSERT(lpEventStart, "lpEventStart");
        CGS_ASSERT(lpRaceEventData, "lpRaceEventData");

        // Seed the bounding box with the event start marker. The console does the
        // world -> map-plane swizzle with one vperm (see the mask note in the banner);
        // lanes 2/3 are the redundant copy of lane 0 that mask produces.
        Vector2 lv2Min;
        lv2Min.x = lpEventStart->mv3Position.x;
        lv2Min.y = lpEventStart->mv3Position.z;
        lv2Min.z = lpEventStart->mv3Position.x;
        lv2Min.w = lpEventStart->mv3Position.x;

        Vector2 lv2Max = lv2Min;

        // The count is re-read every pass on the console; GetCheckpointCount() in the
        // condition keeps that. GetCheckpointData carries the inlined bounds assert.
        for (s32 liCheckpointIndex = 0;
             liCheckpointIndex < lpRaceEventData->GetCheckpointCount();
             ++liCheckpointIndex)
        {
            GuiEventUpdateSatNav::SatNavIconInfo lIconInfo;
            mpGuiCache->GetLandmarkInfoFromID(
                lpRaceEventData->GetCheckpointData(liCheckpointIndex)->GetLandmarkId(),
                &lIconInfo);

            const Vector4& lrv4IconPosition = lIconInfo.GetPositionLane();

            Vector2 lv2Point;
            lv2Point.x = lrv4IconPosition.x;
            lv2Point.y = lrv4IconPosition.z;
            lv2Point.z = lrv4IconPosition.x;
            lv2Point.w = lrv4IconPosition.x;

            // vminfp / vmaxfp, written out per lane.
            lv2Min.x = (lv2Min.x < lv2Point.x) ? lv2Min.x : lv2Point.x;
            lv2Min.y = (lv2Min.y < lv2Point.y) ? lv2Min.y : lv2Point.y;
            lv2Min.z = (lv2Min.z < lv2Point.z) ? lv2Min.z : lv2Point.z;
            lv2Min.w = (lv2Min.w < lv2Point.w) ? lv2Min.w : lv2Point.w;

            lv2Max.x = (lv2Max.x > lv2Point.x) ? lv2Max.x : lv2Point.x;
            lv2Max.y = (lv2Max.y > lv2Point.y) ? lv2Max.y : lv2Point.y;
            lv2Max.z = (lv2Max.z > lv2Point.z) ? lv2Max.z : lv2Point.z;
            lv2Max.w = (lv2Max.w > lv2Point.w) ? lv2Max.w : lv2Point.w;
        }

        // (max + min) * 0.5 -- see the vrefp note in the banner.
        Vector2 lv2Centre;
        lv2Centre.x = (lv2Max.x + lv2Min.x) * 0.5f;
        lv2Centre.y = (lv2Max.y + lv2Min.y) * 0.5f;
        lv2Centre.z = (lv2Max.z + lv2Min.z) * 0.5f;
        lv2Centre.w = (lv2Max.w + lv2Min.w) * 0.5f;
        mMainMapComponent.SetDesiredWorldCentre(lv2Centre);

        // A box that is taller than it is wide gets framed against the tall rect.
        const f32 lfWidth  = lv2Max.x - lv2Min.x;
        const f32 lfHeight = lv2Max.y - lv2Min.y;
        const Vector2 lv2DisplayRect = (lfHeight > lfWidth) ? K_CRASHNAV_TALL_DISPLAY_RECT
                                                            : K_CRASHNAV_LONG_DISPLAY_RECT;

        // X360 flt_82F27384 == 1.7777778, the 16:9 display aspect (a shared rodata scalar).
        return MapTransform::CalculateZoomFactor(lv2Min, lv2Max, lv2DisplayRect,
                                                 16.0f / 9.0f);
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT / Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the streamed assert)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiAudioTriggerEvent
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                          // GuiTracker::ClearTracker
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager

namespace BrnGui
{
    namespace
    {
        // The out-queue channel the GUI event wire uses (X360 `li r5, 0x28`).
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // GuiAudioTriggerEvent::meAction values for the map's view-event cue (X360
        // `li r4, 8` on the first inspecting frame, `li r4, 9` on the tear-down).
        // FLAG: the action enum's name is not in the recovered DWARF slice, so the
        // measured literals stand in -- same convention as BrnOnlineGameOptions_wI_02.cpp.
        const s32 KI_AUDIO_ACTION_VIEW_EVENT_START = 8;
        const s32 KI_AUDIO_ACTION_VIEW_EVENT_STOP  = 9;

        // TU static, DWARF BrnCrashNavMap.cpp:43 -- rodata pointer off_82F26E80.
        const char KPC_SOUND_MAP_VIEW_EVENT[] = "CodeMapViewEvent";

        // The X360 assert-site file string, verbatim (aGamesourceGuiF_63).
        const char KAC_ASSERT_FILE[] =
            "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp";

        // ---- out-queue wire record (see the banner) ------------------------------------
        const u32 KU_WIRE_ID_AUDIO_TRIGGER = 457;

        struct GuiAudioTriggerWire : public GuiAudioTriggerEvent
        {
            GuiAudioTriggerWire()
            {
                muHeader0 = static_cast<u32>(sizeof(GuiAudioTriggerEvent) -
                                             sizeof(CgsGui::GuiEvent<201>));
                muEventType = KU_WIRE_ID_AUDIO_TRIGGER;
            }
        };

        // Record-size pin. The payload is pointer-free, so the X360's AddEvent size
        // immediate must survive unchanged on the x64 host.
        static_assert(sizeof(GuiAudioTriggerWire) == 112, "audio trigger record is 112 bytes");
    }

    // ================================================================================
    //  UpdateEvent  @ 0x824CC3F8  (cpp:1890 / cpp:1959 asserts)
    //
    //  The per-frame event half of UpdateMainMap: refresh the info panel for whatever the
    //  cursor is on, and drive the "inspecting an event" mode -- on the first inspecting
    //  frame zoom the map onto the whole event and switch the icon manager into
    //  route-display, then every frame reveal the route's landmarks in proportion to how
    //  long the player has been inspecting. Leaving inspection tears all of that down.
    // ================================================================================
    void CrashNavMap::UpdateEvent()
    {
        // cpp:1890 -- non-fatal on the X360; the cache is dereferenced either way.
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        UpdateEventInfoPanel();

        switch (meCursorMode)
        {
            // The four non-inspecting modes share one jump-table arm.
            case E_CURSORMODE_NONE:
            case E_CURSORMODE_SELECTING_ICONS:
            case E_CURSORMODE_ZOOMEDOUT:
            case E_CURSORMODE_PANNING:
                // A non-zero stamp means we were inspecting last frame and have just left.
                if (mfInspectingEventTime != 0.0f)
                {
                    // No null check here on the console -- see the banner.
                    mpIconManager->mbShowingCrashNavRoute = false;
                    mpIconManager->miSelectedCheckpoint   = 0;
                    mpIconManager->muSelectedJunctionID   = 0;
                    mpIconManager->miNumUsedIcons         = 0;

                    mpGuiCache->GetGuiTracker()->ClearTracker();

                    mfInspectingEventTime = 0.0f;
                    muInspectingEventID   = 0;

                    GuiAudioTriggerWire lAudio;
                    lAudio.Construct(KI_AUDIO_ACTION_VIEW_EVENT_STOP, "",
                                     KPC_SOUND_MAP_VIEW_EVENT, "");
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lAudio),
                        KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lAudio)));
                }
                break;

            case E_CURSORMODE_INSPECTING_ICONS:
                if (muInspectingEventID != 0)
                {
                    // A zero stamp means this is the first frame of the inspection.
                    if (mfInspectingEventTime == 0.0f)
                    {
                        const f32 lfZoomFactor = CalculateEventZoomFactor();
                        mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM,
                                                  lfZoomFactor, false);

                        if (mpIconManager != 0)
                        {
                            mpIconManager->mbShowingCrashNavRoute = true;
                            mpIconManager->miSelectedCheckpoint   = 0;
                            mpIconManager->muSelectedJunctionID   =
                                mpGuiCache->GetProfileEventDisplayInfo(muInspectingEventID)
                                    ->muJunctionId;   // lwz record+0x14 @0x824CC594
                        }

                        mfInspectingEventTime = mpGuiCache->GetTime();

                        GuiAudioTriggerWire lAudio;
                        lAudio.Construct(KI_AUDIO_ACTION_VIEW_EVENT_START, "",
                                         KPC_SOUND_MAP_VIEW_EVENT, "");
                        mpStateInterface->GetOutputEventQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lAudio),
                            KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lAudio)));
                    }

                    // The two fsel instructions, transcribed operand for operand -- see
                    // the NaN-polarity note in the banner.
                    const f32 lfElapsed  = mpGuiCache->GetTime() - mfInspectingEventTime;
                    const f32 lfFloored  = (-lfElapsed >= 0.0f) ? 0.0f : lfElapsed;
                    const f32 lfProgress = ((1.0f - lfFloored) >= 0.0f) ? lfFloored : 1.0f;

                    mpGuiCache->HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID(
                        muInspectingEventID, lfProgress, false);
                }
                break;

            default:
            {
                // Streamed diagnostic: the X360 composes it into
                // CgsDev::Assert::gpcMessageBuffer through a StrStream, then fires it.
                // Non-fatal; the frame simply does nothing for the unknown mode.
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unhandled cursor mode "
                           << static_cast<s32>(meCursorMode)
                           << " in CrashNavMap::UpdateEvent\n";
                CgsDev::Assert::FireAssert(lacMessage, KAC_ASSERT_FILE, 1959);
                CgsDev::Assert::EndAssert();
                break;
            }
        }
    }
}
