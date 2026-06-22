#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CrashNavSettings - the crash-navigation "settings" screen state. This leaf
// header carries the class shape and the one inline resource accessor attributed to the
// header (the single ledger function for this TU). The menu component, sponsor
// product-code keyboard listener, sys-config util wiring and the out-of-line state and
// virtual machinery are reconstructed with the class:BrnGui::CrashNavSettings TU.
// Layout/virtuals and the CgsGui::State derivation are from the DecFIGS DWARF
// (BrnCrashNavSettings.h).
namespace BrnGui
{
    struct CrashNavSettings : public CgsGui::State
    {
        // @ 0x82500028 - hands the settings screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x820663C0 (unk_820663C0, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x820663C8 (dword_820663C8, .rdata) == 1
    };
}
