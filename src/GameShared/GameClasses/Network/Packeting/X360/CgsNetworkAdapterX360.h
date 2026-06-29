#pragma once

// ===================================================================================
// CgsNetwork::NetworkAdapterX360 -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/X360/CgsNetworkAdapterX360.h
//
// The Xbox-360 concrete network adapter: it owns the DirtySock NetConn* connection
// lifecycle (Startup / Connect / Idle / Status / Disconnect / Shutdown) and routes the
// chosen game-mode server type into the right network environment. It extends the
// platform NetworkAdapter (-> NetworkAdapterBase) and overrides Prepare / Update /
// Release / SetServerType; the base init runs first and the X360 adapter layers the
// connection-state machine on top.
//
// LAYOUT (X360 asm @ 0x8287F4D8 Construct / 0x8288BD90 Prepare / 0x8288BEC0 Update --
// the concrete-adapter members begin at +0x28, right after NetworkAdapterBase's 0x28-byte
// footprint):
//   +0x28  mpEnvironment       (stw r11,0x28 in StartupNetworking; read for NetConnConnect)
//   +0x2C  mePrepareState      (lwz/stw 0x2C; cmplwi 1 / cmplwi 3 state machine)
//   +0x30  mbConnecting        (stb/lbz 0x30 byte flag)
//   +0x34  miIdlePerfMon       (AddMonitor "AdaptorX360 - Idle")
//   +0x38  miConnectPerfMon    (AddMonitor "AdaptorX360 - Connect")
//   +0x3C  miDisconnectPerfMon (AddMonitor "AdaptorX360 - Disconnect")
//   +0x40  miCallBasePerfMon   (AddMonitor "AdaptorX360 - CallBase")
//
// The EPrepareState enum is X360-attested by the asm state machine (Prepare compares the
// state against 1 and 3, asserting "Unknown prepare state" at >= 3); its enumerators mirror
// the PS3 adapter's identical EPrepareState (DecFIGS CgsNetworkAdapterPS3.h:86).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h"

namespace CgsNetwork
{
    struct NetworkAdapterX360 : NetworkAdapter
    {
        // The X360 network-prepare state machine. Prepare advances INIT -> PREPARING ->
        // FULLY_PREPARED; >= COUNT asserts "Unknown prepare state". (X360 Prepare asm.)
        enum EPrepareState
        {
            E_PREPARE_INIT          = 0,
            E_PREPARING_NETADAPTER  = 1,
            E_FULLY_PREPARED        = 2,
            E_PREPARING_COUNT       = 3,
        };

        // @ 0x8287F4D8 -- zero the state, register the four perfmon counters, then chain
        // the base Construct. liParentPerfMon nests the adapter's monitors under a parent
        // in the perfmon tree (the 5th AddMonitor arg).
        void Construct(s32 liParentPerfMon);

        // @ 0x8288BD90 -- drive the prepare state machine; on the INIT/PREPARING edge it
        // runs the base prepare, brings networking up (StartupNetworking) and arms the
        // DirtySock connect timeout, landing in FULLY_PREPARED.
        virtual ENetworkStatus Prepare(NetworkAdapterPrepareParams* lpParams) override;

        // @ 0x8288BEC0 -- pump NetConnIdle, drive connect-when-ready, and poll the
        // connection status for a disconnect (bracketed by the four perfmon monitors).
        virtual void Update() override;

        // @ 0x8287F5B8 -- tear the connection down (disconnect + shutdown if ever started),
        // reset the adapter state, then chain the base release.
        virtual bool Release() override;

        // Select the network environment + re-run startup for a new server type. The X360
        // SetServerType forwards into StartupNetworking (dossier: StartupNetworking is called
        // by SetServerType). Declared-only here; bodied in the SetServerType TU.
        virtual void SetServerType(EServerType leServerType) override;

        // @ 0x8287F610 -- select the network environment for the current server type and
        // start DirtySock with the matching secure/insecure flags.
        void StartupNetworking();

        void* mpEnvironment;        // +0x28
        EPrepareState mePrepareState; // +0x2C
        bool  mbConnecting;         // +0x30
        u8    mPad31[3];            // +0x31  pad to word
        s32   miIdlePerfMon;        // +0x34
        s32   miConnectPerfMon;     // +0x38
        s32   miDisconnectPerfMon;  // +0x3C
        s32   miCallBasePerfMon;    // +0x40
    };
}
