#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineGameOptions - the online "create match / game options" screen state. This
// leaf header carries the class shape and the one inline resource accessor attributed to
// the header (the single ledger function for this TU). The create-match option set,
// help-bar items, menu/toggle component wiring and the out-of-line state and virtual
// machinery are reconstructed with the class:BrnGui::OnlineGameOptions TU.
// Layout/virtuals and the CgsGui::State derivation are from the DecFIGS DWARF
// (BrnOnlineGameOptions.h).
namespace BrnGui
{
    struct OnlineGameOptions : public CgsGui::State
    {
        // @ 0x8251AFA8 - hands the game-options screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F004 (unk_8205F004, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F014 (dword_8205F014, .rdata) == 2
    };
}
