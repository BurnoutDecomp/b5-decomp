#pragma once

// ===================================================================================
// BrnNetwork::AutoLoginManager -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkAutoLoginManager.{h,cpp}
//
// The small online "auto login" state machine owned by BrnNetworkManager. It drives the
// silent sign-in / reconnect flow that the manager runs each frame: it waits for the
// network sub-system to reach its ready state, kicks off a connect, watches the
// connection complete, then -- once the per-process "auto login complete" bit set is
// fully populated -- disconnects from the server if the local player is not in a game.
//
// Layout is recovered from the X360 Construct/Destruct asm (0x8255E138 / 0x8255E1F8) and
// the accessors all sibling functions use:
//
//   +0x00 (8)  mTimer                 CgsSystem::Time (miSeconds @ +0, mfFraction @ +4).
//                                     Construct/Destruct/Prepare/Connect drive it through
//                                     Time::SetFloatVal (X360 calls SetFloatVal with r3 ==
//                                     this+0); UpdateWaitAutoLogin decrements it by the
//                                     frame delta (operator-=) and UpdateConnected adds the
//                                     frame delta back (its GetFloatVal feeds the 120s guard).
//   +0x08 (4)  meState                E_AUTO_LOGIN_STATE (the switch in ProcessBeforeSimulation:
//                                     1->UpdateWaitAutoLogin, 2->UpdateConnecting,
//                                     3->UpdateConnected; Construct stores 0).
//   +0x10 (8)  mProcessesComplete     CgsContainers::BitArray<4> -- one bit per auto-login
//                                     process. Construct/Prepare/Disconnected clear it
//                                     (std r11(=0),0x10); AutoLoginProcessComplete sets the
//                                     completed-process bit; UpdateConnected scans it for the
//                                     first clear bit to decide when every process is done.
//   +0x18 (4)  mpNetworkModule        BrnNetworkModule* (Construct a2; asm lwz 0x18).
//   +0x1C (4)  mpServerInterface      BrnServerInterface* (Construct a3; asm lwz 0x1C).
//   +0x20 (4)  mfReserved             f32 cleared to 0 by Construct/Destruct (stfs f31(=0),0x20).
//                                     Touched only as a zero-init scalar in the recovered code.
//
// Naming follows references/CXX_NAMING_CONVENTIONS.md. The state and process-count
// constants below are grounded in the asm (cmpwi against 1/2/3 in ProcessBeforeSimulation;
// the "leCompletedProcess < E_AUTO_LOGIN_PROCESS_COUNT" assert compares against 4). The
// individual process *names* are not recovered, so only the grounded count is modelled.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include "GameShared/GameClasses/System/Timer/CgsTime.h"         // CgsSystem::Time
#include "GameShared/GameClasses/Containers/CgsBitArray.h"       // CgsContainers::BitArray<4>

namespace BrnNetwork
{
    class BrnNetworkModule;
    class BrnServerInterface;

    class AutoLoginManager
    {
    public:
        // Number of distinct auto-login processes tracked in mProcessesComplete. Grounded:
        // the X360 "leCompletedProcess < E_AUTO_LOGIN_PROCESS_COUNT" assert compares the
        // index against 4 (cmpwi 4) and UpdateConnected scans exactly 4 bits.
        static const u32 KU_AUTO_LOGIN_PROCESS_COUNT = 4;

        // The auto-login state machine states. Values are grounded in ProcessBeforeSimulation's
        // switch (cmpwi against 1/2/3) and the stores in Construct(0)/Disconnected/Prepare(1)/
        // UpdateWaitAutoLogin(2)/Connect(3).
        enum E_AUTO_LOGIN_STATE
        {
            E_AUTO_LOGIN_STATE_IDLE              = 0,  // Construct default
            E_AUTO_LOGIN_STATE_WAIT_AUTO_LOGIN   = 1,  // Prepare / Disconnected set this
            E_AUTO_LOGIN_STATE_CONNECTING        = 2,  // UpdateWaitAutoLogin -> connecting
            E_AUTO_LOGIN_STATE_CONNECTED         = 3   // Connect -> connected
        };

        // X360 0x8255E138 -- bring the manager to its constructed state (called by
        // BrnNetworkManager::Construct). lpNetworkModule / lpServerInterface are asserted
        // non-null.
        void Construct(BrnNetworkModule* lpNetworkModule, BrnServerInterface* lpServerInterface);

        // X360 0x8255E1F8 -- tear the manager back down to its zero state (called by
        // BrnNetworkManager::Destruct).
        void Destruct();

        // X360 0x8255E260 -- arm the wait timer (60s) and clear the completed-process set;
        // returns true (called by BrnNetworkManager::Prepare).
        bool Prepare();

        // X360 0x8255E2A0 -- clear the completed-process set; returns true (called by
        // BrnNetworkManager::Release).
        bool Release();

        // X360 0x82556DB8 -- the server-interface signalled a disconnect: clear the
        // completed-process set and drop back to the wait-auto-login state (called by
        // BrnNetworkManager::TriggerEventFromServerInterface).
        void Disconnected();

        // X360 0x82556CB8 -- transition from connecting to connected: re-arm the timer from
        // the downloadable-config timeout, clear the completed-process set, notify the network
        // manager (OnAutoLogin) and enter E_AUTO_LOGIN_STATE_CONNECTED. (called by
        // UpdateWaitAutoLogin.)
        void Connect();

        // X360 0x82579910 -- per-frame tick dispatched on meState (called by
        // BrnNetworkManager::ProcessBeforeSimulation). The X360 body is a pure tail-call
        // dispatch that forwards its frame-delta / force-stay-connected arguments untouched to
        // the matching per-state update step (so the registers carrying them, f1 and r4, pass
        // straight through).
        void ProcessBeforeSimulation(f32 lfFrameDelta, bool lbForceStayConnected);

        // X360 0x82556DD0 -- mark one auto-login process complete (set its bit). leCompletedProcess
        // must be < KU_AUTO_LOGIN_PROCESS_COUNT.
        void AutoLoginProcessComplete(u32 luCompletedProcess);

    private:
        // ---- per-state update steps (dispatched by ProcessBeforeSimulation) -------------------
        // X360 0x82576230 -- wait for the network sub-system to be ready, then either Connect()
        // immediately (already logged in) or trigger sign-in. lfFrameDelta is the frame time step.
        void UpdateWaitAutoLogin(f32 lfFrameDelta);

        // X360 0x8254B688 -- while connecting, watch for the login to drop and fall back to the
        // wait-auto-login state.
        void UpdateConnecting();

        // X360 0x8255E2B8 -- while connected, accumulate the frame delta, assert if the connection
        // outlives the 120s budget, and once every auto-login process has completed disconnect from
        // the server if the local player is not in a game. lbForceStayConnected suppresses the
        // disconnect (the X360 `a4 & 1` flag).
        void UpdateConnected(f32 lfFrameDelta, bool lbForceStayConnected);

        CgsSystem::Time                       mTimer;              // +0x00
        E_AUTO_LOGIN_STATE                    meState;             // +0x08
        CgsContainers::BitArray<KU_AUTO_LOGIN_PROCESS_COUNT> mProcessesComplete; // +0x10
        BrnNetworkModule*                     mpNetworkModule;     // +0x18
        BrnServerInterface*                   mpServerInterface;   // +0x1C
        f32                                   mfReserved;          // +0x20
    };
}
