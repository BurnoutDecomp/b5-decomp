#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineNews - the online news / scrolling-message screen flow state. This header
// carries the class shape and the inline resource accessor (the one ledger function
// attributed to the header); the controller-input/scroll machinery and the out-of-line
// state/virtual machinery (OnEnter/OnLeave/Update + the .rdata tables) are reconstructed
// with the class:BrnGui::OnlineNews TU. Layout/virtuals from the DecFIGS DWARF
// (BrnOnlineNews.h).
namespace BrnGui
{
    struct OnlineNews : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82500500 - hands the online-news state's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F810 (.rdata, 1 entry)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F818 (.rdata, == 1)
    };
}
