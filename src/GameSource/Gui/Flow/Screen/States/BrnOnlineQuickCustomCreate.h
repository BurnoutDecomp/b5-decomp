#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineQuickCustomCreate - the online "quick / custom / create match" main-menu
// flow state. This leaf header carries the class shape and the one inline resource
// accessor attributed to the header (the single ledger function for this TU). The guard
// cache, internal-state machine, the news/main-menu components, friends-list / sign-in
// sys-util wiring and the out-of-line state and virtual machinery are reconstructed with
// the class:BrnGui::OnlineQuickCustomCreate TU. The base derivation (CgsGui::State) and
// the virtual layout are from the DecFIGS DWARF (BrnOnlineQuickCustomCreate.h).
namespace BrnGui
{
    struct OnlineQuickCustomCreate : public CgsGui::State
    {
        // DWARF BrnOnlineQuickCustomCreate.h:24 -- the three main-menu options.
        enum EMainMenuOptions
        {
            E_MAIN_MENU_OPTIONS_QUICK_MATCH  = 0,
            E_MAIN_MENU_OPTIONS_CUSTOM_MATCH = 1,
            E_MAIN_MENU_OPTIONS_CREATE_MATCH = 2,
            E_MAIN_MENU_OPTIONS_COUNT        = 3,
        };

        // ADDITIVE GROW (the freeburn flavour's override @0x82487458 pins the vtable
        // slot): dispatch the picked main-menu option. The base body is its own
        // ledger function (declaration-only here).
        virtual void ProcessSelectedMenuOption(EMainMenuOptions leOption);

        // @ 0x825005A0 - hands this screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x8205F9C8 (unk_8205F9C8, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x8205F9D8 (dword_8205F9D8, .rdata) == 2
    };
}
