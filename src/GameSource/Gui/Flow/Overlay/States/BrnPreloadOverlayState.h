#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::PreloadOverlayState - the flow-overlay preload GUI state. This header
// carries the class shape and the inline resource accessor (the one ledger function
// attributed to the header); the cache wiring, the internal-state machine and the
// out-of-line state/virtual machinery are reconstructed with the class TU.
// Layout/virtuals from the DecFIGS DWARF (BrnPreloadOverlayState.h).
namespace BrnGui
{
    class GuiCache;

    struct PreloadOverlayState : public CgsGui::State
    {
        // The overlay's internal step machine (DecFIGS DWARF: BrnPreloadOverlayState.h:47).
        enum OverlayInternalState
        {
            E_OVERLAYINTERNALSTATE_START      = 0,
            E_OVERLAYINTERNALSTATE_GETCACHE   = 1,
            E_OVERLAYINTERNALSTATE_WFLOADMAIN = 2,
            E_OVERLAYINTERNALSTATE_WFLOAD     = 3,
            E_OVERLAYINTERNALSTATE_DONE       = 4,
            E_OVERLAYINTERNALSTATE_COUNT      = 5,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @0x824B2070 - drain the state's in-event queue looking for the cache-ready event
        // (event type 64); on receipt, latch the carried GuiCache pointer into mpGuiCache and
        // return true. Returns false until that event arrives.
        bool GetCache();

        // @ 0x82501190 - hands the preload-overlay state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // X360-attested instance members (DecFIGS DWARF). Base CgsGui::State is 0x38 bytes
        // (the size-56 state shell), so these follow it directly.
        OverlayInternalState meInternalState;   // +0x38
        GuiCache*            mpGuiCache;         // +0x3C

        // The single GUI event id this state observes (.rdata, no value in the export -> declared
        // here and resolved at link time, as the other GUI states).
        static const s32 maiEventToObserve[1];
        static const s32 miNumEventsObserved;

        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F269F8 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F269F4 (.rdata)
    };
}
