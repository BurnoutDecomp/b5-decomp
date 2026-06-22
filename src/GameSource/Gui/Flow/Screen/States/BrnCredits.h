#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::Credits - the end-game "credits" screen state. This leaf header carries the
// class shape and the one inline resource accessor attributed to the header (the single
// ledger function for this TU). The observed event list, the internal load/run/leave
// state machine and the out-of-line virtual machinery are reconstructed with the
// class:BrnGui::Credits TU. Layout/virtuals and the CgsGui::State derivation are from the
// DecFIGS DWARF (BrnCredits.h).
namespace BrnGui
{
    struct Credits : public CgsGui::State
    {
        // @ 0x82500100 - hands the credits screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82066654 (unk_82066654, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x8206665C (dword_8206665C, .rdata) == 1
    };
}
