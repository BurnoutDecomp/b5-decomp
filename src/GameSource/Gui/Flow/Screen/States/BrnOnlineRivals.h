#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

namespace BrnGui { class GuiCache; }
namespace BrnGui { struct GuiEventControllerInputPressed; }   // declaration-only consumer param
namespace BrnGui { struct GuiEventCache; }                    // declaration-only consumer param

// BrnGui::OnlineRivals - the online "rivals" flow state (DWARF home BrnOnlineRivals.h:47,
// base CgsGui::State). A three-substate loader screen (loading-screen -> loading-components
// -> selecting-params) that streams its one resource tuple, plays the "ON_RIVAL" apt movie,
// then waits on the apt components before entering param selection.
//
// The class shape and the inline resource accessor (the single header-attributed ledger
// function) live here; the out-of-line bodies (OnEnter/OnLeave/Update/GetResourcesToLoad
// and the four Handle*/Check* helpers below) are their own ledger functions. Virtual layout
// and member set from the DecFIGS DWARF (BrnOnlineRivals.h), gated on the X360 ledger.
namespace BrnGui
{
    struct OnlineRivals : public CgsGui::State
    {
        // DWARF BrnOnlineRivals.h:74.
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

        // @ 0x82500520 - hands this screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        // @0x8248DD38 (this TU) -- validate the controller-input event, then, while in
        // E_SUBSTATE_SELECTING_PARAMS, forward it to HandleControllerInputSelectParams.
        void HandleControllerInput(const GuiEventControllerInputPressed* lpEvent);

        // @0x82486D60 (this TU) -- act on a controller-action event during param selection:
        // action sub-id 50 (KI_ACTION_START) -> "GO_BACK".
        void HandleControllerInputSelectParams(const GuiEventControllerInputPressed* lpEvent);

        // @0x82486E18 (this TU) -- latch the GuiCache carried by a GuiEventCache the first
        // time one arrives (mpGuiCache stays whatever it was once set).
        void HandleGuiCacheEvent(const GuiEventCache* lpEvent);

        // @0x824A05F0 (this TU) -- the resource/component load state machine, ticked by
        // Update: load the resources, play the "ON_RIVAL" apt movie, then wait for the apt
        // components to initialise.
        void CheckForCompletedLoads();

        // ---- statics (DWARF cpp:30-46; .rdata values partly recovered) ----
        static const s32                    maiEventToObserve[];        // cpp:30 (5 entries)
        static const s32                    miNumEventsObserved;        // cpp:39 == 5
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[];   // cpp:41 @0x8205F854 (unk_8205F854, .rdata)
        static const s32                    miNumResourcesToLoad;       // cpp:46 @0x8205F85C == 1

        static const s32                    KI_MAX_CREATE_GAME_OPTIONS = 6;  // DWARF h:84

        // ---- members (DWARF h:93/h:96; X360 offsets +0x38 / +0x3C) ----
        ESubState meSubState;   // +0x38 (the load / param-select state machine selector)
        GuiCache* mpGuiCache;   // +0x3C (latched from the first GuiEventCache)
    };
}
