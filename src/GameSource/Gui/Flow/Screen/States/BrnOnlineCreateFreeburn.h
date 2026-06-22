#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineCreateFreeburn - the online "create freeburn" screen state. This leaf
// header carries the class shape and the one inline resource accessor attributed to the
// header (the single ledger function for this TU). The sub-state machine, GUI-cache
// wiring, controller / in-game event handlers and the out-of-line state and virtual
// machinery are reconstructed with the class:BrnGui::OnlineCreateFreeburn TU.
// Layout/virtuals and the CgsGui::State derivation are from the DecFIGS DWARF
// (BrnOnlineCreateFreeburn.h).
namespace BrnGui
{
    struct OnlineCreateFreeburn : public CgsGui::State
    {
        // @ 0x825004A0 - hands the create-freeburn screen's static resource list to the
        // loader (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F880 (unk_8205F880, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F890 (dword_8205F890, .rdata) == 2
    };
}
