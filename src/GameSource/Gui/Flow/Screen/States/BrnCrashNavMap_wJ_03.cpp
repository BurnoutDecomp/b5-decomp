// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 03: the map-scroll sound debouncer.
//
// Group 3 of the wave-J CrashNavMap keystone is
//   CheckForLoadComplete @0x824CB660  (BrnCrashNavMap.cpp:576 assert)
//   UpdateSoundEvents    @0x824CB8B0
//   UpdateCursorStatus   @0x824CB770
//
// All three bodies are landed; the shared-header declarations they waited on have since
// been applied.
//
// The DecFIGS DWARF splits the map-scroll sound debouncer out of UpdateSoundEvents as its
// own struct, CrashNavMapSoundData, with Construct/Prepare/Update (BrnCrashNavMap.h:61/70/
// 80), and the X360 folded all three inline. UpdateSoundEvents below therefore calls them
// by name rather than reproducing the folded code. NONE of the three has a body anywhere
// in b5-decomp/src yet -- they are declared in the owning BrnCrashNavMap.h and are
// LINK-TIME EXTERNALS reported by this wave, not defined here and not defined by partfile
// 04 either (an earlier revision of both banners claimed one file or the other had landed
// them; neither had).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @0x824CB660 / @0x824CB770 / @0x824CB8B0 (the
// per-address assembly under .ida-exports/BURNOUT_X360_ARTIST.XEX/). X360 offsets appear
// only in comments; the host layout is name-based.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // GuiEventWrapper / GuiEventQueueLarge
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (out-queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiFlow

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ------------------------
        const s32 KI_CHANNEL_VIEW_STATE   = 41;   // OutputViewState
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;   // OutputInternalState

        // The apt movie this screen plays once its resources are in, and the level the
        // X360 passes with it (`li r5, 3`; same KI_APT_MOVIE_LEVEL the wave-H
        // OnlineGameRoomPlayerInfo partfile 02 uses).
        const char KPC_CRASHNAV_APT_MOVIE[] = "BrnCrashNavMapMain";   // X360 off_82F27AF0
        const s32  KI_APT_MOVIE_LEVEL       = 3;

        // The float in the ShowHideSatNav payload. MEASURED: X360 flt_82065668 == 0.5f
        // (headless IDA dump, scratchpad/waveJ/prfb_init.txt line 146 and
        // scratchpad/waveJ/g03_flt.txt). FLAG consumer-named -- the wave-J spec calls it
        // KF_MAP_FADE_IN_TIME; the rodata word carries no DWARF symbol of its own.
        const f32 KF_MAP_FADE_IN_TIME = 0.5f;

        // ---- out-queue payload view -------------------------------------------------
        // GuiEventShowHideSatNav (wire id 213, payload 12 bytes -- the
        // OutputViewState/OutputInternalState<GuiEventShowHideSatNav> instantiations
        // @0x82476DD8 / @0x82476E38 build "id 213 size 12 off 12, record 24", which is
        // exactly what this body stack-builds). Its real home,
        // BrnGuiDemangledEventTypes.h, models the type as an empty struct, so the three
        // payload words are named generically -- this view is copied verbatim from
        // BrnOnlineGameRoomPlayerInfo_wH_09.cpp, which posts the same record. FLAG: word
        // roles not recovered; the receiving CustomRendererManager keys event 213 by a
        // sub-mode word (0 == MainMap, 1 == SatNav) and a renderable flag.
        struct GuiEventShowHideSatNavPayload
        {
            s32  miSubMode;   // +0x00  X360 stores 0        (`stw r9`)
            f32  mfValue;     // +0x04  X360 stores 0.5f     (`stfs f0`, flt_82065668)
            bool mbFlag;      // +0x08  X360 stores 1        (`stb r10`)
            u8   maPad[3];    // +0x09  never written by the X360; modelled zeroed

            GuiEventShowHideSatNavPayload(s32 liSubMode, f32 lfValue, bool lbFlag)
                : miSubMode(liSubMode), mfValue(lfValue), mbFlag(lbFlag)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }

            s32 GetEventType() const { return 213; }
        };
    }

    // --------------------------------------------- CheckForLoadComplete @ 0x824CB660
    // Per-frame poll while the screen is coming up: once the cache reports every one of
    // this screen's seven resources loaded, start the screen's apt movie, re-arm the
    // expected-component bookkeeping for the component set the derived screen declares,
    // and tell the view and internal channels to bring the sat-nav up (a half-second
    // fade). Runs once -- mbIsScreenLoaded latches it -- and never while the screen is
    // already tearing down.
    void CrashNavMap::CheckForLoadComplete()
    {
        // cpp:576 -- non-fatal on the X360; the cache pointer is dereferenced either way
        // below.
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        if (mbIsScreenLoaded || mbIsExiting)
        {
            return;
        }

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return;
        }

        mpStateInterface->PlayAptMovie(KPC_CRASHNAV_APT_MOVIE, KI_APT_MOVIE_LEVEL);

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mpGuiCache->ClearExpectedControlledAptComponentList();

        // Virtual, vtable byte +0x2C: the derived screen appends its own component set.
        AppendExpectedAptComponents();

        mbItemsLoaded    = false;
        mbIsScreenLoaded = true;

        // The X360 stack-builds the OutputViewState / OutputInternalState wrapper record
        // inline: { payload size 12, type 213, payload offset 12, payload } posted at 24
        // bytes on channel 41 and then again on channel 42. Sizes are host sizeof
        // expressions, never the console's baked 12 / 24 immediates.
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
            mpStateInterface->GetOutputEventQueue();

        GuiEventShowHideSatNavPayload lShowHideSatNav(0, KF_MAP_FADE_IN_TIME, true);

        CgsGui::GuiEventWrapper<GuiEventShowHideSatNavPayload, KI_CHANNEL_VIEW_STATE>
            lViewStateRecord(lShowHideSatNav);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lViewStateRecord),
                             lViewStateRecord.GetChannel(),
                             static_cast<s32>(sizeof(lViewStateRecord)));

        CgsGui::GuiEventWrapper<GuiEventShowHideSatNavPayload, KI_CHANNEL_GUI_INTERNAL>
            lInternalStateRecord(lShowHideSatNav);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInternalStateRecord),
                             lInternalStateRecord.GetChannel(),
                             static_cast<s32>(sizeof(lInternalStateRecord)));
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameSource/Gui/Flow/Screen/Components/BrnCursor.h"   // BrnGui::GuiCursor

namespace BrnGui
{
    // ------------------------------------------------ UpdateCursorStatus @ 0x824CB770
    // Reconcile the cursor with what the icon pass decided is under it. When nothing is
    // hovered any more the cursor drops its icon lock and goes back to the un-snapped
    // display state; either way it is re-activated, this frame's hovered set becomes next
    // frame's "last frame" set, and the cursor publishes itself.
    void CrashNavMap::UpdateCursorStatus()
    {
        // Did any part of the hovered set move since last frame?
        bool lbHoveredSetChanged = false;
        if (muHoveredEventID != muHoveredEventIDLastFrame
            || mHoveredDriveThruID != mHoveredDriveThruIDLastFrame
            || mHoveringRivalId != mHoveringRivalIdLastFrame
            || mpLockedIconName != mpLockedIconNameLastFrame)
        {
            lbHoveredSetChanged = true;
        }

        // Is anything hovered at all? (The X360 tests the local-player flag byte
        // against 1 rather than against zero -- `cmplwi r10, 1` at 0x824CB808.)
        bool lbAnythingHovered = false;
        if (muHoveredEventID != 0
            || mHoveredDriveThruID != 0
            || mHoveringRivalId != 0
            || mpLockedIconName != 0
            || mbLocalPlayerSelected)
        {
            lbAnythingHovered = true;
        }

        // See the BRANCH SHAPE note in the banner: the X360 branches on
        // lbHoveredSetChanged first, but both arms run this same test and share the
        // release block, so the change flag does not affect the outcome.
        (void)lbHoveredSetChanged;

        if (!lbAnythingHovered)
        {
            mCursor.muLockedToIndex = GuiCursor::KU_INVALID_SNAP_INDEX;
            if (mCursor.meDisplayState != GuiCursor::E_DISPLAY_ACTIVE_UNSNAP)
            {
                mCursor.meDisplayState = GuiCursor::E_DISPLAY_ACTIVE_UNSNAP;
            }
        }

        mCursor.SetActive();

        muHoveredEventIDLastFrame    = muHoveredEventID;
        mHoveredDriveThruIDLastFrame = mHoveredDriveThruID;
        mHoveringRivalIdLastFrame    = mHoveringRivalId;
        mpLockedIconNameLastFrame    = mpLockedIconName;

        mCursor.Update();
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (out-queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/Flow/Screen/Components/BrnCursor.h"              // BrnGui::GuiCursor::GetPosition
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // MainMapComponent::IsZooming
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                         // BrnGui::MapTransform

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) -------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // X360 `li r5, 0x28` inside OutputGuiEvent

        // The GuiAudioTriggerEvent action every one of these map cues carries
        // (X360 `li r4, 7`). FLAG: the action enum's name is not in the recovered DWARF
        // slice -- same note BrnOnlineGameOptions_wI_02.cpp makes for its own cue.
        const s32 KI_AUDIO_ACTION_MAP_CUE = 7;

        // The three sound labels, from the TU's own rodata pointer table. DWARF names and
        // cpp lines are from the wave-J spec section 1 (BrnCrashNavMap.cpp:40-42):
        //   KPC_SOUND_MAP_MOVE_START   X360 off_82F26E74
        //   KPC_SOUND_MAP_SCROLL_START X360 off_82F26E78
        //   KPC_SOUND_MAP_SCROLL_END   X360 off_82F26E7C
        // CONDUCTOR NOTE: these are TU statics, not file-local ones -- the fourth member
        // of the same table, KPC_SOUND_MAP_VIEW_EVENT (off_82F26E80,
        // "CodeMapViewEvent"), is used by group 06's UpdateEvent. Hoist all four into the
        // TU statics block rather than leaving a copy in each partfile.
        const char KPC_SOUND_MAP_MOVE_START[]   = "CodeMapMoveStart";
        const char KPC_SOUND_MAP_SCROLL_START[] = "CodeMapScrollStart";
        const char KPC_SOUND_MAP_SCROLL_END[]   = "CodeMapScrollEnd";

        // ---- out-queue wire record ---------------------------------------------------
        // The record the inlined OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> builds:
        // { 100, 457, 12, the 100-byte payload }, channel 40, 112 bytes. The committed PC
        // GuiAudioTriggerEvent already carries that 12-byte queue header in front of its
        // 100-byte payload, so the event object IS the record -- but its GuiEvent<201>
        // base seeds the payload-size word 0 and the id 201, while the X360 wire words are
        // the payload size and the id 457. This view restores them without forking the
        // payload type; copied verbatim from BrnOnlineGameOptions_wI_02.cpp (identical
        // view in BrnOnlineGameRoomPlayerInfo_wH_08.cpp).
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

        static_assert(sizeof(GuiAudioTriggerWire) == 112, "audio trigger record is 112 bytes");
    }

    // ------------------------------------------------- UpdateSoundEvents @ 0x824CB8B0
    // Edge-trigger the map's move/scroll audio. Whether the map counts as scrolling
    // depends on the cursor mode: while the player is picking icons the caller (the main
    // map update) has already worked it out and passes it in -- unless the map is
    // mid-zoom, during which no scroll cue is wanted -- and while the player is panning
    // it is measured directly from how far the cursor has travelled in world space.
    // The start pair fires on the rising edge and the end cue on the falling edge; the
    // debouncer's latch at the tail is what makes both edges detectable next frame.
    void CrashNavMap::UpdateSoundEvents(bool lbIsScrolling)
    {
        const Vector3 lv3CursorWorldPos = MapTransform::DeviceToWorld(mCursor.GetPosition());

        bool lbIsMapScrolling = false;

        if (meCursorMode == E_CURSORMODE_SELECTING_ICONS)
        {
            if (!mMainMapComponent.IsZooming())
            {
                lbIsMapScrolling = lbIsScrolling;
            }
        }
        else if (meCursorMode == E_CURSORMODE_PANNING)
        {
            lbIsMapScrolling = mSoundData.Prepare(lv3CursorWorldPos);
        }

        if (lbIsMapScrolling)
        {
            if (!mSoundData.mbPrevIsScrolling)
            {
                GuiAudioTriggerWire lMoveStart;
                lMoveStart.Construct(KI_AUDIO_ACTION_MAP_CUE, "", KPC_SOUND_MAP_MOVE_START, "");
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lMoveStart), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lMoveStart)));

                GuiAudioTriggerWire lScrollStart;
                lScrollStart.Construct(KI_AUDIO_ACTION_MAP_CUE, "", KPC_SOUND_MAP_SCROLL_START, "");
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lScrollStart), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lScrollStart)));
            }
        }
        else if (mSoundData.mbPrevIsScrolling)
        {
            GuiAudioTriggerWire lScrollEnd;
            lScrollEnd.Construct(KI_AUDIO_ACTION_MAP_CUE, "", KPC_SOUND_MAP_SCROLL_END, "");
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lScrollEnd), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lScrollEnd)));
        }

        mSoundData.Update(lv3CursorWorldPos, lbIsMapScrolling);
    }
}
