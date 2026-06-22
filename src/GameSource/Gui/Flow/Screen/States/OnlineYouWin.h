#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineYouWin - the online "you win" celebration flow state. This leaf header
// carries the class shape and the one inline resource accessor attributed to the header
// (the single ledger function for this TU). The screen / photo / animator wiring and the
// out-of-line state and virtual machinery (OnEnter / OnLeave / Update / the per-internal-
// state helpers) are reconstructed with the class:BrnGui::OnlineYouWin TU. The base
// derivation (CgsGui::State) and the virtual layout are from the DecFIGS DWARF
// (OnlineYouWin.h).
namespace BrnGui
{
    struct OnlineYouWin : public CgsGui::State
    {
        // @ 0x82500910 - hands this screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count == 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];    // @ 0x8205FB64 (unk_8205FB64, .rdata)
        static const u32                    muNumResourcesToLoad;   // @ 0x8205FB70 (dword_8205FB70, .rdata) == 1
    };
}
