#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OfflineRivalShutdown - the offline "rival shut down" post-event GUI state.
// This header carries the class shape and the inline resource accessor that hands the
// screen's static resource list to the loader. Layout/virtuals from the DecFIGS DWARF
// (BrnOfflineRivalShutdown.h); the inline body is behaviour-faithful to the X360
// pseudocode @ 0x825008D0 (*result = table; *a2 = count).
namespace BrnGui
{
    struct OfflineRivalShutdown : public CgsGui::State
    {
        // @ 0x825008D0 - hands the rival-shutdown screen's resource list out.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @ 0x82F27318 (.rdata, 4 entries)
        static const u32                    muNumResourcesToLoad;  // @ 0x82066898 (.rdata, == 4)
    };
}
