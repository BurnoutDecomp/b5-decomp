#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::BootPreload - the boot preload GUI state. This header carries the class
// shape and the inline resource accessor (the second-phase resource list it hands to
// the loader). Layout/virtuals from the DecFIGS DWARF (BrnBootPreload.h).
namespace BrnGui
{
    struct BootPreload : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82508070 - hands the preload screen's second-phase resource list out.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maSecondPhaseResourcesToLoad;
            *lpuNumberOfResources = muSecondPhaseNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maSecondPhaseResourcesToLoad[];  // @ .rdata
        static const u32                    muSecondPhaseNumResourcesToLoad; // @ .rdata
    };
}
