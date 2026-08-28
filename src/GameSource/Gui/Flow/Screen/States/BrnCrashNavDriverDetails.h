#pragma once

// ===================================================================================
// BrnGui::CrashNavDriverDetails -- THE OFFLINE IN-GAME PAUSE SCREEN.
//
// ⭐ WHAT THIS SCREEN IS. It is what the console opens when the player presses START
// during free burn: InGame::HandleControllerInput case KI_ACTION_PAUSE_DRIVER_DETAILS
// (45 == GUI_START) -> InGame::PauseGame(true, true) -> InGame::OpenDriverDetails()
// -> ShutDownHudComponents + SendStateEvent("TO_D_DETAIL") -> the BRNSCREENFSM
// transition Transition_4INGAME_111CN_D_DETAIL -> this state (script id CN_D_DETAIL,
// BrnScreenFlow.cpp:308). Its OnEnter posts the SAME deactivate pair CrashNavMapMain's
// does -- GuiEventActivateCrashNav(false) + GuiEventNetworkSuspension(true) -- which is
// what stops the sim, so this screen IS the pause, not merely a screen shown while
// paused. HandleControllerInputPressed's 45/50 arm posts the mirror-image resume records
// and "GO_BACK". Three menu rows: $DDETAILS_LICENSE / $DDETAILS_RECORDS /
// $DDETAILS_DISCOVER.
//
// SHAPE. DecFIGS DWARF (dwarfdump .../BrnCrashNavDriverDetails.h): the base is
// CgsGui::State and every member below is the DWARF's, in the DWARF's declaration
// order -- which the X360 ctor @0x825001E0 and OnEnter @0x824CEC80 independently
// confirm store-for-store (the guest offsets in the trailing comments are MEASURED
// from those two bodies, not inferred from declaration order):
//    +0x0038  maDistrictBillboardsTextfields[5]   (0x128 stride, vptr stores 0x38..0x4D8)
//    +0x0600  maDistrictJumpsTextfields[5]
//    +0x0BC8  maDistrictSmashesTextfields[5]
//    +0x1190  mStatsPanelAnimator                 (the ONE off_82072F68 vptr in the run)
//    +0x121C  maStatTextfields[33]                (the ctor's 33-iteration vptr loop)
//    +0x3844  meInternalState                     (OnEnter's closing store)
//    +0x3848  mMenuComponent                      (ctor: MenuComponent::MenuComponent)
//    +0x4908  mbStatsDataReceived
//    +0x490C  mpGuiCache
//    +0x4910  mLicenseComponent                   (ctor: off_82073014)
//    +0x50DC  mbShowLicense
//    +0x50E0  mPhotoBoothComponent                (ctor: off_82073018)
//    +0x5514  mTakePhotoPrompt                    (HelpItem: off_82072FE8 + 2 TextFields)
//    +0x56C0  mbRefreshTakePhotoPrompt
//    +0x56C1  mbIsCameraConnected
//    +0x56C4  meAchievementsTickerState
//    +0x56C8  mAchievementsPrompt
// The guest offsets are DOCUMENTARY: the host is LLP64 and every embedded component
// widens, so the reconstruction is name-based throughout.
//
// ⚠️ OBJECT SIZE. This state is ~22 KB on the console and larger on x64. It is carved
// out of BrnGuiModule's 2 MB s_screenStatePoolBacking by BrnScreenFlow::Prepare's
// LinearMalloc; an overflow is NOT silent -- NewPoolState returns 0 and the per-slot
// null assert at BrnScreenFlow.cpp:227 fires.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"                                // TextField (embedded x43)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"   // AnimationComponent
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"        // MenuComponent
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"             // HelpItem
#include "GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.h"     // LicenseComponent
#include "GameSource/Gui/Flow/Screen/Components/BrnPhotoBoothComponent.h"  // PhotoBoothComponent

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;                 // pointer-only member
    struct GuiEventStatsResponse;   // BrnGuiDemangledEventTypes.h (event 436 payload)

    struct CrashNavDriverDetails : public CgsGui::State
    {
        // DWARF BrnCrashNavDriverDetails.h:73. The ladder Update walks; each rung's
        // Update* returns "advance now" and Update falls straight through to the next.
        enum InternalState
        {
            E_INTERNALSTATE_GETCACHE      = 0,
            E_INTERNALSTATE_SETUPLICENSE  = 1,
            E_INTERNALSTATE_LOADRESOURCES = 2,
            E_INTERNALSTATE_WFINIT        = 3,
            E_INTERNALSTATE_RUNNING       = 4,
            E_INTERNALSTATE_LEAVING       = 5,
            E_INTERNALSTATE_COUNT         = 6,
        };

        // DWARF BrnCrashNavDriverDetails.h:188.
        enum E_ACHIEVEMENTS_TICKER
        {
            E_ACHIEVEMENTS_TICKER_INACTIVE = 0,
            E_ACHIEVEMENTS_TICKER_SHOWING  = 1,
            E_ACHIEVEMENTS_TICKER_SHOWN    = 2,
        };

        // Table sizes, all X360-measured: 33 == the ctor's vptr-loop count and the
        // KAPC_STAT_TEXTFIELD_NAMES span (off_82F271B8..off_82F2723C), 5 == the three
        // district loops' `cmpwi r28, 0x14` byte bound / 4.
        static const s32 KI_NUM_STAT_TEXTFIELDS = 33;
        static const s32 KI_NUM_DISTRICTS       = 5;

        // DWARF BrnCrashNavDriverDetails.cpp:46.
        static const s32 KI_MAX_LENGTH_STATS_STRING = 64;

        // @0x825001E0 -- the vtable-establishing ctor (the X360 body is nothing but the
        // vptr stores for this object and every embedded component, plus the nested
        // MenuComponent ctor call). The C++ default ctor reproduces it exactly, so it is
        // implicitly defined; declaring it would force an empty body that says less.

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @0x82500308 -- the whole X360 body is the two stores below.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        bool UpdateInitSetup();                                          // @0x824CF038
        bool UpdateSetupLicense();                                       // @0x824C1C58
        bool UpdateLoadResources();                                      // @0x824CF1E0
        bool UpdateWFInit();                                             // @0x824BFE40
        bool UpdateRunning();                                            // @0x824B83E0
        bool UpdatePermanent();                                          // @0x824D9C18
        void ShowMenu();                                                 // @0x824BD0A0
        void HandleTriggers(const CgsModule::Event* lpAptTrigger);       // @0x824B8548
        void HandleControllerInputPressed(const CgsModule::Event* lpEvent);  // @0x824CF3B8
        void HandleStatData(const GuiEventStatsResponse* lpStatsEvent);  // @0x824B8618
        void UpdateStatsPanel();                                         // @0x824B8B80

        // DWARF BrnCrashNavDriverDetails.cpp:1205. No X360 symbol -- the console INLINES
        // it, as the single {2, 536, 12, 0} post at the head of OnLeave @0x824CEF18
        // (`a1[5553] == 1` guard == meAchievementsTickerState == E_ACHIEVEMENTS_TICKER_SHOWING).
        // De-inlined back to the DWARF's own name.
        void HideTickerMessage();

        // @0x820664F0 (.rdata, 8 words) / @0x82066518 + @0x82066528 (.rdata).
        static const s32                    maiEventToObserve[8];
        static const s32                    miNumEventsObserved;
        static const CgsGui::sResourceTuple maResourcesToLoad[];
        static const u32                    muNumResourcesToLoad;

        TextField             maDistrictBillboardsTextfields[KI_NUM_DISTRICTS];  // +0x0038
        TextField             maDistrictJumpsTextfields[KI_NUM_DISTRICTS];       // +0x0600
        TextField             maDistrictSmashesTextfields[KI_NUM_DISTRICTS];     // +0x0BC8
        AnimationComponent    mStatsPanelAnimator;                               // +0x1190
        TextField             maStatTextfields[KI_NUM_STAT_TEXTFIELDS];          // +0x121C
        InternalState         meInternalState;                                   // +0x3844
        MenuComponent         mMenuComponent;                                    // +0x3848
        bool                  mbStatsDataReceived;                               // +0x4908
        GuiCache*             mpGuiCache;                                        // +0x490C
        LicenseComponent      mLicenseComponent;                                 // +0x4910
        bool                  mbShowLicense;                                     // +0x50DC
        PhotoBoothComponent   mPhotoBoothComponent;                              // +0x50E0
        HelpItem              mTakePhotoPrompt;                                  // +0x5514
        bool                  mbRefreshTakePhotoPrompt;                          // +0x56C0
        bool                  mbIsCameraConnected;                               // +0x56C1
        E_ACHIEVEMENTS_TICKER meAchievementsTickerState;                         // +0x56C4
        HelpItem              mAchievementsPrompt;                               // +0x56C8
    };
}
