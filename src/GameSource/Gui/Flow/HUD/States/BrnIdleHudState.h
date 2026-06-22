#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::IdleHudState - the idle HUD flow state. This leaf header carries the class shape
// and the one inline resource accessor attributed to the header (the single ledger
// function for this TU). The HUD component wiring and the out-of-line state and virtual
// machinery (OnEnter / OnLeave / Update) are reconstructed with the
// class:BrnGui::IdleHudState TU. The base derivation (CgsGui::State) and the virtual
// layout are from the DecFIGS DWARF (BrnIdleHudState.h).
namespace BrnGui
{
    struct IdleHudState : public CgsGui::State
    {
        // @ 0x82508530 - hands this state's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, a runtime count).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F264BC (unk_82F264BC, .data)
        static       u32                    muNumResourcesToLoad; // @ 0x82F264CC (dword_82F264CC, .data)
    };
}
