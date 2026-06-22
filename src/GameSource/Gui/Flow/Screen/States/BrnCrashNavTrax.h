#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CrashNavTrax - the crash-navigation EA-Trax audio "trax" screen state. This leaf
// header carries the class shape and the one inline resource accessor attributed to the
// header (the single ledger function for this TU). The EA-Trax menu component, the
// play-order mode wiring, track preview and all the out-of-line state and virtual
// machinery are reconstructed with the class:BrnGui::CrashNavTrax TU. Layout/virtuals and
// the CgsGui::State derivation are from the DecFIGS DWARF (BrnCrashNavTrax.h).
namespace BrnGui
{
    struct CrashNavTrax : public CgsGui::State
    {
        // @ 0x825000E0 - hands the trax screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F27278 (unk_82F27278, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F27288 (dword_82F27288, .rdata) == 2
    };
}
