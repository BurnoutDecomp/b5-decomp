#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h" // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"      // BrnGui::MenuComponent (by value)

// BrnGui::OnlineQuickCustomCreate - the online "quick / custom / create match" main-menu
// flow state (ON_QK_CST_CR; the freeburn flavour derives from it). It loads its apt
// packages, posts the online ticker, runs the three-row menu and opens the guide's friends
// list on request. The base derivation (CgsGui::State), the member names and order and the
// virtual set are the original declaration's; member placement is the console's.
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // pointer member only

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

        // The screen's internal state machine (console +0x3C).
        enum InternalState
        {
            E_INTERNALSTATE_GETCACHE               = 0,
            E_INTERNALSTATE_LOADSCREEN             = 1,
            E_INTERNALSTATE_LOADCOMPONENTS         = 2,
            E_INTERNALSTATE_RUNNING                = 3,
            E_INTERNALSTATE_LEFT                   = 4,
            E_SUBSTATE_WAIT_SIGN_IN_FINISH         = 5,
            E_SUBSTATE_WAIT_COLLISION_WORLD_UNLOAD = 6,
            E_SUBSTATE_WAIT_FRIENDS_LIST_FINISH    = 7,
            E_SUBSTATE_WAIT_COLLISION_WORLD_LOAD   = 8,
            E_INTERNALSTATE_COUNT                  = 9,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825005A0 - hands this screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        void UpdateGetCache();
        bool UpdateLoadResources();
        bool UpdateLoadComponents();
        void UpdateRunning();
        void UpdatePermanent();
        // The in-queue hands the handler the header-stripped payload (declared over
        // GuiEventControllerInputPressed).
        void HandleControllerInputPressed(const CgsModule::Event* lpEvent);
        // Dispatch the picked main-menu option (the freeburn flavour overrides it).
        virtual void ProcessSelectedMenuOption(EMainMenuOptions leOption);
        void ShowFriendsMenu();

        static const s32                   maiEventToObserve[4];
        static const s32                    miNumEventsObserved;  // == 4
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x8205F9C8 (unk_8205F9C8, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x8205F9D8 (dword_8205F9D8, .rdata) == 2

        static const char        KAC_NEW_NEWS_ANIMATION_COMPONENT[18];   // "NewNewsTransition"
        static const char        KAC_MAIN_MENU_COMPONENT[9];             // "MenuItem"
        static const char* const KAPC_MAIN_MENU_TEXT[E_MAIN_MENU_OPTIONS_COUNT];
        static const char* const KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[E_MAIN_MENU_OPTIONS_COUNT];

        // ---- data members (declaration order; console offsets in the comments) -------
        GuiCache*          mpGuiCache;           // +0x38
        InternalState      meInternalState;      // +0x3C
        AnimationComponent mNewNewsAnimation;    // +0x40
        MenuComponent      mMainMenuComponent;   // +0xD0
        // The guide's system-notification listener (the sign-in wait reads it).
        void*              mhNotifyListener;     // +0x1190
    };
}
