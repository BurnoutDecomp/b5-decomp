#pragma once

#include <cstddef>   // offsetof (_AssertLayout relative pins)
#include "types.hpp"
#include "BrnCommonTypes.h"                                              // Vector2 / Vector4 (rw::math::vpu)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"          // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // GameStateModuleIO::EGameModeType (IsMap*/IsMapPan* params; house precedent: BrnScreenShared.h / BrnNorthIndicator.h)
#include "GameSource/Gui/BrnGuiTextField.h"                              // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"               // BrnGui::IconComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h" // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/SatNav/BrnMainMap.h"                            // BrnGui::MainMapComponent (by value)
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                     // BrnGui::MapIconManager (::OwnerId member)

// Pointer-only in this header: the queued apt-trigger record HandleAptEvents consumes.
// Full type: GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h (the .cpp
// includes it) -- forward-declared here to keep the apt-communicator cascade out of the
// class header (documented exception (b)).
namespace CgsGui { struct GuiEventAptTriggerPayload; }
namespace BrnGui { class GuiCache; }

// OPAQUE-ENUM forward declaration (documented exception (b) -- avoids pulling the whole
// BrnGuiShared.h vocabulary here). Real home + values: BrnGui::ECompassPoints,
// DecFIGS DWARF BrnGuiShared.h:959 (N=0, NW=1, W=2, SW=3, S=4, SE=5, E=6, NE=7, COUNT=8)
// -- the same order as this TU's KAAC_COMPASS_DIRECTION string table @0x82F27820
// ("DIRECTION_N","DIRECTION_NW",...). A shared_header_request adds the definition to
// GameSource/Gui/BrnGuiShared.h **with this exact fixed underlying type** (`: s32`).
namespace BrnGui { enum ECompassPoints : s32; }

// BrnGui::PreRaceFlyByState - the pre-event fly-by flow state (event titles, map intro,
// medals). Owns the title/mode/description TextFields, the destination IconComponent,
// the state AnimationComponent and an embedded MainMapComponent, and drives the
// MapIconManager while the fly-by camera runs.
//
// SHAPE AUTHORITY: DecFIGS DWARF GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h
// (member set/order, method declarations, EPreRaceState), gated on the X360 ledger.
// X360 byte offsets in the comments are BURNOUT_X360_ARTIST.XEX (32-bit console ABI,
// object size 0x1040 == 4160, matching BrnHudFlow::Prepare's size-4160 state slot);
// the HOST layout is name-based -- pointers and by-value components widen, so only
// RELATIVE deltas inside pointer-free scalar runs are asserted (_AssertLayout).
//
// NOTE (conductor): GameSource/Gui/Flow/HUD/States/BrnPreRaceFlyBy.{h,cpp} carries a
// pre-wave SHELL fork of this same class (ctor @0x82514E58 + IsMapApplicableToGameMode
// @0x824B3120 + IsMapPanApplicableToGameMode @0x824B3190). Measured 2026-08-03: that
// .cpp does NOT compile against its own empty-shell header (memsets members the shell
// never declares, uses an undeclared GSM namespace). THIS header is the DWARF-canonical
// home; the HUD TU should be re-pointed at it and its ctor rebuilt against the real
// members (see the wave-J spec, scratchpad/waveJ/PreRaceFlyBy.spec.md).
namespace BrnGui
{
    struct PreRaceFlyByState : public CgsGui::State
    {
        // DWARF BrnPreRaceFlyBy.h:84 -- the fly-by flow state machine (meCurrentState).
        enum EPreRaceState
        {
            E_PRERACE_INVALID                = -1,
            E_PRERACE_UNLOADED               = 0,
            E_PRERACE_LOADING_COMPONENTS     = 1,
            E_PRERACE_ACTIVE_MAP_ICON_DELAY  = 2,
            E_PRERACE_ACTIVE_EVENT_TITLES    = 3,
            E_PRERACE_ACTIVE_MAP_INTRO       = 4,
            E_PRERACE_ACTIVE_SHOW_MAP        = 5,
            E_PRERACE_ACTIVE_MEDALS          = 6,
            E_PRERACE_ACTIVE_TRANS_OUT       = 7,
            E_PRERACE_ACTIVE_DONE            = 8,
            E_PRERACE_COUNT                  = 9,
        };

        // @ 0x82514E58 -- constructs the component set + the embedded map manager (the
        // body currently lives with the class-keyed TU; see the conductor note above).
        PreRaceFlyByState();

        // The three flow virtuals (override the CgsFsm::ScriptedState surface through
        // CgsGui::State). X360: OnEnter @0x824C6498, OnLeave @0x824C68F0, Update @0x824DC540.
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82514EE8 - hands the pre-race fly-by state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // ---- out-of-line workers (DWARF declaration order; X360 addresses) ----
        void SetupComponents();                       // @0x824D6228 (cpp:638)
        void TriggerExitState();                      // @0x824C6BD0 (cpp:722)
        void AppendExpectedComponents();              // @0x824B4DB0 (cpp:742)
        void SetEventIconResource();                  // @0x824BB5F8 (cpp:1689)
        f32  CalculateZoomFactor();                   // @0x824BE8F0 (cpp:1763)
        void UpdateIconManager();                     // @0x824C7B70 (cpp:1874)

        // DWARF BrnPreRaceFlyBy.h:331/:396. Bodied with the class-keyed TU
        // (currently GameSource/Gui/Flow/HUD/States/BrnPreRaceFlyBy.cpp -- X360
        // @0x824B3120 / @0x824B3190); this TU calls both from OnEnter/OnLeave/Update.
        bool IsMapApplicableToGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameMode);
        bool IsMapPanApplicableToGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameMode);

        void SetRaceDescription();                    // @0x824C6E90 (cpp:1128)
        void SetRoadRageDescription();                // @0x824C7078 (cpp:1203)
        void SetFreestyleDescription();               // @0x824C7230 (cpp:1266)
        void SetMarkedManDescription();               // @0x824C7470 (cpp:1346)
        void SetBurningRouteDescription();            // @0x824C76D8 (cpp:1451)

        // @0x824B4EC8 (cpp:1596). DWARF return: BrnGui::ECompassPoints (opaque-declared
        // above; N..NE counter-clockwise, COUNT == 8 -- the cpp:1648 assert bound).
        ECompassPoints FindEventDirection();

        void HandleIncomingEvents();                  // @0x824D6410 (cpp:795)

        // @0x824C6C48 (cpp:996). DWARF spells the parameter `const GuiEventAptTrigger*`;
        // the queued event-21 record is the payload (committed CgsAptCommunicator
        // convention -- the same cast BrnIntro::HandleIncomingEvents makes).
        void HandleAptEvents(const CgsGui::GuiEventAptTriggerPayload* lpTrigger);

        // ---- statics (values read from BURNOUT_X360_ARTIST.XEX -- see the wave-J spec) ----
        static const s32 KI_MAX_LINES_DESCRIPTION_TEXT = 5;             // DWARF h:109

        static const CgsGui::sResourceTuple maResourcesToLoad[1];       // @0x82F26BE0 {{200,(ResourceRequestTypes)7}}
        static const u32                    muNumResourcesToLoad;       // @0x82F26BE8 (== 1)
        static const CgsGui::sResourceTuple maPerGamemodeScreens[10];   // @0x82F26C00 {{207,4}..{216,4}} (per EGameModeType 0..9)

        static const s32 maiEventToObserve[8];                          // @0x82065CAC {6,21,64,159,160,162,164,213}
        static const s32 miNumEventsObserved;                           // @0x82065CCC (== 8)

        static const char KAC_EVENT_NAME_TEXTFIELD_NAME[10];            // @0x82065CD0 "EventName"
        static const char KAC_MODE_TYPE_TEXTFIELD_NAME[9];              // "RaceType"
        static const char* const KAAC_EVENT_DESC_TEXTFIELD_NAMES[5];    // @0x82F26BEC "DescriptionText1".."DescriptionText5"
        static const char KAC_LARGE_EVENT_ICON_NAME[9];                 // "destIcon"
        static const char KAC_STATE_ANIMATOR_NAME[14];                  // "flyByAnim_cpt"
        static const char KAC_STATE_COMPONENT_NAME[12];                 // @0x82065D04 "flyByHud_mc"

        static const f32 KAF_MODE_TYPE_PRE_EVENT_DURATION[10];          // @0x82065D10 {6,5.63,5.63,4,5.63,8,5.63,5.63,8,5.63}
        static const f32 KF_MAP_PAN_TIME;                               // @0x82065D38 (5.0; cpp-order adjacency)
        static const f32 KF_MAP_PAN_RESET_TIME;                         // @0x82065D3C (1.0; cpp-order adjacency)
        static const char macSatNavIconBaseName[11];                    // @0x82065D40 "SatNavIcon"
        static const s32 KI_PRERACEMAP_NUMICONS = 16;                   // @0x82065D4C (the OnEnter/Append 16-icon loops)
        static const f32 KF_PRERACE_MAP_VIEW_BUFFERZONE_X;              // @0x82065D50 (50.0; cpp-order adjacency)
        static const f32 KF_PRERACE_MAP_VIEW_BUFFERZONE_Y;              // @0x82065D54 (50.0; cpp-order adjacency)
        static const f32 KF_MAP_FADEIN_TIME;                            // @0x82065D58 (0.5; cpp-order adjacency)
        static const f32 KF_MAP_FADEOUT_TIME;                           // @0x82065D5C (0.5; cpp-order adjacency)
        static const f32 KAF_ICON_ANIMATION_TIME[10];                   // @0x82065D60 {1.85 x5, 2.0, 1.85, 1.85, 2.0, 1.85}
        static const f32 KAF_ICON_ANIMATION_DELAY[10];                  // @0x82065D88 {1.85 x5, 3.8, 1.85, 1.85, 3.6, 1.85}

        // Runtime-initialised on the X360 (zero in the static image; values recovered from
        // the per-static initialiser stubs @0x82C54BD0..0x82C54CDC -- see the wave-J spec).
        static const Vector4 KV4_VIEW_RECT;                             // @0x82FB4C30 {0.0, 0.1875, 1.0, 0.83888888}
        static const Vector4 KV4_PADDING_RECT;                          // @0x82FB4AF0 {0.4, 0.1875, 0.9375, 0.78333336}
        static const Vector2 K_PRERACE_LONG_DISPLAY_RECT;               // @0x82FB4AB0 {638.0, 349.8} (declaration/init order)
        static const Vector2 K_PRERACE_TALL_DISPLAY_RECT;               // @0x82FB4AA0 {638.0, 349.8} (CalculateZoomFactor picks it when the event bounds are taller than wide)

        // ---- members (DWARF order; X360 32-bit offsets in comments) ----
        TextField          mEventName;                                  // +0x038 (56)
        TextField          mModeType;                                   // +0x160 (352)
        TextField          maEventDescriptionText[KI_MAX_LINES_DESCRIPTION_TEXT]; // +0x288 (648), stride 0x128
        IconComponent      mLargeEventIcon;                             // +0x850 (2128)
        AnimationComponent mStateAnimator;                              // +0x8E4 (2276)
        CgsGui::sResourceTuple mLargeIconResource;                      // +0x970 (2416) {muId, meType(4)}
        EPreRaceState      meCurrentState;                              // +0x978 (2424)
        f32                mfTimeRemaining;                             // +0x97C (2428) counts down by GetTimeStep once state > LOADING
        bool               mbEndRequestSent;                            // +0x980 (2432) the one-shot id-163 "time expired" post
        bool               mbDoMapPan;                                  // +0x981 (2433) IsMapPanApplicableToGameMode latch
        f32                mfIconAnimationStartTime;                    // +0x984 (2436)
        GuiCache*          mpGuiCache;                                  // +0x988 (2440)
        MapIconManager*    mpIconManager;                               // +0x98C (2444)
        MapIconManager::OwnerId mIconManagerOwnerId;                    // +0x990 (2448) seeded E_PRERACE_FLYBY_MAP (3)
        MainMapComponent   mMainMapComponent;                           // +0x9A0 (2464) (16-aligned on X360)
        Vector2            mv2WorldCenterPoint;                         // +0x1020 (4128) (VMX lane; MainMapComponent::Update round-trip)
        bool               mbHiddenDueToPause;                          // +0x1030 (4144)
        s32                miPreviousIconCount;                         // +0x1034 (4148) (-1 on enter; the map-scroll-end audio edge)

        // Compile-time layout pins: RELATIVE deltas inside pointer-free scalar runs only
        // (console offsets are 32-bit and must NOT be asserted absolutely on the host).
        // offsetof on private members is legal here: a member function body is a
        // complete-class context.
        void _AssertLayout()
        {
            static_assert(offsetof(PreRaceFlyByState, meCurrentState)
                              - offsetof(PreRaceFlyByState, mLargeIconResource) == 8,
                          "sResourceTuple is {u32,enum} == 8 bytes ahead of meCurrentState");
            static_assert(offsetof(PreRaceFlyByState, mfTimeRemaining)
                              - offsetof(PreRaceFlyByState, meCurrentState) == 4,
                          "mfTimeRemaining follows meCurrentState");
            static_assert(offsetof(PreRaceFlyByState, mbEndRequestSent)
                              - offsetof(PreRaceFlyByState, mfTimeRemaining) == 4,
                          "mbEndRequestSent follows mfTimeRemaining");
            static_assert(offsetof(PreRaceFlyByState, mbDoMapPan)
                              - offsetof(PreRaceFlyByState, mbEndRequestSent) == 1,
                          "mbDoMapPan is the byte after mbEndRequestSent");
            static_assert(offsetof(PreRaceFlyByState, mfIconAnimationStartTime)
                              - offsetof(PreRaceFlyByState, mbEndRequestSent) == 4,
                          "mfIconAnimationStartTime closes the scalar run");
            static_assert(offsetof(PreRaceFlyByState, miPreviousIconCount)
                              - offsetof(PreRaceFlyByState, mbHiddenDueToPause) == 4,
                          "miPreviousIconCount is the word after mbHiddenDueToPause");
        }
    };
}
