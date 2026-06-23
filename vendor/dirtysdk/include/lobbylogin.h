#ifndef DIRTYSDK_LOBBYLOGIN_H
#define DIRTYSDK_LOBBYLOGIN_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/lobbylogin.h
// The lobby-login module drives the front-end login state machine (connect ->
// create / login -> TOS / share-info -> done). The CGS DirtySock connection
// component polls it through these helpers. Only the prototypes + enums the
// reconstructed CGS code actually uses are declared here; signatures mirror the
// DirtySDK sources verbatim (opaque ref handle + fixed-width integer C interface).

// The current login "context" (which step of the login flow is active). The
// ServerInterfaceConnection maps these onto its own ELoginStatus 1:1.
enum LobbyLoginContextE
{
    LOBBYLOGIN_CONTEXT_INVALID         = -1,
    LOBBYLOGIN_CONTEXT_OFFLINE         = 0,
    LOBBYLOGIN_CONTEXT_CONNECT         = 1,
    LOBBYLOGIN_CONTEXT_CREATE          = 2,
    LOBBYLOGIN_CONTEXT_LOGIN           = 3,
    LOBBYLOGIN_CONTEXT_DUPACCT         = 4,
    LOBBYLOGIN_CONTEXT_TOS             = 5,
    LOBBYLOGIN_CONTEXT_SHARE           = 6,
    LOBBYLOGIN_CONTEXT_PMAIL           = 7,
    LOBBYLOGIN_CONTEXT_INVREGKEY       = 8,
    LOBBYLOGIN_CONTEXT_SELPERS         = 9,
    LOBBYLOGIN_CONTEXT_DEACTIVATED     = 10,
    LOBBYLOGIN_CONTEXT_NUCLEUS_WELCOME = 11,
    LOBBYLOGIN_CONTEXT_DONE            = 12,
    LOBBYLOGIN_NUMCONTEXTS             = 13,
    LOBBYLOGIN_CONTEXT_PREVIOUS        = 4096,
};

// The transient status within the current context (idle / busy / alert / ...).
enum LobbyLoginStatusE
{
    LOBBYLOGIN_STATUS_IDLE    = 0,
    LOBBYLOGIN_STATUS_NETWORK = 1,
    LOBBYLOGIN_STATUS_ALERT   = 2,
    LOBBYLOGIN_STATUS_GOTO    = 3,
    LOBBYLOGIN_STATUS_OFFLINE = 4,
    LOBBYLOGIN_NUMSTATUS      = 5,
};

// Opaque login-module instance handle.
struct LobbyLoginRefT;

#ifdef __cplusplus
extern "C" {
#endif

// @ X360 0x82B20C50 -- return the current login context (a LobbyLoginContextE).
s32 LobbyLoginGetContext(LobbyLoginRefT* pRef);

// @ X360 0x82B20C28 -- return the transient status for the given context (a
// LobbyLoginStatusE; nonzero means the step is still mid-flight).
s32 LobbyLoginGetStatus(LobbyLoginRefT* pRef, s32 eContext);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYLOGIN_H
