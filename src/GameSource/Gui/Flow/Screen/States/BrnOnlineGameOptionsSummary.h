#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineGameOptionsSummary - the online game-options summary screen flow state.
// This header carries the class shape and the inline resource accessor (the one ledger
// function attributed to the header); the large component/substate machinery and the
// out-of-line state/virtual machinery (OnEnter/OnLeave/Update + the observed-event and
// string-id .rdata tables) are reconstructed with the class:BrnGui::OnlineGameOptionsSummary
// TU. Layout/virtuals from the DecFIGS DWARF (BrnOnlineGameOptionsSummary.h).
namespace BrnGui
{
    struct OnlineGameOptionsSummary : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x8251B038 - hands the online-game-options-summary state's static resource list
        // to the loader (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F1FC (.rdata, 2 entries)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F20C (.rdata, == 2)
    };
}
