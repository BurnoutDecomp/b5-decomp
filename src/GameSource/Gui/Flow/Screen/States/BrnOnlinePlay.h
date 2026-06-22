#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlinePlay - the online-play main-menu screen state. This leaf header carries
// the class shape and the one inline resource accessor attributed to the header (the
// single ledger function for this TU). The new-news animation component, main-menu menu
// component, network player-stats display/event, GUI cache, sign-in/friends sys-util
// wiring, sub-state machine and all out-of-line virtual/handler machinery
// (OnEnter/OnLeave/Update, the Handle*/Show*/Check* helpers) are reconstructed with the
// class:BrnGui::OnlinePlay TU. Layout/virtuals and the CgsGui::State derivation are from
// the DecFIGS DWARF (BrnOnlinePlay.h).
namespace BrnGui
{
    struct OnlinePlay : public CgsGui::State
    {
        // @ 0x82508C00 - hands the online-play screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = (u32)miNumResourcesToLoad,
        // count = 3).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205EF88 (unk_8205EF88, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205EFA0 (dword_8205EFA0, .rdata) == 3
    };
}
