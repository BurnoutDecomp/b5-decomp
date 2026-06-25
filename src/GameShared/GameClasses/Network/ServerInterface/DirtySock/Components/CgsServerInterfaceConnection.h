#ifndef CGS_SERVER_INTERFACE_CONNECTION_H
#define CGS_SERVER_INTERFACE_CONNECTION_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceConnection
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceConnection.{h,cpp}
//
// The DirtySock "connection" server-interface component: it drives the lobby
// connect/login state machine and reports the current login step to the front
// end. Derives from CgsNetwork::ServerInterfaceComponent (DWARF
// CgsServerInterfaceConnection.h:64
// `ServerInterfaceConnection : public CgsNetwork::ServerInterfaceComponent`).
//
// Layout (CgsServerInterfaceConnection.h DWARF), after the 4-word Component base:
//     +0x10  mpServerInterface   (ServerInterfaceDirtySock*)
//     +0x14  meCurrentAction     (EAction)
//     +0x18  miConnectCallback   (s32)
//     +0x1C  mbConnecting        (bool)
//     +0x1D  mbConnected         (bool)
//
// This TU owns:
//   * GetLoginStatus (ledger "GetLogi") @ 0x82876F18  -- poll the lobby-login
//     module via mpServerInterface->GetLobbyLoginRef(); returns an ELoginStatus.
//   * the X360 vector-deleting destructor @ 0x827DE118 (stores the component
//     vtable off_820CDBF8 at this+0 and conditionally frees).
// The behavioural methods (Construct / Prepare / Update / ConnectToServer / ...)
// are declared for layout fidelity but bodied in their own TUs.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterfaceDirtySock;   // forward; pointer member only

    class ServerInterfaceConnection : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfaceConnection.h:68 -- the front-end login step. Mirrors the
        // DirtySDK LobbyLoginContextE 1:1; GetLoginStatus returns one of these.
        enum ELoginStatus
        {
            E_LOGIN_STATUS_INVALID              = -1,
            E_LOGIN_STATUS_OFFLINE              = 0,
            E_LOGIN_STATUS_CONNECT              = 1,
            E_LOGIN_STATUS_CREATE               = 2,
            E_LOGIN_STATUS_LOGIN                = 3,
            E_LOGIN_STATUS_DUPLICATE_ACCOUNT    = 4,
            E_LOGIN_STATUS_AGREE_TOS            = 5,
            E_LOGIN_STATUS_AGREE_SHARE_INFO     = 6,
            E_LOGIN_STATUS_ENTER_PARENTAL_EMAIL = 7,
            E_LOGIN_STATUS_INAVLID_REGISTRY_KEY = 8,
            E_LOGIN_STATUS_SELECT_PERSONA       = 9,
            E_LOGIN_STATUS_DEACTIVATED          = 10,
            E_LOGIN_STATUS_NUCLEUS_WELCOME      = 11,
            E_LOGIN_STATUS_LOGIN_DONE           = 12,
            E_LOGIN_STATUS_BUSY                 = 13,
            E_LOGIN_STATUS_NUM                  = 14,
        };

        // CgsServerInterfaceConnection.h:90 -- the active server-interface action.
        enum EAction
        {
            E_ACTION_CONNECTTOSERVER = 0,
            E_ACTION_ISSUESELECT     = 1,
            E_ACTION_LOGINTOSERVER   = 2,
            E_ACTION_AGREETOS        = 3,
            E_ACTION_AGREESHAREINFO  = 4,
            E_ACTION_COUNT           = 5,
        };

        ServerInterfaceConnection();

        // CgsServerInterfaceConnection.h:64 -- vector deleting destructor @ 0x827DE118.
        virtual ~ServerInterfaceConnection();

        // @ X360 0x82876F18 (DWARF GetLoginStatus, cpp:736). Reached from
        // BrnNetwork::LoginManagerBase::UpdateLoggingIn.
        ELoginStatus GetLoginStatus();
        bool IsLoggedIn() const;
        void DisconnectFromServer();

    private:
        ServerInterfaceDirtySock* mpServerInterface;   // +0x10
        EAction                   meCurrentAction;       // +0x14
        s32                       miConnectCallback;     // +0x18
        bool                      mbConnecting;          // +0x1C
        bool                      mbConnected;           // +0x1D
    };
}

#endif // CGS_SERVER_INTERFACE_CONNECTION_H
