#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlineCreateFreeburn - the online "create freeburn" screen state (ON_CREATE_FB).
// It loads its two apt packages, posts the create-game request (GUI 256) for a free-burn
// lobby with the "entering game" overlay up, and advances to the game room once the
// network reports the game created and joined (GUI 50); GUI 51 (failed) backs out.
// The base derivation (CgsGui::State), the member names and order and the virtual set are
// the original declaration's; member placement is the console's.
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // pointer member only

    struct OnlineCreateFreeburn : public CgsGui::State
    {
        // The screen's sub-state machine (console +0x38).
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN     = 0,
            E_SUBSTATE_LOADING_COMPONENTS = 1,
            E_SUBSTATE_SELECTING_PARAMS   = 2,
            E_SUBSTATE_WAIT_IN_GAME       = 3,
            E_SUBSTATE_COUNT              = 4,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825004A0 - hands the create-freeburn screen's static resource list to the
        // loader (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        // The in-queue hands the handlers the header-stripped payload, so they take the
        // bare event (declared over GuiEventControllerInputPressed / GuiEventCache /
        // GuiEventNetworkInGameFailed).
        void HandleControllerInput(const CgsModule::Event* lpEvent);
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);
        void HandleInGameEvent(const CgsModule::Event* lpInGameEvent);
        void HandleInGameFailedEvent(const CgsModule::Event* lpEvent);
        void CheckForCompletedLoads();
        // No out-of-line copy on the console: HandleGuiCacheEvent inlines it (its assert
        // fires at the console's line 376).
        bool CheckPrivileges();

        static const s32                    maiEventToObserve[7];
        static const s32                    miNumEventsObserved;      // == 7
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F880 (unk_8205F880, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F890 (dword_8205F890, .rdata) == 2

        // ---- data members (declaration order; console offsets in the comments) -------
        ESubState meSubState;   // +0x38
        GuiCache* mpGuiCache;   // +0x3C
    };
}
