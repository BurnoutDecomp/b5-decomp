#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                                   // CgsID (mLastHostCarID)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"              // CgsGui::GuiComponent (animator base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlineCountdown.h"   // mOnlineCountdown  (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerList.h"  // mOnlinePlayerList (by value)

namespace CgsModule { struct Event; }

// BrnGui::CarSelectOnlineEnd - the online car-select "ready / countdown" screen flow state. It
// latches a GuiCache, drives the online countdown clock + the lobby player-list component, and (on
// the online host) the host-choosing transition clip. Class shape / member NAMES + declaration
// order are from the DecFIGS DWARF for this exact path (BrnCarSelectOnlineEnd.h). The byte offsets
// in the trailing comments are the X360 32-bit-ABI offsets proven by BURNOUT_X360_ARTIST.XEX; the
// gate compiles 64-bit, so the embedded components widen and the offsets are documentary only --
// every member is accessed BY NAME.
namespace BrnGui
{
    class  GuiCache;               // mpGuiCache (pointer only)
    struct LobbyPlayerStatusData;  // mpHostStatusData (pointer only; no committed home yet)

    // The host-choosing transition clip embedded by value at X360 +0x1A20. DWARF types it
    // BrnGui::AnimationComponent; its concrete class is not committed yet and this state only
    // reaches its GuiComponent base surface (Construct in OnEnter, GetName / AddOutputAptViewState),
    // so it is modelled as a GuiComponent-derived carrier with an opaque animation tail (the same
    // pattern BrnOnlineQuickCustomCreate.h uses). GROW to the real AnimationComponent when it lands.
    struct AnimationComponent : public CgsGui::GuiComponent
    {
        u8 maReservedAnimationTail[0x40];
    };

    struct CarSelectOnlineEnd : public CgsGui::State
    {
        // BrnCarSelectOnlineEnd.h:71 (DWARF) -- the load / init / run state machine.
        enum InternalState
        {
            E_INTERNALSTATE_LOADRESOURCES = 0,
            E_INTERNALSTATE_WFINIT        = 1,
            E_INTERNALSTATE_RUNNING       = 2,
            E_INTERNALSTATE_LEFT          = 3,
            E_INTERNALSTATE_COUNT         = 4,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82508A80 - hands the online-car-select-end state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // @ 0x824967F0 - ensure the screen's resources are loaded; when they are, play the screen
        //   apt movie and register the host-choosing clip as an expected component. Returns true
        //   the moment the resources are ready.
        bool UpdateLoadResources();
        // @ 0x824849F0 - wait for the flow's apt components to finish initialising; on an
        //   online-host game, kick the host-choosing clip visible. Returns true once initialised.
        bool UpdateWFInit();
        // @ 0x824A3408 - drain the in-queue while running: set-countdown-time + lobby-player-list.
        void UpdateRunning();

        // ---- declared for the class shape / the UpdateRunning + Update call sites; bodied by a
        //      later wave (each depends on un-recovered collaborators -- see BrnCarSelectOnlineEnd.cpp).
        // @ 0x8248FBA8 - the always-run in-queue drain (load-notify / player-disconnect / advance).
        void UpdatePermanent();
        // @ 0x824968F0 - apply a lobby-player-list update: per-player show/name/car/final-selection
        //   plus the host's car-change selection.
        void HandleLobbyPlayerList(const CgsModule::Event* lpEvent);

        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x8205E714 (.rdata, 2 entries)
        static const u32                    muNumResourcesToLoad; // @ 0x8205E724 (== 2)

        GuiCache*                    mpGuiCache;            // +0x38
        InternalState                meInternalState;       // +0x3C
        CarSelectOnlineCountdown     mOnlineCountdown;      // +0x40
        CarSelectOnlinePlayerList    mOnlinePlayerList;     // +0x1F8
        const LobbyPlayerStatusData* mpHostStatusData;      // +0x1A14
        CgsID                        mLastHostCarID;        // +0x1A18
        AnimationComponent           mHostChoosingAnimator; // +0x1A20
    };
}
