#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::sResourceTuple

// BrnGui::GenericForwardState (DecFIGS DWARF BrnGenericForwardState.h:42) : CgsGui::State.
//
// A screen-flow FSM state whose Update simply advances the flow -- it raises the "ADVANCE"
// state event so the owning controller sequences to the next phase.
//
// ⭐ 2026-09-16 -- THE VTABLE SETTLES WHICH DWARF OVERRIDES ARE REAL. The old banner here
// said OnEnter / OnLeave / GetResourcesToLoad "are reconstructed by their own TUs and will
// extend this header", i.e. three missing overrides. Reading the console's vtable (anchored
// by the known Update @0x82500950, found at slot 0x82074588) shows only ONE of the three is:
//     GetResourcesToLoad  0x82500930   <- real, recovered below
//     OnEnter             0x8284CB38   <- the ICF-folded bare `blr`, i.e. EMPTY
//     OnLeave             0x8284CB38   <- EMPTY
//     Update              0x82500950
// The slot order was pinned by aligning against CrashNavOptions, whose four overrides are
// all distinct real functions (GetResourcesToLoad 0x82508B00 / OnEnter 0x824CE110 /
// OnLeave 0x824CE228 / Update 0x824E00A0) at the same offsets. So declaring OnEnter/OnLeave
// here would add nothing the console does -- their absence is FAITHFUL, not a gap.
//
// Update is declared virtual (it overrides the FSM state's Update slot) without `override`
// so the header stays compilable while the CgsFsm::ScriptedState base virtual set is still
// being filled in.

namespace BrnGui
{
    struct GenericForwardState : public CgsGui::State
    {
        // @0x82500950 -- advance the flow by sending the "ADVANCE" state event.
        virtual void Update();

        // @0x82500930 -- hand the loader this state's three APT resources. The X360 body is
        // the shared two-store accessor shape: *lppResourceTuples = maResourcesToLoad (the
        // .rdata table at 0x82F26BB8) and *lpuNumberOfResources = the count word at
        // 0x82F26BD0 (== 3).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[3];  // @0x82F26BB8 (.rdata)
        static const u32                    muNumResourcesToLoad;  // @0x82F26BD0 (== 3)
    };
}
