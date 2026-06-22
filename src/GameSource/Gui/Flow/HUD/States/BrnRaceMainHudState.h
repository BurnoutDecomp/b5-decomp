#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::RaceMainHudState - the race main-HUD GUI state. This header carries the
// class shape and the inline resource accessor (the one ledger function attributed
// to the header); the many HUD-component members and out-of-line update methods are
// reconstructed with the class:BrnGui::RaceMainHudState TU. Layout/virtuals from the
// DecFIGS DWARF (BrnRaceMainHudState.h).
namespace BrnGui
{
    struct RaceMainHudState : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82508368 - hands the race main-HUD state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F25F88 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F25F84 (.rdata)
    };
}
