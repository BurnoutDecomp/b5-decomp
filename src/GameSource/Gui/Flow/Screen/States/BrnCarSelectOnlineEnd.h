#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CarSelectOnlineEnd - the online car-select "ready / countdown" screen flow
// state. This header carries the class shape and the inline resource accessor (the one
// ledger function attributed to the header); the large component/substate machinery and
// the out-of-line state/virtual machinery (OnEnter/OnLeave/Update + the Update* / Handle*
// helpers and the observed-event/string-id .rdata tables) are reconstructed with the
// class:BrnGui::CarSelectOnlineEnd TU. Layout/virtuals from the DecFIGS DWARF
// (BrnCarSelectOnlineEnd.h).
namespace BrnGui
{
    struct CarSelectOnlineEnd : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82508A80 - hands the online-car-select-end state's static resource list to
        // the loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x8205E714 (.rdata, 2 entries)
        static const u32                    muNumResourcesToLoad; // @ 0x8205E724 (.rdata, == 2)
    };
}
