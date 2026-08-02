#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"   // CgsGui::State (base)

// ===========================================================================
// BrnScreenStatesLinkStubs.h -- placeholder classes for the SCREEN-flow states the
// BrnScreenFlow 61-state pool instantiates but whose real classes are not yet
// reconstructed in the tree. Each placeholder:
//   * derives from CgsGui::State so it slots into the state machine + SetStates table;
//   * is registered by BrnScreenFlow::Prepare under its REAL script id (so a
//     BRNSCREENFSM Lua SetState onto it resolves instead of faulting);
//   * logs OnEnter once and is otherwise inert (OnLeave/Update empty, base
//     GetResourcesToLoad default = no resources) -- the BrnHudStatesLinkStubs pattern.
// The X360 (4-byte-pointer) sizeof each real class -- from BrnScreenFlow::Prepare
// @0x82523E50 / PrintStateSizes @0x824F2150 -- is noted per class for the eventual
// faithful reconstruction. FLAG link scaffold: every class below is a stand-in for an
// un-reconstructed real class, not a reconstruction.
// ===========================================================================

namespace BrnGui
{
    class ProfileManager;   // forward-only: the CN_PROFILE stub must NOT call into it

    // FLAG PC-platform leaf: placeholder -- real BrnGui::NullState (X360 56B; trivial
    // vtable-only ctor @0x82523E98) not yet reconstructed. Script id "NULL".
    struct NullState : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // FLAG PC-platform leaf: placeholder -- real BrnGui::CarSelectUnlock (X360 1216B;
    // inline ctor wiring text-selection/menu-item component vtables) not yet
    // reconstructed. Script id "CS_UNLOCK".
    struct CarSelectUnlock : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // (BrnGui::CarSelectLivery -- the CS_LIVERY screen -- was RECONSTRUCTED 2026-08-02;
    //  its real home is States/BrnCarSelectLivery.h + three partfiles.)

    // FLAG PC-platform leaf: placeholder -- real BrnGui::CrashNavMapEvent (X360 25104B;
    // CrashNavMap base ctor + the event-map vtable @0x82077044) not yet reconstructed
    // (nor is its CrashNavMap base). Script id "CN_MAP_EVENT".
    struct CrashNavMapEvent : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // FLAG PC-platform leaf: placeholder -- real BrnGui::CrashNavMapMain (X360 24944B;
    // CrashNavMap base ctor + the main-map vtable @0x8207707C) not yet reconstructed
    // (nor is its CrashNavMap base). Script id "CN_MAP_MAIN".
    struct CrashNavMapMain : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // FLAG PC-platform leaf: placeholder -- real BrnGui::CrashNavProfile (X360 5896B;
    // out-of-line ctor + the wider Construct(id, fsm, ProfileManager&) @0x8252448C the
    // flow's Prepare calls DIRECTLY instead of the vtbl+0x18 dispatch) not yet
    // reconstructed. Script id "CN_PROFILE". The stub's wider Construct forwards to the
    // base 2-arg Construct and never touches the profile manager (its backing object may
    // be an un-reconstructed shell).
    struct CrashNavProfile : public CgsGui::State
    {
        using CgsGui::State::Construct;   // keep the base (id, fsm) overload visible
        void Construct(CgsID lId, CgsFsm::ScriptedFsm* lpFsm, ProfileManager& lrProfileManager);
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // FLAG PC-platform leaf: placeholder -- real BrnGui::OnlineGameRoomPlayerInfo (X360
    // 87968B, the largest screen state) not yet reconstructed. Script id "ON_GAME_ROOM".
    // DELIBERATE RENAME (+State suffix): a committed non-State partial slice of the real
    // class (ShowSettingsOptions only) already lives in BrnCrashNavOptions.h under the
    // real name, and the flow container includes that header -- defining a second
    // BrnGui::OnlineGameRoomPlayerInfo here would be an ODR fork. The real class keeps
    // its name when it is reconstructed; this placeholder then dies.
    struct OnlineGameRoomPlayerInfoState : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // FLAG PC-platform leaf: placeholder -- real BrnGui::OnlineTeamSelection (X360 4792B;
    // out-of-line ctor; absent from the Dec-2007 DWARF, X360-only) not yet reconstructed.
    // Script id "ON_TEAMS".
    struct OnlineTeamSelection : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // FLAG PC-platform leaf: placeholder -- real BrnGui::ReplayClips (X360 4496B; inline
    // ctor + embedded MenuComponent) not yet reconstructed. Script id "RE_CLIPS".
    struct ReplayClips : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // FLAG PC-platform leaf: placeholder -- real BrnGui::ReplayClipsOnline (X360 4496B;
    // inline ctor + embedded MenuComponent) not yet reconstructed. Script id "RE_CLIPS_ON".
    struct ReplayClipsOnline : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // FLAG PC-platform leaf: placeholder -- real BrnGui::ReplayOptions (X360 19320B;
    // inline ctor + embedded MenuToggleGroupVarSize<5>) not yet reconstructed. Script id
    // "RE_OPTIONS".
    struct ReplayOptions : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // FLAG PC-platform leaf: placeholder -- real BrnGui::ReplayIntro not yet
    // reconstructed. Script id "RE_INTRO". (X360 evidence disagrees with itself: Prepare
    // @0x82524xxx allocates 72B with a vtable-only ctor for the RE_INTRO slot, while
    // PrintStateSizes @0x824F2150 prints sizeof(ReplayIntro) == 1096 -- resolve when the
    // real class is reconstructed.)
    struct ReplayIntro : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };

    // FLAG PC-platform leaf: placeholder -- real BrnGui::ReplayCredits (X360 72B;
    // vtable-only ctor) not yet reconstructed. Script id "RE_CREDITS".
    struct ReplayCredits : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
    };
}
