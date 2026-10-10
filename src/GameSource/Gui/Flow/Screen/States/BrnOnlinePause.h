#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"   // BrnGui::MenuComponent (mPauseOptions)

namespace CgsModule { struct Event; }

// BrnGui::OnlinePause - the online pause screen state: a three-row menu (continue, quit the
// game, the debug finish-round row) shown over the running online game. The CgsGui::State
// derivation, the sub-state enum, the members and the virtual layout follow BrnOnlinePause.h.
// Bodies in BrnOnlinePause.cpp, apart from the inline resource accessor.
namespace BrnGui
{
    class GuiCache;

    struct OnlinePause : public CgsGui::State
    {
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN     = 0,
            E_SUBSTATE_LOADING_COMPONENTS = 1,
            E_SUBSTATE_PROMPT             = 2,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // Hands the online-pause screen's static resource list (one APT package) to the loader.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        // Load the screen, then the menu's apt components, then dress the menu.
        void CheckForCompletedLoads();
        // A controller action: move / pick in the menu, or back out.
        void HandleControllerInput(const CgsModule::Event* lpEvent);
        // The quit-confirmation overlay closed.
        void HandleOverlayComplete(const CgsModule::Event* lpOverlayCompleteEvent);

        static const CgsGui::sResourceTuple maResourceTuplesToLoad[];
        static const s32                    miNumResourcesToLoad;
        static const s32                    maiEventToObserve[6];
        static const s32                    miNumEventsObserved;

        static const s32         KI_NUM_COMPONENTS_TO_LOAD = 3;
        static const char        KAC_PAUSE_OPTIONS_COMPONENT[7];
        static const char* const KAPC_PAUSE_OPTION_STRING_IDS[KI_NUM_COMPONENTS_TO_LOAD];

        MenuComponent mPauseOptions;   // console +0x38
        GuiCache*     mpGuiCache;      // console +0x10F8
        ESubState     meSubState;      // console +0x10FC
    };
}
