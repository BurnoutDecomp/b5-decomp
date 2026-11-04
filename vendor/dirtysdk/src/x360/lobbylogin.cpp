// DirtySDK 5.5.3 lobby login state primitives, ARTIST 82B20B68..82B20C88.
#include "lobbylogin.h"
#include "dirtymem.h"
#include "dirtynet_x360_internal.h"
#include <stddef.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

struct LobbyLoginNameListT
{
    char strNames[4][32];
    s32 iNumNames;
};

struct LobbyLoginRefT
{
    LobbyApiRefT* pLobbyApi;
    s32 iMemGroup;
    const LobbyLoginAlertT* pAlertList;
    s32 iConnCbID;
    s32 iIdleCbID;
    LobbyLoginAlertT DynamicAlert;
    LobbyLoginContextE eContextStack[16];
    s32 iContextDepth;
    LobbyLoginStatusE eCurStatus;
    LobbyLoginContextE eAlertContext;
    s32 iRequestID;
    LobbyLoginContextE eCallbackContext;
    LobbyLoginCallbackT* pCallbackProc;
    void* pCallbackData;
    LobbyLoginNameListT Personas;
    LobbyLoginNameListT AutoInfo;
    s32 iDelPersona;
    u32 uLastKind;
    u32 uLastCode;
    char strRespBuf[1024];
    char strUser[20];
    char strPers[16];
    char strRetnBuf[128];
    char strTagfBuf[1024];
    u8 strCheatResp[33];
    char strAutoPassword[20];
    char strDeacEmail[64];
    char strSpam[16];
    u8 bAutoLogin;
    u8 bMultiPers;
    u8 bNucleusMessage;
    u8 bAddNucleusMessage;
    u8 bIncopleteAccount;
    u8 bFailBillingServers;
    u8 bAutoCreate;
};
typedef char LoginSizeCheck[sizeof(LobbyLoginRefT) == 2756 ? 1 : -1];
typedef char LoginDepthCheck[offsetof(LobbyLoginRefT, iContextDepth) == 100 ? 1 : -1];
typedef char LoginFlagCheck[offsetof(LobbyLoginRefT, bAutoCreate) == 2755 ? 1 : -1];

extern "C" void LobbyLoginReset(LobbyLoginRefT* pLogin)
{
    LobbyApiRefT* pLobbyApi = pLogin->pLobbyApi;
    const LobbyLoginAlertT* pAlertList = pLogin->pAlertList;
    const s32 iMemGroup = pLogin->iMemGroup;
    const s32 iConnCbID = pLogin->iConnCbID;
    const s32 iIdleCbID = pLogin->iIdleCbID;
    memset(pLogin, 0, sizeof(*pLogin));
    pLogin->pLobbyApi = pLobbyApi;
    pLogin->pAlertList = pAlertList;
    pLogin->iMemGroup = iMemGroup;
    pLogin->iConnCbID = iConnCbID;
    pLogin->iIdleCbID = iIdleCbID;
    pLogin->eContextStack[0] = LOBBYLOGIN_CONTEXT_OFFLINE;
    pLogin->iContextDepth = 0;
    pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
    pLogin->bAutoCreate = 1;
}

extern "C" void LobbyLoginDestroy(LobbyLoginRefT* pLogin)
{
    LobbyApiClearCallback(pLogin->pLobbyApi, pLogin->iConnCbID);
    LobbyApiClearCallback(pLogin->pLobbyApi, pLogin->iIdleCbID);
    DirtyMemFree(pLogin, 0x6c6c676e, pLogin->iMemGroup);
}

extern "C" s32 LobbyLoginGetStatus(LobbyLoginRefT* pLogin, s32 eContext)
{
    return pLogin->eContextStack[pLogin->iContextDepth] == eContext ? pLogin->eCurStatus : LOBBYLOGIN_STATUS_GOTO;
}

extern "C" s32 LobbyLoginGetContext(LobbyLoginRefT* pLogin)
{
    return pLogin->eContextStack[pLogin->iContextDepth];
}

extern "C" LobbyLoginAlertE LobbyLoginGetAlertEnum(LobbyLoginRefT* pLogin, const LobbyLoginAlertT* pAlert)
{
    return pAlert->uFlags == -1 ? LOBBYLOGIN_ALERT_SERVERDOWN :
        static_cast<LobbyLoginAlertE>(pAlert - pLogin->pAlertList);
}
#include "dirtylib.h"

// ARTIST transition table 82146590 and alert code table 82F7DFC8.
static const signed char kLoginTransitions[13][3] = {
    {1,-1,-1}, {-1,-1,-1}, {-1,-1,-1}, {-1,-1,-1}, {-1,-1,-1},
    {-1,-1,-1}, {-1,-1,-1}, {-1,-1,-1}, {-1,-1,-1}, {-1,-1,-1},
    {-1,-1,-1}, {-1,-1,-1}, {-1,-1,-1}
};
static const u32 kLoginAlertCodes[27] = {
    0x6e6f6e65,0x64626572,0x66696c74,0x696d7374,0x6d697373,0x6c6f676e,
    0x6c6f636b,0x6162616e,0x61646973,0x6170656e,0x72737276,0x74696d65,
    0x646f776e,0x64697363,0x63646576,0x64617368,0x70736574,0x69707373,
    0x696b6579,0x6475706c,0x6d61696c,0x706d616c,0x746f6f79,0x736d616c,
    0x6d616e79,0x70617373,0x6465656d
};

static void _LobbyLoginSetContext(LobbyLoginRefT* pLogin, s32 eContext, u32 bPush)
{
    if (eContext == LOBBYLOGIN_CONTEXT_PREVIOUS)
    {
        if (pLogin->iContextDepth > 0)
        {
            NetPrintfCode("_LobbyLoginSetContext back from [%d] to [%d]\n", pLogin->iContextDepth, pLogin->iContextDepth - 1);
            --pLogin->iContextDepth;
        }
        else
            NetPrintfCode("lobbylogin: warning, attempt to go to previous context when there is no previous context.\n");
        return;
    }
    if (eContext >= LOBBYLOGIN_NUMCONTEXTS)
    {
        NetPrintfCode("lobbylogin: ignoring unknown context");
        return;
    }
    if (eContext == LOBBYLOGIN_CONTEXT_OFFLINE)
    {
        while (pLogin->eContextStack[pLogin->iContextDepth] != LOBBYLOGIN_CONTEXT_OFFLINE)
            _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_PREVIOUS, 1);
        return;
    }
    if (pLogin->iContextDepth >= 15)
    {
        NetPrintfCode("lobbylogin: exceeded max context depth");
        return;
    }
    if (bPush == 1)
        ++pLogin->iContextDepth;
    NetPrintfCode("_LobbyLoginSetContext from [%d] to [%d]\n", pLogin->iContextDepth - 1, eContext);
    pLogin->eContextStack[pLogin->iContextDepth] = static_cast<LobbyLoginContextE>(eContext);
}

extern "C" void LobbyLoginSetContextCB(LobbyLoginRefT* pLogin, s32 eNext,
                                        LobbyLoginCallbackT*, void*)
{
    const s32 eCurrent = pLogin->eContextStack[pLogin->iContextDepth];
    bool bValid = eNext == LOBBYLOGIN_CONTEXT_PREVIOUS;
    if (!bValid)
        for (s32 i = 0; i < 3; ++i)
            if (kLoginTransitions[eCurrent][i] == eNext)
            {
                bValid = true;
                break;
            }
    if (bValid)
        _LobbyLoginSetContext(pLogin, eNext,
            eNext != LOBBYLOGIN_CONTEXT_PREVIOUS && (eCurrent == 4 || eCurrent == 5) ? 0 : 1);
    else
        NetPrintfCode("lobbylogin: invalid attempt to set context %d in context %d\n", eNext, eCurrent);
}

extern "C" const LobbyLoginAlertT* LobbyLoginAlert(LobbyLoginRefT* pLogin, s32 eContext)
{
    const s32 eCurrent = pLogin->eContextStack[pLogin->iContextDepth];
    if (eContext != eCurrent)
    {
        NetPrintfCode("lobbylogin: attempt to get alert in context %d, when current context is %d\n", eContext, eCurrent);
        return 0;
    }
    u32 uAlert = 0;
    while (uAlert < 27 && pLogin->uLastCode != kLoginAlertCodes[uAlert])
        ++uAlert;
    pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
    if (pLogin->eAlertContext != LOBBYLOGIN_CONTEXT_INVALID)
    {
        _LobbyLoginSetContext(pLogin, pLogin->eAlertContext, 1);
        pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_INVALID;
    }
    if (uAlert >= 27)
        uAlert = 0;
    if (pLogin->uLastCode == 0x646f776e)
    {
        pLogin->DynamicAlert = pLogin->pAlertList[uAlert];
        pLogin->DynamicAlert.uFlags = -1;
        pLogin->DynamicAlert.pBody = pLogin->strRespBuf;
        return &pLogin->DynamicAlert;
    }
    return &pLogin->pAlertList[uAlert];
}
#include "lobbytagfield.h"
extern "C" char* ds_strnzcpy(char*, const char*, s32);
// ARTIST 82B21A08 / DecFIGS _LobbyLoginDoLogin; auth chain recovered separately.
extern "C" void _LobbyLoginDoLogin(LobbyLoginRefT*, char*, s32);

static void _LobbyLoginSaveResponse(LobbyLoginRefT* pLogin, LobbyApiMsgT* pMsg)
{
    if (pMsg->pData)
        TagFieldDupl(pLogin->strRespBuf, sizeof(pLogin->strRespBuf), pMsg->pData);
    else
        pLogin->strRespBuf[0] = 0;
    pLogin->uLastKind = pMsg->kind;
    pLogin->uLastCode = pMsg->code;
}

static void _LobbyLoginIdleCallback(LobbyApiRefT*, LobbyApiMsgT*, void* pUserData)
{
    LobbyLoginRefT* pLogin = static_cast<LobbyLoginRefT*>(pUserData);
    LobbyLoginCallbackT* pCallback = pLogin->pCallbackProc;
    const LobbyLoginContextE eContext = pLogin->eCallbackContext;
    if (!pCallback || eContext == LOBBYLOGIN_CONTEXT_INVALID || pLogin->bAutoLogin)
        return;
    const LobbyLoginStatusE eStatus = pLogin->eContextStack[pLogin->iContextDepth] == eContext
        ? pLogin->eCurStatus : LOBBYLOGIN_STATUS_GOTO;
    if (eStatus != LOBBYLOGIN_STATUS_GOTO && eStatus != LOBBYLOGIN_STATUS_ALERT)
        return;
    if (eStatus == LOBBYLOGIN_STATUS_GOTO || pLogin->eAlertContext == LOBBYLOGIN_CONTEXT_INVALID)
    {
        pLogin->pCallbackProc = 0;
        pLogin->eCallbackContext = LOBBYLOGIN_CONTEXT_INVALID;
    }
    pCallback(pLogin, eContext, eStatus, pLogin->pCallbackData);
}

static void _LobbyLoginConnCallback(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyLoginRefT* pLogin = static_cast<LobbyLoginRefT*>(pUserData);
    _LobbyLoginSaveResponse(pLogin, pMsg);
    if (pMsg->kind == 0x636f6e6e)
    {
        if (pLogin->bAutoLogin == 1)
        {
            pLogin->strTagfBuf[0] = 0;
            TagFieldSetString(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "GTAG", pLogin->AutoInfo.strNames[0]);
            ds_strnzcpy(pLogin->strUser, pLogin->AutoInfo.strNames[0], sizeof(pLogin->strUser));
            TagFieldSetString(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "PASS", pLogin->AutoInfo.strNames[1]);
            _LobbyLoginDoLogin(pLogin, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf));
        }
        else
        {
            _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_LOGIN, 1);
            pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
        }
    }
    if (pMsg->kind == 0x64697363)
    {
        pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_OFFLINE;
        pLogin->bAutoLogin = 0;
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
        if (pMsg->code != 0x646f776e && pMsg->code != 0x74696d65)
            pLogin->uLastCode = 0x64697363;
    }
}

extern "C" LobbyLoginRefT* LobbyLoginCreate(LobbyApiRefT* pLobbyApi, const LobbyLoginAlertT* pAlertList)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbyLoginRefT* pLogin = static_cast<LobbyLoginRefT*>(DirtyMemAlloc(sizeof(LobbyLoginRefT), 0x6c6c676e, iMemGroup));
    if (!pLogin)
        return 0;
    memset(pLogin, 0, sizeof(*pLogin));
    pLogin->iMemGroup = iMemGroup;
    pLogin->pLobbyApi = pLobbyApi;
    pLogin->pAlertList = pAlertList;
    pLogin->eContextStack[0] = LOBBYLOGIN_CONTEXT_OFFLINE;
    pLogin->iContextDepth = 0;
    pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
    pLogin->bAutoCreate = 1;
    pLogin->iConnCbID = LobbyApiSetCallback(pLobbyApi, 3, _LobbyLoginConnCallback, pLogin);
    pLogin->iIdleCbID = LobbyApiSetCallback(pLobbyApi, 5, _LobbyLoginIdleCallback, pLogin);
    return pLogin;
}
#include "lobbyname.h"
extern "C" void _LobbyLoginRequest(LobbyLoginRefT*, s32, char*, s32, LobbyApiCallbackT*);
extern "C" void _LobbyLoginLathCallback(LobbyApiRefT*, LobbyApiMsgT*, void*);
extern "C" void _LobbyLoginAuthCallback(LobbyApiRefT*, LobbyApiMsgT*, void*);

extern "C" void _LobbyLoginDoLogin(LobbyLoginRefT* pLogin, char* pTagfield, s32 iTagSize)
{
    const char* pLKey = TagFieldFind(pTagfield, "LKEY");
    const char* pConf = static_cast<const char*>(LobbyApiInfo(pLogin->pLobbyApi, 0x636f6e66));
    if (!pLKey)
    {
        if (TagFieldFind(pConf, "LKEYLOGIN"))
        {
            pLKey = static_cast<const char*>(LobbyApiInfo(pLogin->pLobbyApi, 0x6c6b6579));
            if (pLKey)
            {
                char strUser[20];
                char strUser0[20];
                TagFieldGetString(TagFieldFind(pTagfield, "NAME"), strUser, sizeof(strUser), "");
                LobbyApiStatus(pLogin->pLobbyApi, 0x61636374, strUser0, sizeof(strUser0));
                if (LobbyNameCmp(strUser, strUser0))
                    pLKey = 0;
                else
                    TagFieldSetString(pTagfield, iTagSize, "LKEY", pLKey);
            }
        }
    }
    if (pLKey && (pLogin->uLastKind != 0x6c617468 || !pLogin->uLastCode))
        _LobbyLoginRequest(pLogin, 0x6c617468, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), _LobbyLoginLathCallback);
    else
    {
        TagFieldSetRaw(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "TOSVER", TagFieldFind(pConf, "TOSVER"));
        _LobbyLoginRequest(pLogin, 0x61757468, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), _LobbyLoginAuthCallback);
    }
}
// ARTIST 82B20F98: persona-response state transition.
static void _LobbyLoginPersCallback(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyLoginRefT* pLogin = static_cast<LobbyLoginRefT*>(pUserData);
    _LobbyLoginSaveResponse(pLogin, pMsg);
    if (pMsg->code == 0)
    {
        if (!pLogin->bAutoCreate && (pLogin->bNucleusMessage == 1 || pLogin->bAddNucleusMessage == 1))
            _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_NUCLEUS_WELCOME, 0);
        else
            _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_DONE, 1);
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
    }
    else if (pMsg->code == 0x696b6579)
    {
        _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_INVREGKEY, 0);
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
    }
    else
    {
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
        pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_INVALID;
    }
    pLogin->bAutoLogin = 0;
}

extern "C" void _LobbyLoginLathCallback(LobbyApiRefT* pApiRef, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyLoginRefT* pLogin = static_cast<LobbyLoginRefT*>(pUserData);
    _LobbyLoginSaveResponse(pLogin, pMsg);
    if (!pMsg->code)
        _LobbyLoginPersCallback(pApiRef, pMsg, pUserData);
    else
    {
        const s32 iCode = pMsg->code;
        NetPrintfCode("lobbylogin: got unexpected response '%c%c%c%c' from 'lath' request\n",
                      iCode >> 24, (iCode >> 16) & 255, (iCode >> 8) & 255, iCode & 255);
        if (TagFieldFind(pLogin->strTagfBuf, "GTAG"))
            _LobbyLoginDoLogin(pLogin, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf));
    }
}
#include "netconn.h"
// Native X360-only account metadata helper at ARTIST 82B204B0; exact original name unconfirmed.
extern "C" void sub_82B204B0(LobbyLoginRefT*, char*, s32, s32);

extern "C" void _LobbyLoginRequest(LobbyLoginRefT* pLogin, s32 iKind, char* pMsg,
                                   s32 iMsgSize, LobbyApiCallbackT* pCallback)
{
    sub_82B204B0(pLogin, pMsg, iMsgSize, iKind == 0x61636374);
    TagFieldSetString(pMsg, iMsgSize, "MAC", NetConnMAC());
    if (iKind == 0x70657273)
    {
        char strVersion[64];
        LobbyApiStatus(pLogin->pLobbyApi, 0x76657273, strVersion, 5);
        if (strcmp(strVersion, "FLM/"))
            TagFieldSetString(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "CDEV", reinterpret_cast<const char*>(pLogin->strCheatResp));
    }
    pLogin->iRequestID = LobbyApiRequestCB(pLogin->pLobbyApi, iKind, pMsg, pCallback, pLogin);
    pLogin->eCurStatus = LOBBYLOGIN_STATUS_NETWORK;
}
extern "C" void sub_82B20390(LobbyLoginRefT*);
extern "C" void _LobbyLoginCreateAccount(LobbyLoginRefT*, s32);
extern "C" void CheatCalcResponse(const char*, u32, unsigned char*, u32);

extern "C" void _LobbyLoginAuthCallback(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyLoginRefT* pLogin = static_cast<LobbyLoginRefT*>(pUserData);
    _LobbyLoginSaveResponse(pLogin, pMsg);
    if (pMsg->code)
    {
        s32 eNext;
        if (pMsg->code == 0x746f7361) eNext = LOBBYLOGIN_CONTEXT_TOS;
        else if (pMsg->code == 0x696b6579) eNext = LOBBYLOGIN_CONTEXT_INVREGKEY;
        else if (pMsg->code == 0x73686172) eNext = LOBBYLOGIN_CONTEXT_SHARE;
        else if (pMsg->code == 0x706d616c) eNext = LOBBYLOGIN_CONTEXT_PMAIL;
        else if (pMsg->code == 0x696d7374)
        {
            if (pLogin->bAutoCreate)
            {
                TagFieldSetString(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "GEND", "M");
                _LobbyLoginCreateAccount(pLogin, LOBBYLOGIN_CONTEXT_CREATE);
                return;
            }
            TagFieldGetString(TagFieldFind(pMsg->pData, "AUTOPASS"), pLogin->strAutoPassword, sizeof(pLogin->strAutoPassword), "");
            sub_82B204B0(pLogin, pLogin->strRespBuf, sizeof(pLogin->strRespBuf), 1);
            if (!TagFieldFind(pMsg->pData, "MAIL") && !TagFieldFind(pLogin->strRespBuf, "MAIL"))
            {
                pLogin->bIncopleteAccount = 1;
                TagFieldSetString(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "PASS", pLogin->strAutoPassword);
                _LobbyLoginCreateAccount(pLogin, LOBBYLOGIN_CONTEXT_CREATE);
                return;
            }
            TagFieldGetString(TagFieldFind(pMsg->pData, "SPAM"), pLogin->strSpam, sizeof(pLogin->strSpam), "");
            eNext = LOBBYLOGIN_CONTEXT_CREATE;
        }
        else if (pMsg->code == 0x61646561)
        {
            _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_DEACTIVATED, 0);
            TagFieldGetString(TagFieldFind(pMsg->pData, "SPAM"), pLogin->strSpam, sizeof(pLogin->strSpam), "");
            if (!pLogin->strDeacEmail[0])
                TagFieldGetString(TagFieldFind(pLogin->strRespBuf, "MAIL"), pLogin->strDeacEmail, sizeof(pLogin->strDeacEmail), "");
            pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
            pLogin->bAutoLogin = 0;
            return;
        }
        else
        {
            if (pMsg->code == 0x6e636c73 && pLogin->bFailBillingServers == 1)
                pLogin->uLastCode = 0x74696d65;
            pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
            pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_INVALID;
            pLogin->bAutoLogin = 0;
            return;
        }
        _LobbyLoginSetContext(pLogin, eNext, 0);
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
        pLogin->bAutoLogin = 0;
        return;
    }
    TagFieldGetString(TagFieldFind(pMsg->pData, "NAME"), pLogin->strUser, sizeof(pLogin->strUser), "");
    s32 i;
    for (i = 0; i < 4; ++i)
        if (TagFieldGetDelim(TagFieldFind(pMsg->pData, "PERSONAS"), pLogin->Personas.strNames[i], 32, "", i, ',') < 1)
            break;
    pLogin->Personas.iNumNames = i;
    sub_82B20390(pLogin);
    char strVersion[16];
    LobbyApiStatus(pLogin->pLobbyApi, 0x76657273, strVersion, 5);
    if (strcmp(strVersion, "FLM/"))
    {
        const char* pChallenge = TagFieldFind(pMsg->pData, "CPAT");
        if (pChallenge)
        {
            char strPattern[256];
            TagFieldGetString(pChallenge, strPattern, sizeof(strPattern), "");
            CheatCalcResponse(strPattern, static_cast<u32>(-1), pLogin->strCheatResp, sizeof(pLogin->strCheatResp));
            NetPrintfCode("lobbylogin: cheat detection returned MD5 string [%s]\n", pLogin->strCheatResp);
        }
    }
    pLogin->bNucleusMessage = static_cast<u8>(TagFieldGetNumber(TagFieldFind(pMsg->pData, "NUCLEUSMSG"), 0));
    if (pLogin->bAddNucleusMessage)
        pLogin->bAddNucleusMessage = (pLogin->bNucleusMessage == 0);
    if (pLogin->Personas.iNumNames > 1 && !pLogin->bMultiPers)
        pLogin->Personas.iNumNames = 1;
    if (!pLogin->Personas.iNumNames)
    {
        NetPrintfCode("lobbylogin: auth response has an empty persona list\n");
        pLogin->bAutoLogin = 0;
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
        pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_INVALID;
        pMsg->code = 0x64617368;
        pLogin->uLastCode = 0x64617368;
    }
    else if ((pLogin->Personas.iNumNames == 1 && !pLogin->bMultiPers) || pLogin->bAutoLogin == 1)
    {
        char strInit[32];
        TagFieldGetString(TagFieldFind(pLogin->strTagfBuf, "INIT"), strInit, sizeof(strInit), "");
        pLogin->strTagfBuf[0] = 0;
        if (strInit[0])
            TagFieldSetString(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "INIT", strInit);
        const char* pPersona = pLogin->Personas.iNumNames == 1 ? pLogin->Personas.strNames[0] : pLogin->AutoInfo.strNames[2];
        TagFieldSetString(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "GTAG", pPersona);
        _LobbyLoginRequest(pLogin, 0x70657273, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), _LobbyLoginPersCallback);
    }
    else
    {
        _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_SELPERS, 0);
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
    }
}
extern "C" void _LobbyLoginAcctCallback(LobbyApiRefT*, LobbyApiMsgT*, void*);

static bool _LobbyLoginValidateLength(const char* pField, s32 iMin, s32 iMax)
{
    if (!pField)
        return false;
    char strTemp[64];
    TagFieldGetString(pField, strTemp, sizeof(strTemp), "");
    const s32 iLength = static_cast<s32>(strlen(strTemp));
    return iLength >= iMin && iLength <= iMax;
}

static bool _LobbyLoginValidateEmail(const char* pField)
{
    if (!pField)
        return false;
    char strTemp[64];
    TagFieldGetString(pField, strTemp, sizeof(strTemp), "");
    if (strTemp[0] == '@')
        return false;
    const char* pAt = strchr(strTemp, '@');
    return pAt && pAt[1] != '.' && strchr(pAt + 1, '.') != 0;
}

extern "C" void _LobbyLoginCreateAccount(LobbyLoginRefT* pLogin, s32 eContext)
{
    char strMsg[1024];
    strMsg[0] = 0;
    bool bMailLength = true, bMailForm = true, bPassLength = true;
    TagFieldMerge(strMsg, sizeof(strMsg), pLogin->strTagfBuf);
    sub_82B204B0(pLogin, strMsg, sizeof(strMsg), 1);
    TagFieldSetString(strMsg, sizeof(strMsg), "GTAG", pLogin->strUser);
    if (pLogin->strSpam[0])
        TagFieldSetString(strMsg, sizeof(strMsg), "SPAM", pLogin->strSpam);
    if (TagFieldFind(strMsg, "MAIL"))
    {
        bMailLength = _LobbyLoginValidateLength(TagFieldFind(strMsg, "MAIL"), 7, 64);
        bMailForm = _LobbyLoginValidateEmail(TagFieldFind(strMsg, "MAIL"));
    }
    if (TagFieldFind(strMsg, "PASS"))
        bPassLength = _LobbyLoginValidateLength(TagFieldFind(strMsg, "PASS"), 4, 16);
    if (pLogin->bAutoCreate || (bMailLength && bPassLength && bMailForm))
    {
        pLogin->bAddNucleusMessage = 1;
        _LobbyLoginRequest(pLogin, 0x61636374, strMsg, sizeof(strMsg), _LobbyLoginAcctCallback);
    }
    else
    {
        NetPrintfCode("Alert on : %s", strMsg);
        pLogin->eAlertContext = static_cast<LobbyLoginContextE>(eContext);
        pLogin->bAutoLogin = 0;
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
        if (bMailLength && bMailForm)
        {
            if (!bPassLength)
            {
                pLogin->uLastCode = 0x69707373;
                TagFieldDelete(pLogin->strRespBuf, "MAIL");
                TagFieldSetRaw(pLogin->strRespBuf, sizeof(pLogin->strRespBuf), "MAIL", TagFieldFind(strMsg, "MAIL"));
            }
        }
        else
            pLogin->uLastCode = 0x6d61696c;
    }
}
extern "C" void _LobbyLoginAcctCallback(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyLoginRefT* pLogin = static_cast<LobbyLoginRefT*>(pUserData);
    _LobbyLoginSaveResponse(pLogin, pMsg);
    if (!pMsg->code)
    {
        _LobbyLoginRequest(pLogin, 0x61757468, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), _LobbyLoginAuthCallback);
        _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_LOGIN, 0);
    }
    else if (pMsg->code == 0x70617373)
    {
        if (pLogin->eContextStack[pLogin->iContextDepth] == LOBBYLOGIN_CONTEXT_DUPACCT)
        {
            pLogin->bAutoLogin = 0;
            pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
            pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_DUPACCT;
        }
        else
        {
            pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
            _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_DUPACCT, 0);
        }
        pLogin->bAutoLogin = 0;
    }
    else if (pMsg->code == 0x706d616c)
    {
        _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_PMAIL, 0);
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_IDLE;
        pLogin->bAutoLogin = 0;
    }
    else
    {
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
        if (pMsg->kind == 0x64697363)
        {
            pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_OFFLINE;
            pLogin->bAutoLogin = 0;
            if (pMsg->code != 0x646f776e && pMsg->code != 0x74696d65)
                pLogin->uLastCode = 0x64697363;
        }
        else
        {
            pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_INVALID;
            pLogin->bAutoLogin = 0;
        }
    }
}
static void _LobbyLoginLostPasswordCallback(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyLoginRefT* pLogin = static_cast<LobbyLoginRefT*>(pUserData);
    _LobbyLoginSaveResponse(pLogin, pMsg);
    if (pMsg->code)
    {
        const s32 c = pMsg->code;
        NetPrintfCode("lobbylogin: got unexpected response '%c%c%c%c' from 'lost' request\n", c >> 24, (c >> 16) & 255, (c >> 8) & 255, c & 255);
    }
    else
        pLogin->uLastCode = 0x736d616c;
    pLogin->bAutoLogin = 0;
    pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_DUPACCT;
    pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
}

static void _LobbyLoginPMailCallback(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyLoginRefT* pLogin = static_cast<LobbyLoginRefT*>(pUserData);
    _LobbyLoginSaveResponse(pLogin, pMsg);
    if (!pMsg->code)
        pLogin->uLastCode = 0x6170656e;
    else if (pMsg->code != 0x706d616c)
    {
        const s32 c = pMsg->code;
        NetPrintfCode("lobbylogin: got unexpected response '%c%c%c%c' from 'pmal' request\n", c >> 24, (c >> 16) & 255, (c >> 8) & 255, c & 255);
    }
    else
        pLogin->uLastCode = 0x706d616c;
    pLogin->bAutoLogin = 0;
    pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_PMAIL;
    pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
}
extern "C" s32 ds_strnicmp(const char*, const char*, u32);
extern "C" s32 LobbyApiConnect(LobbyApiRefT*, const char*, s32, LobbyApiCallbackT*, void*, u32);

extern "C" s32 LobbyLoginSubmitCB(LobbyLoginRefT* pLogin, s32 eContext, const char* pPacket,
                                  LobbyLoginCallbackT* pCallback, void* pCallbackData)
{
    const s32 eCurrent = pLogin->eContextStack[pLogin->iContextDepth];
    if (eContext != eCurrent)
    {
        NetPrintfCode("lobbylogin: attempt to submit in context %d, when current context is %d\n", eContext, eCurrent);
        return -1;
    }
    if (pLogin->eCurStatus != LOBBYLOGIN_STATUS_IDLE && pLogin->eCurStatus != LOBBYLOGIN_STATUS_ALERT)
    {
        NetPrintfCode("lobbylogin: attempt to submit info when not in idle state. context = %d\n", eContext);
        return -2;
    }
    if (eContext == LOBBYLOGIN_CONTEXT_CONNECT)
    {
        char strName[64];
        TagFieldGetString(TagFieldFind(pPacket, "NAME"), strName, sizeof(strName), "");
        const s32 iPort = TagFieldGetNumber(TagFieldFind(pPacket, "PORT"), 0);
        const s32 iTimeout = TagFieldGetNumber(TagFieldFind(pPacket, "TIME"), 120000);
        const s32 iResult = LobbyApiConnect(pLogin->pLobbyApi, strName, iPort, 0, 0, iTimeout);
        if (iResult < 0)
        {
            NetPrintfCode("lobbylogin: LobbyApiConnect() returned %d", iResult);
            return -1;
        }
        pLogin->eCurStatus = LOBBYLOGIN_STATUS_NETWORK;
    }
    if (eContext == LOBBYLOGIN_CONTEXT_LOGIN)
    {
        TagFieldDupl(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), pPacket);
        TagFieldGetString(TagFieldFind(pPacket, "GTAG"), pLogin->strUser, sizeof(pLogin->strUser), "");
        pLogin->bMultiPers = static_cast<u8>(TagFieldGetNumber(TagFieldFind(pLogin->strTagfBuf, "MULTIPERS"), 0));
        TagFieldDelete(pLogin->strTagfBuf, "MULTIPERS");
        pLogin->strCheatResp[0] = 0;
        _LobbyLoginDoLogin(pLogin, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf));
    }
    if (eContext == LOBBYLOGIN_CONTEXT_CREATE)
    {
        TagFieldMerge(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), pPacket);
        _LobbyLoginCreateAccount(pLogin, LOBBYLOGIN_CONTEXT_CREATE);
    }
    if (eContext == LOBBYLOGIN_CONTEXT_DEACTIVATED)
    {
        const char* pMail = TagFieldFind(pPacket, "MAIL");
        if (pMail && pLogin->strDeacEmail[0])
        {
            char strMail[64];
            TagFieldGetString(pMail, strMail, sizeof(strMail), "");
            if (!ds_strnicmp(strMail, pLogin->strDeacEmail, 64))
            {
                pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_DEACTIVATED;
                pLogin->bAutoLogin = 0;
                pLogin->uLastCode = 0x6465656d;
                pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
                goto done;
            }
        }
        TagFieldMerge(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), pPacket);
        TagFieldSetString(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "SPAM", pLogin->strSpam);
        TagFieldSetNumber(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "TOS", 1);
        TagFieldSetNumber(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "SHARE", 1);
        _LobbyLoginCreateAccount(pLogin, LOBBYLOGIN_CONTEXT_DEACTIVATED);
    }
    if (eContext == LOBBYLOGIN_CONTEXT_TOS)
    {
        TagFieldSetRaw(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "TOS", TagFieldFind(pPacket, "TOS"));
        if (pLogin->uLastCode == 0x746f7361)
        {
            TagFieldSetNumber(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "SHARE", 0);
            _LobbyLoginDoLogin(pLogin, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf));
            goto done;
        }
        _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_SHARE, 0);
    }
    if (eContext == LOBBYLOGIN_CONTEXT_SHARE)
    {
        TagFieldSetRaw(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "SPAM", TagFieldFind(pPacket, "SPAM"));
        TagFieldSetNumber(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "SHARE", 1);
        _LobbyLoginDoLogin(pLogin, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf));
    }
    if (eContext == LOBBYLOGIN_CONTEXT_INVREGKEY)
    {
        if (pLogin->uLastKind == 0x61757468)
        {
            TagFieldSetRaw(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "REGKEY", TagFieldFind(pPacket, "REGKEY"));
            _LobbyLoginDoLogin(pLogin, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf));
            goto done;
        }
        if (pLogin->uLastKind == 0x70657273)
        {
            TagFieldSetRaw(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), "ENTL_MASK", TagFieldFind(pPacket, "ENTL_MASK"));
            _LobbyLoginRequest(pLogin, 0x70657273, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), _LobbyLoginPersCallback);
            goto done;
        }
        const u32 c = pLogin->uLastKind;
        NetPrintfCode("lobbylogin: error - submitting in INVREGKEY context after '%c%c%c%c' command\n", c >> 24, (c >> 16) & 255, (c >> 8) & 255, c & 255);
    }
    if (eContext == LOBBYLOGIN_CONTEXT_SELPERS)
    {
        TagFieldDupl(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), pPacket);
        _LobbyLoginRequest(pLogin, 0x70657273, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), _LobbyLoginPersCallback);
    }
    if (eContext == LOBBYLOGIN_CONTEXT_DUPACCT)
    {
        const char* pMail = TagFieldFind(pPacket, "MAIL");
        if (pMail && pLogin->strDeacEmail[0])
        {
            char strMail[64];
            TagFieldGetString(pMail, strMail, sizeof(strMail), "");
            if (!ds_strnicmp(strMail, pLogin->strDeacEmail, 64))
            {
                pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_DUPACCT;
                pLogin->bAutoLogin = 0;
                pLogin->uLastCode = 0x6465656d;
                pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
                goto done;
            }
        }
        TagFieldMerge(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), pPacket);
        if (TagFieldFind(pPacket, "LOST"))
        {
            _LobbyLoginRequest(pLogin, 0x6c6f7374, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), _LobbyLoginLostPasswordCallback);
            goto done;
        }
        _LobbyLoginCreateAccount(pLogin, LOBBYLOGIN_CONTEXT_DUPACCT);
    }
    if (eContext == LOBBYLOGIN_CONTEXT_NUCLEUS_WELCOME)
        _LobbyLoginSetContext(pLogin, LOBBYLOGIN_CONTEXT_DONE, 0);
    if (eContext == LOBBYLOGIN_CONTEXT_PMAIL)
    {
        TagFieldMerge(pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), pPacket);
        const bool bLength = _LobbyLoginValidateLength(TagFieldFind(pLogin->strTagfBuf, "PMAIL"), 7, 64);
        const bool bEmail = _LobbyLoginValidateEmail(TagFieldFind(pLogin->strTagfBuf, "PMAIL"));
        if (bLength && bEmail)
            _LobbyLoginRequest(pLogin, 0x61636374, pLogin->strTagfBuf, sizeof(pLogin->strTagfBuf), _LobbyLoginPMailCallback);
        else
        {
            pLogin->eAlertContext = LOBBYLOGIN_CONTEXT_PMAIL;
            pLogin->bAutoLogin = 0;
            pLogin->uLastCode = 0x706d616c;
            pLogin->eCurStatus = LOBBYLOGIN_STATUS_ALERT;
        }
    }
done:
    if (pLogin->bAutoLogin != 1 || pCallback)
    {
        if (pLogin->eCurStatus == LOBBYLOGIN_STATUS_NETWORK || pLogin->eContextStack[pLogin->iContextDepth] != eContext)
        {
            pLogin->eCallbackContext = static_cast<LobbyLoginContextE>(eContext);
            pLogin->pCallbackProc = pCallback;
            pLogin->pCallbackData = pCallbackData;
        }
        else
            pLogin->eCallbackContext = LOBBYLOGIN_CONTEXT_INVALID;
    }
    return 0;
}
// X360 address/name remap parsers at ARTIST 82B1FD80 and 82B20070.
extern "C" s32 sub_82B1FD80(LobbyLoginRefT*, const char*);
extern "C" s32 sub_82B20070(LobbyLoginRefT*, const char*);

extern "C" void sub_82B20390(LobbyLoginRefT* pLogin)
{
    const char* pConfig = static_cast<const char*>(LobbyApiInfo(pLogin->pLobbyApi, 0x636f6e66));
    char strRemap[512];
    TagFieldGetString(TagFieldFind(pConfig, "XBOX_ADDR_REMAP"), strRemap, sizeof(strRemap), "");
    if (strRemap[0])
        sub_82B1FD80(pLogin, strRemap);
    TagFieldGetString(TagFieldFind(pConfig, "XBOX_NAME_REMAP"), strRemap, sizeof(strRemap), "");
    if (strRemap[0])
        sub_82B20070(pLogin, strRemap);
}
#include <xtl.h>
#include "platform.h"
extern "C" s32 NetConnQuery(const char*, NetConfigRecT*, s32);
extern "C" s32 Utf8EncodeFromUCS2(char*, s32, const u16*);
typedef char LoginNativeProfileSizeCheck[sizeof(GET_USER_INFO_RESPONSE) == 60 ? 1 : -1];
typedef char LoginNativeProfileNameCheck[offsetof(GET_USER_INFO_RESPONSE, wszFirstName) == 2 ? 1 : -1];
typedef char LoginNativeProfileAgeCheck[offsetof(GET_USER_INFO_RESPONSE, bAge) == 59 ? 1 : -1];

static void _LobbyLoginSetUCS2Field(char* pBuf, s32 iSize, const char* pName, const WCHAR* pValue)
{
    char strValue[256] = {0};
    if (pValue)
        Utf8EncodeFromUCS2(strValue, sizeof(strValue), reinterpret_cast<const u16*>(pValue));
    TagFieldSetString(pBuf, iSize, pName, strValue);
}

extern "C" void sub_82B204B0(LobbyLoginRefT* pLogin, char* pBuf, s32 iSize, s32 bAccount)
{
    if (!pLogin->strUser[0])
        return;
    const s32 iUser = NetConnQuery(pLogin->strUser, 0, 0);
    if (iUser < 0)
    {
        NetPrintfCode("lobbylogin: unable to resolve index for user '%s'\n", pLogin->strUser);
        return;
    }
    XUID xuid;
    NetConnStatus(0x78756964, iUser, &xuid, sizeof(xuid));
    TagFieldSetBinary(pBuf, iSize, "XUID", &xuid, sizeof(xuid));
    TagFieldSetBinary(pBuf, iSize, "PUID", &xuid, sizeof(xuid));
    TagFieldSetBinary(pBuf, iSize, "MID", &xuid, sizeof(xuid));
    XNADDR addr;
    NetConnStatus(0x78616464, 0, &addr, sizeof(addr));
    char strEncoded[64];
    char strAddress[64];
    char strMid[64];
    strEncoded[0] = 0;
    TagFieldSetBinary7(strEncoded, sizeof(strEncoded), "M", &addr, sizeof(addr));
    TagFieldGetString(TagFieldFind(strEncoded, "M"), strAddress, sizeof(strAddress), "");
    TagFieldGetString(TagFieldFind(pBuf, "MID"), strMid, sizeof(strMid), "");
    strcat(strMid, strAddress);
    TagFieldSetString(pBuf, iSize, "MADDR", strMid);
    GET_USER_INFO_RESPONSE* pInfo;
    const s32 iInfoResult = NetConnStatus(0x786e666f, iUser, &pInfo, sizeof(pInfo));
    if (iInfoResult < 0)
        pLogin->bFailBillingServers = 1;
    else
    {
        SYSTEMTIME time;
        GetLocalTime(&time);
        const s32 iDay = time.wMonth == 2 && time.wDay == 29 ? 28 : time.wDay;
        char strBorn[16];
        ds_snzprintf(strBorn, sizeof(strBorn), "%4d%02d%02d", time.wYear - pInfo->bAge, time.wMonth, iDay);
        TagFieldSetString(pBuf, iSize, "BORN", strBorn);
        const u32 uLocale = static_cast<u32>(NetConnStatus(0x6c6f636c, iUser, 0, 0));
        char strLocale[5] = { char(uLocale >> 24), char(uLocale >> 16), char(uLocale >> 8), char(uLocale), 0 };
        NetPrintfCode("lobbylogin: locale=%s\n", strLocale);
        TagFieldSetToken(pBuf, iSize, "LOC", uLocale);
    }
    if (bAccount == 1 && iInfoResult >= 0)
    {
        _LobbyLoginSetUCS2Field(pBuf, iSize, "FIRSTNAME", pInfo->wszFirstName);
        _LobbyLoginSetUCS2Field(pBuf, iSize, "LASTNAME", pInfo->wszLastName);
        _LobbyLoginSetUCS2Field(pBuf, iSize, "STREET1", pInfo->addressInfo.wszStreet1);
        _LobbyLoginSetUCS2Field(pBuf, iSize, "STREET2", pInfo->addressInfo.wszStreet2);
        _LobbyLoginSetUCS2Field(pBuf, iSize, "DISTRICT", pInfo->addressInfo.wszDistrict);
        _LobbyLoginSetUCS2Field(pBuf, iSize, "CITY", pInfo->addressInfo.wszCity);
        _LobbyLoginSetUCS2Field(pBuf, iSize, "STATE", pInfo->addressInfo.wszState);
        _LobbyLoginSetUCS2Field(pBuf, iSize, "ZIP", pInfo->addressInfo.wszPostalCode);
        if (!TagFieldFind(pBuf, "MAIL"))
            _LobbyLoginSetUCS2Field(pBuf, iSize, "MAIL", pInfo->wszEmail);
        if (!TagFieldFind(pBuf, "PASS"))
            TagFieldSetString(pBuf, iSize, "PASS", pLogin->strAutoPassword);
        TagFieldSetNumber(pBuf, iSize, "PARTOPTIN", pInfo->bParterOptIn);
        TagFieldSetNumber(pBuf, iSize, "MSFTOPTIN", pInfo->bMsftOptIn);
    }
}
extern "C" u32 SocketHtonl(u32);
typedef char LoginNativeServiceSizeCheck[sizeof(XONLINE_SERVICE_INFO) == 12 ? 1 : -1];

extern "C" u32 sub_82B1FCE0(LobbyLoginRefT*, u32 uServiceId)
{
    XONLINE_SERVICE_INFO service;
    for (s32 i = 0; i < 4; ++i)
        if (NetConnStatus(0x73657276, i, &service, sizeof(service)) >= 0 && service.dwServiceID == uServiceId)
            return SocketHtonl(service.serviceIP.s_addr);
    NetPrintfCode("lobbylogin: could not map serviceID 0x%08x - using fallback method\n", uServiceId);
    return static_cast<u32>(NetConnStatus(0x686f7374, 0, 0, 0));
}

extern "C" u32 SocketInTextGetAddr(const char* text);
extern "C" const char* SocketInAddrGetText(u32 address);

static bool _LobbyLoginIsDecimalDigit(char value)
{
    const u8 uValue = (u8)value;
    return (uValue >= (u8)'0') && (uValue <= (u8)'9');
}

static u16 _LobbyLoginReadPortStart(const char** ppCursor)
{
    const char* pCursor = *ppCursor;
    u16 uPort = 0;
    for (;;)
    {
        const char cValue = *pCursor++;
        if (!_LobbyLoginIsDecimalDigit(cValue))
        {
            break;
        }
        uPort = (u16)(10 * uPort + ((u8)cValue - (u8)'0'));
    }
    *ppCursor = pCursor;
    return uPort;
}

static u16 _LobbyLoginReadPortEnd(const char** ppCursor)
{
    const char* pCursor = *ppCursor;
    u16 uPort = 0;
    while (_LobbyLoginIsDecimalDigit(*pCursor))
    {
        uPort = (u16)(10 * uPort + ((u8)*pCursor - (u8)'0'));
        ++pCursor;
    }
    *ppCursor = pCursor;
    return uPort;
}

static void _LobbyLoginReadPortRange(const char** ppCursor,
                                     u16* pPort, u16* pRemapPort)
{
    *pPort = _LobbyLoginReadPortStart(ppCursor);
    *pRemapPort = _LobbyLoginReadPortEnd(ppCursor);
}

extern "C" s32 sub_82B1FD80(LobbyLoginRefT* pLogin, const char* pText)
{
    NetPrintfCode("lobbylogin: parsing new-style remap table\n");

    // ARTIST supplies a 64-DWORD stack buffer (16 SocketAddrMapX360 records)
    // and clears its first nine entries before constructing the table.
    SocketAddrMapX360 aMappings[16];
    memset(aMappings, 0, 144);

    s32 iMappingCount = 0;
    const char* pRule = pText;
    while ((*pRule != '\0') && (iMappingCount < 8))
    {
        const char* pDelimiter = pRule;
        while ((*pDelimiter != '\0') && (*pDelimiter != ':') &&
               (*pDelimiter != '/'))
        {
            ++pDelimiter;
        }
        if (*pDelimiter == '\0')
        {
            break;
        }

        const u32 uAddress = SocketInTextGetAddr(pRule);
        const char* pPortList = NULL;
        const char* pMask;
        if (*pDelimiter == ':')
        {
            pPortList = pDelimiter + 1;
            const char* pPortSlash = pPortList;
            while ((*pPortSlash != '\0') && (*pPortSlash != '/'))
            {
                ++pPortSlash;
            }
            if (*pPortSlash == '\0')
            {
                break;
            }
            pMask = pPortSlash + 1;
        }
        else
        {
            pMask = pDelimiter + 1;
        }

        const u32 uMask = SocketInTextGetAddr(pMask);
        const char* pServiceSlash = pMask;
        while ((*pServiceSlash != '\0') && (*pServiceSlash != '/'))
        {
            ++pServiceSlash;
        }
        if (*pServiceSlash == '\0')
        {
            break;
        }

        // The configuration field is '/$<hex service id>'; ARTIST skips the
        // slash and dollar before calling strtol with radix 16.
        const char* pServiceID = pServiceSlash + 2;
        const u32 uServiceAddress = sub_82B1FCE0(
            pLogin, (u32)strtol(pServiceID, NULL, 16));

        if (pPortList != NULL)
        {
            const char* pPort = pPortList;
            while ((*pPort != '\0') && (*pPort != '/'))
            {
                if (iMappingCount >= 16)
                {
                    break;
                }
                SocketAddrMapX360* pMapping = &aMappings[iMappingCount++];
                pMapping->address = uAddress;
                pMapping->mask = uMask;
                pMapping->remap = uServiceAddress;
                _LobbyLoginReadPortRange(&pPort,
                                         &pMapping->port,
                                         &pMapping->remapPort);
                while ((*pPort != '\0') && (*pPort != ',') && (*pPort != '/'))
                {
                    ++pPort;
                }
                if (*pPort != ',')
                {
                    break;
                }
                ++pPort;
            }
        }

        if (iMappingCount < 16)
        {
            SocketAddrMapX360* pMapping = &aMappings[iMappingCount++];
            pMapping->address = uAddress;
            pMapping->mask = uMask;
            pMapping->remap = uServiceAddress;
            pMapping->port = 0;
            pMapping->remapPort = 0;
        }

        const char* pNextRule = pServiceID;
        while ((*pNextRule != '\0') && (*pNextRule != ','))
        {
            ++pNextRule;
        }
        if (*pNextRule == '\0')
        {
            break;
        }
        pRule = pNextRule + 1;
    }

    for (s32 iIndex = 0; iIndex < iMappingCount; ++iIndex)
    {
        const SocketAddrMapX360* pMapping = &aMappings[iIndex];
        NetPrintfCode("  %d) addr=%s, ", iIndex,
                      SocketInAddrGetText(pMapping->address));
        NetPrintfCode("mask=%s, ", SocketInAddrGetText(pMapping->mask));
        NetPrintfCode("remap=%s ", SocketInAddrGetText(pMapping->remap));
        NetPrintfCode("port=%d,%d\n", pMapping->port, pMapping->remapPort);
    }

    if (iMappingCount > 0 && NetConnStatus(0x73656375, 0, NULL, 0) != 0)
    {
        NetPrintfCode("lobbylogin: setting remap table\n");
        NetConnControl(0x786d6170, iMappingCount, 0, aMappings, NULL); // xmap
        return NetConnControl(0x78646e73, (s32)aMappings[0].address,
                              0, NULL, NULL); // xdns
    }
    return 0;
}

extern "C" s32 sub_82B20070(LobbyLoginRefT* pLogin, const char* pText)
{
    NetPrintfCode("lobbylogin: parsing new-style remap table\n");

    // The native frame contains a 0x400-byte table workspace and clears nine
    // 104-byte SocketNameMapX360 entries (eight usable entries plus sentinel).
    u8 aMapStorage[1024];
    memset(aMapStorage, 0, 936);
    SocketNameMapX360* pMappings = (SocketNameMapX360*)aMapStorage;

    s32 iMappingCount = 0;
    const char* pRule = pText;
    while ((*pRule != '\0') && (iMappingCount < 8))
    {
        char strMatch[64];
        u32 uMatchLength = 0;
        const char* pDelimiter = pRule;
        while ((*pDelimiter != '\0') && (*pDelimiter != ':') &&
               (*pDelimiter != '/'))
        {
            if (uMatchLength + 1 < sizeof(strMatch))
            {
                strMatch[uMatchLength] = *pDelimiter;
                ++uMatchLength;
            }
            ++pDelimiter;
        }
        if (*pDelimiter == '\0')
        {
            break;
        }
        strMatch[uMatchLength] = '\0';

        const char* pPortList = NULL;
        const char* pRemap;
        if (*pDelimiter == ':')
        {
            pPortList = pDelimiter + 1;
            const char* pPortSlash = pPortList;
            while ((*pPortSlash != '\0') && (*pPortSlash != '/'))
            {
                ++pPortSlash;
            }
            if (*pPortSlash == '\0')
            {
                break;
            }
            pRemap = pPortSlash + 1;
        }
        else
        {
            pRemap = pDelimiter + 1;
        }

        char strRemapName[32];
        u32 uRemapAddress = 0;
        const char* pNextRule = pRemap;
        if (*pRemap == '$')
        {
            uRemapAddress = sub_82B1FCE0(
                pLogin, (u32)strtol(pRemap + 1, NULL, 16));
            strRemapName[0] = '\0';
            ++pNextRule;
        }
        else
        {
            u32 uNameLength = 0;
            while ((*pNextRule != '\0') && (*pNextRule != ','))
            {
                if (uNameLength + 1 < sizeof(strRemapName))
                {
                    strRemapName[uNameLength] = *pNextRule;
                    ++uNameLength;
                }
                ++pNextRule;
            }
            strRemapName[uNameLength] = '\0';
        }

        if (pPortList != NULL)
        {
            const char* pPort = pPortList;
            while ((*pPort != '\0') && (*pPort != '/'))
            {
                if (iMappingCount >= 9)
                {
                    break;
                }
                SocketNameMapX360* pMapping = &pMappings[iMappingCount++];
                ds_strnzcpy(pMapping->match, strMatch, sizeof(pMapping->match));
                ds_strnzcpy(pMapping->remapName, strRemapName,
                            sizeof(pMapping->remapName));
                pMapping->remapAddress = uRemapAddress;
                _LobbyLoginReadPortRange(&pPort,
                                         &pMapping->port,
                                         &pMapping->remapPort);
                while ((*pPort != '\0') && (*pPort != ',') && (*pPort != '/'))
                {
                    ++pPort;
                }
                if (*pPort != ',')
                {
                    break;
                }
                ++pPort;
            }
        }

        if (iMappingCount < 9)
        {
            SocketNameMapX360* pMapping = &pMappings[iMappingCount++];
            ds_strnzcpy(pMapping->match, strMatch, sizeof(pMapping->match));
            ds_strnzcpy(pMapping->remapName, strRemapName,
                        sizeof(pMapping->remapName));
            pMapping->remapAddress = uRemapAddress;
            pMapping->port = 0;
            pMapping->remapPort = 0;
        }

        while ((*pNextRule != '\0') && (*pNextRule != ','))
        {
            ++pNextRule;
        }
        if (*pNextRule == '\0')
        {
            break;
        }
        pRule = pNextRule + 1;
    }

    for (s32 iIndex = 0; iIndex < iMappingCount; ++iIndex)
    {
        const SocketNameMapX360* pMapping = &pMappings[iIndex];
        NetPrintfCode("  %d) dnsname=%s, ", iIndex, pMapping->match);
        NetPrintfCode("name=%s, ", pMapping->remapName);
        NetPrintfCode("addr=%s ", SocketInAddrGetText(pMapping->remapAddress));
        NetPrintfCode("port=%d,%d\n", pMapping->port, pMapping->remapPort);
    }

    if (iMappingCount > 0 && NetConnStatus(0x73656375, 0, NULL, 0) != 0)
    {
        NetPrintfCode("lobbylogin: setting remap table\n");
        return NetConnControl(0x786e616d, iMappingCount, 0,
                              aMapStorage, NULL); // xnam
    }
    return 0;
}
