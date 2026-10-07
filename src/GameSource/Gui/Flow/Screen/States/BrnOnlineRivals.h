#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineRivals - the online "rivals" flow state (ON_RIVAL). It loads its apt
// package, waits for the apt components and then only listens for the back button.
// The base derivation (CgsGui::State), the member names and order and the virtual set are
// the original declaration's; member placement is the console's.
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // pointer member only

    struct OnlineRivals : public CgsGui::State
    {
        // The screen's sub-state machine (console +0x38).
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN     = 0,
            E_SUBSTATE_LOADING_COMPONENTS = 1,
            E_SUBSTATE_SELECTING_PARAMS   = 2,
            E_SUBSTATE_COUNT              = 3,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82500520 - hands this screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        // The in-queue hands the handlers the header-stripped payload, so they take the
        // bare event (declared over GuiEventControllerInputPressed / GuiEventCache).
        void HandleControllerInput(const CgsModule::Event* lpEvent);
        void HandleControllerInputSelectParams(const CgsModule::Event* lpEvent);
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);
        void CheckForCompletedLoads();

        static const s32                    maiEventToObserve[5];
        static const s32                    miNumEventsObserved;      // == 5
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F854 (unk_8205F854, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F85C (dword_8205F85C, .rdata) == 1

        // ---- data members (declaration order; console offsets in the comments) -------
        ESubState meSubState;   // +0x38
        GuiCache* mpGuiCache;   // +0x3C
    };
}
