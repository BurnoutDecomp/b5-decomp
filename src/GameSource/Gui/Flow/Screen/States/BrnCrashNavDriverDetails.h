#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CrashNavDriverDetails - the crash-navigation "driver details" screen state.
// This leaf header carries the class shape and the one inline resource accessor
// attributed to the header (the single ledger function for this TU). The licence /
// photo-booth / stats-panel components, the menu wiring and the out-of-line state and
// virtual machinery are reconstructed with the class:BrnGui::CrashNavDriverDetails TU.
// Layout/virtuals and the CgsGui::State derivation are from the DecFIGS DWARF
// (BrnCrashNavDriverDetails.h).
namespace BrnGui
{
    struct CrashNavDriverDetails : public CgsGui::State
    {
        // @ 0x82500308 - hands the driver-details screen's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82066518 (unk_82066518, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82066528 (dword_82066528, .rdata) == 2
    };
}
