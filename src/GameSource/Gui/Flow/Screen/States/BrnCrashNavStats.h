#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CrashNavStats - the crash-navigation post-crash "stats" screen state. This leaf
// header carries the class shape and the one inline resource accessor attributed to the
// header (the single ledger function for this TU). The stat text-field bank, the observed
// event list, the stat-response handling and all the out-of-line state and virtual
// machinery are reconstructed with the class:BrnGui::CrashNavStats TU. Layout/virtuals and
// the CgsGui::State derivation are from the DecFIGS DWARF (BrnCrashNavStats.h).
namespace BrnGui
{
    struct CrashNavStats : public CgsGui::State
    {
        // @ 0x82500008 - hands the stats screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F26D88 (unk_82F26D88, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F26D90 (dword_82F26D90, .rdata) == 1
    };
}
