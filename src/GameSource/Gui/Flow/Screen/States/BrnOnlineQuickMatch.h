#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineQuickMatch - the online "quick match" flow state. This leaf header
// carries the class shape and the one inline resource accessor attributed to the header
// (the single ledger function for this TU). The screen / component wiring and the
// out-of-line state and virtual machinery are reconstructed with the
// class:BrnGui::OnlineQuickMatch TU. The base derivation (CgsGui::State) and the virtual
// layout are from the DecFIGS DWARF (BrnOnlineQuickMatch.h).
namespace BrnGui
{
    struct OnlineQuickMatch : public CgsGui::State
    {
        // @ 0x825004E0 - hands this screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F71C (unk_8205F71C, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F724 (dword_8205F724, .rdata) == 1
    };
}
