#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::BootLegal - the boot legal-screen GUI state. This header carries the class
// shape and the inline resource accessor (the one ledger function attributed to the
// header); the menu-component members and out-of-line menu methods are reconstructed
// with the class:BrnGui::BootLegal TU. Layout/virtuals from the DecFIGS DWARF
// (BrnBootLegal.h).
namespace BrnGui
{
    struct BootLegal : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82508090 - hands the legal screen's static resource list to the loader.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F25CEC (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F25CF4 (.rdata)
    };
}
