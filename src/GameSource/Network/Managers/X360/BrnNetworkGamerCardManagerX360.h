#pragma once

// ===================================================================================
// BrnNetwork::NetworkGamerCardManagerX360 -- owning header
//   b5-decomp/src/GameSource/Network/Managers/X360/BrnNetworkGamerCardManagerX360.{h,cpp}
//
// The Xbox-360 gamer-card / player-profile-card manager owned by BrnNetworkManager. It
// drives a tiny process / sub-state / action state machine on top of the Xbox Guide UI
// (XShowGamerCardUI / XShowSigninUI), the Live sign-in state (XUserGetSigninState) and a
// Live notification listener (XNotifyCreateListener / XNotifyGetNext). The high-level
// requests it serves are:
//   * request a player's XUID by gamer name through the server-interface player-info
//     component (GetXuidForPlayer / ActionReqXuidByName),
//   * show a remote player's gamer card UI for a given controller port, after first
//     ensuring the local player is signed in to Live (ShowGamerCard / ShowGamerCardForXuid
//     / ActionShowSignInUI / ActionShowGamercardUI).
//
// STATE-MACHINE LAYOUT (a small array of dispatch tables embedded at the head of the
// object, then the working state). The dispatch tables and offsets are X360-authoritative
// (Construct/InitialiseProcessArray/InitialiseUpdateArray/InitialiseActionArray asm and the
// per-method member accesses):
//
//   +0x00  maProcessActions[KI_NUMBER_OF_PROCESSES * KI_ACTIONS_PER_PROCESS] (5*4 = 20 s32)
//          The per-process action sequence. InitialiseProcessArray fills every slot with
//          KI_NO_ACTION (3) then writes the real action ids; SetNextAction walks
//          maProcessActions[meProcess*4 + miActionIndex].
//   +0x50  mapUpdateFunctions[KI_NUMBER_OF_SUBSTATES] (3 fn-ptrs)   -- index 20..22
//          The per-sub-state update function dispatched each frame by
//          ProcessBeforeSimulation (indexed by meSubState).
//   +0x5C  mapActionFunctions[KI_NUMBER_OF_ACTIONS] (3 fn-ptrs)     -- index 23..25
//          The action functions SetNextAction dispatches (indexed by an action id).
//   +0x68  maRequestedPlayerName    BrnNetwork::PlayerName (16B; stb/lbz on first byte)
//   +0x78  mRequestedGamerCardXUID  u64 (XUID; std/ld as a unit)
//   +0x80  muRequestingControllerPort u32 (the signed-in user/controller index; 4 == none)
//   +0x84  meSubState               s32 (index into mapUpdateFunctions / process selector)
//   +0x88  meProcess                s32 (process id; KI_PROCESS_NONE == 5 when idle)
//   +0x8C  miActionIndex            u8  (action cursor within the current process; stb/lbz)
//   +0x90  mNotifyListener          void* (XNotifyCreateListener HANDLE)
//   +0x94  mpServerInterface        BrnNetwork::BrnServerInterface*
//   +0x98  mpCompleteCallback       CompleteCallback (fired by ProcessComplete; 0 == none)
//   +0x9C  miCompleteUserData       s32 (user data passed to mpCompleteCallback)
//
// The X360 byte offsets above are 32-bit-pointer/Xenon layout facts; on a 64-bit host the
// fn-ptr tables and pointers widen, so the offsets are NOT static_asserted -- members are
// accessed BY NAME and the dispatch tables are real arrays.
//
// Original home (from the asm assert strings):
//   ..\..\..\GameSource\Network/Managers/X360/BrnNetworkGamerCardManagerX360.cpp
// ===================================================================================

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::PlayerName

namespace BrnNetwork
{
    class BrnServerInterface;

    class NetworkGamerCardManagerX360
    {
    public:
        // The per-sub-state update functions and per-action action functions are dispatched
        // through the embedded tables; they take the `this` pointer and return an s32 status
        // (non-zero == proceed / success). Modelled as plain member-function pointers built
        // from the asm dispatch (`bctrl` on the loaded slot with r3 == this).
        typedef s32 (NetworkGamerCardManagerX360::*UpdateFunction)();
        typedef s32 (NetworkGamerCardManagerX360::*ActionFunction)();

        // Fired by ProcessComplete when a request finishes (X360 `bctrl` on +0x98 with
        // r3 == status, r4 == the requested XUID, r5 == the user data). 0 == no callback.
        typedef s32 (*CompleteCallback)(s32 liStatus, u64 lXuid, s32 liUserData);

        // X360 0x82561890 -- build the dispatch tables and bring the manager to its idle
        // state (called by BrnNetworkManager::Construct). Stores mpServerInterface, clears
        // the request fields, meProcess = KI_PROCESS_NONE (5), muRequestingControllerPort =
        // KU_NUMBER_OF_PADS (4).
        void Construct(BrnServerInterface* lpServerInterface);

        // X360 0x8254CA40 -- tear the manager back down to its zero/idle state (called by
        // BrnNetworkManager::Destruct).
        void Destruct();

        // X360 0x8254C9A8 -- create the Live notification listener and clear the sub-state
        // (called by BrnNetworkManager::Prepare). Always returns true.
        bool Prepare();

        // X360 0x8254C9F0 -- close the Live notification listener (called by
        // BrnNetworkManager::Release). Always returns true.
        bool Release();

        // X360 0x8254CC50 -- the per-frame tick: dispatch the update function for the current
        // sub-state (called by BrnNetworkManager::ProcessBeforeSimulation).
        s32 ProcessBeforeSimulation();

        // X360 0x8254D348 -- reset the request state on a network disconnect (called by
        // BrnNetworkManager::TriggerEventFromServerInterface).
        void Disconnected();

        // X360 0x8254D258 -- request a remote player's XUID by gamer name (called by
        // BrnNetwork::ScoreboardManager). lpfnComplete (r6) is the completion callback fired
        // with the resolved XUID; liUserData (r7) is passed back to it. Returns 1 if the
        // request started, 0 if busy. Arg order is the X360 register order (the PPC Hex-Rays
        // mis-orders the trailing callback/user-data pair).
        s32 GetXuidForPlayer(u32 luRequestingControllerPort, const PlayerName* lpPlayerName,
                             CompleteCallback lpfnComplete, s32 liUserData);

        // X360 0x8256C9F0 -- show a remote player's gamer card, identifying them by name. If
        // the player is in the current game their XUID is resolved through the games component;
        // otherwise their XUID is resolved by-name. Called by BrnNetwork::StateManager.
        s32 ShowGamerCard(u32 luRequestingControllerPort, const PlayerName* lpPlayerName);

        // X360 0x8254D1A0 -- show a remote player's gamer card, identifying them directly by
        // XUID. Called by BrnNetwork::StateManager.
        s32 ShowGamerCardForXuid(u32 luRequestingControllerPort, u64 lPlayerXuid);

    private:
        // -- dispatch-table builders (called by Construct) ----------------------------------
        // X360 0x8255E838 -- fill mapUpdateFunctions and assert every slot is non-null.
        void InitialiseUpdateArray();
        // X360 0x82557AC8 -- fill mapActionFunctions and assert every slot is non-null.
        void InitialiseActionArray();
        // X360 0x8254CA78 -- fill maProcessActions (default KI_NO_ACTION, then the real ids)
        // and assert every process supplies at least one action.
        void InitialiseProcessArray();

        // -- per-sub-state update functions (mapUpdateFunctions) -----------------------------
        // mapUpdateFunctions[0] -- the idle/no-op sub-state update. In the X360 image its
        // address is COMDAT-folded with CgsSceneManager::CgsCollision::BaseCollisionGenerator::
        // Destruct (an identical empty `blr`-only function); the original source slot is this
        // class's own do-nothing idle update. Reconstructed as the empty function it is.
        s32 UpdateIdle();
        // X360 0x82557B60 -- poll the by-name xuid request, then advance the action sequence.
        s32 UpdateWaitPlayerInfo();
        // X360 0x82557BA0 -- poll the sign-in, then advance the action sequence.
        s32 UpdateWaitSignInFinish();

        // -- per-action action functions (mapActionFunctions) --------------------------------
        // X360 0x8254CD18 -- ask the player-info component to resolve the requested name's XUID.
        s32 ActionReqXuidByName();
        // X360 0x8254CDE0 -- pop the Xbox Guide sign-in UI.
        s32 ActionShowSignInUI();
        // X360 0x8254CE28 -- pop the Xbox Guide gamer-card UI for the requested XUID.
        s32 ActionShowGamercardUI();

        // -- internal helpers -----------------------------------------------------------------
        // X360 0x8254CED0 -- poll the player-info component's idle/error state for the by-name
        // xuid request.
        s32 PlayerInfoIdle();
        // X360 0x8254D0B0 -- poll the Live notification queue for the sign-in-completed event.
        s32 IsSignedInToLive();
        // X360 0x8254CBA8 -- advance to the next action in the current process (or complete).
        s32 SetNextAction();
        // X360 0x8254CB28 -- finish the current request: fire the completion callback (if any)
        // with liStatus and reset the manager to idle.
        s32 ProcessComplete(s32 liStatus);

        // -- dispatch-table dimensions (X360-authoritative loop counts / immediates) ---------
        // InitialiseProcessArray walks 5 processes (li 5 @ the assert loop) of 4 actions each
        // (the 20-slot fill loop + the *4 stride in SetNextAction).
        static const s32 KI_NUMBER_OF_PROCESSES   = 5;
        static const s32 KI_ACTIONS_PER_PROCESS   = 4;
        // InitialiseUpdateArray / InitialiseActionArray each fill 3 slots (li 3 assert loops).
        static const s32 KI_NUMBER_OF_SUBSTATES   = 3;
        static const s32 KI_NUMBER_OF_ACTIONS     = 3;

        // The "no action" sentinel InitialiseProcessArray pre-fills every action slot with
        // (X360 `*v1++ = 3`); SetNextAction treats it (and the end-of-process index 4) as
        // "process complete". DWARF/asm immediate 3.
        static const s32 KI_NO_ACTION             = 3;

        // The idle process id (no request in flight). Construct/Destruct/Disconnected/
        // ProcessComplete store 5 into meProcess; GetXuidForPlayer/ShowGamerCardForXuid only
        // start when meProcess == 5 (X360 cmpwi 5). It is one past the last real process index.
        static const s32 KI_PROCESS_NONE          = 5;

        // The "signed in to Live" value XUserGetSigninState returns (X360 cmpwi r3, 2). Mirrors
        // the committed BuddyManagerX360 (XUserSigninState_SignedInToLive == 2).
        static const s32 KI_SIGNIN_STATE_LIVE     = 2;

        // The Live notification id IsSignedInToLive consumes: the sign-in-changed notification
        // (X360 cmpwi v3, 9) and the cross-game-invite notification it asserts on
        // (cmpwi 0x2000002 == 33554434). Grounded asm immediates.
        static const s32 KI_NOTIFY_SIGNIN_CHANGED   = 9;
        static const s32 KI_NOTIFY_INVITE_ACCEPTED  = 0x2000002;

        // The "no controller port" sentinel (X360 cmplwi 4 == CgsInput::KU_NUMBER_OF_PADS) the
        // show paths assert against. Pads are indexed [0,4); 4 means "none".
        static const u32 KU_NUMBER_OF_PADS        = 4;

        // -- members (offsets pinned in the header note) -------------------------------------
        // maProcessActions holds ACTION IDS (small s32 indices into mapActionFunctions, or
        // KI_NO_ACTION), NOT function pointers -- InitialiseProcessArray stores plain integers
        // (`*v1++ = 3`, then `result[5] = 1`, etc.) and SetNextAction uses the slot as an index.
        s32            maProcessActions[KI_NUMBER_OF_PROCESSES * KI_ACTIONS_PER_PROCESS]; // +0x00
        UpdateFunction mapUpdateFunctions[KI_NUMBER_OF_SUBSTATES];   // +0x50
        ActionFunction mapActionFunctions[KI_NUMBER_OF_ACTIONS];     // +0x5C
        PlayerName     maRequestedPlayerName;                        // +0x68 (16B)
        u64            mRequestedGamerCardXUID;                      // +0x78
        u32            muRequestingControllerPort;                  // +0x80
        s32            meSubState;                                   // +0x84
        s32            meProcess;                                    // +0x88
        u8             miActionIndex;                                // +0x8C
        void*          mNotifyListener;                              // +0x90
        BrnServerInterface* mpServerInterface;                       // +0x94
        CompleteCallback    mpCompleteCallback;                      // +0x98
        s32                 miCompleteUserData;                      // +0x9C
    };
}
