// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 04.
//
//   Construct @0x824B6660  (cpp:133 assert site)
//   OnEnter   @0x824CB158
//
// Both bodies are COMPLETE reconstructions walked store-for-store off the raw X360
// assembly (the Hex-Rays listing for OnEnter is badly garbled and was cross-check only),
// and both are landed. The declarations they waited on have since been applied:
// CrashNavPanel::StoreSettings (@0x82418708) and its virtual Construct (@0x82425C60,
// slot 0) with the DWARF's `: public CgsGui::GuiComponent` base; GuiCursor::Construct
// (@0x82416690) / SetAlwaysSnap (@0x82416CA0) / GetPosition / the position-lane writer;
// and GuiEventUpdateSatNav::SatNavIconInfo::SetPositionLane.
//
// CrashNavMapSoundData::Construct is NOT defined here (nor in partfile 03, which owns
// Prepare/Update's call sites): all three are declared in BrnCrashNavMap.h and are
// LINK-TIME EXTERNALS this wave reports rather than defines. An earlier revision of this
// banner said Construct "ships" here -- it does not.
//
// ONE CORRECTION FOR THE WAVE RECORD: the group brief and the Hex-Rays listing both say
// mbUseRoadSigns is set true. The raw asm sets mbUseRoadSigns (+24705) FALSE and
// mbDrawDriveThrus (+24706) TRUE, in both Construct (0x824B66E4/0x824B66E8) and OnEnter
// (0x824CB3C0/0x824CB3C4). The name-to-offset mapping is corroborated independently by
// ResetIconManager @0x824B6D4C..0x824B6D5C, which loads +0x6081/+0x6082/+0x6083 into
// r8/r9/r10 as MapIconManager::SetOwnerParameters' (useRoadSigns, drawDriveThrus,
// selectDriveThrus) argument triple.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Containers/CgsHash.h"                    // CgsHash::CalculateHash
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventEnableSatNavIcons::EIconDisplayType

#include <cstring>   // std::strlen (the asm's inline `while (*p++);` length measure)

namespace BrnGui
{
    namespace
    {
        // DWARF BrnCrashNavMap.h:221 -- the sat-nav icon component count. It is a
        // class-scope constant in the original header; the committed recon header does not
        // declare it, so it is file-local here. (Header finding, not a fabrication: the
        // value 50 is the `li r8, 0x32` of OnEnter and the `li r6, 0x32` of
        // ResetIconManager, and the DWARF prints "= 50".)
        const s32 KAC_CRASHNAVMAP_NUMICONS = 50;

        // DWARF BrnCrashNavMap.cpp:35 (char[11]) -- the base of every per-icon apt
        // component name. X360 rodata aSatnavicon_1.
        const char macSatNavIconBaseName[11] = "SatNavIcon";

        // X360 rodata aSD_1, the SPrintf format the loop passes (r5). Hex-Rays mislabels
        // variadic args here; the raw asm is unambiguous: r3=buffer, r4=16, r5="%s%d",
        // r6=macSatNavIconBaseName, r7=the loop index.
        const char KAC_ICON_NAME_FORMAT[5] = "%s%d";

    }

    // DWARF BrnCrashNavMap.cpp:45 -- uint32_t[50], X360 .bss @0x82FB4890 (the loop bound is
    // pinned by dword_82FB4958 - 0x82FB4890 == 0xC8 == 50*4). In the original this is a
    // file-scope object of the single BrnCrashNavMap.cpp, written here by Construct and
    // read by AppendExpectedAptComponents.
    //
    // ONE OBJECT, NOT ONE PER PARTFILE. Wave J lands BrnCrashNavMap.cpp as eight separate
    // translation units, so an anonymous-namespace definition would give each partfile its
    // own private array -- Construct would fill this file's copy and
    // AppendExpectedAptComponents (BrnCrashNavMap_wJ_02.cpp) would register 50 zeros from a
    // different one, and the screen would wait on the wrong expected-component set. It is
    // therefore defined ONCE, here (the partfile that owns Construct), with external
    // linkage; wJ_02 carries the matching `extern` declaration. Fold both back into the
    // anonymous namespace if the partfiles are ever concatenated into one .cpp.
    u32 mauComponentHashIds[KAC_CRASHNAVMAP_NUMICONS];

    // ------------------------------------------------------------- Construct @0x824B6660
    // Bring the crash-nav map screen state up: chain to the GUI state base, put every
    // scalar member in its cold-start value, pre-hash the 50 sat-nav icon component names
    // the apt movie will be asked for, and zero the scroll/sound bookkeeping.
    void CrashNavMap::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CGS_ASSERT(lpFsm != 0, "lpFsm");                                   // cpp:133

        CgsGui::State::Construct(liId, lpFsm);

        // The store walk, in raw-asm order (0x824B66B8..0x824B674C).
        meEventIconDisplayType       = GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT;  // 5
        mfInspectingEventTime        = 0.0f;                               // flt_82001CC0
        mIconManagerOwnerId          = 0;
        mpIconManager                = 0;
        mbUseRoadSigns               = false;                              // stb 0, +0x6081
        mbDrawDriveThrus             = true;                               // stb 1, +0x6082
        mbSelectDriveThrus           = false;                              // stb 0, +0x6083
        mbItemsLoaded                = false;
        mpLockedIconName             = 0;
        mpLockedIconNameLastFrame    = 0;
        meTitleButtonsState          = E_VISIBLE_ANIMATION_STATES_COUNT;   // 2
        muHoveredEventID             = 0;
        muInspectingEventID          = 0;
        mHoveredDriveThruID          = 0;                                  // std (8-byte CgsID)
        mHoveringRivalId             = 0;                                  // std (8-byte CgsID)
        meCursorMode                 = E_CURSORMODE_NONE;
        meScreenType                 = E_SCREEN_TYPE_OFFLINE;
        meNavigationButtonsState     = E_BUTTON_PROMPT_ANIMATION_STATES_COUNT;  // 12
        mbLocalPlayerSelected        = false;
        muHoveredEventIDLastFrame    = 0;
        mHoveredDriveThruIDLastFrame = 0;
        mHoveringRivalIdLastFrame    = 0;
        mPlayerName.macName[0]       = '\0';                               // stb 0, +0x60C8

        // Pre-hash "SatNavIcon0".."SatNavIcon49" into the TU's component-id table. The
        // X360 measures the string with an inline `while (*p++);` and hands the length
        // (excluding the NUL) to the container CRC-32; CalculateHash takes a mutable
        // char* but never writes, so the buffer is passed straight through.
        for (s32 liIcon = 0; liIcon < KAC_CRASHNAVMAP_NUMICONS; ++liIcon)
        {
            char lacIconName[16];
            CgsCore::SPrintf(lacIconName, sizeof(lacIconName), KAC_ICON_NAME_FORMAT,
                             macSatNavIconBaseName, liIcon);
            mauComponentHashIds[liIcon] = CgsContainers::CgsHash::CalculateHash(
                lacIconName, static_cast<int>(std::strlen(lacIconName)));
        }

        mv2MapScrollVelocity.SetZero();          // stvx128 of a zero quad at +0x6070
        mCrashNavPanel.StoreSettings(true);      // @0x82418708
        mSoundData.Construct();                  // inlined by the X360: zero quad at +0x6130 + zero byte at +0x6140
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // GuiComponent::Construct
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // MainMapComponent + bundle
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // EIconDisplayType / SatNavIconInfo
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                         // MapTransform::DeviceToWorld

namespace BrnGui
{
    namespace
    {
        // DWARF BrnCrashNavMap.h:221 -- see the group-4 Construct park for provenance.

        // DWARF BrnCrashNavMap.cpp:34/:36/:37 (char[10]/[17]/[18]) and cpp:90/:91 -- the
        // apt component names this state constructs. All five are X360 rodata strings.
        const char mCursorName[10]           = "cursor_mc";
        const char macCrashNavPanelName[17]  = "CrashNavPanel_mc";
        const char macCrashNavLegendName[18] = "CrashNavLegend_mc";
        const char KAC_TITLE_BUTTONS_ANIMATION_COMPONENT[22]  = "TitleButtonsAnimation";
        const char KAC_BUTTON_PROMPTS_ANIMATION_COMPONENT[17] = "ButtonsAnimation";

        // The AddEvent channel word (`li r5, 0x28`).
        const s32 KI_CHANNEL_GUI_EVENT_OUT = 40;

        // The reference device rect OnEnter installs on the cursor (cursor +0xD0).
        const f32 KF_CURSOR_RECT_LEFT   = 0.0f;      // flt_82001CC0
        const f32 KF_CURSOR_RECT_TOP    = 0.0f;      // flt_82001CC0
        const f32 KF_CURSOR_RECT_RIGHT  = 1280.0f;   // flt_82066040
        const f32 KF_CURSOR_RECT_BOTTOM = 720.0f;    // flt_8201A7A4

        // The cursor's construction parameters (f1/f2/f3 of GuiCursor::Construct).
        const f32 KF_CURSOR_MOVEMENT_SCALAR = 1.0f;  // flt_82001C98, f30
        const f32 KF_CURSOR_START_X         = 0.0f;  // flt_82001CC0, f31
        const f32 KF_CURSOR_START_Y         = 0.0f;  // flt_82001CC0, f31

        // Out-queue payload view for the wire event this state posts on entry. Wire id 555
        // has no homed type; the record is {payload size 1, id 555, payload offset 12} and
        // the single payload byte is never written by the X360 (no stb targets it), so it
        // is modelled zero-initialised. FLAG consumer-named: neither the field's role nor
        // the event's name is recovered.
        struct GuiEventUnnamed555Payload
        {
            bool mbFlag;   // +0x00 -- left as stack garbage by the X360; posted zeroed here

            GuiEventUnnamed555Payload() : mbFlag(false) {}

            s32 GetEventType() const { return 555; }
        };
    }

    // ---------------------------------------------------------------- OnEnter @0x824CB158
    // Entering the crash-nav map screen: build and construct the main map view, announce
    // the entry on the GUI out-queue, construct the cursor and the four apt components,
    // then reset every piece of per-visit selection/hover state and seed the map-move
    // sound debouncer from the cursor's current world position.
    void CrashNavMap::OnEnter()
    {
        // The X360 assembles this bundle in three stack quads at 0xF0+var_70 and passes
        // its address in r5.
        MainMapComponent::MainMapParameterBundle lParameters;
        lParameters.mv4ViewRect.x    = 0.0f;
        lParameters.mv4ViewRect.y    = 0.16972221f;
        lParameters.mv4ViewRect.z    = 1.0f;
        lParameters.mv4ViewRect.w    = 0.83888888f;
        lParameters.mv4PaddingRect.x = 0.4f;
        lParameters.mv4PaddingRect.y = 0.22805555f;
        lParameters.mv4PaddingRect.z = 0.9375f;
        lParameters.mv4PaddingRect.w = 0.78333336f;
        lParameters.meMapType        = GuiEventRenderMainMap::E_MAPTYPE_MAINMAP;

        mMainMapComponent.Construct(mpStateInterface, &lParameters);
        mMainMapComponent.Prepare();
        mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_MEDIUM, 0.0f, false);
        mMainMapComponent.SetStickMapToScreenEdges(false, false, false, false);

        // The X360 stack-builds the wrapper record inline: { payload size 1, type 555,
        // payload offset 12 } posted at 16 bytes on channel 40.
        GuiEventUnnamed555Payload lEnteredEvent;
        CgsGui::GuiEventWrapper<GuiEventUnnamed555Payload, KI_CHANNEL_GUI_EVENT_OUT>
            lEnteredRecord(lEnteredEvent);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEnteredRecord),
            lEnteredRecord.GetChannel(),
            static_cast<s32>(sizeof(lEnteredRecord)));

        meCursorMode = E_CURSORMODE_NONE;
        mCursor.Construct(mCursorName, mpStateInterface,
                          KF_CURSOR_MOVEMENT_SCALAR, KF_CURSOR_START_X, KF_CURSOR_START_Y, 0);
        // The X360 does not call a setter here: it INLINES GuiCursor::SetBounds as a single
        // 16-byte store into the cursor's mv4BoundsRect lane -- `li r11, 0x5F30;
        // lvx128 v0, r0, r10; stvx128 v0, r31, r11` @0x824CB2B8..0x824CB2C8, where
        // 0x5F30 == mCursor (state +0x5E60) + 0xD0. The stack quad it loads was filled at
        // 0x824CB198..0x824CB214 with {flt_82001CC0, flt_82001CC0, flt_82066040,
        // flt_8201A7A4} == {0, 0, 1280, 720}, i.e. {left, top, right, bottom}.
        // SetBounds is the DWARF's own name for that writer (BrnCursor.h:212).
        const Vector4 lv4CursorBounds = { KF_CURSOR_RECT_LEFT, KF_CURSOR_RECT_TOP,
                                          KF_CURSOR_RECT_RIGHT, KF_CURSOR_RECT_BOTTOM };
        mCursor.SetBounds(lv4CursorBounds);
        mCursor.SetAlwaysSnap(true);

        // The four apt components, each through vtable slot 0 with a NULL parent name.
        mCrashNavPanel.Construct(macCrashNavPanelName, mpStateInterface, 0);
        mCrashNavLegend.Construct(macCrashNavLegendName, mpStateInterface, 0);
        mTitleButtonsAnimation.Construct(KAC_TITLE_BUTTONS_ANIMATION_COMPONENT, mpStateInterface, 0);
        mButtonPromptsAnimation.Construct(KAC_BUTTON_PROMPTS_ANIMATION_COMPONENT, mpStateInterface, 0);

        // The per-visit reset, in raw-asm store order (0x824CB360..0x824CB420). It repeats
        // most of Construct's cold-start walk plus the members Construct leaves alone.
        mfInspectingEventTime    = 0.0f;                                   // flt_82001CC0
        meNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_COUNT; // 12
        meTitleButtonsState      = E_VISIBLE_ANIMATION_STATES_COUNT;       // 2
        miSatNavIconsToLoad      = KAC_CRASHNAVMAP_NUMICONS;               // li r8, 0x32
        mLockedIconInfo.SetIconType(GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR);  // stb 0, +0x6128
        mbItemsLoaded            = false;
        mpGuiCache               = 0;
        meMapState               = E_MAPSTATE_PANEL;
        mbIsCursorLockedToIcon   = false;
        mbIsScreenLoaded         = false;
        mpIconManager            = 0;
        mbSelectRivals           = false;                                  // stb 0, +0x6080
        mbUseRoadSigns           = false;                                  // stb 0, +0x6081
        mbDrawDriveThrus         = true;                                   // stb 1, +0x6082
        mbSelectDriveThrus       = false;                                  // stb 0, +0x6083
        meEventIconDisplayType   = GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT;  // 5
        mbIsInEvent              = false;
        mbIsExiting              = false;
        meScreenType             = E_SCREEN_TYPE_OFFLINE;
        mi8CurrentEventIndex     = 0;
        meWaitforData            = E_WAITFOR_NONE;                         // 3
        mpLockedIconName         = 0;
        muHoveredEventID         = 0;
        muInspectingEventID      = 0;
        mLockedIconInfo.SetPositionLane(Vector4());                        // stvx128 zero, +0x6100
        mHoveredDriveThruID      = 0;
        mHoveringRivalId         = 0;
        mbLocalPlayerSelected    = false;
        mPlayerName.macName[0]   = '\0';
        mv2MapScrollVelocity.SetZero();                                    // stvx128 zero, +0x6070
        mv2WorldCentrePoint.SetZero();                                     // stvx128 zero, +0x60F0

        // Seed the map-move/scroll debouncer with the cursor's world position and clear
        // the "was scrolling" latch. GuiCursor::GetPosition() returns Vector2 BY VALUE (DWARF h:334).
        mSoundData.Update(MapTransform::DeviceToWorld(mCursor.GetPosition()), false);
    }
}
