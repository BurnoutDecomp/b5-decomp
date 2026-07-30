#pragma once

// ===================================================================================
// BrnGui::CrashNavMap  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h
//   TU: GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.cpp (19 functions, todo)
//
// The crash-nav map screen-state BASE: owns the main map component, the crash-nav
// panel/legend, the map cursor and the map-icon bookkeeping. Derived screens:
// CrashNavMapMain / CrashNavMapEvent (own TUs) and OnlineGameRoomPlayerInfo (which
// embeds ~63 KB of its own members after this ~24.9 KB base).
//
// CLASS SHAPE + MEMBER ORDER: DecFIGS DWARF
//   references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h
// MEMBER PLACEMENT: X360 ARTIST asm (offsets below are X360 byte offsets):
//   Construct @0x824B6660 / OnEnter @0x824CB158 / Update @0x824DD6D8 store-walked, plus
//   the OnlineGameRoomPlayerInfo TU's accesses into this base (its dossier, wave H).
//   CgsGui::State base ends at +56 on X360 (CgsID alignment rounds 52->56).
//     +56   meMapState            (stw; OnEnter 0=PANEL, OGRPI CheckForCompletedLoads
//                                  case LOADING_MAP_SCREEN_COMPONENTS stores 1=MAP)
//     +60   mePreviousMapState
//     +64   meWaitforData         (stw; OnEnter 3 = E_WAITFOR_NONE)
//     +68   mbIsScreenLoaded      (lbz/stb; CheckForLoadComplete sets it)
//     +69   mbIsExiting
//     +70   mi8CurrentEventIndex
//     +72   mpGuiCache            (Update event-64: *(this+72) = payload cache)
//     +76   mpIconManager         ("mpIconManager" assert @+76)
//     +80   mIconManagerOwnerId
//     +96   mMainMapComponent     (MainMapComponent::Construct(this+96, ...))
//     +1760 mCrashNavPanel        ("CrashNavPanel_mc" ctor vcall @+1760)
//     +22960 mCrashNavLegend      ("CrashNavLegend_mc" ctor vcall @+22960)
//     +24148 miSatNavIconsToLoad  (OnEnter 50 == KAC_CRASHNAVMAP_NUMICONS)
//     +24152 mbItemsLoaded
//     +24160 mCursor              (GuiCursor::Construct(this+24160,"cursor_mc",...); 240 B)
//     +24400 meCursorMode         (OnEnter 0=NONE; map-release handlers store 1=SELECTING_ICONS)
//     +24404 mTitleButtonsAnimation   ("TitleButtonsAnimation" ctor vcall; 140 B)
//     +24544 mButtonPromptsAnimation  ("ButtonsAnimation" ctor vcall; 140 B)
//     +24688 mv2MapScrollVelocity     (16-aligned vector-zero store)
//     +24704 mbSelectRivals  +24705 mbUseRoadSigns  +24706 mbDrawDriveThrus
//     +24707 mbSelectDriveThrus
//     +24708 meEventIconDisplayType   (ctor/OnEnter 5; OGRPI::Construct stores 0)
//     +24712 mbIsCursorLockedToIcon
//     +24716 mpLockedIconName  +24720 mpLockedIconNameLastFrame
//     +24724 muHoveredEventID  +24728 muHoveredEventIDLastFrame
//     +24732 muInspectingEventID      (map-release handlers zero it)
//     +24736 mfInspectingEventTime
//     +24744 mHoveredDriveThruID      +24752 mHoveredDriveThruIDLastFrame  (CgsID)
//     +24760 mHoveringRivalId         +24768 mHoveringRivalIdLastFrame     (CgsID)
//     +24776 mPlayerName (16 B)
//     +24792 mbLocalPlayerSelected  +24793 mbIsInEvent
//     +24796 meScreenType         (OGRPI stores 1=ONLINE / 2=ONLINE_FROM_PAUSE)
//     +24800 meTitleButtonsState  (ctor/OnEnter 2 = E_VISIBLE_ANIMATION_STATES_COUNT)
//     +24804 meNavigationButtonsState (ctor/OnEnter 12 = E_BUTTON_PROMPT_..._COUNT)
//     +24816 mv2WorldCentrePoint  (16-aligned)
//     +24832 mLockedIconInfo      (SatNavIconInfo, 48 B vector-zeroed)
//     +24880 mSoundData           (vector-zero + word @24896)
//     +24900 mfMapPanningStopTime
//   X360 sizeof(CrashNavMap) == 24928 (the derived OGRPI's own members start there).
//
// The x64 host layout is NAME-BASED (pointers widen); the X360 offsets above are
// documentation + the relative scalar-run pins in _AssertLayout below. Method bodies
// belong to the BrnCrashNavMap.cpp TU (still todo) -- declarations only here.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                             // Vector2 / Vector3 / CgsID
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"         // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"     // CgsGui::GuiComponent (stopgap base)
#include "GameSource/GameState/BrnCgsPlayerName.h"                      // CgsNetwork::PlayerName
#include "GameSource/Gui/SatNav/BrnMainMap.h"                           // BrnGui::MainMapComponent (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h"     // BrnGui::CrashNavPanel (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavLegend.h"    // BrnGui::CrashNavLegend (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnCursor.h"            // BrnGui::GuiCursor (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h" // BrnGui::AnimationComponent (by value)

#include <cstddef>   // offsetof (layout pins in _AssertLayout)

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;         // GameSource/Gui/BrnGuiCache.h (pointer member only)
    class MapIconManager;   // GameSource/Gui/SatNav/BrnMapIconManager.h (pointer member only)

    // The AnimationComponent stopgap that used to stand in here is RETIRED (intro wave
    // 2026-07-29): the real BrnGui::AnimationComponent is homed in
    // GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h, included above. Its
    // X360 sizeof is 140 == CgsGui::GuiComponent's 0x8C (this state's own OnEnter proves
    // the stride: 24404 -> 24544 -> 24684), so it adds no members and the stopgap's
    // 0x40-byte reserved tail was surplus.

    // DWARF BrnCrashNavMap.h:52 -- the map-move/scroll sound debouncer embedded at the
    // class tail (mSoundData @X360 +24880).
    struct CrashNavMapSoundData
    {
        Vector3 mPrevCurPos;        // h:86 (16-aligned vector slot on X360)
        bool    mbPrevIsScrolling;  // h:87 (X360 +24896 word-stored 0 by the ctor)

        void Construct();                              // h:61
        bool Prepare(Vector3 lv3CursorPos);            // h:70
        void Update(Vector3 lv3CursorPos, bool lbIsScrolling);  // h:80
    };

    // DWARF BrnCrashNavMap.h:102 -- struct tag per the DWARF (public inheritance).
    struct CrashNavMap : public CgsGui::State
    {
        // ---- enums (DWARF BrnCrashNavMap.h:106..179) --------------------------------
        enum EVisibleAnimationStates
        {
            E_VISIBLE_ANIMATION_STATES_VISIBLE   = 0,
            E_VISIBLE_ANIMATION_STATES_INVISIBLE = 1,
            E_VISIBLE_ANIMATION_STATES_COUNT     = 2,
        };

        enum EButtonPromptAnimationStates
        {
            E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_NONE_SELECTED          = 0,
            E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_EVENT                  = 1,
            E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES           = 2,
            E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_ONLINE_SCORES            = 3,
            E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE                         = 4,
            E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_PAUSE                   = 5,
            E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_RIVAL                   = 6,
            E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_PAUSE_RIVAL             = 7,
            E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_X360      = 8,
            E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_BACK      = 9,
            E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_ONLINE_SCORES_BACK       = 10,
            E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_X360_BACK = 11,
            E_BUTTON_PROMPT_ANIMATION_STATES_COUNT                          = 12,
        };

        enum MapState
        {
            E_MAPSTATE_PANEL  = 0,
            E_MAPSTATE_MAP    = 1,
            E_MAPSTATE_LEGEND = 2,
        };

        enum WaitForData
        {
            E_WAITFOR_LANDMARK_INFO = 0,
            E_WAITFOR_RIVAL_INFO    = 1,
            E_WAITFOR_PLAYER_INFO   = 2,
            E_WAITFOR_NONE          = 3,
        };

        enum CursorMode
        {
            E_CURSORMODE_NONE            = 0,
            E_CURSORMODE_SELECTING_ICONS = 1,
            E_CURSORMODE_INSPECTING_ICONS = 2,
            E_CURSORMODE_ZOOMEDOUT       = 3,
            E_CURSORMODE_PANNING         = 4,
            E_CURSORMODE_COUNT           = 5,
        };

        enum EScreenType
        {
            E_SCREEN_TYPE_OFFLINE           = 0,
            E_SCREEN_TYPE_ONLINE            = 1,
            E_SCREEN_TYPE_ONLINE_FROM_PAUSE = 2,
            E_SCREEN_TYPE_COUNT             = 3,
        };

        CrashNavMap();   // compiler-visible default ctor (the pool NewPoolState path)

        // ---- lifecycle virtuals (DWARF; bodies are the BrnCrashNavMap.cpp TU) -------
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);   // @0x824B6660
        virtual void OnEnter();                                           // @0x824CB158
        virtual void OnLeave();                                           // @0x824CB4B8 (DWARF cpp:418)
        virtual void Update();                                            // @0x824DD6D8

    protected:
        // ---- protected virtuals (DWARF order) ---------------------------------------
        virtual void HandleCrashNavInputPressed(const CgsModule::Event* lpEvent);   // @0x824B6798
        virtual void HandleCrashNavInputReleased(const CgsModule::Event* lpEvent);  // @0x824B67B8
        virtual void AppendExpectedAptComponents();                                 // @0x824B67E0
        virtual void SetupComponents();                                             // @0x824D8C40

        // ---- non-virtual helpers this TU owns (DWARF set; declarations only) --------
        void CheckForLoadComplete();                        // @0x824CB660
        void UpdateGuiCache(const CgsModule::Event* lpEvent);
        void UpdateMainMap();
        void UpdateSoundEvents(bool lbIsScrolling);
        void ResetIconManager(const CgsModule::Event* lpEvent);   // @0x824B6BE0
        void UpdateIconManager();
        void MoveCursor(const CgsModule::Event* lpEvent);         // @0x824BF100
        void SetMapPanelState(MapState leMapState);
        void SetFilterFromPanel();
        void UpdateRoadRule(const CgsModule::Event* lpEvent);
        void UpdateEvent();                                       // @0x824CC3F8
        void UpdateDrivethru();                                   // @0x824B7158
        void UpdateRival();
        void UpdateCursorStatus();                                // @0x824CB770
        void UpdateButtonPrompts();                               // @0x824B68B8
        void UpdateEventInfoPanel();
        f32  CalculateEventZoomFactor();                          // @0x824BF4B0
        bool PlaceCursorOnPlayer();                               // @0x824CBAA8 region
        void ZoomIn();
        void ZoomOut();
        void ZoomUpdate();

        // ---- statics (DWARF; values in the BrnCrashNavMap.cpp TU) -------------------
        // maResourcesToLoad @0x82F26E84 = {132,4},{83,4},{48,4},{70,4},{72,4},{49,4},{50,4}
        // muNumResourcesToLoad @0x82F26EBC = 7 (both dumped, boundaries observed --
        // scratchpad/waveH/ogrpi_rodata2.txt). The OGRPI screen unloads THIS table in
        // UnloadMapResources, so the statics are protected (derived access).
        static const CgsGui::sResourceTuple maResourcesToLoad[7];
        static const u32                    muNumResourcesToLoad;

        // ---- data members (DWARF order; X360 offsets in the header banner) ----------
        MapState        meMapState;               // X360 +56
        MapState        mePreviousMapState;       // X360 +60
        WaitForData     meWaitforData;            // X360 +64
        bool            mbIsScreenLoaded;         // X360 +68
        bool            mbIsExiting;              // X360 +69
        s8              mi8CurrentEventIndex;     // X360 +70
        GuiCache*       mpGuiCache;               // X360 +72
        MapIconManager* mpIconManager;            // X360 +76
        u32             mIconManagerOwnerId;      // X360 +80 (MapIconManager::OwnerId)
        MainMapComponent mMainMapComponent;       // X360 +96
        CrashNavPanel   mCrashNavPanel;           // X360 +1760
        CrashNavLegend  mCrashNavLegend;          // X360 +22960
        bool            mbShouldUpdateRoute;      // DWARF h:236 (X360 slot un-attested; kept in order)
        s32             miSatNavIconsToLoad;      // X360 +24148 (OnEnter: 50)
        bool            mbItemsLoaded;            // X360 +24152
        GuiCursor       mCursor;                  // X360 +24160 (240 B on X360)
        CursorMode      meCursorMode;             // X360 +24400
        AnimationComponent mTitleButtonsAnimation;   // X360 +24404
        AnimationComponent mButtonPromptsAnimation;  // X360 +24544
        Vector2         mv2MapScrollVelocity;     // X360 +24688 (16-aligned)
        bool            mbSelectRivals;           // X360 +24704
        bool            mbUseRoadSigns;           // X360 +24705 (OGRPI::Construct stores false)
        bool            mbDrawDriveThrus;         // X360 +24706
        bool            mbSelectDriveThrus;       // X360 +24707
        s32             meEventIconDisplayType;   // X360 +24708 (GuiEventDrawEventIcons::EIconDisplayType)
        bool            mbIsCursorLockedToIcon;   // X360 +24712
        const char*     mpLockedIconName;         // X360 +24716
        const char*     mpLockedIconNameLastFrame; // X360 +24720
        u32             muHoveredEventID;         // X360 +24724
        u32             muHoveredEventIDLastFrame; // X360 +24728
        u32             muInspectingEventID;      // X360 +24732
        f32             mfInspectingEventTime;    // X360 +24736
        CgsID           mHoveredDriveThruID;          // X360 +24744
        CgsID           mHoveredDriveThruIDLastFrame; // X360 +24752
        CgsID           mHoveringRivalId;             // X360 +24760
        CgsID           mHoveringRivalIdLastFrame;    // X360 +24768
        CgsNetwork::PlayerName mPlayerName;       // X360 +24776 (16 B)
        bool            mbLocalPlayerSelected;    // X360 +24792
        bool            mbIsInEvent;              // X360 +24793
        EScreenType     meScreenType;             // X360 +24796
        EVisibleAnimationStates      meTitleButtonsState;      // X360 +24800
        EButtonPromptAnimationStates meNavigationButtonsState; // X360 +24804
        Vector2         mv2WorldCentrePoint;      // X360 +24816 (16-aligned)
        // DWARF h:288 GuiEventUpdateSatNav::SatNavIconInfo -- 48-byte record on X360
        // (+24832..+24879, vector-zeroed by the ctor). The SatNavIconInfo home
        // (BrnMapIconManager.h) is not pulled in here to keep this state header light;
        // real named record, opaque width, same recipe as the icon manager's own slice.
        struct LockedIconInfo { u8 maRecord[48]; };
        LockedIconInfo  mLockedIconInfo;          // X360 +24832
        CrashNavMapSoundData mSoundData;          // X360 +24880
        f32             mfMapPanningStopTime;     // X360 +24900

    private:
        // Never called -- complete-class context so offsetof works on the protected
        // members above. Pins are RELATIVE deltas inside pointer-free scalar runs (the
        // x64 gate widens pointers, so absolute X360 offsets cannot hold host-side).
        static void _AssertLayout()
        {
            // run A: the State-adjacent scalar block +56..+70 (X360 deltas exact)
            static_assert(offsetof(CrashNavMap, mePreviousMapState) - offsetof(CrashNavMap, meMapState) == 4,  "run A order");
            static_assert(offsetof(CrashNavMap, meWaitforData)      - offsetof(CrashNavMap, meMapState) == 8,  "run A order");
            static_assert(offsetof(CrashNavMap, mbIsScreenLoaded)   - offsetof(CrashNavMap, meMapState) == 12, "run A order");
            static_assert(offsetof(CrashNavMap, mbIsExiting)        - offsetof(CrashNavMap, meMapState) == 13, "run A order");
            static_assert(offsetof(CrashNavMap, mi8CurrentEventIndex) - offsetof(CrashNavMap, meMapState) == 14, "run A order");
            // run B: the bool/enum cluster +24704..+24712 (X360 deltas exact)
            static_assert(offsetof(CrashNavMap, mbUseRoadSigns)     - offsetof(CrashNavMap, mbSelectRivals) == 1, "run B order");
            static_assert(offsetof(CrashNavMap, mbDrawDriveThrus)   - offsetof(CrashNavMap, mbSelectRivals) == 2, "run B order");
            static_assert(offsetof(CrashNavMap, mbSelectDriveThrus) - offsetof(CrashNavMap, mbSelectRivals) == 3, "run B order");
            static_assert(offsetof(CrashNavMap, meEventIconDisplayType) - offsetof(CrashNavMap, mbSelectRivals) == 4, "run B order");
            // run C: the screen-type tail +24792..+24804 (X360 deltas exact)
            static_assert(offsetof(CrashNavMap, mbIsInEvent)  - offsetof(CrashNavMap, mbLocalPlayerSelected) == 1, "run C order");
            static_assert(offsetof(CrashNavMap, meScreenType) - offsetof(CrashNavMap, mbLocalPlayerSelected) == 4, "run C order");
            static_assert(offsetof(CrashNavMap, meTitleButtonsState) - offsetof(CrashNavMap, mbLocalPlayerSelected) == 8, "run C order");
            static_assert(offsetof(CrashNavMap, meNavigationButtonsState) - offsetof(CrashNavMap, mbLocalPlayerSelected) == 12, "run C order");
        }

    };
}
