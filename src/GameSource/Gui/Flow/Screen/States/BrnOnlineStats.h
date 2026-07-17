#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineStats - the online-stats screen state. This leaf header carries the class shape
// and the one inline resource accessor attributed to the header (the single ledger function for
// this TU). The stat text-field bank, the stat-response handling and all the out-of-line state
// and virtual machinery are reconstructed with the class:BrnGui::OnlineStats TU. Layout/virtuals
// and the CgsGui::State derivation are from the DecFIGS DWARF (BrnOnlineStats.h).
namespace BrnGui
{
    struct OnlineStats : public CgsGui::State
    {
        // @ 0x825004C0 - hands the online-stats screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x8205FA1C (unk_8205FA1C, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x8205FA24 (dword_8205FA24, .rdata) == 1
    };
}
