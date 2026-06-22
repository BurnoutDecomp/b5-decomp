#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::InstantResultsState - the offline post-event instant-results presentation
// flow state. This header carries the class shape and the inline resource accessor (the
// one ledger function attributed to the header); the large component/substate machinery
// and the out-of-line state/virtual machinery are reconstructed with the class TU.
// Layout/virtuals from the DecFIGS DWARF (BrnOfflineInstantResults.h).
namespace BrnGui
{
    struct InstantResultsState : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82500808 - hands the instant-results state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F26AFC (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F26B0C (.rdata)
    };
}
