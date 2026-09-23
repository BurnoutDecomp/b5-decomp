// ============================================================================
// lobbyloginpc.cpp -- the lobby-login state machine (LobbyLogin*) for the PC build.
//
// [PC platform layer] Not console code. Kept from the console module: the context stack
// (LobbyLoginGetContext is the top of it), the per-context status, the submit checks and the
// completion callback, which LobbyLogin calls from its lobby idle callback once the submitted
// context is left (status LOBBYLOGIN_STATUS_GOTO) or raised an alert.
// The server conversation is replaced:
//   CONNECT submit -> LobbyApiPcConnect: LAN posts 'conn' (-> context LOGIN); offline fails (-1),
//                     which the offline build never reaches (NetConnStatus('onln') stays 0).
//   LOGIN submit   -> an 'auth' lobby request the PC lobby answers from the PC identity
//                     (-> context DONE). There is no terms-of-service or share-info step: the
//                     PC has no account server to ask, and those contexts would stop at a
//                     front-end question the LAN flow has nobody to answer.
//   'disc' on the lobby connection -> alert LOBBYLOGIN_ALERT_DISC.
// ============================================================================

#include "lobbylogin.h"
#include "lobbyapi.h"
#include "lobbytagfield.h"
#include "dirtymem.h"
#include "dirtylib.h"

#include "lan/pclan_lobby.h"

#include <cstring>

namespace
{
    constexpr s32 FourCC(char a, char b, char c, char d)
    {
        return (s32)(((u32)(u8)a << 24) | ((u32)(u8)b << 16) | ((u32)(u8)c << 8) | (u32)(u8)d);
    }

    const s32 KI_MEMID      = FourCC('l', 'l', 'g', 'n');
    const s32 KI_KIND_AUTH  = FourCC('a', 'u', 't', 'h');
    const s32 KI_EVENT_CONN = FourCC('c', 'o', 'n', 'n');
    const s32 KI_EVENT_DISC = FourCC('d', 'i', 's', 'c');

    const s32 KI_CONTEXT_STACK = 16;
}

struct LobbyLoginRefT
{
    LobbyApiRefT*           pLobbyApi;
    s32                     iMemGroup;
    const LobbyLoginAlertT* pAlertList;
    s32                     iConnCbID;
    s32                     iIdleCbID;
    s32                     eContextStack[KI_CONTEXT_STACK];
    s32                     iContextDepth;
    s32                     eCurStatus;
    s32                     eAlertContext;     // context to enter once the alert is read, -1 none
    s32                     iAlert;            // LobbyLoginAlertE of the pending alert
    s32                     iRequestID;
    s32                     eCallbackContext;  // -1 == no callback pending
    LobbyLoginCallbackT*    pCallbackProc;
    void*                   pCallbackData;
    char                    strUser[20];
};

// ---- internal helpers --------------------------------------------------------------------

static void _LobbyLoginSetContext(LobbyLoginRefT* pLogin, s32 eContext, bool bPushContext)
{
    if (eContext >= LOBBYLOGIN_NUMCONTEXTS)
    {
        if (eContext != LOBBYLOGIN_CONTEXT_PREVIOUS)
        {
            NetPrintf(("lobbylogin: ignoring unknown context"));
            return;
        }
        if (pLogin->iContextDepth <= 0)
        {
            NetPrintf(("lobbylogin: warning, attempt to go to previous context when there is no previous context.\n"));
            return;
        }
        pLogin->iContextDepth -= 1;
        return;
    }

    if (eContext == LOBBYLOGIN_CONTEXT_OFFLINE)
    {
        // back down the stack to the offline entry at the bottom
        pLogin->iContextDepth = 0;
        return;
    }

    if (pLogin->iContextDepth >= (KI_CONTEXT_STACK - 1))
    {
        NetPrintf(("lobbylogin: exceeded max context depth"));
        return;
    }
    if (bPushContext)
    {
        pLogin->iContextDepth += 1;
    }
    pLogin->eContextStack[pLogin->iContextDepth] = eContext;
}

static s32 _LobbyLoginCurrent(const LobbyLoginRefT* pLogin)
{
    return pLogin->eContextStack[pLogin->iContextDepth];
}

// Lobby connection events: 'conn' moves CONNECT on to LOGIN, 'disc' raises the disconnect alert.
static void _LobbyLoginConnCallback(LobbyApiRefT* /*pLobbyApi*/, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyLoginRefT* pLogin = (LobbyLoginRefT*)pUserData;
    if (pMsg->kind == KI_EVENT_CONN)
    {
        _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_LOGIN, true);
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
    }
    if (pMsg->kind == KI_EVENT_DISC)
    {
        pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_OFFLINE;
        pLogin->iAlert        = LOBBYLOGIN_ALERT_DISC;
        pLogin->eCurStatus    = LOBBYLOGIN_STATUS_ALERT;
    }
}

// The 'auth' response: success completes the login.
static void _LobbyLoginAuthCallback(LobbyApiRefT* /*pLobbyApi*/, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyLoginRefT* pLogin = (LobbyLoginRefT*)pUserData;
    pLogin->iRequestID = 0;
    if (pMsg->code == 0)
    {
        _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_DONE, true);
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
    }
    else
    {
        pLogin->eAlertContext = -1;
        pLogin->iAlert        = LOBBYLOGIN_ALERT_ERROR;
        pLogin->eCurStatus    = LOBBYLOGIN_STATUS_ALERT;
    }
}

// Runs on every LobbyApiUpdate: tell the submitter once its context is left or alerted.
static void _LobbyLoginIdleCallback(LobbyApiRefT* /*pLobbyApi*/, LobbyApiMsgT* /*pMsg*/, void* pUserData)
{
    LobbyLoginRefT* pLogin = (LobbyLoginRefT*)pUserData;
    if ((pLogin->eCallbackContext == -1) || (pLogin->pCallbackProc == NULL))
    {
        return;
    }
    const s32 eContext = pLogin->eCallbackContext;
    const s32 eStatus = LobbyLoginGetStatus(pLogin, eContext);
    if ((eStatus != LOBBYLOGIN_STATUS_GOTO) && (eStatus != LOBBYLOGIN_STATUS_ALERT))
    {
        return;
    }
    LobbyLoginCallbackT* pCallbackProc = pLogin->pCallbackProc;
    void* pCallbackData = pLogin->pCallbackData;
    // a context change, or an alert that leaves no context to go to, ends the wait
    if ((eStatus == LOBBYLOGIN_STATUS_GOTO) || (pLogin->eAlertContext == -1))
    {
        pLogin->eCallbackContext = -1;
        pLogin->pCallbackProc    = NULL;
    }
    pCallbackProc(pLogin, (LobbyLoginContextE)eContext, (LobbyLoginStatusE)eStatus, pCallbackData);
}

// ---- public API --------------------------------------------------------------------------

extern "C" LobbyLoginRefT* LobbyLoginCreate(LobbyApiRefT* pLobbyApi, const LobbyLoginAlertT* pAlertList)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbyLoginRefT* pLogin = (LobbyLoginRefT*)DirtyMemAlloc(sizeof(LobbyLoginRefT), KI_MEMID, iMemGroup);
    if (pLogin == NULL)
    {
        return NULL;
    }
    memset(pLogin, 0, sizeof(*pLogin));
    pLogin->iMemGroup        = iMemGroup;
    pLogin->pLobbyApi        = pLobbyApi;
    pLogin->pAlertList       = pAlertList;
    pLogin->eAlertContext    = -1;
    pLogin->eCallbackContext = -1;
    pLogin->iConnCbID = LobbyApiSetCallback(pLobbyApi, LOBBYAPI_CBTYPE_CONN, &_LobbyLoginConnCallback, pLogin);
    pLogin->iIdleCbID = LobbyApiSetCallback(pLobbyApi, LOBBYAPI_CBTYPE_IDLE, &_LobbyLoginIdleCallback, pLogin);
    return pLogin;
}

extern "C" void LobbyLoginDestroy(LobbyLoginRefT* pLogin)
{
    LobbyApiClearCallback(pLogin->pLobbyApi, pLogin->iConnCbID);
    LobbyApiClearCallback(pLogin->pLobbyApi, pLogin->iIdleCbID);
    DirtyMemFree(pLogin, KI_MEMID, pLogin->iMemGroup);
}

extern "C" void LobbyLoginReset(LobbyLoginRefT* pLogin)
{
    LobbyApiRefT* pLobbyApi = pLogin->pLobbyApi;
    const s32 iMemGroup = pLogin->iMemGroup;
    const LobbyLoginAlertT* pAlertList = pLogin->pAlertList;
    const s32 iConnCbID = pLogin->iConnCbID;
    const s32 iIdleCbID = pLogin->iIdleCbID;

    memset(pLogin, 0, sizeof(*pLogin));
    pLogin->pLobbyApi        = pLobbyApi;
    pLogin->iMemGroup        = iMemGroup;
    pLogin->pAlertList       = pAlertList;
    pLogin->iConnCbID        = iConnCbID;
    pLogin->iIdleCbID        = iIdleCbID;
    pLogin->eAlertContext    = -1;
    pLogin->eCallbackContext = -1;
}

extern "C" void LobbyLoginSetContextCB(LobbyLoginRefT* pLogin, s32 eNextContext, LobbyLoginCallbackT* pCallbackProc,
                                       void* pCallbackData)
{
    _LobbyLoginSetContext(pLogin, eNextContext, true);
    if (pCallbackProc != NULL)
    {
        pLogin->eCallbackContext = eNextContext;
        pLogin->pCallbackProc    = pCallbackProc;
        pLogin->pCallbackData    = pCallbackData;
    }
}

extern "C" s32 LobbyLoginSubmitCB(LobbyLoginRefT* pLogin, s32 eContext, const char* pPacket,
                                  LobbyLoginCallbackT* pCallbackProc, void* pCallbackData)
{
    if (eContext != _LobbyLoginCurrent(pLogin))
    {
        NetPrintf(("lobbylogin: attempt to submit in context %d, when current context is %d\n",
                   eContext, _LobbyLoginCurrent(pLogin)));
        return -1;
    }
    if ((pLogin->eCurStatus != LOBBYLOGIN_STATUS_IDLE) && (pLogin->eCurStatus != LOBBYLOGIN_STATUS_ALERT))
    {
        NetPrintf(("lobbylogin: attempt to submit info when not in idle state. context = %d\n", eContext));
        return -2;
    }

    if (eContext == LOBBYLOGIN_CONTEXT_CONNECT)
    {
        if (LobbyApiPcConnect(pLogin->pLobbyApi) < 0)
        {
            return -1;
        }
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_NETWORK;
    }
    else if ((eContext == LOBBYLOGIN_CONTEXT_LOGIN) || (eContext == LOBBYLOGIN_CONTEXT_TOS) ||
             (eContext == LOBBYLOGIN_CONTEXT_SHARE))
    {
        TagFieldGetString(TagFieldFind(pPacket, "GTAG"), pLogin->strUser, sizeof(pLogin->strUser), "");
        pLogin->iRequestID = LobbyApiRequestCB(pLogin->pLobbyApi, KI_KIND_AUTH, pPacket,
                                               &_LobbyLoginAuthCallback, pLogin);
        if (pLogin->iRequestID < 0)
        {
            pLogin->iRequestID = 0;
            return -1;
        }
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_NETWORK;
    }

    if ((pLogin->eCurStatus == LOBBYLOGIN_STATUS_NETWORK) || (_LobbyLoginCurrent(pLogin) != eContext))
    {
        pLogin->eCallbackContext = eContext;
        pLogin->pCallbackProc    = pCallbackProc;
        pLogin->pCallbackData    = pCallbackData;
    }
    else
    {
        pLogin->eCallbackContext = -1;
    }
    return 0;
}

extern "C" s32 LobbyLoginGetContext(LobbyLoginRefT* pLogin)
{
    return _LobbyLoginCurrent(pLogin);
}

extern "C" s32 LobbyLoginGetStatus(LobbyLoginRefT* pLogin, s32 eContext)
{
    if (_LobbyLoginCurrent(pLogin) == eContext)
    {
        return pLogin->eCurStatus;
    }
    return LOBBYLOGIN_STATUS_GOTO;
}

extern "C" const LobbyLoginAlertT* LobbyLoginAlert(LobbyLoginRefT* pLogin, s32 eContext)
{
    if (eContext != _LobbyLoginCurrent(pLogin))
    {
        NetPrintf(("lobbylogin: attempt to get alert in context %d, when current context is %d\n",
                   eContext, _LobbyLoginCurrent(pLogin)));
        return NULL;
    }
    pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
    if (pLogin->eAlertContext != -1)
    {
        _LobbyLoginSetContext(pLogin, pLogin->eAlertContext, false);
        pLogin->eAlertContext = -1;
    }
    s32 iAlert = pLogin->iAlert;
    if ((iAlert < 0) || (iAlert >= LOBBYLOGIN_NUMALERTS))
    {
        iAlert = 0;
    }
    return &pLogin->pAlertList[iAlert];
}

extern "C" LobbyLoginAlertE LobbyLoginGetAlertEnum(LobbyLoginRefT* pLogin, const LobbyLoginAlertT* pAlert)
{
    if (pAlert->uFlags == -1)
    {
        return LOBBYLOGIN_ALERT_SERVERDOWN;
    }
    return (LobbyLoginAlertE)(pAlert - pLogin->pAlertList);
}
