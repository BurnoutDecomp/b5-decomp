#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineInstantResultsState - the online "instant results" post-event GUI
// state. This header carries the class shape and the inline resource accessor that
// hands the screen's static resource list to the loader. Layout/virtuals from the
// DecFIGS DWARF (BrnOnlineInstantResults.h); the inline body is behaviour-faithful
// to the X360 pseudocode @ 0x82508F30 (*result = table; *a2 = count).
namespace BrnGui
{
    struct OnlineInstantResultsState : public CgsGui::State
    {
        // @ 0x82508F30 - hands the instant-results screen's resource list out.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[];  // @ 0x8205DE98 (.rdata, 1 entry)
        static const s32                    miNumResourcesToLoad;      // @ 0x8205DEA0 (.rdata, == 1)
    };
}
