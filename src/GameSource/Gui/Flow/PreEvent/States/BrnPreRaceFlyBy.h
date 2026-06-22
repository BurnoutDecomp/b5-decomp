#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::PreRaceFlyByState - the pre-event fly-by flow state (event titles, map intro,
// medals). This header carries the class shape and the inline resource accessor (the one
// ledger function attributed to the header); the map/icon/component machinery and the
// out-of-line state/virtual machinery are reconstructed with the class TU. Layout/virtuals
// from the DecFIGS DWARF (BrnPreRaceFlyBy.h).
namespace BrnGui
{
    struct PreRaceFlyByState : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82514EE8 - hands the pre-race fly-by state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F26BE0 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F26BE8 (.rdata)
    };
}
