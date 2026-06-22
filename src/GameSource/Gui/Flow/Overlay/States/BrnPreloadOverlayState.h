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

        // @ 0x82501190 - hands the preload-overlay state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F269F8 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F269F4 (.rdata)
    };
}
