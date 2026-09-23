#ifndef CGS_SERVER_INTERFACE_PING_REGIONS_H
#define CGS_SERVER_INTERFACE_PING_REGIONS_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"
#include "pingmanager.h"   // PingManagerRefT, the PingManager* C API
#include "protoname.h"     // HostentT, ProtoNameAsync

// ===========================================================================
// CgsNetwork::ServerInterfacePingRegions
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfacePingRegions.{h,cpp}
//
// The DirtySock server-interface component that pings the candidate game-server regions and
// records each region's round-trip ping. StartPingRegions resolves the first region host
// named in the lobby's "GPS_REGIONS" config record; Update waits for the name lookup, then
// pings the resolved address through the DirtySDK ping manager; every result (or failure,
// recorded as -1) moves on to the next region until the list runs out.
//
// LAYOUT, after the 4-word Component base:
//     +0x10  maiPingResults[KI_MAX_PING_REGIONS] -- per-region ping values (-1 = none)
//     +0xD8  mpServerInterface
//     +0xDC  mpPingManagerRefT
//     +0xE0  mpHostAddress      (the pending name lookup)
//     +0xE4  miCurrentRegion    (the count of regions filled so far)
//     +0xE8  miPingRequest      (the outstanding ping request, -1 = none)
//     +0xEC  meState
//     +0xF0  meCurrentAction
// ===========================================================================

// DirtySDK handles (vendor SDK, global namespace like LobbyApiRefT).
struct LobbyApiRefT;
struct LobbyApiMsgT;

namespace CgsNetwork
{
    struct ServerInterfaceDirtySock;
    struct DSErrorToServerInterfaceErrorTable;
    namespace DirtySock { using ::PingManagerRefT; }

    class ServerInterfacePingRegions : public ServerInterfaceComponent
    {
    public:
        enum EAction
        {
            E_ACTION_PING_REGIONS = 0,
            E_ACTION_COUNT        = 1,
        };

        enum EState
        {
            E_STATE_RESOLVING = 0,
            E_STATE_PINGING   = 1,
            E_STATE_COUNT     = 2,
        };

        // CgsServerInterfacePingRegions.h -- max number of ping regions (asm literal 0x32).
        static const s32 KI_MAX_PING_REGIONS = 50;

        ServerInterfacePingRegions();

        virtual ~ServerInterfacePingRegions();

        // The recorded ping for liRegion (0 <= liRegion < miCurrentRegion).
        s32 GetPingValue(s32 liRegion) const;

        // ADDITIVE GROW (flagged by the ServerInterfaceGames group): the recorded ping-region
        // count (miCurrentRegion, +0xE4). ServerInterfaceGames::CreateGame iterates it to build
        // the "REGIONS" preference list. Exposed by name; layout unchanged.
        s32 GetNumberOfPingRegions() const { return miCurrentRegion; }

        // ---- ADDITIVE GROW (BrnNetworkLoginManagerBase TU) --------------------------------
        // Begin pinging the region servers (X360: LoginManagerBase::PreparePingRegions @
        // 0x82543E08 reaches this component through *(mpNetworkManager+0x38EC) + ping-regions slot
        // and calls StartPingRegions once the DirtySock interface reports idle). Declared-only
        // here; the body lives in this component's own (DirtySock) TU.
        void StartPingRegions();
        void StopPingRegions();

        // --- component overrides and lifecycle ---
        virtual void Construct();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);
        void Destruct();
        bool Prepare(ServerInterfaceDirtySock* lpServerInterface);
        bool Release();
        void Update();
        void Suspend();
        void Resume();

    private:
        void EndAction(s32 liError);
        bool ResolveRegion(s32 liRegion);
        void HandlePingResults(s32 liPing);
        // PingManager result callback: (host address, ping, user data = the component).
        static void PingManagerCallback(void* lpAddress, u32 luPing, void* lpUserData);

        static const DSErrorToServerInterfaceErrorTable KA_DS_ERROR_TABLE_LOOKUP[E_ACTION_COUNT];
        static const char* KAPC_ACTION_NAMES[E_ACTION_COUNT];

        s32                         maiPingResults[KI_MAX_PING_REGIONS];   // +0x10
        ServerInterfaceDirtySock*   mpServerInterface;                     // +0xD8
        DirtySock::PingManagerRefT* mpPingManagerRefT;                     // +0xDC
        HostentT*                   mpHostAddress;                         // +0xE0
        s32                         miCurrentRegion;                       // +0xE4
        s32                         miPingRequest;                         // +0xE8
        EState                      meState;                               // +0xEC
        EAction                     meCurrentAction;                       // +0xF0
    };
}

#endif // CGS_SERVER_INTERFACE_PING_REGIONS_H
