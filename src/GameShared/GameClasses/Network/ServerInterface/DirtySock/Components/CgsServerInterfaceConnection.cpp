#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"  // ServerInterfaceDirtySock::GetLobbyLoginRef
#include "lobbylogin.h"   // DirtySDK LobbyLoginGetContext / LobbyLoginGetStatus
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "lobbyapi.h"       // LobbyApiStatus / LobbyApiInfo / LobbyApiRequestCB / LobbyApiSetCallback / ...
#include "lobbytagfield.h"  // TagFieldFind / TagFieldGetEpoch / TagFieldSetString / TagFieldSetNumber
#include "netconn.h"       // NetConnStatus

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceConnection::GetLoginStatus               @ 0x82876F18
//   CgsNetwork::ServerInterfaceConnection::~ServerInterfaceConnection   @ 0x827DE118
//
// GetLoginStatus polls the lobby-login module the connection owns through its
// server interface (mpServerInterface->GetLobbyLoginRef(), the +0x7C ref):
//   * no login ref           -> E_LOGIN_STATUS_NUM (14)
//   * LobbyLoginGetStatus != 0 (step still mid-flight) -> E_LOGIN_STATUS_BUSY (13)
//   * otherwise              -> the LobbyLoginGetContext() value cast to ELoginStatus
// The DirtySDK LobbyLoginContextE enum maps 1:1 onto ELoginStatus, so the context
// is returned verbatim.

namespace CgsNetwork
{

ServerInterfaceConnection::ELoginStatus ServerInterfaceConnection::GetLoginStatus()
{
    DirtySock::LobbyLoginRefT* lpLoginRef = mpServerInterface->GetLobbyLoginRef();
    if (lpLoginRef == 0)
        return E_LOGIN_STATUS_NUM;

    // The DirtySock LobbyLoginRefT and the SDK ::LobbyLoginRefT are the same opaque
    // login-module handle; bridge across the namespace-forward-decl boundary.
    LobbyLoginRefT* lpSdkRef = reinterpret_cast<LobbyLoginRefT*>(lpLoginRef);

    const s32 liContext = LobbyLoginGetContext(lpSdkRef);
    if (LobbyLoginGetStatus(lpSdkRef, liContext) != 0)
        return E_LOGIN_STATUS_BUSY;

    return static_cast<ELoginStatus>(liContext);
}

// Out-of-line destructor: the X360 vector-deleting destructor restores the
// component vtable at this+0 then conditionally frees (the `operator delete`
// branch lives in the compiler-synthesised deleting thunk). Defining the dtor
// here forces the vtable emission for this leaf component.
ServerInterfaceConnection::~ServerInterfaceConnection()
{
}

// ---------------------------------------------------------------------------
// The login state machine. The "DirtySock: ..." progress lines ConnectToServer /
// LogInToServer stream to the network log channel are not reproduced: that stream
// object has no home in the tree.
// ---------------------------------------------------------------------------

namespace
{
    // The per-action display names handed to StartActionCore.
    const char* const KAPC_CONNECTION_ACTION_NAMES[ServerInterfaceConnection::E_ACTION_COUNT] =
    {
        "Connect To Server",
        "Issue select",
        "Log in To Server",
        "Agreeing to TOS",
        "Agreeing to share info"
    };

    // Lobby-login alert -> EServerInterfaceError (ConvertError). A zero alert never
    // reaches the table.
    const DSErrorToServerInterfaceError KA_LOGIN_ALERT_ERROR_MAPPINGS[16] =
    {
        { 0,  E_SERVER_INTERFACE_CONNECTION_ERROR_GENERAL },
        { 1,  E_SERVER_INTERFACE_CONNECTION_ERROR_DATABASE_ERROR },
        { 2,  E_SERVER_INTERFACE_CONNECTION_ERROR_FILTER },
        { 3,  E_SERVER_INTERFACE_CONNECTION_ERROR_INVALID_MASTER },
        { 4,  E_SERVER_INTERFACE_CONNECTION_ERROR_MISSING_PARAMETERS },
        { 5,  E_SERVER_INTERFACE_CONNECTION_ERROR_ALREADY_LOGGED_IN },
        { 6,  E_SERVER_INTERFACE_CONNECTION_ERROR_LOCKED },
        { 10, E_SERVER_INTERFACE_CONNECTION_ERROR_RESERVED },
        { 11, E_SERVER_INTERFACE_CONNECTION_ERROR_CONNECTION_TIMEOUT },
        { 12, E_SERVER_INTERFACE_CONNECTION_ERROR_SERVER_DOWN },
        { 13, E_SERVER_INTERFACE_CONNECTION_ERROR_DISCONNECT },
        { 14, E_SERVER_INTERFACE_CONNECTION_ERROR_CHEAT_DEVICE },
        { 15, E_SERVER_INTERFACE_CONNECTION_ERROR_DASHBOARD },
        { 16, E_SERVER_INTERFACE_CONNECTION_ERROR_PERSONA_SET },
        { 17, E_SERVER_INTERFACE_CONNECTION_ERROR_INVALID_PASSWORD },
        { 18, E_SERVER_INTERFACE_CONNECTION_ERROR_INVALID_REGKEY },
    };

    // The 'sele' action's DirtySock-error mapping (the only action finished through
    // EndAction): a single entry mapping 0 to no error.
    const DSErrorToServerInterfaceError KA_ISSUE_SELECT_DS_ERROR_MAPPINGS[1] =
    {
        { 0, E_SERVER_INTERFACE_ERROR_NONE }
    };

    // Per-action { mapping table, count } used by EndAction.
    const DSErrorToServerInterfaceErrorTable
        KA_CONNECTION_DS_ERROR_TABLE_LOOKUP[ServerInterfaceConnection::E_ACTION_COUNT] =
    {
        { 0, 0 },
        { KA_ISSUE_SELECT_DS_ERROR_MAPPINGS, 1 },
        { 0, 0 },
        { 0, 0 },
        { 0, 0 }
    };
}

void ServerInterfaceConnection::Construct()
{
    ServerInterfaceComponent::Construct();
    mpServerInterface = 0;
    meCurrentAction   = E_ACTION_COUNT;
    miConnectCallback = -1;
    mbConnecting      = false;
    mbConnected       = false;
}

void ServerInterfaceConnection::Destruct()
{
    mpServerInterface = 0;
    meCurrentAction   = E_ACTION_COUNT;
    miConnectCallback = -1;
    mbConnecting      = false;
    mbConnected       = false;
}

bool ServerInterfaceConnection::Prepare(ServerInterfaceDirtySock* lpServerInterface)
{
    CGS_ASSERT(miConnectCallback == -1, "miConnectCallback == -1");

    mpServerInterface = lpServerInterface;
    meCurrentAction   = E_ACTION_COUNT;
    mbConnecting      = false;
    miConnectCallback = LobbyApiSetCallback(lpServerInterface->GetLobbyAPIRef(), 3,
                                            &ServerInterfaceConnection::ConnStatusCallback, this);
    return true;
}

bool ServerInterfaceConnection::Release()
{
    if (miConnectCallback > -1)
    {
        LobbyApiClearCallback(mpServerInterface->GetLobbyAPIRef(), miConnectCallback);
        miConnectCallback = -1;
    }
    mpServerInterface = 0;
    meCurrentAction   = E_ACTION_COUNT;
    mbConnecting      = false;
    return true;
}

// While connecting, poll the lobby-login module: a finished login brings the lobby
// online, dropping back offline reports a failed connect, and an alert ends the
// current action with the mapped error.
void ServerInterfaceConnection::Update()
{
    if (!mbConnecting)
    {
        return;
    }

    LobbyLoginRefT* lpLoginRef = reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef());
    const s32 liContext = LobbyLoginGetContext(lpLoginRef);
    const s32 liStatus  = LobbyLoginGetStatus(lpLoginRef, liContext);
    if (liStatus == LOBBYLOGIN_STATUS_IDLE)
    {
        if (liContext == LOBBYLOGIN_CONTEXT_DONE)
        {
            mpServerInterface->OnEvent(E_SERVER_INTERFACE_CONNECTION_EVENT_CONNECTED, 0);
            mbConnecting = false;
            mbConnected  = true;
        }
        else if (liContext == LOBBYLOGIN_CONTEXT_OFFLINE)
        {
            mpServerInterface->OnEvent(E_SERVER_INTERFACE_CONNECTION_EVENT_CONNECT_FAILED, 0);
            mbConnecting = false;
        }
    }
    else if (liStatus == LOBBYLOGIN_STATUS_ALERT)
    {
        const LobbyLoginAlertT* lpAlert = LobbyLoginAlert(
            reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()), liContext);
        const s32 liAlertEnum = LobbyLoginGetAlertEnum(
            reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()), lpAlert);
        EndActionCore(ConvertError(liAlertEnum, meCurrentAction));
    }
}

void ServerInterfaceConnection::Suspend()
{
}

void ServerInterfaceConnection::Resume()
{
}

void ServerInterfaceConnection::OnEvent(EServerInterfaceEvent leEvent, void* /*lpData*/)
{
    if (leEvent == E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_CREATED)
    {
        if (miConnectCallback == -1)
        {
            miConnectCallback = LobbyApiSetCallback(mpServerInterface->GetLobbyAPIRef(), 3,
                                                    &ServerInterfaceConnection::ConnStatusCallback, this);
        }
    }
    else if (leEvent == E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_DESTROYING)
    {
        if (miConnectCallback > -1)
        {
            LobbyApiClearCallback(mpServerInterface->GetLobbyAPIRef(), miConnectCallback);
            miConnectCallback = -1;
        }
    }
    else if (leEvent == E_SERVER_INTERFACE_CONNECTION_EVENT_DISCONNECTED)
    {
        meCurrentAction = E_ACTION_COUNT;
        mbConnecting    = false;
    }
}

bool ServerInterfaceConnection::IsLoggedIn() const
{
    if (LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), 0x6C6F676E /* 'logn' */, 0, 0) != 1)
    {
        return false;
    }
    return LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), 0x73746174 /* 'stat' */, 0, 0)
           != 0x6F66666C /* 'offl' */;
}

bool ServerInterfaceConnection::IsFirstLogin() const
{
    const char* lpcRecord =
        static_cast<const char*>(LobbyApiInfo(mpServerInterface->GetLobbyAPIRef(), 0x70657264 /* 'perd' */));
    return TagFieldGetEpoch(TagFieldFind(lpcRecord, "LAST"), 1) == 1;
}

bool ServerInterfaceConnection::IsConnectedToNetworkService() const
{
    return NetConnStatus(0x6F6E6C6E /* 'onln' */, 0, 0, 0) == 1;
}

void ServerInterfaceConnection::DisconnectFromServer()
{
    LobbyApiDisconnect(mpServerInterface->GetLobbyAPIRef(), 0);
}

void ServerInterfaceConnection::ConnectToServer(const char* lpcServerIP, s32 liServerPort, s32 liTimeoutMs)
{
    meCurrentAction = E_ACTION_CONNECTTOSERVER;
    StartActionCore(KAPC_CONNECTION_ACTION_NAMES[E_ACTION_CONNECTTOSERVER]);
    mbConnecting = true;

    CGS_ASSERT(mpServerInterface->GetLobbyLoginRef() != 0, "mpServerInterface->GetLobbyLoginRef() != NULL");
    LobbyLoginReset(reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()));
    LobbyLoginSetContextCB(reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()),
                           LOBBYLOGIN_CONTEXT_CONNECT, 0, 0);
    const s32 liContext =
        LobbyLoginGetContext(reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()));

    mpServerInterface->GetMessageBuffer()[0] = 0;
    TagFieldSetString(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "NAME", lpcServerIP);
    TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "PORT", liServerPort);
    TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "TIME", liTimeoutMs);
    LobbyLoginSubmitCB(reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()), liContext,
                       mpServerInterface->GetMessageBuffer(),
                       &ServerInterfaceConnection::ServerConnectCallback, this);
}

void ServerInterfaceConnection::LogInToServer(const char* lpcUserName, const char* lpcPassword)
{
    CGS_ASSERT(mpServerInterface->GetLobbyLoginRef() != 0, "mpServerInterface->GetLobbyLoginRef() != NULL");
    CGS_ASSERT(lpcUserName, "lpcUsername");

    mbConnecting = true;
    const s32 liContext =
        LobbyLoginGetContext(reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()));

    mpServerInterface->GetMessageBuffer()[0] = 0;
    TagFieldSetString(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "GTAG", lpcUserName);
    if (lpcPassword != 0 && lpcPassword[0] != 0)
    {
        TagFieldSetString(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "PASS", lpcPassword);
    }
    LobbyLoginSubmitCB(reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()), liContext,
                       mpServerInterface->GetMessageBuffer(),
                       &ServerInterfaceConnection::ServerLoginCallback, this);

    meCurrentAction = E_ACTION_LOGINTOSERVER;
    StartActionCore(KAPC_CONNECTION_ACTION_NAMES[E_ACTION_LOGINTOSERVER]);
}

void ServerInterfaceConnection::AgreeTOS(bool lbAgree)
{
    const s32 liContext =
        LobbyLoginGetContext(reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()));

    mpServerInterface->GetMessageBuffer()[0] = 0;
    TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "TOS", lbAgree ? 1 : 0);
    LobbyLoginSubmitCB(reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()), liContext,
                       mpServerInterface->GetMessageBuffer(),
                       &ServerInterfaceConnection::ServerLoginCallback, this);

    meCurrentAction = E_ACTION_AGREETOS;
    StartActionCore(KAPC_CONNECTION_ACTION_NAMES[E_ACTION_AGREETOS]);
}

void ServerInterfaceConnection::AgreeShareInfo(bool lbAgreeShare1, bool lbAgreeShare2)
{
    // The two share answers as a two-letter Y/N flag string.
    char lacSpam[3];
    lacSpam[0] = lbAgreeShare1 ? 'Y' : 'N';
    lacSpam[1] = lbAgreeShare2 ? 'Y' : 'N';
    lacSpam[2] = 0;

    const s32 liContext =
        LobbyLoginGetContext(reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()));

    mpServerInterface->GetMessageBuffer()[0] = 0;
    TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "SHARE", 1);
    TagFieldSetString(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "SPAM", lacSpam);
    LobbyLoginSubmitCB(reinterpret_cast<LobbyLoginRefT*>(mpServerInterface->GetLobbyLoginRef()), liContext,
                       mpServerInterface->GetMessageBuffer(),
                       &ServerInterfaceConnection::ServerLoginCallback, this);

    meCurrentAction = E_ACTION_AGREESHAREINFO;
    StartActionCore(KAPC_CONNECTION_ACTION_NAMES[E_ACTION_AGREESHAREINFO]);
}

void ServerInterfaceConnection::EndAction(s32 liError)
{
    const DSErrorToServerInterfaceErrorTable& lrEntry = KA_CONNECTION_DS_ERROR_TABLE_LOOKUP[meCurrentAction];
    CGS_ASSERT(lrEntry.mpMappingTable, "lpMappingTable");
    ServerInterfaceComponent::EndActionCore(
        ServerInterfaceComponent::ConvertError(liError, lrEntry.mpMappingTable, lrEntry.miNumMappings));
    meCurrentAction = E_ACTION_COUNT;
}

EServerInterfaceError ServerInterfaceConnection::ConvertError(s32 liLoginAlert, EAction /*leAction*/)
{
    if (liLoginAlert == 0)
    {
        return E_SERVER_INTERFACE_ERROR_NONE;
    }
    for (s32 liIndex = 0; liIndex < 16; ++liIndex)
    {
        if (KA_LOGIN_ALERT_ERROR_MAPPINGS[liIndex].miDSCode == liLoginAlert)
        {
            return KA_LOGIN_ALERT_ERROR_MAPPINGS[liIndex].meError;
        }
    }
    return E_SERVER_INTERFACE_ERROR_UNHANDLED;
}

void ServerInterfaceConnection::ServerConnectCallback(LobbyLoginRefT* /*lpLoginRef*/, LobbyLoginContextE leContext,
                                                      LobbyLoginStatusE leStatus, void* lpUserData)
{
    ServerInterfaceConnection* lpConnection = static_cast<ServerInterfaceConnection*>(lpUserData);

    if (leStatus == LOBBYLOGIN_STATUS_GOTO)
    {
        // Connected: issue the standard lobby select.
        lpConnection->meCurrentAction = E_ACTION_ISSUESELECT;
        LobbyApiRequestCB(lpConnection->mpServerInterface->GetLobbyAPIRef(), 0x73656C65 /* 'sele' */,
                          lpConnection->mpServerInterface->GetStandardSelectOptions(),
                          &ServerInterfaceConnection::DefaultCallback, lpConnection);
        return;
    }

    if (lpConnection->meStatus == 0)
    {
        const LobbyLoginAlertT* lpAlert =
            LobbyLoginAlert(reinterpret_cast<LobbyLoginRefT*>(lpConnection->mpServerInterface->GetLobbyLoginRef()),
                            leContext);
        const s32 liAlertEnum =
            LobbyLoginGetAlertEnum(reinterpret_cast<LobbyLoginRefT*>(lpConnection->mpServerInterface->GetLobbyLoginRef()), lpAlert);
        lpConnection->EndActionCore(lpConnection->ConvertError(liAlertEnum, lpConnection->meCurrentAction));
    }
    lpConnection->meCurrentAction = E_ACTION_COUNT;
}

void ServerInterfaceConnection::ServerLoginCallback(LobbyLoginRefT* lpLoginRef, LobbyLoginContextE leContext,
                                                    LobbyLoginStatusE leStatus, void* lpUserData)
{
    ServerInterfaceConnection* lpConnection = static_cast<ServerInterfaceConnection*>(lpUserData);

    if (leStatus == LOBBYLOGIN_STATUS_ALERT)
    {
        const LobbyLoginAlertT* lpAlert =
            LobbyLoginAlert(reinterpret_cast<LobbyLoginRefT*>(lpConnection->mpServerInterface->GetLobbyLoginRef()),
                            leContext);
        const s32 liAlertEnum =
            LobbyLoginGetAlertEnum(reinterpret_cast<LobbyLoginRefT*>(lpConnection->mpServerInterface->GetLobbyLoginRef()), lpAlert);
        lpConnection->EndActionCore(lpConnection->ConvertError(liAlertEnum, lpConnection->meCurrentAction));
    }
    else if (leStatus == LOBBYLOGIN_STATUS_GOTO)
    {
        LobbyLoginGetContext(lpLoginRef);
        lpConnection->EndActionCore(0);
    }
}

void ServerInterfaceConnection::ConnStatusCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* lpMsg,
                                                   void* lpUserData)
{
    ServerInterfaceConnection* lpConnection = static_cast<ServerInterfaceConnection*>(lpUserData);
    if (lpMsg->kind == 0x64697363 /* 'disc' */)
    {
        lpConnection->mpServerInterface->OnEvent(E_SERVER_INTERFACE_CONNECTION_EVENT_DISCONNECTED,
                                                 const_cast<char*>(lpMsg->pData));
        lpConnection->mbConnected = false;
    }
}

void ServerInterfaceConnection::DefaultCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* lpMsg,
                                                void* lpUserData)
{
    static_cast<ServerInterfaceConnection*>(lpUserData)->EndAction(lpMsg->code);
}

}
