#ifndef CGS_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_H
#define CGS_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceDownloadableConfig
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceDownloadableConfig.{h,cpp}
//
// The DirtySock server-interface component that downloads a "configuration" record
// (a tagfield text blob) from the lobby's game-news channel and parses named fields
// out of it into caller-supplied memory addresses. Derives from
// CgsNetwork::ServerInterfaceComponent (Feb-2007 source +
// CgsServerInterfaceDownloadableConfig.h DWARF:
//   `ServerInterfaceDownloadableConfig : public CgsNetwork::ServerInterfaceComponent`).
//
// LAYOUT (X360 ARTIST asm), after the 4-word Component base (vptr / mpcCurrentAction /
// meStatus / miLastError):
//   +0x10  mpServerInterface     (ServerInterfaceDirtySock*)  Prepare a1[4]=a2
//   +0x14  mpaDataIDToMemAddrs   (DataIDToMemoryAddr*)        the parse table base (a1[5])
//   +0x18  meCurrentAction       (EAction)                    Construct/Prepare/EndAction = 1
//   +0x1C  miMemAddrTableSize    (s32)                        parse-table entry count (a1[7])
//   +0x20  miRequestedNewsIndex  (s32)                        requested news index (a1[8])
// (offsets read off Construct @0x8287ACC0 / Prepare @0x8287AD08 /
//  GetConfigurationDataFromNews @0x828916C8 / ParseConfigFile @0x8287AE78.)
//
// The 12-byte DataIDToMemoryAddr describes one named field: where it lives (an offset
// relative to a destination object) and how to interpret it (EDataTypes). ParseConfigFile
// strides the table by 12 bytes and switches on meDataType.
// ===========================================================================

namespace CgsNetwork
{
    // Forward-declared DirtySock facade; the component reaches the lobby ref and the
    // shared message buffer through it (see CgsServerInterfaceDirtySock.h).
    struct ServerInterfaceDirtySock;
}

// DirtySDK lobby request types used by the request/callback path.
struct LobbyApiRefT;
struct LobbyApiMsgT;
typedef void (LobbyApiCallbackT)(LobbyApiRefT* pLobbyApi, LobbyApiMsgT* pMsg, void* pUserData);

namespace CgsNetwork
{
    // CgsServerInterfaceDownloadableConfig.h:50 -- how a config field is interpreted.
    enum EDataTypes
    {
        E_TYPE_INTEGER = 0,
        E_TYPE_STRING  = 1,
        E_TYPE_FLOAT   = 2,
        E_TYPE_BOOL    = 3,
        E_TYPE_COUNT   = 4,
    };

    // CgsServerInterfaceDownloadableConfig.h:61 -- a single "field name -> destination
    // member" descriptor. 12 bytes; ParseConfigFile reads { name@+0, type@+4, offset@+8 }.
    class DataIDToMemoryAddr
    {
    public:
        DataIDToMemoryAddr() {}

        DataIDToMemoryAddr(char* lpacName, EDataTypes leDataType, int32_t liOffset)
            : mpacName(lpacName)
            , meDataType(leDataType)
            , miOffset(liOffset)
        {
        }

        char*      mpacName;     // +0x00  field name in the tagfield record
        EDataTypes meDataType;   // +0x04
        int32_t    miOffset;     // +0x08  byte offset into the destination object
    };

    class ServerInterfaceDownloadableConfig : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfaceDownloadableConfig.h:92
        enum EAction
        {
            E_ACTION_DOWNLOADING_GAME_NEWS = 0,
            E_ACTION_COUNT                 = 1,
        };

        ServerInterfaceDownloadableConfig();

        // CgsServerInterfaceDownloadableConfig.h -- vector deleting destructor.
        virtual ~ServerInterfaceDownloadableConfig();

        // ---- Lifecycle (vtable; bodies in the .cpp) --------------------------------
        virtual void Construct();
        virtual void Destruct();
        virtual bool Prepare(ServerInterfaceDirtySock* lpServerInterface);
        virtual bool Release();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);

        // Suspend / Resume reset the in-flight action (non-virtual in this build).
        void Suspend();
        void Resume();

        // ---- Public API ------------------------------------------------------------
        // Kick off a game-news config download for liRequestIndex, parsing the named
        // fields described by lpaDataIDToMemAddrs[liSize] when it completes.
        void GetConfigurationDataFromNews(int32_t liRequestIndex,
                                          DataIDToMemoryAddr* lpaDataIDToMemAddrs,
                                          int32_t liSize);

        // Parse the already-downloaded client config record (no network round-trip).
        void GetConfigurationDataFromConf(DataIDToMemoryAddr* lpaDataIDToMemAddrs,
                                          int32_t liSize);

    private:
        // Map a DirtySock error code to an EServerInterfaceError for the given action.
        EServerInterfaceError ConvertError(int32_t liErrorCode, EAction leAction);

        // Begin the action: assemble the request and fire it with lpCallback.
        void StartAction(EAction leAction, LobbyApiCallbackT* lpCallback);

        // Finish the action: convert + record liErrorCode and return to idle.
        void EndAction(int32_t liErrorCode);

        // LobbyApiRequestCB completion for the game-news download.
        static void GetGameNewsCallback(LobbyApiRefT* lpApiRef, LobbyApiMsgT* lpMsg,
                                        void* lpData);

        // Parse one downloaded config record into the registered memory addresses.
        void ParseConfigFile(const char* lpacConfigData);

        // --- Static lookup tables (homed in the .cpp) -------------------------------
        static const int                            KAI_ACTION_CODE_MAPPING[E_ACTION_COUNT];
        static const char*                          KAPC_ACTION_NAMES[E_ACTION_COUNT];
        static const struct DSErrorToServerInterfaceErrorTable KA_DS_ERROR_TABLE_LOOKUP[E_ACTION_COUNT];

        // --- Data members (after the ServerInterfaceComponent base) -----------------
        ServerInterfaceDirtySock* mpServerInterface;     // +0x10
        DataIDToMemoryAddr*       mpaDataIDToMemAddrs;    // +0x14
        EAction                   meCurrentAction;        // +0x18
        int32_t                   miMemAddrTableSize;     // +0x1C
        int32_t                   miRequestedNewsIndex;   // +0x20
    };
}

#endif // CGS_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_H
