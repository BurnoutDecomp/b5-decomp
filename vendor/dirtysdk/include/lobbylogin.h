#ifndef DIRTYSDK_LOBBYLOGIN_H
#define DIRTYSDK_LOBBYLOGIN_H

#include "types.hpp"
#include "lobbyapi.h"   // LobbyApiRefT

// DirtySDK 5.5.3 - core/include/lobbylogin.h
// The lobby-login module drives the front-end login state machine (connect -> login ->
// TOS / share-info -> done) on top of the lobby connection. The CGS DirtySock connection
// component polls it through these entry points.
// PC body: ../src/pc/lobbyloginpc.cpp.
//
// Context arguments are LobbyLoginContextE values; the entry points take them as s32 so the
// callers can pass the value LobbyLoginGetContext returned.

// Login alerts, in the order of the alert table handed to LobbyLoginCreate.
enum LobbyLoginAlertE
{
    LOBBYLOGIN_ALERT_ERROR             = 0,
    LOBBYLOGIN_ALERT_DB_ERROR          = 1,
    LOBBYLOGIN_ALERT_FILTER            = 2,
    LOBBYLOGIN_ALERT_INV_MASTER        = 3,
    LOBBYLOGIN_ALERT_MISSING_PARAM     = 4,
    LOBBYLOGIN_ALERT_ALREADY_LOGGED_IN = 5,
    LOBBYLOGIN_ALERT_LOCKED            = 6,
    LOBBYLOGIN_ALERT_BANNED            = 7,
    LOBBYLOGIN_ALERT_DISABLED          = 8,
    LOBBYLOGIN_ALERT_PENDING           = 9,
    LOBBYLOGIN_ALERT_RESERVED          = 10,
    LOBBYLOGIN_ALERT_CONNTIMEOUT       = 11,
    LOBBYLOGIN_ALERT_SERVERDOWN        = 12,
    LOBBYLOGIN_ALERT_DISC              = 13,
    LOBBYLOGIN_ALERT_CHEAT_DEVICE      = 14,
    LOBBYLOGIN_ALERT_DASHBOARD         = 15,
    LOBBYLOGIN_ALERT_PERSONA_SET       = 16,
    LOBBYLOGIN_ALERT_INV_PASS          = 17,
    LOBBYLOGIN_ALERT_INV_REGKEY        = 18,
    LOBBYLOGIN_ALERT_DUPLICATE         = 19,
    LOBBYLOGIN_ALERT_INV_MAIL          = 20,
    LOBBYLOGIN_ALERT_INV_PMAIL         = 21,
    LOBBYLOGIN_ALERT_TOO_YOUNG         = 22,
    LOBBYLOGIN_ALERT_SENT_MAIL         = 23,
    LOBBYLOGIN_ALERT_TOO_MANY          = 24,
    LOBBYLOGIN_ALERT_INC_PASS          = 25,
    LOBBYLOGIN_ALERT_DEACTIVATED_EMAIL = 26,
    LOBBYLOGIN_NUMALERTS               = 27
};

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
    LOBBYLOGIN_CONTEXT_PREVIOUS        = 4096
};

// The transient status within the current context.
enum LobbyLoginStatusE
{
    LOBBYLOGIN_STATUS_IDLE    = 0,
    LOBBYLOGIN_STATUS_NETWORK = 1,   // a submit is in flight
    LOBBYLOGIN_STATUS_ALERT   = 2,   // LobbyLoginAlert has the reason
    LOBBYLOGIN_STATUS_GOTO    = 3,   // the queried context is no longer the current one
    LOBBYLOGIN_STATUS_OFFLINE = 4,
    LOBBYLOGIN_NUMSTATUS      = 5
};

// One entry of the alert table handed to LobbyLoginCreate (LOBBYLOGIN_NUMALERTS entries).
struct LobbyLoginAlertT
{
    s32         uFlags;
    const char* pTitle;
    const char* pBody;
    const char* pFooter;
};

struct LobbyLoginRefT;   // opaque

// Completion callback TYPE: (login ref, the submitted context, its status, user data).
typedef void (LobbyLoginCallbackT)(LobbyLoginRefT* pLogin, LobbyLoginContextE eContext,
                                   LobbyLoginStatusE eStatus, void* pCallbackData);

#ifdef __cplusplus
extern "C" {
#endif

LobbyLoginRefT* LobbyLoginCreate(LobbyApiRefT* pLobbyApi, const LobbyLoginAlertT* pAlertList);
void LobbyLoginDestroy(LobbyLoginRefT* pLogin);

// Back to the offline context with no request in flight.
void LobbyLoginReset(LobbyLoginRefT* pLogin);

// Enter context eNextContext; pCallbackProc (may be 0) is told when it is left.
void LobbyLoginSetContextCB(LobbyLoginRefT* pLogin, s32 eNextContext, LobbyLoginCallbackT* pCallbackProc,
                            void* pCallbackData);

// Submit the tagfield pPacket for context eContext (which must be the current one).
// Returns 0, -1 for a wrong context or a failed connect, -2 while a submit is in flight.
s32 LobbyLoginSubmitCB(LobbyLoginRefT* pLogin, s32 eContext, const char* pPacket,
                       LobbyLoginCallbackT* pCallbackProc, void* pCallbackData);

// The current context (a LobbyLoginContextE).
s32 LobbyLoginGetContext(LobbyLoginRefT* pLogin);

// The status of context eContext (a LobbyLoginStatusE): LOBBYLOGIN_STATUS_GOTO when it is
// not the current context.
s32 LobbyLoginGetStatus(LobbyLoginRefT* pLogin, s32 eContext);

// The alert of the current context eContext (0 when eContext is not current); clears the
// alert status.
const LobbyLoginAlertT* LobbyLoginAlert(LobbyLoginRefT* pLogin, s32 eContext);

// The LobbyLoginAlertE of an alert LobbyLoginAlert returned.
LobbyLoginAlertE LobbyLoginGetAlertEnum(LobbyLoginRefT* pLogin, const LobbyLoginAlertT* pAlert);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYLOGIN_H
