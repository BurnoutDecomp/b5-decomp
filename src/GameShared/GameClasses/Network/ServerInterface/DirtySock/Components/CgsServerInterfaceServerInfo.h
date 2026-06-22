#ifndef CGS_SERVER_INTERFACE_SERVER_INFO_H
#define CGS_SERVER_INTERFACE_SERVER_INFO_H

#include "types.hpp"

struct LobbyApiRefT;   // DirtySDK opaque lobby handle (vendor/dirtysdk/include/lobbyapi.h)

// ===========================================================================
// CgsNetwork::ServerInterfaceServerInfo
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceServerInfo.{h,cpp}
//
// The server-info component: queries the lobby's "client config" tagfield records
// for URLs (TOS, news, telemetry auth) and per-key config strings, and tracks news
// freshness. Derives from CgsNetwork::ServerInterfaceComponent.
//
// ServerInterfaceComponent has no committed standalone header (it is defined inline
// in CgsServerInterfaceComponent.cpp, another component TU). To stay member-by-name
// without forking that type, the component's documented data layout is modelled here
// as the leading members (matching the X360 ServerInterfaceComponent offsets used by
// Construct/Prepare/Release):
//   +0x00  vptr
//   +0x04  mpErrorData    (const void*)  static error-string table pointer
//   +0x08  miStatus       (s32)          2 == "no error"
//   +0x0C  miLastError    (s32)
// then this component's own members:
//   +0x10  mpServerInterface  (ServerInterfaceDirtySock*)
//   +0x14  mpGameNewsCallback (CgsLobbyGameNewsCallback)
//   +0x18  mpGameNewsData     (void*)
//   +0x1C  meCurrentAction    (EAction)
//   +0x20  miRequestedNewsIndex (s32)
// (offsets verified against Construct @0x8287A510 / Prepare @0x8287A558 /
//  Release @0x8287A580 / Destruct @0x8287A540.)
//
// mpServerInterface->GetLobbyAPIRef() sits at offset +0x78 of the DirtySock
// interface (the asm reads *(mpServerInterface + 0x78)); it is exposed by name.
// ===========================================================================

namespace CgsNetwork
{
    // Forward-declared DirtySock interface; only its GetLobbyAPIRef() accessor is used.
    class ServerInterfaceDirtySock
    {
    public:
        LobbyApiRefT* GetLobbyAPIRef() const;
    };

    class ServerInterfaceServerInfo
    {
    public:
        // CgsServerInterfaceServerInfo.h:53
        typedef bool (*CgsLobbyGameNewsCallback)(bool, const char*, void*);

        // CgsServerInterfaceServerInfo.h:77
        enum EAction
        {
            E_ACTION_GET_GAME_NEWS = 0,
            E_ACTION_COUNT         = 1,
        };

        // CgsServerInterfaceServerInfo.h:75
        static const s32 KI_URL_LENGTH = 256;

        ServerInterfaceServerInfo();
        // Establishes the vtable slot at offset 0 (the X360 type is polymorphic;
        // Construct/OnEvent/dtor are virtual). The data members below therefore
        // start at +0x04, matching the asm.
        virtual ~ServerInterfaceServerInfo();

        // Lifecycle (CgsServerInterfaceServerInfo.cpp).
        virtual void Construct();
        void Destruct();
        bool Prepare(ServerInterfaceDirtySock* lpServerInterface);
        bool Release();

        // Queries.
        void GetTosUrl(char* lpcOut, s32 liOutLen);
        void GetTelemetryAuthString(char* lpcOut, s32 liOutLen);
        void GetStringFromClientConfig(const char* lpcKey, char* lpcOut, s32 liOutLen);
        bool IsNewsUpdated() const;

    private:
        // CgsServerInterfaceServerInfo.cpp:353 -- format <urlKey>'s record into lpcOut,
        // substituting the language code.
        void FindUrl(const char* lpcUrlKey, char* lpcOut, s32 liOutLen);

        // --- ServerInterfaceComponent base layout (see header note) ---
        const void* mpErrorData;        // +0x04
        s32         miStatus;           // +0x08
        s32         miLastError;        // +0x0C
        // --- own members ---
        ServerInterfaceDirtySock* mpServerInterface;   // +0x10
        CgsLobbyGameNewsCallback  mpGameNewsCallback;   // +0x14
        void*                     mpGameNewsData;        // +0x18
        EAction                   meCurrentAction;       // +0x1C
        s32                       miRequestedNewsIndex;  // +0x20
    };
}

#endif // CGS_SERVER_INTERFACE_SERVER_INFO_H
