#pragma once

// ===================================================================================
// BrnGui::OnlineScoreboards -- owning header
//   TU: GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.cpp (21 functions, wave I)
// The online-leaderboards screen: three filter toggles (category/filter/road) over a
// LeaderboardTableComponent, a loading icon, button-prompt + table-empty animators, and
// the category->index->variation->table request ladder. CLASS SHAPE + MEMBER ORDER:
// DecFIGS DWARF (references/DecFIGS/dwarfdump/.../BrnOnlineScoreboards.h); MEMBER
// PLACEMENT: X360 ARTIST asm (wave-I dossier + scratchpad/waveI/scoreboards_rodata*.txt).
// X360 offsets are documentation; the x64 host layout is name-based.
// X360 sizeof == 20336 (BrnScreenFlow's ON_SCOREB pool-state size -- exact match).
// ARTIST-vs-DWARF deltas (X360 wins): 15 category rows (DWARF said 10 -- OnEnter clears
// 465 == 15*31 bytes, Construct wires 15 pointers); 12 observed events (DWARF said 10);
// mCurrentTargetScorePlayerName @+3568 and the bools @+3586/3587 are merge-window adds.
// In-queue handler params are `const CgsModule::Event*`; bodies cast to file-local
// payload views -- the queue delivers HEADER-STRIPPED payloads (wave-H banner rule).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                 // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::sResourceTuple
#include "GameSource/GameState/BrnCgsPlayerName.h"                              // CgsNetwork::PlayerName (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"           // BrnGui::MenuToggleGroupVarSize<3> (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnLeaderboardTableComponent.h" // BrnGui::LeaderboardTableComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"                      // BrnGui::IconComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"        // BrnGui::AnimationComponent (by value)

#include <cstddef>   // offsetof (layout pins in _AssertLayout)

namespace CgsModule { struct Event; }   // in-queue records (CgsVariableEventQueue.h; BrnCrashNavMap.h precedent)

namespace BrnGui
{
    class GuiCache;   // pointer-only (own home BrnGuiCache.h)

    // DWARF BrnOnlineScoreboards.h:46.
    struct OnlineScoreboards : public CgsGui::State
    {
        // ---- EInternalState (DWARF h:82; the +60 machine the foreign Update drives) --
        enum EInternalState
        {
            E_INTERNALSTATE_GETCACHE        = 0,
            E_INTERNALSTATE_LOADRESOURCES   = 1,
            E_INTERNALSTATE_WFDATA          = 2,
            E_INTERNALSTATE_PLAYSWF         = 3,
            E_INTERNALSTATE_WFINIT          = 4,
            E_INTERNALSTATE_SETUPCOMPONENTS = 5,
            E_INTERNALSTATE_RUNNING         = 6,
            E_INTERNALSTATE_LEFT            = 7,
            E_INTERNALSTATE_COUNT           = 8,
        };

        // ---- ELeaderboardRequestState (DWARF h:97; the +340 request ladder) ----------
        enum ELeaderboardRequestState
        {
            E_LEADERBOARD_REQUEST_STATE_GET_CATEGORIES = 0,
            E_LEADERBOARD_REQUEST_STATE_WF_CATEGORIES  = 1,
            E_LEADERBOARD_REQUEST_STATE_GET_INDEX      = 2,
            E_LEADERBOARD_REQUEST_STATE_WF_INDEX       = 3,
            E_LEADERBOARD_REQUEST_STATE_GET_VARIATIONS = 4,
            E_LEADERBOARD_REQUEST_STATE_WF_VARIATIONS  = 5,
            E_LEADERBOARD_REQUEST_STATE_GET_TABLE      = 6,
            E_LEADERBOARD_REQUEST_STATE_WF_TABLE       = 7,
            E_LEADERBOARD_REQUEST_STATE_DONE           = 8,
            E_LEADERBOARD_REQUEST_STATE_COUNT          = 9,
        };

        // One alphabetical-sort record (DWARF h:122; BuildAlphabeticalRoadIndexes' qsort
        // element -- the X360 stride 24 is sizeof(RoadSortData), never the literal).
        struct RoadSortData
        {
            static const s32 KI_MAX_ROAD_STRING = 20;   // DWARF h:124

            s32  liIndex;                            // h:126
            char lacRoadString[KI_MAX_ROAD_STRING];  // h:127
        };

        // ARTIST bounds (see the banner for the DWARF deltas).
        static const u32 KU_MAX_INIT_COMPONENTS_NUM = 4;    // DWARF h:139
        static const s32 KI_MAX_CATEGORIES          = 15;   // ARTIST (DWARF said 10)
        static const s32 KI_MAX_INDEXES             = 10;
        static const s32 KI_MAX_VARIATIONS          = 66;
        static const s32 KI_NUM_ALPHABETICAL_ROADS  = 64;

        OnlineScoreboards();   // DWARF-attested; X360 body is compiler-synthesised (the
                               // screen-flow pool ctor). Not one of this TU's 21 ledger
                               // functions -- no definition here (OGRPI precedent).

        // ---- lifecycle virtuals (dumped vtable @0x820755F0) --------------------------
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);   // @0x824866F0 cpp:123
        virtual void OnEnter();                                           // @0x8249F1D0 cpp:224
        virtual void OnLeave();                                           // @0x8249F3B0 cpp:383
        // @0x824B05B0 cpp:285 -- the meInternalState machine. Body owned by a FOREIGN
        // ledger TU (`reviewed` there; defined nowhere yet). Declared for the vtable.
        virtual void Update();

        // @ 0x82508DA8 - hands the scoreboards screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

        // DWARF h:77 -- public accessor; body owned by a foreign TU (declare-only).
        const char* GetCurrentlySelectedGame();

    private:
        // ---- this TU's remaining 20 ledger functions (X360 @; cpp:<line> = DWARF) ----
        void UpdateGetCache();                                        // @0x824917E0 cpp:459
        bool UpdateWFInit();                                          // @0x824869A0 cpp:497
        void UpdateSetupComponents();                                 // @0x824918C8 cpp:519
        void UpdateRunning();                                         // @0x824ADA18 cpp:541
        void UpdatePermanent();                                       // @0x824A96A0 cpp:590
        void ClearExpectedComponent();                                // @0x82486928 cpp:420
        void HandleControllerInputPressedUsingTable(const CgsModule::Event* lpEvent); // @0x824A9B78 cpp:697
        void HandleLeaderboardRequestState();                         // @0x8249F458 cpp:953
        void SetupCategories();                                       // @0x8248F838 cpp:1277
        void SetupIndexes();                                          // @0x8248F8D8 cpp:1299
        void SetupVariations();                                       // @0x8248F998 cpp:1330
        void ShowTableEmpty(bool lbShow);                             // @0x82486A08 cpp:1390
        void BuildAlphabeticalRoadIndexes();                          // @0x82486860 cpp:183
        void TriggerSound(s32 leGameInputAction);                     // @0x8249F710 cpp:1418 (EGameInputActions)
        static int _RoadQSortFunction(const void* lpRoadData1, const void* lpRoadData2); // @0x824867C8 cpp:164
        void PageUp(bool lbMoveHighlight);                            // @0x8249F820 cpp:1470
        void PageDown(bool lbMoveHighlight);                          // @0x8249F8A8 cpp:1503
        void SetupButtons();                                          // @0x82486A58 cpp:1535

        // ---- methods of THIS class owned by FOREIGN ledger TUs (ledger `reviewed`
        //      there, defined nowhere in the tree yet -- the wave-H phenomenon).
        //      Declared so this TU's bodies can call them. DWARF params are typed event
        //      pointers; the queue delivers header-stripped payloads (banner note). ----
        void HandleCategoryList(const CgsModule::Event* lpEvent);     // cpp:1066
        void HandleIndexList(const CgsModule::Event* lpEvent);        // cpp:1105
        void HandleVariationList(const CgsModule::Event* lpEvent);    // cpp:1151
        void HandleTableData(const CgsModule::Event* lpEvent);        // cpp:1217
        void HandleControllerInputPressedUsingFilters(const CgsModule::Event* lpEvent); // cpp:774

        // ---- statics (values in BrnOnlineScoreboards.cpp; maResourcesToLoad +
        //      muNumResourcesToLoad are DEFINED in BrnScreenStatesDataLinkStubs.cpp) ---
        static const s32 maiEventToObserve[12];             // cpp:31 @0x8205F648 (ARTIST 12; DWARF said 10)
        static const s32 miNumEventsObserved;               // cpp:47 == 12 (@0x8205F678)
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x8205F67C (unk_8205F67C, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x8205F684 (dword_8205F684, .rdata) == 1
        static const char* const KAPC_FILTER_TITLE_STRINGS[3];    // cpp:57 @0x82F2687C
        static const char KAC_FILTER_GROUP_NAME[10];        // "filter_mc"
        static const char KAC_LEADERBOARD_TABLE_NAME[9];    // "Table_mc"
        static const char KAC_LOADING_ICON_NAME[11];        // "Loading_mc"
        static const char KAC_BUTTON_ANIMATOR_NAME[19];     // "ButtonPrompts_anim"
        static const char KAC_TABLE_EMPTY_ANIMATOR_NAME[16];// "TableEmpty_anim"
        static const f32 KF_DELAY_TO_REQUEST_TABLE;         // cpp:98 == 0.5f (the case-118 literal)

        // ---- data members (DWARF order + ARTIST merge-window adds; X360 offsets) -----
        GuiCache*                mpGuiCache;                  // +56
        EInternalState           meInternalState;             // +60
        u32  mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM]; // +64
        u32  muNumExpectedComponents;                        // +80
        s32  maiAlphabeticalRoadIndex[KI_NUM_ALPHABETICAL_ROADS]; // +84
        ELeaderboardRequestState meLeaderboardRequestState;  // +340
        s32  miCurrentCategory;                              // +344
        s32  miMaxCategories;                                // +348
        char maacCategories[KI_MAX_CATEGORIES][31];          // +352 (15 rows -- ARTIST)
        const char* mapcCategories[KI_MAX_CATEGORIES];       // +820
        s32  miCurrentIndex;                                 // +880
        s32  miMaxIndexes;                                   // +884
        char maacIndexes[KI_MAX_INDEXES][31];                // +888
        const char* mapcIndexes[KI_MAX_INDEXES];             // +1200
        s32  miCurrentVariation;                             // +1240
        s32  miMaxVariations;                                // +1244
        char maacVariations[KI_MAX_VARIATIONS][31];          // +1248
        const char* mapcVariations[KI_MAX_VARIATIONS];       // +3296
        s32  miCurrentFilterHighlighted;                     // +3560
        f32  mfCurrentTableWaitTime;                         // +3564
        // +3568 (16B) -- named by the "!mCurrentTargetScorePlayerName.IsEmpty()" assert
        // (cpp:711/730); absent from the Dec-07 DWARF (merge-window add). "IsEmpty" ==
        // macName[0] == 0; the X360 clears it with a single lead-word store.
        CgsNetwork::PlayerName mCurrentTargetScorePlayerName;
        bool mbUsingFilters;                                 // +3584 (OnEnter = true)
        bool mbPerRoadLeaderboard;                           // +3585
        // FLAG consumer-named: gates the '3' ev-score-target request and swaps the road
        // toggle title to "$HUD_INFO_EVENT" (absent from the DWARF).
        bool mbEventLeaderboard;                             // +3586
        // FLAG consumer-named: armed by input '3', consumed by the event-122 response.
        bool mbWaitingForTargetScoreResponse;                // +3587
        MenuToggleGroupVarSize<3> mFilterToggleGroup;        // +3592 "filter_mc" (DWARF type
                                                             //   MenuToggleGroup; the X360 calls
                                                             //   the VarSize<3> instantiation)
        LeaderboardTableComponent mTable;                    // +15376 "Table_mc"
        IconComponent            mLoadingIcon;               // +19896 "Loading_mc"
        bool                     mbCurrentlyLoading;         // +20044
        AnimationComponent       mButtonAnimator;            // +20048 "ButtonPrompts_anim"
        AnimationComponent       mTableEmptyAnimator;        // +20188 "TableEmpty_anim"
        bool                     mbTableEmptyShowing;        // +20328 (X360 sizeof == 20336)

        // Never called -- complete-class context so offsetof works on the private
        // members. Pins are RELATIVE deltas inside pointer-free scalar runs (pointers
        // widen on the x64 gate, so absolute X360 offsets cannot hold host-side).
        static void _AssertLayout()
        {
            // run A: the expected-component block through the request state (X360 +64..+344)
            static_assert(offsetof(OnlineScoreboards, muNumExpectedComponents)  - offsetof(OnlineScoreboards, mauExpectedComponentIds) == 16,  "run A order");
            static_assert(offsetof(OnlineScoreboards, maiAlphabeticalRoadIndex) - offsetof(OnlineScoreboards, mauExpectedComponentIds) == 20,  "run A order");
            static_assert(offsetof(OnlineScoreboards, meLeaderboardRequestState)- offsetof(OnlineScoreboards, mauExpectedComponentIds) == 276, "run A order");
            static_assert(offsetof(OnlineScoreboards, miCurrentCategory)        - offsetof(OnlineScoreboards, mauExpectedComponentIds) == 280, "run A order");
            // run B: the scalar tail before the filter group (X360 +3560..+3587)
            static_assert(offsetof(OnlineScoreboards, mfCurrentTableWaitTime)         - offsetof(OnlineScoreboards, miCurrentFilterHighlighted) == 4,  "run B order");
            static_assert(offsetof(OnlineScoreboards, mCurrentTargetScorePlayerName)  - offsetof(OnlineScoreboards, miCurrentFilterHighlighted) == 8,  "run B order");
            static_assert(offsetof(OnlineScoreboards, mbUsingFilters)                 - offsetof(OnlineScoreboards, miCurrentFilterHighlighted) == 24, "run B order");
            static_assert(offsetof(OnlineScoreboards, mbPerRoadLeaderboard)           - offsetof(OnlineScoreboards, miCurrentFilterHighlighted) == 25, "run B order");
            static_assert(offsetof(OnlineScoreboards, mbEventLeaderboard)             - offsetof(OnlineScoreboards, miCurrentFilterHighlighted) == 26, "run B order");
            static_assert(offsetof(OnlineScoreboards, mbWaitingForTargetScoreResponse)- offsetof(OnlineScoreboards, miCurrentFilterHighlighted) == 27, "run B order");
            // component ORDER pins (relative only -- component widths widen)
            static_assert(offsetof(OnlineScoreboards, mTable) > offsetof(OnlineScoreboards, mFilterToggleGroup),              "component order");
            static_assert(offsetof(OnlineScoreboards, mbCurrentlyLoading) > offsetof(OnlineScoreboards, mLoadingIcon),        "component order");
            static_assert(offsetof(OnlineScoreboards, mbTableEmptyShowing) > offsetof(OnlineScoreboards, mTableEmptyAnimator),"component order");
        }
    };
}
