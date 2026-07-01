#ifndef CGS_SERVER_INTERFACE_DIRTY_SOCK_H
#define CGS_SERVER_INTERFACE_DIRTY_SOCK_H

#include "types.hpp"

// FLAGGED opaque DirtySock SDK ref handles (vendor SDK; reached by pointer). These live
// in the GLOBAL namespace to match the DirtySDK vendor headers (vendor/dirtysdk/include/
// lobbyapi.h), so a TU that includes both this header and the SDK header sees one type.
struct LobbyApiRefT;
struct LobbyApiMsgT;

// ===========================================================================
// CgsNetwork::ServerInterfaceDirtySock
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/
//         CgsServerInterfaceDirtySock.{h,cpp}
//
// The DirtySock server-interface facade: the platform-shared base that owns the
// twelve server-interface components, the DirtySock SDK ref handles, and the
// shared 2 KB message buffer. The PC build derives ServerInterfaceDirtySockX360
// from this (see CgsServerInterfaceDirtySockX360.{h,cpp}); CgsNetwork::ServerInterface
// in turn derives from that.
//
// LAYOUT from dwarfdump (CgsServerInterfaceGameParams sibling dump) +
// CgsServerInterfaceDirtySock.h DWARF, with offsets confirmed against the X360
// ARTIST asm:
//   GetMessageBuffer @ 0x82580B48 reads mpacMessageBuffer at this+0x94 (=148) and
//   asserts it is non-null ("mpacMessageBuffer", CgsServerInterfaceDirtySock.h:719).
//
// This TU bodies only GetMessageBuffer and the vector deleting destructor; every
// other declared member is reconstructed in its own dossier. The DirtySock SDK
// ref types and the component classes are owned by OTHER groups / not-yet-homed
// TUs, so they are FLAGGED here as opaque incomplete types reached by pointer.
// Pinning is BY NAME; the X360 +148 offset for mpacMessageBuffer is a 32-bit-
// pointer layout fact and is intentionally NOT static_asserted (the host vptr +
// pointers widen to 8 bytes).
// ===========================================================================

namespace CgsNetwork
{
    // CgsServerInterfaceDirtySock.h:47
    const s32 KI_MESSAGE_BUFFER_SIZE = 2048;

    // Suspension update mask bits (CgsServerInterfaceDirtySock.h:209..216).
    const s32 KI_SUSPENSION_UPDATE_MASK_USERS    = 1;
    const s32 KI_SUSPENSION_UPDATE_MASK_GAMES    = 2;
    const s32 KI_SUSPENSION_UPDATE_MASK_MYGAME   = 4;
    const s32 KI_SUSPENSION_UPDATE_MASK_ROOMS    = 8;
    const s32 KI_SUSPENSION_UPDATE_MASK_MESSAGES = 16;
    const s32 KI_SUSPENSION_UPDATE_MASK_ASYNC    = 32;
    const s32 KI_SUSPENSION_UPDATE_MASK_USERSETS = 64;
    const s32 KI_SUSPENSION_UPDATE_MASK_STATS    = 128;

    // CgsServerInterfaceDirtySock.h:80
    enum EComponents
    {
        E_COMPONENTS_START              = 0,
        E_COMPONENTS_CONNECTION         = 0,
        E_COMPONENTS_GAMES              = 1,
        E_COMPONENTS_PLAYER_INFO        = 2,
        E_COMPONENTS_BROADCAST_MESSAGES = 3,
        E_COMPONENTS_HTTP               = 4,
        E_COMPONENTS_SERVERINFO         = 5,
        E_COMPONENTS_DOWNLOADABLE_CONFIG= 6,
        E_COMPONENTS_TELEMETRY          = 7,
        E_COMPONENTS_RANKINGS           = 8,
        E_COMPONENTS_CUSTOM_COMMANDS    = 9,
        E_COMPONENTS_USERSETS           = 10,
        E_COMPONENTS_PING_REGIONS       = 11,
        E_COMPONENTS_COUNT              = 12
    };

    // CgsServerInterfaceDirtySock.h:101
    enum EKickReason
    {
        E_KICKREASON_HOSTKICK       = 0,
        E_KICKREASON_NOGAME         = 1,
        E_KICKREASON_BANNED_BUDDY   = 2,
        E_KICKREASON_UNNATABLE      = 3,
        E_KICKREASON_LOST_CONNECTION= 4,
        E_KICKREASON_COUNT          = 5
    };

    // ---- FLAGGED opaque component classes (owned by other groups / not yet homed).
    class ServerInterfaceComponent;
    class ServerInterfaceConnection;
    class ServerInterfaceGames;
    class ServerInterfacePlayerInfo;
    class ServerInterfaceBroadcastMessages;
    class ServerInterfaceHttp;
    class ServerInterfaceServerInfo;
    class ServerInterfaceDownloadableConfig;
    class ServerInterfaceTelemetry;
    class ServerInterfaceRankings;
    class ServerInterfaceCustomCommands;
    class ServerInterfaceUsersets;
    class ServerInterfacePingRegions;

    enum EServerInterfaceEvent : int;
    enum EServerInterfaceError : int;

    // ---- FLAGGED: the DirtySock->game error mapping table type (CgsNetwork-owned;
    // home CgsServerInterfaceDirtySockErrorHelpers.h). LobbyApiRefT / LobbyApiMsgT are
    // vendor SDK types declared at global scope above.
    struct DSErrorToServerInterfaceError;

    namespace DirtySock
    {
        struct ConnApiRefT;
        struct ConnApiCbInfoT;
        struct LobbyLoginRefT;
        struct GameManagerRefT;
        struct LobbySettingRefT;
    }

    // CgsServerInterfaceDirtySock.h:153 -- ConnAPI prepare params (POD).
    struct ConnAPIPrepareParams
    {
        s32  miPort;
        s32  miMaxPlayers;
        bool mbPeerToPeer;

        void Construct();
    };

    // CgsServerInterfaceDirtySock.h:193 -- per-component slot.
    struct ServerInterfaceComponentData
    {
        // CgsServerInterfaceDirtySock.h:115
        typedef void (*ServerInterfaceConnApiCallback)(DirtySock::ConnApiCbInfoT*,
                                                       ServerInterfaceComponent*);

        ServerInterfaceComponent*     mpComponent;
        ServerInterfaceConnApiCallback mConnApiCallback;
    };

    struct ServerInterfaceDirtySock
    {
    public:
        // CgsServerInterfaceDirtySock.h:219
        enum EStatus
        {
            E_STATUS_BUSY  = 0,
            E_STATUS_ERROR = 1,
            E_STATUS_IDLE  = 2,
            E_STATUS_COUNT = 3
        };

        // CgsServerInterfaceDirtySock.h:120
        typedef void (*ConnectionStatusChangeCallback)(DirtySock::ConnApiRefT*,
                                                        DirtySock::ConnApiCbInfoT*,
                                                        void*);

        ServerInterfaceDirtySock();

        // CgsServerInterfaceDirtySock.h:229
        virtual ~ServerInterfaceDirtySock();

        // Lifecycle virtuals (bodies in their own dossiers).
        virtual void Construct();
        virtual bool Prepare(struct ServerInterfacePrepareParams* lpParams);
        virtual bool Release();
        virtual void Destruct();
        virtual void Update();
        virtual void Suspend(s32 liUpdateFlags);
        virtual void Resume();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);

        // CgsServerInterfaceDirtySock.h:717 -- THIS TU. Returns mpacMessageBuffer.
        char* GetMessageBuffer() const;
        // CgsServerInterfaceDirtySock.h:724
        s32 GetMessageBufferLength() const;

        // ADDITIVE GROW (flagged by cgs-scene-net group): inline accessor for the
        // lobby-login ref handle (the +0x7C member). ServerInterfaceConnection::GetLogi
        // (X360 0x82876F18) reads mpServerInterface->mpLobbyLoginRef to poll the login
        // module; the X360 component is a friend of the server interface and touches the
        // field directly. We expose a read accessor instead of widening friendship, so
        // the member stays private and the layout is unchanged.
        DirtySock::LobbyLoginRefT* GetLobbyLoginRef() const { return mpLobbyLoginRef; }

        // ADDITIVE GROW: read accessor for the DirtySock lobby API ref (the +0x78
        // member). The leaf components (e.g. ServerInterfaceDownloadableConfig::StartAction,
        // X360 0x8287AD88, reads *(mpServerInterface + 0x78)) drive lobby requests through
        // this handle; exposed by name rather than widening friendship, leaving the member
        // private and the layout unchanged.
        LobbyApiRefT* GetLobbyAPIRef() const { return mpLobbyAPIRef; }

        // ADDITIVE GROW: read accessor for the DirtySock lobby-settings ref (the +0x88
        // member). ServerInterfacePlayerInfo's account-settings actions (EditAccount /
        // GetAccountSettings / LoadSettings, X360 0x82879458 / 0x82879528 / 0x82879638)
        // drive LobbySettingSetNumber/GetNumber/Save/Load through this handle
        // (*(mpServerInterface + 0x88)). Exposed by name rather than widening friendship,
        // leaving the member private and the layout unchanged.
        DirtySock::LobbySettingRefT* GetSettingRef() const { return mpSettingRef; }

        // ADDITIVE GROW (flagged by the ServerInterfaceGames group): read accessors for
        // the ConnAPI (+0x80) and GameManager (+0x84) DirtySDK handles, and the SKU
        // string. ServerInterfaceGames drives game create/join/connection-type/result
        // actions through these handles (e.g. SetGameServerConnectionType reads
        // *(mpServerInterface + 0x80) / +0x84; SendGameResult reads the SKU). Exposed by
        // name rather than widening friendship, leaving the members private and the
        // layout unchanged.
        DirtySock::ConnApiRefT*     GetConnAPIRef() const     { return mpConnApiRef; }
        DirtySock::GameManagerRefT* GetGameManagerRef() const { return mpGameManagerRef; }
        const char*                 GetSKU() const            { return mpcSKU; }

        // ADDITIVE GROW (flagged by the BrnNetworkScoreboardManager group): the rankings
        // component slot accessor. BrnNetwork::ScoreboardManager::Prepare (X360 0x825471B0)
        // reads mpServerInterface->maComponents[E_COMPONENTS_RANKINGS].mpComponent
        // (the +0x44 slot) and stores it as its mpRankings back-pointer. Exposed as a named
        // accessor that returns the shared ServerInterfaceComponent base (the caller
        // reinterpret_casts to the ServerInterfaceRankings leaf -- the same documented
        // base-pointer component-accessor idiom used by GetDownloadableConfigComponent et al.
        // in CgsServerInterface.h), keeping maComponents private and the layout unchanged.
        ServerInterfaceComponent* GetRankingsComponent() const
        {
            return maComponents[E_COMPONENTS_RANKINGS].mpComponent;
        }

        // ADDITIVE GROW (flagged by the CgsNetworkAdapterX360 group): the connection
        // component slot accessor (E_COMPONENTS_CONNECTION == slot 0, this+0x04 on the
        // X360 32-bit layout). CgsNetwork::NetworkAdapterX360::Update (X360 0x8288BEC0)
        // reads *(mpServerInterface + 4) to reach the ServerInterfaceConnection component
        // before walking into it for the DirtySock lobby-disconnect handle. Exposed as a
        // named accessor (same base-pointer component-accessor idiom as
        // GetRankingsComponent/GetGameComponent above); callers reinterpret_cast the shared
        // ServerInterfaceComponent base to the concrete ServerInterfaceConnection leaf.
        // maComponents stays private; layout unchanged.
        ServerInterfaceComponent* GetConnectionComponent() const
        {
            return maComponents[E_COMPONENTS_CONNECTION].mpComponent;
        }

        // ADDITIVE GROW (flagged by the ServerInterfaceGames group): the ping-regions
        // component slot accessor + the games component's ConnAPI-callback registration.
        // ServerInterfaceGames::CreateGame reads the ping-regions component to append the
        // region-preference list; ServerInterfaceGames::Prepare registers its per-game
        // ConnAPI callback into the games component slot (the X360 stores the callback into
        // maComponents[E_COMPONENTS_GAMES].mConnApiCallback after asserting the component is
        // registered). Exposed by name; layout unchanged.
        ServerInterfaceComponent* GetPingRegionsComponent() const
        {
            return maComponents[E_COMPONENTS_PING_REGIONS].mpComponent;
        }

        // ADDITIVE GROW (flagged by the BrnNetworkConnectionManager group): the games and
        // usersets component slot accessors. The NAT-kick policy reads both
        // (maComponents[E_COMPONENTS_GAMES].mpComponent / [E_COMPONENTS_USERSETS].mpComponent --
        // X360 KickUnNATablePlayer @ 0x82566860 reads *(this+0x38F4) and *(this+0x393C), i.e.
        // the +1 and +10 slots) to drive the kick through whichever component the local player
        // is in. Exposed by name (the same base-pointer component-accessor idiom as
        // GetRankingsComponent/GetPingRegionsComponent); callers reinterpret_cast the shared
        // ServerInterfaceComponent base to the concrete leaf. maComponents stays private; layout
        // unchanged.
        ServerInterfaceComponent* GetGameComponent() const
        {
            return maComponents[E_COMPONENTS_GAMES].mpComponent;
        }
        ServerInterfaceComponent* GetUsersetsComponent() const
        {
            return maComponents[E_COMPONENTS_USERSETS].mpComponent;
        }

        // ADDITIVE GROW (flagged by the BrnNetworkConnectionManager group): the per-component
        // status query. Returns the EStatus (BUSY/ERROR/IDLE) of the component at slot
        // liComponent (an EComponents value). X360 callers gate kicks on it being non-busy
        // (e.g. KickUnNATablePlayer / UpdateNATData call GetStatus(E_COMPONENTS_GAMES) /
        // GetStatus(E_COMPONENTS_USERSETS)). Declared-only here; the body belongs to this
        // facade's own behavioural TU.
        EStatus GetStatus(s32 liComponent) const;
        bool IsGameComponentRegistered() const
        {
            return maComponents[E_COMPONENTS_GAMES].mpComponent != 0;
        }

        // ADDITIVE GROW (BrnNetworkLoginManagerBase TU): the DirtySock server-interface memory
        // helpers. BrnNetwork::LoginManagerBase::UpdateDownloadingTOS @ 0x82566320 / ::Disconnected
        // @ 0x82543D18 allocate and free the cached terms-of-service buffer through these
        // (X360 calls MemAlloc(size, 0, 0) and MemFree(ptr, 0, 0) -- the two trailing zero args are
        // an alignment / pool selector). They are static helpers (the X360 call sites pass no
        // `this`). Declared-only here; the bodies live in this facade's own behavioural TU.
        static void* MemAlloc(s32 liSize, s32 liAlignment, s32 liPool);
        static void  MemFree(void* lpBlock, s32 liAlignment, s32 liPool);
        void SetConnApiGameCallback(ServerInterfaceComponentData::ServerInterfaceConnApiCallback lpfnCallback)
        {
            maComponents[E_COMPONENTS_GAMES].mConnApiCallback = lpfnCallback;
        }

    protected:
        // ConvertError: map a DirtySock error to an EServerInterfaceError via the table.
        virtual EServerInterfaceError ConvertError(int liError,
                                                   const DSErrorToServerInterfaceError* lpTable,
                                                   int liCount) const;

    private:
        // ---- Data members, DWARF order (CgsServerInterfaceDirtySock.h). The vptr
        // sits at +0 (this struct's first non-static data). On the X360 build
        // mpacMessageBuffer lands at +148; not asserted (see header note).
        ServerInterfaceComponentData maComponents[E_COMPONENTS_COUNT];  // :473
        ConnAPIPrepareParams         mConnApiPrepareParams;             // :474
        ConnectionStatusChangeCallback mpfConnectionStatusChangeCallback; // :477
        void*                        mpConnectionStatusChangeUserData;  // :478
        LobbyApiRefT*                mpLobbyAPIRef;                      // :481
        DirtySock::LobbyLoginRefT*   mpLobbyLoginRef;                   // :482
        DirtySock::ConnApiRefT*      mpConnApiRef;                      // :483
        DirtySock::GameManagerRefT*  mpGameManagerRef;                  // :484
        DirtySock::LobbySettingRefT* mpSettingRef;                      // :485
        bool                         mbWaitingToSuspend;                // :487
        s32                          miSuspendUpdateFlags;              // :488
        char*                        mpacMessageBuffer;                 // :490 (this+148 on X360)
        // mpMemoryBlock (CgsMemory::HeapMalloc*):
        // FLAGGED opaque; modelled as void* to avoid pulling an un-homed allocator.
        void*                        mpMemoryBlock;
        const char*                  mpcCurrentAction;                  // :496
        EStatus                      meStatus;                          // :497
        s32                          miLastError;                       // :498
        const char*                  mpcStandardSelectOptions;          // :500
        const char*                  mpcVersion;                        // :504
        const char*                  mpcSKU;                            // :505
        const char*                  mpcSLUS;                           // :506
        s32                          miLanguage;                        // :507
        bool                         mbRecreateDirtySock;               // :509
        // mNetStreamLogChannelOutput
        // (CgsDev::Log::LogChannelOutput): FLAGGED opaque; modelled by storage so the
        // trailing member offset (miCgsNetworkServerInterfacePM1) is preserved. Two
        // pointers wide is sufficient for the named-member contract on this TU.
        void*                        maNetStreamLogChannelOutput[2];
        s32                          miCgsNetworkServerInterfacePM1;    // :517
    };
}

#endif // CGS_SERVER_INTERFACE_DIRTY_SOCK_H
