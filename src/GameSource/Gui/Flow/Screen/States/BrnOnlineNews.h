#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"   // BrnGui::TextField (by value)

// BrnGui::OnlineNews - the online news / terms-of-service screen flow state (ON_NEWS): a
// news / TOS toggle over a scrolling text field, filled as the two downloads finish.
// The base derivation (CgsGui::State), the member names and order and the virtual set are
// the original declaration's; member placement is the console's.
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // pointer member only

    struct OnlineNews : public CgsGui::State
    {
        enum EToggleItems
        {
            E_TOGGLE_ITEM_NEWS  = 0,
            E_TOGGLE_ITEM_TOS   = 1,
            E_TOGGLE_ITEM_COUNT = 2,
        };

        enum EDownloadState
        {
            E_DOWNLOAD_STATE_DOWNLOADING = 0,
            E_DOWNLOAD_STATE_DONE        = 1,
            E_DOWNLOAD_STATE_FAILED      = 2,
            E_DOWNLOAD_STATE_COUNT       = 3,
        };

        // The screen's sub-state machine (console +0x38).
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN     = 0,
            E_SUBSTATE_LOADING_COMPONENTS = 1,
            E_SUBSTATE_SELECTING_PARAMS   = 2,
            E_SUBSTATE_COUNT              = 3,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82500500 - hands the online-news state's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        // The in-queue hands the handlers the header-stripped payload, so they take the
        // bare event (declared over GuiEventControllerInputPressed / GuiEventCache /
        // GuiEventNetworkNewsAndTOS / GuiEventControllerAxis).
        void HandleControllerInput(const CgsModule::Event* lpEvent);
        void HandleControllerInputSelectParams(const CgsModule::Event* lpEvent);
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);
        void HandleNewsAndTOSEvent(const CgsModule::Event* lpEvent);
        void HandleControllerAxis(const CgsModule::Event* lpEvent);
        void CheckForCompletedLoads();
        void ShowText();

        static const s32                    maiEventToObserve[8];
        static const s32                    miNumEventsObserved;      // == 8
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F810 (.rdata, 1 entry)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F818 (.rdata, == 1)
        static const f32                    KF_TIME_TO_SCROLL_ONE_LINE;
        static const f32                    KF_AXIS_DEAD_ZONE;
        static const char                   KAC_TOGGLE_TEXT_COMPONENT[11];   // "ToggleText"
        static const char                   KAC_NEWS_TEXT_COMPONENT[9];      // "NewsText"
        static const char* const            KPAC_TOGGLE_TEXT_STRING_ID[E_TOGGLE_ITEM_COUNT];
        static const char* const            KPAC_NEWS_TEXT_STRING_ID[E_TOGGLE_ITEM_COUNT][E_DOWNLOAD_STATE_COUNT];

        // ---- data members (declaration order; console offsets in the comments) -------
        ESubState      meSubState;                              // +0x38
        TextField      mToggleText;                             // +0x3C
        TextField      mNewsText;                               // +0x164
        GuiCache*      mpGuiCache;                              // +0x28C
        EToggleItems   meCurrentToggleItem;                     // +0x290
        EDownloadState maeDownloadState[E_TOGGLE_ITEM_COUNT];   // +0x294
        f32            mfScrollAmount;                          // +0x29C
        f32            mfLastAxisValue;                         // +0x2A0
    };
}
