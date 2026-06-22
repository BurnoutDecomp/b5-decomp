#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineRivals - the online "rivals" flow state. This leaf header carries the
// class shape and the one inline resource accessor attributed to the header (the single
// ledger function for this TU). The screen / component wiring and the out-of-line state
// and virtual machinery (Update etc.) are reconstructed with the
// class:BrnGui::OnlineRivals TU. The base derivation (CgsGui::State) and the virtual
// layout are from the DecFIGS DWARF (BrnOnlineRivals.h).
namespace BrnGui
{
    struct OnlineRivals : public CgsGui::State
    {
        // @ 0x82500520 - hands this screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F854 (unk_8205F854, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F85C (dword_8205F85C, .rdata) == 1
    };
}
