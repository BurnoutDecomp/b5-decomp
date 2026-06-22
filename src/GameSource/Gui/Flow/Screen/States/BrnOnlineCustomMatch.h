#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineCustomMatch - the online "custom match" search/join screen state. This
// leaf header carries the class shape and the one inline resource accessor attributed to
// the header (the single ledger function for this TU). The sub-state machine, found-games
// table, search-param component wiring and the out-of-line state and virtual machinery are
// reconstructed with the class:BrnGui::OnlineCustomMatch TU.
// Layout/virtuals and the CgsGui::State derivation are from the DecFIGS DWARF
// (BrnOnlineCustomMatch.h).
namespace BrnGui
{
    struct OnlineCustomMatch : public CgsGui::State
    {
        // @ 0x82508CE8 - hands the custom-match screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205E77C (unk_8205E77C, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205E784 (dword_8205E784, .rdata) == 1
    };
}
