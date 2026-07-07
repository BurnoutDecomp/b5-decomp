#pragma once

// ===================================================================================
// GameSource/Gui/Flow/Screen/States/BrnOnlineNews.h
//
// BrnGui::OnlineNews - the online news / TOS scrolling-message screen flow state. A
// CgsGui::State leaf that toggles between the News and TOS items, scrolls the body text
// on the stick, and tracks the per-item download state (downloading / done / failed).
// Virtual layout + member set from the DecFIGS DWARF (BrnOnlineNews.h), gated on the ledger.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"                         // BrnGui::TextField (embedded by value)

namespace CgsModule { struct Event; }

namespace BrnGui { class GuiCache; }
namespace BrnGui { struct GuiEventControllerInputPressed; }   // declaration-only consumer param
namespace BrnGui { struct GuiEventControllerAxis; }           // declaration-only consumer param
namespace BrnGui { struct GuiEventCache; }                    // declaration-only consumer param
namespace BrnGui { struct GuiEventNetworkNewsAndTOS; }        // declaration-only consumer param

namespace BrnGui
{
    struct OnlineNews : public CgsGui::State
    {
        // DWARF BrnOnlineNews.h:51.
        enum EToggleItems
        {
            E_TOGGLE_ITEM_NEWS  = 0,
            E_TOGGLE_ITEM_TOS   = 1,
            E_TOGGLE_ITEM_COUNT = 2,
        };

        // DWARF BrnOnlineNews.h:59.
        enum EDownloadState
        {
            E_DOWNLOAD_STATE_DOWNLOADING = 0,
            E_DOWNLOAD_STATE_DONE        = 1,
            E_DOWNLOAD_STATE_FAILED      = 2,
            E_DOWNLOAD_STATE_COUNT       = 3,
        };

        // DWARF BrnOnlineNews.h:91.
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

        // @ 0x82500500 - hands the online-news state's static resource list to the loader.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        // @0x824AA518 -- validate the controller-input event, then, while in
        // E_SUBSTATE_SELECTING_PARAMS, forward it to HandleControllerInputSelectParams.
        void HandleControllerInput(const GuiEventControllerInputPressed* lpEvent);

        // @0x824A0338 -- act on a controller-action event during param selection. (SKIPPED;
        // declared-only -- HandleControllerInput calls it, the body lands with its own slice.)
        void HandleControllerInputSelectParams(const GuiEventControllerInputPressed* lpEvent);

        // @0x82486C48 -- latch the GuiCache carried by a GuiEventCache the first time one
        // arrives, then register the news/toggle text fields as expected apt components.
        void HandleGuiCacheEvent(const GuiEventCache* lpEvent);

        // @0x824AA5D0 -- fold a news/TOS network-status event into the per-item download state.
        void HandleNewsAndTOSEvent(const GuiEventNetworkNewsAndTOS* lpEvent);

        // @0x82486B58 -- accumulate the controller stick axis for scrolling (past the dead zone).
        void HandleControllerAxis(const GuiEventControllerAxis* lpEvent);

        // @0x824AA420 -- drive the load state machine: ensure resources are loaded then play the
        // "ON_NEWS" movie, and once all apt components come up, advance to param selection.
        void CheckForCompletedLoads();

        // @0x824A0458 -- push the current toggle item's text into the two fields. (SKIPPED;
        // declared-only -- HandleNewsAndTOSEvent calls it, the body lands with its own slice.)
        void ShowText();

        // Stick dead-zone below which an axis sample is ignored (X360 HandleControllerAxis rodata).
        static const f32 KF_AXIS_DEAD_ZONE;

        // ---- statics (DWARF cpp:30-62; .rdata) ----
        static const s32                    maiEventToObserve[];         // @0x8205F7EC (8 entries)
        static const s32                    miNumEventsObserved;         // == 8
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[];    // @0x8205F810 (1 entry)
        static const s32                    miNumResourcesToLoad;        // @0x8205F818 == 1

        // ---- members (DWARF h:119-132; X360 offsets) ----
        ESubState      meSubState;                            // +0x38
        TextField      mToggleText;                           // +0x3C
        TextField      mNewsText;                             // +0x164
        GuiCache*      mpGuiCache;                            // +0x28C
        EToggleItems   meCurrentToggleItem;                   // +0x290
        EDownloadState maeDownloadState[E_TOGGLE_ITEM_COUNT]; // +0x294 / +0x298
        f32            mfScrollAmount;                         // +0x29C
        f32            mfLastAxisValue;                        // +0x2A0
    };
}
