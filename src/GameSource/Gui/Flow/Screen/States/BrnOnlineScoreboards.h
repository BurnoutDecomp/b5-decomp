#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineScoreboards - the online-leaderboards screen state. This leaf header carries
// the class shape and the one inline resource accessor attributed to the header (the single
// ledger function for this TU). The leaderboard-table component, the category/index/variation
// request machinery, the filter toggle group and all the out-of-line state and virtual
// machinery are reconstructed with the class:BrnGui::OnlineScoreboards TU. Layout/virtuals and
// the CgsGui::State derivation are from the DecFIGS DWARF (BrnOnlineScoreboards.h).
namespace BrnGui
{
    struct OnlineScoreboards : public CgsGui::State
    {
        // @ 0x82508DA8 - hands the scoreboards screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x8205F67C (unk_8205F67C, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x8205F684 (dword_8205F684, .rdata) == 1
    };
}
