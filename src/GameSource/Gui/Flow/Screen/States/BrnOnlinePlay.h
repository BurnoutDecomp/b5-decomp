#pragma once

// ============================================================================
// GameSource/Gui/Flow/Screen/States/BrnOnlinePlay.h
//
// BrnGui::OnlinePlay -- the online-play main-menu screen state. A CgsGui::State
// leaf: it observes eight GUI events, latches the GuiCache, drives the main-menu
// (BrnGui::MenuComponent, 7 options) + the "new news" transition animation
// component + the network player-stats panel, runs a small sub-state machine
// (loading-screen -> loading-components -> main -> left, with the sign-in /
// collision-world / friends-list wait sub-states) and owns the local player's
// stats snapshot + the Xbox XNotify system listener handle.
//
// CLASS SHAPE + MEMBER ORDER: DecFIGS DWARF (BrnOnlinePlay.h), X360-attested.
// OnlinePlay : public CgsGui::State; member order (guest 32-bit byte offsets are
// documentary -- the x64 gate widens pointers, so members are reached BY NAME):
//   mNewNewsAnimation    (X360 +0x38, "new news" transition component)
//   mMainMenuComponent   (X360 +0xC8, main-menu, 7 rows)
//   mPlayerStatsDisplay  (X360 +0x1188, the network player-stats panel)
//   mPlayerStatsEvent    (X360 +0x2378, the local player's stats snapshot event)
//   ...meSubState, invite flags, cache, listener...
//
// Store-for-store from BURNOUT_X360_ARTIST.XEX for the reconstructed ledger
// bodies (addresses in the .cpp). The GuiCache apt-component watcher calls go
// through the committed real APIs (GuiCache::ClearExpectedAptComponentList /
// AppendExpectedAptComponent, MenuComponent::AppendExpectedAptComponent); the
// overlay posts go through the sibling-established GuiOverlayRequestWire.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                   // CgsGui::State (base, DWARF-authoritative)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"          // BrnGui::AnimationComponent (embedded @+0x38)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"    // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"               // BrnGui::MenuComponent (embedded @+0xC8)
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.h"       // BrnGui::GuiNetworkPlayerStats (embedded @+0x1188)
#include "GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h"                  // BrnGui::GuiEventNetworkPlayerStats (embedded @+0x2378)

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;                              // latched cache (pointer only)
    struct GuiEventCache;                        // HandleGuiCacheEvent param (pointer only)
    struct GuiEventControllerInputPressed;       // controller-input param (pointer only)

    struct OnlinePlay : public CgsGui::State
    {
        // DWARF BrnOnlinePlay.h:87 -- the screen's internal sub-state machine.
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN              = 0,
            E_SUBSTATE_LOADING_COMPONENTS          = 1,
            E_SUBSTATE_MAIN                        = 2,
            E_SUBSTATE_LEFT_SCREEN                 = 3,
            E_SUBSTATE_WAIT_SIGN_IN_FINISH         = 4,
            E_SUBSTATE_WAIT_COLLISION_WORLD_UNLOAD = 5,
            E_SUBSTATE_WAIT_FRIENDS_LIST_FINISH    = 6,
            E_SUBSTATE_WAIT_COLLISION_WORLD_LOAD   = 7,
            E_SUBSTATE_COUNT                       = 8,
        };

        // DWARF BrnOnlinePlay.h:106 -- the seven main-menu options.
        enum EMainMenuOptions
        {
            E_MAIN_MENU_OPTIONS_FREEBURN        = 0,
            E_MAIN_MENU_OPTIONS_IMAGE_GALLERY   = 1,
            E_MAIN_MENU_OPTIONS_VIEW_CHALLENGES = 2,
            E_MAIN_MENU_OPTIONS_UNRANKED        = 3,
            E_MAIN_MENU_OPTIONS_RANKED          = 4,
            E_MAIN_MENU_OPTIONS_SCOREBOARDS     = 5,
            E_MAIN_MENU_OPTIONS_NEWS            = 6,
            E_MAIN_MENU_OPTIONS_COUNT           = 7,
        };

        // @0x82508B40 -- compiler-emitted ctor: constructs the CgsGui::State base and the four
        //   embedded sub-objects (news-animation carrier / main-menu / player-stats panel /
        //   local player-stats event record). No scalar members are stored in the ctor asm
        //   (OnEnter primes them); the recovered effect is base + member default-construction.
        OnlinePlay();

        // The FSM enter/leave/update
        // virtuals. OnEnter registers the eight observed events, builds the news-animation +
        // player-stats + main-menu components, primes the local-player display block, creates
        // the XNotify listener and posts the screen's open events. OnLeave unregisters, posts
        // the teardown apt-movie + close events, clears the menu, destructs the stats panel and
        // closes the listener handle.
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @0x82508C00 -- hand this screen's static resource list to the loader.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        // ---- reconstructed ledger bodies (this slice) --------------------------------
        // @0x824AD6A0 -- validate the controller-input event, then, while in E_SUBSTATE_MAIN,
        // forward it to HandleControllerInputMainMenu.
        void HandleControllerInput(const CgsModule::Event* lpEvent);

        // @0x824A7488 -- act on a main-menu controller-action event (navigate / select / back /
        // friends / toggle).
        void HandleControllerInputMainMenu(const GuiEventControllerInputPressed* lpEvent);

        // @0x824858A0 -- latch the GuiCache carried by a GuiEventCache the first time one arrives,
        // then mirror the cache's invite-in-progress / performing-invite flags into this state.
        void HandleGuiCacheEvent(const GuiEventCache* lpEvent);

        // @0x82485968 -- is the highlighted menu option currently permitted (multiplayer-allowed
        // for the online options; always for the offline ones)?
        bool CheckPrivileges(EMainMenuOptions leOption);

        // @0x8249C090 -- run the picked option's state-event, latch the ranked/unranked selection
        // into the cache for the online options and post the "start" command.
        void SelectOnlineMenuOption(EMainMenuOptions leOption);

        // @0x8249BF58 -- populate + activate the main-menu rows (disabling the HDD-gated ones when
        // no hard disk is present), show the player-stats panel and post the "main menu ready" event.
        void ShowMainMenuOptions();

        // @0x8249BE98 -- play the "ON_MAIN" apt movie, advance to loading-components and register the
        // menu / stats / news-animation apt components as expected with the cache watcher.
        void ShowMainMenuScreen();

        // Wait for the screen's resources, then for its apt components; on the second, finish
        // the pending overlay waits, raise the disconnect / no-hard-disk overlays and show the menu.
        void CheckForCompletedLoads();

        // Latch a player-stats record (event 248) and, once the stats panel is loaded, show it.
        void HandlePlayerStatsEvent(const CgsModule::Event* lpEvent);

        // Open the guide's friends list, or ask the player to sign in first.
        void ShowFriendsMenu();

        // ---- statics (X360 .rdata) ----------------------------------------------------
        static const s32 KI_PLAYER_NAME_LENGTH = 16;   // mPlayerStatsEvent.macPlayerName (strncpy count)

        // The eight GUI event ids this state observes (X360 dword_8205EF64, count 8). Shared by
        // OnEnter (RegisterForEvents) / OnLeave (UnRegisterForEvents). Values dumped from the
        // ARTIST rodata (headless IDA, 2026-08-04): { 14, 21, 6, 44, 248, 64, 189, 493 }.
        static const s32 maiEventToObserve[];                                     // @0x8205EF64 (XEX-attested)
        static const s32 miNumEventsObserved;                                     // == 8

        static const CgsGui::sResourceTuple maResourceTuplesToLoad[];             // @0x8205EF88 (count 3)
        static const s32                    miNumResourcesToLoad;                 // @0x8205EFA0 == 3

        // The main-menu row localisation-key table ShowMainMenuOptions walks (X360 off_82F267E4,
        // 7 entries), and the picked-option -> state-event name table SelectOnlineMenuOption
        // indexes (X360 off_82F26800). Both tables fully dumped from the ARTIST rodata
        // (headless IDA, 2026-08-04) -- every entry is XEX-attested; see the .cpp definitions.
        static const char* const KAPC_MAIN_MENU_TEXT[E_MAIN_MENU_OPTIONS_COUNT];              // @0x82F267E4
        static const char* const KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[E_MAIN_MENU_OPTIONS_COUNT];// @0x82F26800

        // ---- data members (DWARF names + order; guest 32-bit offsets documentary) ------
        // The "new news" transition animation component (console +0x38).
        AnimationComponent    mNewNewsAnimation;   // console +0x38
        MenuComponent         mMainMenuComponent;  // console +0xC8 (7 rows)
        GuiNetworkPlayerStats mPlayerStatsDisplay; // X360 +0x1188 (the on-screen stats panel)
        // The local player's stats record. OnEnter clears its player id / rank / name tail
        // (+0x23FC / +0x2400 / +0x2404) and SetInfo shows its name and rank.
        GuiEventNetworkPlayerStats mPlayerStatsEvent; // console +0x2378

        ESubState meSubState;                     // X360 +0x2414
        bool      mbInviteInProgress;             // X360 +0x2418 (mirrored from the cache)
        bool      mbPerformingInvite;             // X360 +0x2419 (mirrored from the cache)
        GuiCache* mpGuiCache;                     // X360 +0x241C
        void*     mpNotifyListenerHandle;         // X360 +0x2420 (XNotifyCreateListener HANDLE)
    };
}
