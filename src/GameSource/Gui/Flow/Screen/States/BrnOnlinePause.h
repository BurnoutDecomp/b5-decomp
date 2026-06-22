#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlinePause - the online pause screen state. This leaf header carries the
// class shape and the one inline resource accessor attributed to the header (the single
// ledger function for this TU). The pause-options menu component, GUI cache, sub-state
// machine and all out-of-line virtual/handler machinery (OnEnter/OnLeave/Update,
// CheckForCompletedLoads, HandleControllerInput, HandleOverlayComplete) are reconstructed
// with the class:BrnGui::OnlinePause TU. Layout/virtuals and the CgsGui::State
// derivation are from the DecFIGS DWARF (BrnOnlinePause.h).
namespace BrnGui
{
    struct OnlinePause : public CgsGui::State
    {
        // @ 0x82500678 - hands the online-pause screen's static resource list to the
        // loader (X360: *r4 = &maResourceTuplesToLoad; *r5 = (u32)miNumResourcesToLoad,
        // count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205EF4C (unk_8205EF4C, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205EF54 (dword_8205EF54, .rdata) == 1
    };
}
