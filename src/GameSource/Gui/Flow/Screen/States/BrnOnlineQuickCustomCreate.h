#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base, DWARF-authoritative)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"              // CgsGui::GuiComponent (news-animation component base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"              // BrnGui::MenuComponent (embedded @+0xD0)

// ============================================================================
// GameSource/Gui/Flow/Screen/States/BrnOnlineQuickCustomCreate.h
//
// BrnGui::OnlineQuickCustomCreate -- the online "quick / custom / create match"
// main-menu flow state. A CgsGui::State leaf: it observes four GUI events, latches
// the GuiCache, drives the main-menu (BrnGui::MenuComponent) + the "new news"
// transition animation component, and runs a small internal state machine
// (GetCache -> LoadScreen -> LoadComponents -> Running -> Left, with the
// sign-in / collision-world / friends-list wait sub-states).
//
// Layout + member names + method shapes are DWARF-AUTHORITATIVE (DecFIGS
// BrnOnlineQuickCustomCreate.h): OnlineQuickCustomCreate : public CgsGui::State;
// members in order -- mpGuiCache (X360 +0x38), meInternalState (+0x3C),
// mNewNewsAnimation (BrnGui::AnimationComponent, +0x40), mMainMenuComponent
// (BrnGui::MenuComponent, +0xD0), then the sign-in / friends-list bookkeeping and
// the XNotify listener handle (+0x1190). The guest 32-bit offsets in the comments
// are documentary (the x64 gate widens pointers).
//
// This slice reconstructs the four internal Update* helpers store-for-store:
//   UpdateLoadComponents @0x8248DEF0 (cpp:390)
//   UpdateLoadResources  @0x824A17D8 (cpp:359)
//   UpdateRunning        @0x824926A8 (cpp:425)
//   UpdatePermanent      @0x82492728 (cpp:527)
// The remaining virtuals / sign-in / friends-list machinery are declared-only here
// and link from the other slices of this TU.
//
// FLAG boundaries: the GuiCache apt-component watcher entries the X360 reaches
// (ClearExpectedAptComponentList / AppendExpectedAptComponent), the
// MenuComponent::AppendExpectedAptComponent row-registrar, and the far GuiCache
// member the state writes the picked menu option into (X360 *(mpGuiCache+0x4B40))
// are not on the committed public APIs, so they are reached through the file-local
// boundary namespace in the .cpp (same convention as
// BrnGui::OnlineMarkManCacheBoundary / BrnGui::ReplayMainCacheBoundary).
// ============================================================================

namespace BrnGui
{
    class GuiCache;

    // The "new news" transition animation component embedded at X360 +0x40. DWARF types it
    // BrnGui::AnimationComponent; its concrete class is not committed yet and this TU only
    // reaches its GuiComponent base surface (the virtual Construct in OnEnter, GetName() and
    // AddOutputAptViewState here), so it is modelled as a GuiComponent-derived carrier sized
    // to the X360 slot span [+0x40, +0xD0) (0x90 bytes). GROW to the real AnimationComponent
    // when that type lands.
    struct AnimationComponent : public CgsGui::GuiComponent
    {
        // Component base (vptr + macName[128] + hash + state-interface) occupies the leading
        // bytes; the animation-specific state this TU does not name is held as an opaque tail.
        // NOTE: the X360 slot span is [+0x40, +0xD0) (0x90 bytes); on the 64-bit host the base
        // GuiComponent widens, so the tail is a documentary opaque pad (this TU only reaches the
        // GuiComponent base surface -- GetName / AddOutputAptViewState -- by name).
        u8 maReservedAnimationTail[0x40];
    };

    struct OnlineQuickCustomCreate : public CgsGui::State
    {
        // DWARF BrnOnlineQuickCustomCreate.h:68 -- the three main-menu options.
        enum EMainMenuOptions
        {
            E_MAIN_MENU_OPTIONS_QUICK_MATCH  = 0,
            E_MAIN_MENU_OPTIONS_CUSTOM_MATCH = 1,
            E_MAIN_MENU_OPTIONS_CREATE_MATCH = 2,
            E_MAIN_MENU_OPTIONS_COUNT        = 3,
        };

        // DWARF BrnOnlineQuickCustomCreate.h:80 -- the internal state machine.
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

        // @0x824A1420 / @0x824A1538 -- the FSM enter/leave virtuals. OnEnter registers the four
        // observed GUI events, builds the main-menu (3 rows) + "new news" transition components,
        // null-latches the cache into E_INTERNALSTATE_GETCACHE, posts the screen's two open
        // apt/view-state events and creates the XNotify listener. OnLeave unregisters, posts the
        // teardown apt-movie + close view-state events, clears the menu into E_INTERNALSTATE_LEFT
        // and closes the listener handle.
        virtual void OnEnter();
        virtual void OnLeave();

        // @0x8250xxxx (declared-only; the freeburn flavour override pins the vtable slot) --
        // dispatch the picked main-menu option through the option -> state-event table. Body
        // links from another slice of this TU.
        virtual void ProcessSelectedMenuOption(EMainMenuOptions leOption);

        // @0x825005A0 -- hand this screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad == 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // ---- internal per-phase Update helpers (this slice) ----
        // @0x824A17D8 (cpp:359) -- wait for the screen resources to finish loading, then post
        // the load-string command, prime the expected-apt-component list and return done.
        bool UpdateLoadComponents();   // @0x8248DEF0 (cpp:390)
        // @0x824A17D8 (cpp:359) -- once the screen resources have finished loading, post the
        // load-string apt movie ("ON_QMCMCM", level 3), prime the expected-apt-component list
        // (menu rows + the news-transition component) and report done.
        bool UpdateLoadResources();    // @0x824A17D8 (cpp:359)
        void UpdateRunning();          // @0x824926A8 (cpp:425)
        void UpdatePermanent();        // @0x82492728 (cpp:527)

        // @0x8248DFA8 (cpp:466; declared-only) -- act on a controller-input-pressed event while
        // RUNNING. Body links from another slice of this TU.
        void HandleControllerInputPressed(const CgsModule::Event* lpEvent);

        // ---- recovered members (DWARF names + order; guest 32-bit offsets documentary) ----
        GuiCache*          mpGuiCache;          // X360 +0x38
        InternalState      meInternalState;     // X360 +0x3C
        AnimationComponent mNewNewsAnimation;   // X360 +0x40 ("new news" transition component)
        MenuComponent      mMainMenuComponent;  // X360 +0xD0 (main-menu, 3 rows)

        // DWARF trailing bookkeeping (mMemoryContainer / mNetStartResult / mbFriendsListDone /
        // mbCleanUpThisFrame) plus the XNotify listener handle the X360 stores at +0x1190. This
        // slice never names those; the span is held as opaque reserved storage keeping the DWARF
        // order (the X360 byte offset does not survive the 64-bit widening, so it is documentary).
        // The handle is surfaced by name because OnEnter/OnLeave (other slices) key on it.
        u8   maReservedFriendsAndSigninState[0x1190 - 0xD0];
        void* mpNotifyListenerHandle;           // X360 +0x1190 (XNotifyCreateListener HANDLE)

        // ---- statics (X360 .rdata) ----
        // The four GUI event ids this state observes (X360 dword_8205F9B4, count 4). Shared by
        // OnEnter (RegisterForEvents) / OnLeave (UnRegisterForEvents). FLAG: the id VALUES are not
        // attested in scope (only the base address + the count of 4); placeholders so the state
        // links, adopted with the XEX-recovered ids when decoded.
        static const s32 maiEventToObserve[];                      // @0x8205F9B4 (values not decoded)
        static const s32 miNumEventsObserved;                      // == 4

        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @0x8205F9C8 (count 2)
        static const u32                    muNumResourcesToLoad;  // @0x8205F9D8 == 2

        // The main-menu row localisation-key table (X360 off_82F268EC, 3 entries) UpdateLoadComponents
        // walks to SetText each row, and the load-string apt-name key (X360 off_82F27B94, "ON_QMCMCM").
        static const char* const KAPC_MAIN_MENU_TEXT[E_MAIN_MENU_OPTIONS_COUNT];   // @0x82F268EC
        static const char* const KAC_LOAD_STRING_APT_NAME;                          // @0x82F27B94 ("ON_QMCMCM")

        // Picked-option -> state-event name table ProcessSelectedMenuOption walks (X360 off_82F268F8;
        // [0] = "TO_QWK_MAT"). The freeburn subclass overrides with its own table @0x82F26904.
        static const char* const KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[E_MAIN_MENU_OPTIONS_COUNT];   // @0x82F268F8
    };
}
