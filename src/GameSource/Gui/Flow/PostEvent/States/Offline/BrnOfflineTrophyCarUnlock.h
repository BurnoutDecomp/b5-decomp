#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OfflineTrophyCarUnlock - the offline "trophy car unlock" post-event GUI
// state. This header carries the class shape and the inline resource accessor that
// hands the screen's static resource list to the loader. Layout/virtuals from the
// DecFIGS DWARF (BrnOfflineTrophyCarUnlock.h); the inline body is behaviour-faithful
// to the X360 pseudocode @ 0x825008F0 (*result = table; *a2 = count).
namespace BrnGui
{
    struct OfflineTrophyCarUnlock : public CgsGui::State
    {
        // @ 0x825008F0 - hands the trophy-car-unlock screen's resource list out.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @ 0x82F27338 (.rdata, 4 entries)
        static const u32                    muNumResourcesToLoad;  // @ 0x82066928 (.rdata, == 4)
    };
}
