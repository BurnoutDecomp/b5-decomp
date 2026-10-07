#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"   // BrnGui::TextField (by value)

// BrnGui::OnlineStats - the online-stats screen state (ON_STATS): six text fields filled
// from the stats response. The base derivation (CgsGui::State), the member names and order
// and the virtual set are the original declaration's; member placement is the console's.
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // pointer member only
    struct GuiEventOnlineStatsResponse;

    struct OnlineStats : public CgsGui::State
    {
        // The screen's internal state machine (console +0x3C).
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

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825004C0 - hands the online-stats screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        void UpdateGetCache();
        bool UpdateWFInit();
        void UpdateSetupComponents();
        void UpdateRunning();
        void UpdatePermanent();
        void ClearExpectedComponent();
        // The in-queue hands the handler the header-stripped payload (declared over
        // GuiEventControllerInputPressed).
        void HandleControllerInputPressed(const CgsModule::Event* lpEvent);
        void HandleStatsData(const GuiEventOnlineStatsResponse* lpStats);

        static const s32                    maiEventToObserve[7];
        static const s32                    miNumEventsObserved;  // == 7
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x8205FA1C (unk_8205FA1C, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x8205FA24 (dword_8205FA24, .rdata) == 1

        static const u32 KU_MAX_INIT_COMPONENTS_NUM = 4;

        static const char KAC_TEXTFIELD_NAME_TOTAL_GAMES[14];      // "TotalGames_mc"
        static const char KAC_TEXTFIELD_NAME_WIN_RATE[11];         // "WinRate_mc"
        static const char KAC_TEXTFIELD_NAME_TAKEDOWNS[13];        // "Takedowns_mc"
        static const char KAC_TEXTFIELD_NAME_RIVALS[10];           // "Rivals_mc"
        static const char KAC_TEXTFIELD_NAME_MUGSHOTS[12];         // "Mugshots_mc"
        static const char KAC_TEXTFIELD_NAME_DISCONNECT_RATE[18];  // "DisconnectRate_mc"

        // ---- data members (declaration order; console offsets in the comments) -------
        GuiCache*      mpGuiCache;                                            // +0x38
        EInternalState meInternalState;                                       // +0x3C
        u32            mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM];   // +0x40
        u32            muNumExpectedComponents;                               // +0x50
        TextField      mTotalGames;                                           // +0x54
        TextField      mWinRate;                                              // +0x17C
        TextField      mTakedowns;                                            // +0x2A4
        TextField      mRivals;                                               // +0x3CC
        TextField      mMugshots;                                             // +0x4F4
        TextField      mDisconnectRate;                                       // +0x61C
    };
}
