#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameSource/Gui/Flow/Overlay/Components/BrnOverlayComponent.h"   // BrnGui::OverlayComponent (by value)

// BrnGui::InvisibleOverlayState - the "no overlay" overlay-flow state: it binds the
// shared full-screen overlay component ("Overlays_mc") and plays its "invisible"
// flash label, leaving the screen clear. DWARF home BrnInvisibleOverlayState.h:43.
// OnEnter/OnLeave are bodied in BrnInvisibleOverlayState.cpp (this TU); Update and
// GetResourcesToLoad are their own ledger functions (declaration-only here).
namespace BrnGui
{
    struct InvisibleOverlayState : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        // DWARF h:71 (X360 this+0x38, right after the 0x38-byte State shell).
        OverlayComponent mOverlayComponent;

        // DWARF cpp:26/:32/:34. The two observed event ids are .data @0x82063CAC.
        static const s32  maiEventToObserve[];      // { 6, 185 }
        static const s32  miNumEventsObserved;      // == 2
        static const char macOverlayComponentName[]; // "Overlays_mc"
    };
}
