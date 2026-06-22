#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CrashNavAccountManagement - the crash-nav online-account-management screen
// flow state (terms-of-service text, share-info / telemetry toggles). This header
// carries the class shape and the inline resource accessor (the one ledger function
// attributed to the header); the large component/substate machinery and the out-of-line
// state/virtual machinery (OnEnter/OnLeave/Update + the Update* / Handle* helpers and the
// observed-event/option/string-id .rdata tables) are reconstructed with the
// class:BrnGui::CrashNavAccountManagement TU. Layout/virtuals from the DecFIGS DWARF
// (BrnCrashNavAccountManagement.h).
namespace BrnGui
{
    struct CrashNavAccountManagement : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82508B20 - hands the account-management state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F26FDC (.rdata, 1 entry)
        static const u32                    muNumResourcesToLoad; // @ 0x82F26FE4 (.rdata)
    };
}
