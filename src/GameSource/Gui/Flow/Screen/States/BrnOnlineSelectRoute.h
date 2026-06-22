#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineSelectRoute - the online route-selection screen state. This leaf header carries
// the class shape and the one inline resource accessor attributed to the header (the single
// ledger function for this TU). The route list, the map/preview machinery and all the
// out-of-line state and virtual machinery are reconstructed with the
// class:BrnGui::OnlineSelectRoute TU. Layout/virtuals and the CgsGui::State derivation are from
// the DecFIGS DWARF (BrnOnlineSelectRoute.h), where this TU names its statics
// maResourceTuplesToLoad / miNumResourcesToLoad (count is int32 in the DWARF -> cast to u32).
namespace BrnGui
{
    struct OnlineSelectRoute : public CgsGui::State
    {
        // @ 0x8251AEF8 - hands the route-select screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F338 (unk_8205F338, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F348 (dword_8205F348, .rdata) == 2
    };
}
