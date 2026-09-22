#ifndef CGS_SERVER_INTERFACE_PING_REGIONS_H
#define CGS_SERVER_INTERFACE_PING_REGIONS_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// ===========================================================================
// CgsNetwork::ServerInterfacePingRegions
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfacePingRegions.{h,cpp}
//
// The DirtySock server-interface component that pings the candidate game-server regions and
// records each region's round-trip ping. Derives from CgsNetwork::ServerInterfaceComponent
// (its destructors -- this leaf's @0x827DE2C8 and the sibling ServerInterfaceHttp's
// @0x827DE1F0 -- both restore the shared component vtable off_820CDBF8 at this+0, the
// hallmark of a ServerInterfaceComponent leaf).
//
// LAYOUT (X360 asm @ 0x8287C8A8 Construct + @ 0x82877968 GetPingValue + @ 0x8287CC18
// HandlePingResults), after the 4-word Component base:
//     +0x00  (Component base: vptr / mpcCurrentAction / meStatus / miLastError)
//     +0x10  maiPingResults[KI_MAX_PING_REGIONS] -- per-region ping values; ctor fills -1
//     +0xD8  mpServerInterface  (Prepare stores its argument)
//     +0xDC  mpPingManagerRefT  (Prepare stores the PingManagerCreate result)
//     +0xE0  mpHostAddress      (the pending ProtoNameAsync lookup; HandlePingResults
//                                calls its free slot +0xC)
//     +0xE4  miCurrentRegion    (the count of regions filled so far)
//     +0xE8  miPingRequest      (ctor stores -1)
//     +0xEC  meState            (ctor stores 2 == E_STATE_COUNT, idle)
//     +0xF0  meCurrentAction    (ctor stores 1 == E_ACTION_COUNT, idle)
//
// KI_MAX_PING_REGIONS == 0x32 (50) is grounded in HandlePingResults
// (@ 0x8287CC18: `cmpwi miCurrentRegion, 0x32` -- the "miCurrentRegion < KI_MAX_PING_REGIONS"
// guard) and in Construct's 50-iteration -1 fill loop (mtctr 0x32). The non-ping trailing
// words keep raw-offset names (only the ctor's zero/sentinel stores reach them; their
// descriptive meaning is not present in the available exports) so the array + miCurrentRegion
// that THIS TU's functions touch land at the asm-observed offsets.
//
// This TU owns GetPingValue (@ 0x82877968) and the X360 scalar-deleting destructor
// (@ 0x827DE2C8). The component's behavioural methods (Construct / Prepare / Update /
// StartPingRegions / ...) are declared for layout fidelity but bodied in their own TUs.
// ===========================================================================

// DirtySDK handles (vendor SDK, global namespace like LobbyApiRefT).
struct LobbyApiRefT;
struct LobbyApiMsgT;
struct HostentT;

namespace CgsNetwork
{
    struct ServerInterfaceDirtySock;
    namespace DirtySock { struct PingManagerRefT; }

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

        // CgsServerInterfacePingRegions.h -- scalar/vector deleting destructor @ 0x827DE2C8.
        virtual ~ServerInterfacePingRegions();

        // @ 0x82877968 -- return the recorded ping for liRegion. Asserts
        // 0 <= liRegion < miCurrentRegion (both vacuous), then returns maRegionPings[liRegion].
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

    private:
        void EndAction(s32 liError);
        bool ResolveRegion(s32 liRegion);
        void HandlePingResults(s32 liRegion);
        // PingManager result callback: (host address, ping, user data = the component).
        static void PingManagerCallback(void* lpAddress, u32 luPing, void* lpUserData);

        s32                         maiPingResults[KI_MAX_PING_REGIONS];   // +0x10 (ctor fills -1)
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
