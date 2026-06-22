#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CrashedStuntHudState - the crashed/stunt-run HUD GUI state. This header
// carries the class shape and the inline resource accessor (the one ledger function
// attributed to the header); the score-animator/HUD-message members and out-of-line
// methods are reconstructed with the class:BrnGui::CrashedStuntHudState TU.
// Layout/virtuals from the DecFIGS DWARF (BrnCrashedStuntHudState.h).
namespace BrnGui
{
    struct CrashedStuntHudState : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82508510 - hands the crashed-stunt HUD state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F26488 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F264A8 (.rdata)
    };
}
