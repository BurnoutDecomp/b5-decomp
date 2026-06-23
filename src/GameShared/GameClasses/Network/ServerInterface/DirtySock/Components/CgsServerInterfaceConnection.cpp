#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"  // ServerInterfaceDirtySock::GetLobbyLoginRef
#include "lobbylogin.h"   // DirtySDK LobbyLoginGetContext / LobbyLoginGetStatus

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

}
