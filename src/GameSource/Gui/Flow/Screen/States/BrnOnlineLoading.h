#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineLoading - the online loading / player-list screen flow state. This header
// carries the class shape and the inline resource accessor (the one ledger function
// attributed to the header); the player-list setup/update machinery and the out-of-line
// state/virtual machinery (OnEnter/OnLeave/Update + the .rdata tables) are reconstructed
// with the class:BrnGui::OnlineLoading TU. Layout/virtuals from the DecFIGS DWARF
// (BrnOnlineLoading.h).
namespace BrnGui
{
    struct OnlineLoading : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82515308 - hands the online-loading state's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205EE70 (.rdata, 2 entries)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205EE80 (.rdata, == 2)
    };
}
