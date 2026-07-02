#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::Credits - the end-game "credits" screen state: a load -> wait-for-init ->
// running machine that binds the credits apt movie + music, then advances out on the
// controller action. DWARF home BrnCredits.h:44. This home originally carried only the
// header-attributed GetResourcesToLoad inline; the BrnCredits.cpp TU grew it to the
// full DWARF surface (OnEnter / UpdateLoadResources / UpdateRunning bodied there;
// OnLeave / Update / UpdateWFInit / HandleControllerInputPressed are their own ledger
// functions, declaration-only).
namespace BrnGui
{
    class GuiCache;
    struct GuiEventControllerInputPressed;   // declaration-only consumer param

    struct Credits : public CgsGui::State
    {
        // DWARF BrnCredits.h:71.
        enum InternalState
        {
            E_INTERNALSTATE_LOADRESOURCES = 0,
            E_INTERNALSTATE_WFINIT        = 1,
            E_INTERNALSTATE_RUNNING       = 2,
            E_INTERNALSTATE_LEFT          = 3,
            E_INTERNALSTATE_COUNT         = 4,
        };

        // @0x824CFA08 / (own TUs) -- DWARF cpp:58/143/82.
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82500100 - hands the credits screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // @0x824CFBD8 / @0x824C1EE0 (the BrnCredits.cpp TU, DWARF cpp:175/cpp:224).
        bool UpdateLoadResources();
        void UpdateRunning();

        // DWARF cpp:207/cpp:271 -- declaration-only (their own ledger functions).
        bool UpdateWFInit();
        void HandleControllerInputPressed(const GuiEventControllerInputPressed* lpEvent);

        // ---- members (DWARF h:87/h:88; X360 +0x38/+0x3C) ----
        GuiCache*     mpGuiCache;
        InternalState meInternalState;

        // ---- statics (DWARF cpp:26-39) ----
        static const s32                    maiEventToObserve[];   // .data @0x82066648: { 6, 21 }
        static const s32                    miNumEventsObserved;   // == 2
        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @ 0x82066654 (unk_82066654, .rdata)
        static const u32                    muNumResourcesToLoad;  // @ 0x8206665C (dword_8206665C, .rdata) == 1
    };
}
