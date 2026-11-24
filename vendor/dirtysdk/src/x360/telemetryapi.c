// Native DirtySDK 5.5.3 telemetry state and ring operations.
// Layout: DecFIGS telemetryapi.c/.h; ARTIST 82B10B88, 82B11160, 82B11238.
// XDK size optimization matches ARTIST at 0x82B10C00 and 0x82B11BF8.
// C, as the original (DWARF home core/source/misc/telemetryapi.c); the definitions follow its
// DWARF line order (Create :999 ... PushBack :2081), which fixes the order of the module's
// unpooled literals (TelemetryApiDestroy 0x82B11C68 reaches its warning text 0x20 below its
// anchor) and their split between .data and .bss (TelemetryApiAuthent's two "" at 0x8324E911/12).
// The static helpers carry their DWARF names (_TelemetryApi*).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "telemetryapi.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include "lobbytagfield.h"
#include "platform.h"
#include <stddef.h>
#include <string.h>

typedef struct ProtoAriesRefT ProtoAriesRefT;   // DirtySDK is C
struct ProtoAriesRefT;
typedef void (TelemetryApiBeginTransactionCBT)(TelemetryApiRefT*, void*, s32);
typedef struct TelemetryApiFilterRuleT TelemetryApiFilterRuleT;   // DirtySDK is C
struct TelemetryApiFilterRuleT { s32 iEnable; u32 uModuleID; u32 uGroupID; };
typedef struct TelemetryApiCallbackDataT TelemetryApiCallbackDataT;   // DirtySDK is C
struct TelemetryApiCallbackDataT { TelemetryApiCallbackT* pCallback; void* pUserData; };
typedef struct TelemetryApiRefT TelemetryApiRefT;   // DirtySDK is C
struct TelemetryApiRefT
{
    ProtoAriesRefT* pAries;
    s32 iMemGroup;
    char strAuth[128];
    u32 uAddr;
    s32 iPort, bCanConnect, bFullSendPending;
    char strUserLocale[4];
    char strDisabledCountries[1024];
    s32 bCountryDisabled, bUserDisabled, bTransactionBegPending;
    TelemetryApiBeginTransactionCBT* pTransactionBegCB;
    void* pTransactionBegUserData;
    u32 uTickStartTime, uLastServerSendTime;
    char* pServerString;
    u32 uServerStringSize;
    char* pTagString;
    u32 uTagStringSize;
    char cEntryPrepender, cPad1, cPad2, cPad3;
    u32 uNumFilters;
    TelemetryApiFilterRuleT Filters[32];
    TelemetryApiCallbackDataT callbacks[3];
    TelemetryApiBufferTypeE eBufferType;
    u32 uEventBufSize, uEventHead, uEventTail, uEventState, uEventThreshold, uEventTimeout;
    TelemetryApiEventT* Events;
};
typedef char TelemetryCallbacksOffset[(offsetof(TelemetryApiRefT, callbacks) == 0x650) ? 1 : -1];
typedef char TelemetryEventsOffset[(offsetof(TelemetryApiRefT, Events) == 0x684) ? 1 : -1];

char* ds_strnzcpy(char* pDest, const char* pSource, s32 iLength);
ProtoAriesRefT* ProtoAriesCreate(s32 iInputSize);
s32 ProtoAriesUnconnect(ProtoAriesRefT* pAries);
void ProtoAriesDestroy(ProtoAriesRefT* pAries);
u32 NetTick(void);
s32 ds_strnicmp(const char* pA, const char* pB, s32 iCount);
s32 ProtoAriesStatus(ProtoAriesRefT*, s32, void*, s32);
s32 ProtoAriesConnect(ProtoAriesRefT*, const char*, u32, s32);
s32 ProtoAriesSend(ProtoAriesRefT*, s32, s32, const char*, s32);
void ProtoAriesUpdate(ProtoAriesRefT*);
s32 ProtoAriesPeek(ProtoAriesRefT*, s32*, s32*, char**);
s32 ProtoAriesRecv(ProtoAriesRefT*, s32*, s32*, char*, s32);

static u32 _TelemetryApiNumEvents(const TelemetryApiRefT* pRef)
{
    if (!pRef || (pRef->uEventState & 1)) return 0;
    if (pRef->uEventState & 2) return pRef->uEventBufSize;
    return (pRef->uEventTail < pRef->uEventHead ? pRef->uEventBufSize - pRef->uEventHead + pRef->uEventTail : pRef->uEventTail - pRef->uEventHead) + 1;
}

static void _TelemetryApiSendData(TelemetryApiRefT* pRef, s32 iKind)
{
    if (ProtoAriesStatus(pRef->pAries, 0x73746174, 0, 0) != 3)
    {
        TagFieldSetString(pRef->pTagString, pRef->uTagStringSize, "AUTH", pRef->strAuth);
        ProtoAriesConnect(pRef->pAries, 0, pRef->uAddr, pRef->iPort);
    }
    ProtoAriesSend(pRef->pAries, iKind, 0, pRef->pTagString, -1);
}

static void _TelemetryApiTrySendToServer(TelemetryApiRefT* pRef)
{
    u32 uNow;
    char* pOut;
    u32 i;
    if (!pRef) { NetPrintfCode("TelemetryApi: error - module reference is NULL\n"); return; }
    if (!pRef->strAuth[0]) return;
    uNow = NetTick();
    if (!_TelemetryApiNumEvents(pRef)) { pRef->uLastServerSendTime = uNow; return; }
    if (!pRef->uAddr || !pRef->iPort) return;
    if (!pRef->bCanConnect || !pRef->pAries)
    {
        NetPrintfCode("telemetryapi: TrySendToServer not allowed to connect; try calling TelemetryApiConnect");
        return;
    }
    pOut = pRef->pServerString;
    for (i = 0; i < 21; ++i)
    {
        const u32 uStartSeconds = pRef->uTickStartTime / 1000;
        TelemetryApiEventT event;
        const char* pText;
        s32 iLength;
        NetTick();
        if (!TelemetryApiPopFront(pRef, &event)) break;
        event.iTimestamp -= uStartSeconds;
        pText = event.strEvent[0] ? event.strEvent : "-";
        iLength = ds_snzprintf(pOut, 38, "%08X/%c/%c%c%c%c/%c%c%c%c/%s",
            event.iTimestamp, pRef->cEntryPrepender ? pRef->cEntryPrepender : '-',
            (signed char)(event.uModuleID >> 24), (signed char)(event.uModuleID >> 16),
            (signed char)(event.uModuleID >> 8), (signed char)event.uModuleID,
            (signed char)(event.uGroupID >> 24), (signed char)(event.uGroupID >> 16),
            (signed char)(event.uGroupID >> 8), (signed char)event.uGroupID, pText);
        pOut[iLength] = ',';
        pOut += iLength + 1;
    }
    pOut[-1] = 0;
    pRef->pTagString[0] = 0;
    TagFieldSetString(pRef->pTagString, pRef->uTagStringSize, "STR", pRef->pServerString);
    _TelemetryApiSendData(pRef, 0x266c6772);
    NetPrintfCode("telemetryapi: sending [%u] telemetry entries to the server.\n", i);
    pRef->uLastServerSendTime = uNow;
}

// Filter token parser and event-buffer compaction: ARTIST 82B102E8/82B10480/82B10710.
static s32 _TelemetryApiGetFilterTokenFromChars(const char* pChars, u32* pToken)
{
    u32 uValue;
    s32 bWildcard;
    s32 i;
    *pToken = 0;
    uValue = 0;
    bWildcard = 0;
    for (i = 0; i < 4; ++i)
    {
        const signed char c = pChars[i];
        if (!c) return -1;
        if (c != '*' && !TelemetryAlphaNumeric(c)) return -2;
        if (c == '*') bWildcard = 1;
        uValue = (uValue << 8) + c;
    }
    *pToken = uValue;
    return bWildcard && uValue != 0x2a2a2a2a ? -3 : 0;
}

static s32 TelemetryFilterCharacter(char c)
{
    return c == '*' || TelemetryAlphaNumeric((signed char)c);
}

static void _TelemetryApiSetFilter(TelemetryApiRefT* pRef, const char* pRule)
{
    s32 iState;
    TelemetryApiFilterRuleT rule = {0, 0, 0};
    if (!pRule || !*pRule) { pRef->uNumFilters = 0; memset(pRef->Filters, 0, sizeof(pRef->Filters)); }
    pRef->uNumFilters = 0;
    iState = 0;
    while (*pRule)
    {
        if (iState == 0)
        {
            while (*pRule && *pRule != '+' && *pRule != '-') ++pRule;
            if (!*pRule) break;
            if (pRef->uNumFilters >= 32) { NetPrintfCode("Too many server rules.  Remaining rules dropped: [%s]\n", pRule); break; }
            rule.iEnable = 0; rule.uModuleID = rule.uGroupID = 0;
            iState = 1;
        }
        else if (iState == 1)
        {
            rule.iEnable = *pRule == '+' ? 1 : -1;
            while (*pRule && !TelemetryFilterCharacter(*pRule)) ++pRule;
            if (*pRule) iState = 2;
        }
        else if (iState == 2)
        {
            if (_TelemetryApiGetFilterTokenFromChars(pRule, &rule.uModuleID)) iState = 0;
            else { pRule += 4; if (*pRule) iState = 3; }
        }
        else if (iState == 3)
        {
            while (*pRule && !TelemetryFilterCharacter(*pRule)) ++pRule;
            if (*pRule) iState = 4;
        }
        else
        {
            if (!_TelemetryApiGetFilterTokenFromChars(pRule, &rule.uGroupID))
            {
                pRule += 4;
                pRef->Filters[pRef->uNumFilters++] = rule;
                while (*pRule) { const char c = *pRule++; if (c == ',') break; }
            }
            iState = 0;
        }
    }
}

static void _TelemetryApiEventBufferApplyFilter(TelemetryApiRefT* pRef)
{
    s32 bKept = 0, bRemoved = 0;
    u32 uCapacity = pRef->uEventBufSize, uRead = pRef->uEventHead, uWrite = uCapacity;
    u32 n;
    for (n = 0; n < uCapacity; ++n)
    {
        TelemetryApiEventT* pEvent = &pRef->Events[uRead];
        s32 iEnabled = 1;
        u32 i;
        for (i = 0; i < pRef->uNumFilters; ++i)
        {
            const TelemetryApiFilterRuleT* r = &pRef->Filters[i];
            if ((r->uModuleID == 0x2a2a2a2a || r->uModuleID == pEvent->uModuleID) &&
                (r->uGroupID == 0x2a2a2a2a || r->uGroupID == pEvent->uGroupID)) iEnabled = r->iEnable;
        }
        if (iEnabled == 1)
        {
            if (uWrite != uCapacity)
            {
                memcpy(&pRef->Events[uWrite], pEvent, sizeof(*pEvent));
                if (++uWrite == pRef->uEventBufSize) uWrite = 0;
                memset(pEvent, 0, sizeof(*pEvent));
            }
            bKept = 1;
        }
        else
        {
            if (uWrite == uCapacity) uWrite = uRead;
            bRemoved = 1;
            memset(pEvent, 0, sizeof(*pEvent));
        }
        if (uRead == pRef->uEventTail) break;
        uCapacity = pRef->uEventBufSize;
        if (++uRead == uCapacity) uRead = 0;
    }
    if (bRemoved)
    {
        pRef->uEventState &= ~2u;
        if (!uWrite) uWrite = pRef->uEventBufSize;
        pRef->uEventTail = uWrite - 1;
    }
    if (!bKept)
    {
        pRef->uEventHead = pRef->uEventTail = 0;
        pRef->uEventState |= 1;
        if (pRef->callbacks[1].pCallback) pRef->callbacks[1].pCallback(pRef, pRef->callbacks[1].pUserData);
    }
}

static void _TelemetryApiSetCountryDisabledFlag(TelemetryApiRefT* pRef)
{
    if (!pRef->strUserLocale[0])
        NetPrintfCode("telemetryapi: user locale has not been set.\n");
    else
    {
        const char* pCountry = pRef->strDisabledCountries;
        while (*pCountry)
        {
            char strCountry[3];
            TagFieldGetString(pCountry, strCountry, sizeof(strCountry), "");
            if (!ds_strnicmp(pRef->strUserLocale, strCountry, 2))
            {
                NetPrintfCode("telemetryapi: user locale found in list of disabled countries; telemetry is disabled.\n");
                pRef->bCountryDisabled = 1;
                return;
            }
            pCountry += 3;
        }
        NetPrintfCode("telemetryapi: user locale is not restricted.\n");
    }
    pRef->bCountryDisabled = 0;
}

TelemetryApiRefT* TelemetryApiCreate(u32 uNumEvents, s32 eBufferType)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    u32 uSize;
    TelemetryApiRefT* pRef;
    NetPrintfCode("telemetryapi: create %d events buffertype %d\n", uNumEvents, eBufferType);
    if (!uNumEvents) uNumEvents = 64;
    uSize = sizeof(TelemetryApiRefT) + 2048 + sizeof(TelemetryApiEventT) * uNumEvents;
    pRef = (TelemetryApiRefT*)(DirtyMemAlloc(uSize, 0x74656c6d, iMemGroup));
    if (pRef)
    {
        memset(pRef, 0, uSize);
        pRef->iMemGroup = iMemGroup;
        pRef->uEventBufSize = uNumEvents;
        pRef->eBufferType = (TelemetryApiBufferTypeE)eBufferType;
        pRef->uTagStringSize = pRef->uServerStringSize = 1024;
        pRef->pServerString = (char*)(pRef + 1);
        pRef->pTagString = pRef->pServerString + 1024;
        pRef->Events = (TelemetryApiEventT*)(pRef->pTagString + 1024);
        pRef->uEventState = 5;
        pRef->uEventTimeout = 15000;
        pRef->uEventThreshold = 75 * uNumEvents / 100;
    }
    return pRef;
}

void TelemetryApiDestroy(TelemetryApiRefT* pRef)
{
    NetPrintfCode("telemetryapi: destroy [0x%08x]\n", pRef);
    if (pRef->pAries)
    {
        NetPrintfCode("telemetryapi: Warning - Destroy called before Disconnecting.  Calling Disconnect automatically.\n");
        TelemetryApiDisconnect(pRef);
    }
    DirtyMemFree(pRef, 0x74656c6d, pRef->iMemGroup);
}

void TelemetryApiDisconnect(TelemetryApiRefT* pRef)
{
    NetPrintfCode("telemetryapi: disconnect [0x%08x]\n", pRef);
    pRef->uLastServerSendTime = 0;
    TelemetryApiUpdate(pRef);
    pRef->bCanConnect = 0;
    if (pRef->pAries)
    {
        ProtoAriesUnconnect(pRef->pAries);
        ProtoAriesDestroy(pRef->pAries);
        pRef->pAries = 0;
    }
}

s32 TelemetryApiAuthent(TelemetryApiRefT* pRef, const char* pAuth)
{
    const char* p;
    const char* pLocaleField;
    char* pComma;
    NetPrintfCode("telemetryapi: AuthEnt [0x%08x] (%s)\n", pRef, pAuth);
    pRef->uAddr = TagFieldGetAddress(pAuth, 0);
    p = strchr(pAuth, ',');
    if (!p) { NetPrintfCode("telemetryapi: AuthEnt data does not contain port number\n"); return 0x6d697373; }
    ++p;
    pRef->iPort = TagFieldGetNumber(p, 0);
    p = strchr(p, ',');
    if (!p) { NetPrintfCode("telemetryapi: AuthEnt data does not contain user locale\n"); return 0x6d697373; }
    pLocaleField = p;
    TagFieldGetString(p + 3, pRef->strUserLocale, sizeof(pRef->strUserLocale), "");
    pComma = strchr(pRef->strUserLocale, ',');
    if (pComma) *pComma = 0;
    _TelemetryApiSetCountryDisabledFlag(pRef);
    p = strchr(pLocaleField + 1, ',');
    if (!p) { NetPrintfCode("telemetryapi: AuthEnt data does not contain auth string\n"); return 0x6d697373; }
    TagFieldGetString(p + 1, pRef->strAuth, sizeof(pRef->strAuth), "");
    pRef->uTickStartTime = NetTick();
    return 0;
}

void TelemetryApiUpdate(TelemetryApiRefT* pRef)
{
    char* pData;
    s32 iKind, iCode;
    if (!pRef->bCanConnect || !pRef->pAries) return;
    ProtoAriesUpdate(pRef->pAries);
    while (pRef->pAries && ProtoAriesPeek(pRef->pAries, &iKind, &iCode, &pData) >= 0)
    {
        ProtoAriesRecv(pRef->pAries, 0, 0, 0, 0);
        if (iKind == 0x26626567)
        {
            const s32 iIdent = TagFieldGetNumber(TagFieldFind(pData, "IDENT"), -1);
            if (pRef->bTransactionBegPending && pRef->pTransactionBegCB)
            {
                pRef->bTransactionBegPending = 0;
                pRef->pTransactionBegCB(pRef, pRef->pTransactionBegUserData, iIdent);
            }
        }
        else if (iKind == 0x266c6772)
        {
            if (iCode)
                NetPrintfCode("telemetryapi: TelemetryApi failed to send data to the telemetry server, result code '%c%c%c%c'\n",
                    (signed char)(iCode >> 24), (signed char)(iCode >> 16),
                    (signed char)(iCode >> 8), (signed char)iCode);
            else NetPrintfCode("telemetryapi: TelemetryApi successfully completed send of telemetry data to server.\n");
            if (_TelemetryApiNumEvents(pRef)) _TelemetryApiTrySendToServer(pRef);
        }
    }
    if (pRef->bFullSendPending && !_TelemetryApiNumEvents(pRef) && !ProtoAriesStatus(pRef->pAries, 0x6f627566, 0, 0))
    {
        pRef->bFullSendPending = 0;
        if (pRef->callbacks[2].pCallback) pRef->callbacks[2].pCallback(pRef, pRef->callbacks[2].pUserData);
    }
    if (!(pRef->uEventState & 4) && pRef->bCountryDisabled != 1 && pRef->bUserDisabled != 1)
    {
        const u32 uNow = NetTick();
        if (_TelemetryApiNumEvents(pRef) >= pRef->uEventThreshold || uNow - pRef->uLastServerSendTime > pRef->uEventTimeout)
            _TelemetryApiTrySendToServer(pRef);
    }
}

void TelemetryApiSetCallback(TelemetryApiRefT* pRef, s32 iType, TelemetryApiCallbackT* pCallback, void* pUserData)
{
    NetPrintfCode("telemetryapi: setcallback [0x%08x] type=%d callback=[0x%08x]\n", pRef, iType, pCallback);
    if (pRef != 0)
    {
        if (iType >= 3)
            NetPrintfCode("TelemetryApi: Unhandled telemtry callback type [%d]\n", iType);
        else
        {
            pRef->callbacks[iType].pCallback = pCallback;
            pRef->callbacks[iType].pUserData = pUserData;
        }
    }
}

s32 TelemetryApiConnect(TelemetryApiRefT* pRef)
{
    NetPrintfCode("telemetryapi: connect [0x%08x]\n", pRef);
    DirtyMemGroupEnter(pRef->iMemGroup);
    pRef->pAries = ProtoAriesCreate(0x2000);
    DirtyMemGroupLeave();
    if (!pRef->pAries) return 0;
    pRef->bCanConnect = 1;
    return 1;
}

s32 TelemetryApiStatus(TelemetryApiRefT* pRef, s32 iSelect, void* pBuf, s32 iBufSize)
{
    if (!pRef) return -1;
    switch (iSelect)
    {
    case 1835428980: return 32;
    case 1668575865: return pRef->bCountryDisabled == 0;
    case 1702261864: return pRef->uEventHead;
    case 1702261876: return pRef->uEventTail;
    case 1718971500: return (pRef->uEventState >> 1) & 1;
    case 1751215220: return (pRef->uEventState >> 2) & 1;
    case 1819239276:
        if (!pBuf || !pRef->strUserLocale[0]) return -1;
        ds_strnzcpy((char*)pBuf, pRef->strUserLocale, iBufSize);
        return 0;
    case 1835104357: return pRef->uEventBufSize;
    case 1836086393: return pRef->uEventState & 1;
    case 1851875948: return !(pRef->uEventState & 4) && pRef->bCountryDisabled != 1 && pRef->bUserDisabled != 1;
    case 1852206196: return pRef->uNumFilters;
    case 1853189477:
        if (pRef->uEventState & 1) return 0;
        if (pRef->uEventState & 2) return pRef->uEventBufSize;
        return (pRef->uEventTail < pRef->uEventHead ? pRef->uEventBufSize - pRef->uEventHead + pRef->uEventTail : pRef->uEventTail - pRef->uEventHead) + 1;
    case 1936943482: return 28 * pRef->uEventBufSize + 12;
    case 1953066341: return pRef->uEventTimeout;
    default: return -1;
    }
}

s32 TelemetryApiControl(TelemetryApiRefT* pRef, s32 iKind, s32 iValue, void* pValue)
{
    if (!pRef) return 0;
    NetPrintfCode("telemetryapi: control [0x%08x] %c%c%c%c %d\n", pRef,
        (signed char)(iKind >> 24), (signed char)(iKind >> 16),
        (signed char)(iKind >> 8), (signed char)iKind, iValue);
    switch (iKind)
    {
    case 0x6364626c:
        if (pValue) { strncpy(pRef->strDisabledCountries, (const char*)pValue, sizeof(pRef->strDisabledCountries)); _TelemetryApiSetCountryDisabledFlag(pRef); }
        break;
    case 0x68616c74:
        if (iValue) { NetPrintfCode("telemetryapi: telemetry has been halted.\n"); pRef->uEventState |= 4; }
        else { NetPrintfCode("telemetryapi: telemetry has been un-halted.\n"); pRef->uEventState &= ~4u; }
        break;
    case 0x6c6f6763:
        if (iValue)
        {
            const signed char c = (signed char)iValue;
            if (c != '+' && c != '-' && c != '~' && c != '=' && !TelemetryAlphaNumeric(c))
            { NetPrintfCode("Illegal prepend character [%c]\n", c); return -1; }
            pRef->cEntryPrepender = c;
        }
        else pRef->cEntryPrepender = 0;
        break;
    case 0x74687273:
        if ((u32)iValue <= 100) pRef->uEventThreshold = pRef->uEventBufSize * (u32)iValue / 100;
        if (!pRef->uEventThreshold) pRef->uEventThreshold = 1;
        break;
    case 0x74696d65:
        if (iValue >= 0) pRef->uEventTimeout = iValue;
        break;
    case 0x756e626c:
        if (iValue <= 0) { NetPrintfCode("telemetryapi: telemetry user-setting set to disabled; telemetry data is suspended.\n"); pRef->bUserDisabled = 1; }
        else { NetPrintfCode("telemetryapi: telemetry user-setting set to enabled.\n"); pRef->bUserDisabled = 0; }
        break;
    }
    return 0;
}

void TelemetryApiFilter(TelemetryApiRefT* pRef, const char* pRule)
{
    if (pRef) { _TelemetryApiSetFilter(pRef, pRule); _TelemetryApiEventBufferApplyFilter(pRef); }
}

static s32 TelemetryAlphaNumeric(s32 c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

void TelemetryApiEvent(TelemetryApiRefT* pRef, u32 uModuleID, u32 uGroupID, const char* pEvent)
{
    s32 i;
    s32 iEnabled;
    TelemetryApiEventT event;
    NetPrintfCode("telemetryapi: event [0x%08x] %c%c%c%c/%c%c%c%c %s\n", pRef,
        (signed char)(uModuleID >> 24), (signed char)(uModuleID >> 16),
        (signed char)(uModuleID >> 8), (signed char)uModuleID,
        (signed char)(uGroupID >> 24), (signed char)(uGroupID >> 16),
        (signed char)(uGroupID >> 8), (signed char)uGroupID, pEvent);
    if (!pRef) { NetPrintfCode("WARNING: TelemetryApiEvent() - must provide a valid telemetry pointer with the event.  No event logged.\n"); return; }
    for (i = 0; i < 4; ++i)
    {
        if (!TelemetryAlphaNumeric((signed char)(uModuleID >> (24 - i * 8))) ||
            !TelemetryAlphaNumeric((signed char)(uGroupID >> (24 - i * 8)))) return;
    }
    iEnabled = 1;
    for (i = 0; i < pRef->uNumFilters; ++i)
    {
        const TelemetryApiFilterRuleT* r = &pRef->Filters[i];
        if ((r->uModuleID == 0x2a2a2a2a || r->uModuleID == uModuleID) &&
            (r->uGroupID == 0x2a2a2a2a || r->uGroupID == uGroupID)) iEnabled = r->iEnable;
    }
    if (iEnabled != 1) return;
    memset(&event, 0, sizeof(event));
    if (pEvent)
    {
        u32 i;
        for (i = 0; i < 15 && pEvent[i]; ++i)
        {
            const signed char c = pEvent[i];
            event.strEvent[i] = TelemetryAlphaNumeric(c) || c == '.' || c == '/' || c == '+' || c == '-' || c == '=' || c == '_' || c == '~' ? c : '_';
        }
    }
    event.uModuleID = uModuleID; event.uGroupID = uGroupID;
    event.iTimestamp = NetTick() / 1000;
    TelemetryApiPushBack(pRef, &event);
}

// ARTIST 82B11088, 82B10250 and 82B11498.
TelemetryApiEventT* TelemetryApiPopFront(TelemetryApiRefT* pRef, TelemetryApiEventT* pEvent)
{
    if (!pRef || (pRef->uEventState & 1)) return 0;
    pRef->uEventState &= ~2u;
    if (pEvent) memcpy(pEvent, &pRef->Events[pRef->uEventHead], sizeof(*pEvent));
    if (pRef->uEventHead == pRef->uEventTail)
    {
        pRef->uEventState |= 1;
        if (pRef->callbacks[1].pCallback) pRef->callbacks[1].pCallback(pRef, pRef->callbacks[1].pUserData);
    }
    else if (++pRef->uEventHead == pRef->uEventBufSize) pRef->uEventHead = 0;
    return pEvent;
}

TelemetryApiEventT* TelemetryApiPopBack(TelemetryApiRefT* pRef, TelemetryApiEventT* pEvent)
{
    if (!pRef || (pRef->uEventState & 1)) return 0;
    pRef->uEventState &= ~2u;
    if (pEvent) memcpy(pEvent, &pRef->Events[pRef->uEventTail], sizeof(*pEvent));
    if (pRef->uEventTail == pRef->uEventHead)
    {
        pRef->uEventState |= 1;
        if (pRef->callbacks[1].pCallback)
            pRef->callbacks[1].pCallback(pRef, pRef->callbacks[1].pUserData);
    }
    else
    {
        if (!pRef->uEventTail) pRef->uEventTail = pRef->uEventBufSize;
        --pRef->uEventTail;
    }
    return pEvent;
}

s32 TelemetryApiPushFront(TelemetryApiRefT* pRef, const TelemetryApiEventT* pEvent)
{
    u32 uState;
    if (!pRef || !pEvent) return -1;
    uState = pRef->uEventState;
    if (uState & 2)
    {
        if (pRef->eBufferType == TELEMETRY_BUFFERTYPE_FILLANDSTOP) return -2;
        if (!pRef->uEventTail) pRef->uEventTail = pRef->uEventBufSize;
        --pRef->uEventTail;
    }
    if (uState & 1)
    {
        pRef->uEventTail = 0;
        pRef->uEventState = uState & ~1u;
        pRef->uEventHead = 0;
    }
    else
    {
        if (!pRef->uEventHead) pRef->uEventHead = pRef->uEventBufSize;
        --pRef->uEventHead;
    }
    memcpy(&pRef->Events[pRef->uEventHead], pEvent, sizeof(*pEvent));
    pRef->bFullSendPending = 1;
    if ((pRef->uEventTail + 1) % pRef->uEventBufSize == pRef->uEventHead)
    {
        pRef->uEventState |= 2;
        if (pRef->callbacks[0].pCallback)
            pRef->callbacks[0].pCallback(pRef, pRef->callbacks[0].pUserData);
    }
    return 0;
}

s32 TelemetryApiPushBack(TelemetryApiRefT* pRef, const TelemetryApiEventT* pEvent)
{
    u32 uState;
    if (!pRef || !pEvent) return -1;
    uState = pRef->uEventState;
    if (uState & 2)
    {
        if (pRef->eBufferType == TELEMETRY_BUFFERTYPE_FILLANDSTOP) return -2;
        if (++pRef->uEventHead == pRef->uEventBufSize) pRef->uEventHead = 0;
    }
    if (uState & 1)
    {
        pRef->uEventTail = pRef->uEventHead = 0;
        pRef->uEventState = uState & ~1u;
    }
    else if (++pRef->uEventTail == pRef->uEventBufSize) pRef->uEventTail = 0;
    memcpy(&pRef->Events[pRef->uEventTail], pEvent, sizeof(*pEvent));
    pRef->bFullSendPending = 1;
    if ((pRef->uEventTail + 1) % pRef->uEventBufSize == pRef->uEventHead)
    {
        pRef->uEventState |= 2;
        if (pRef->callbacks[0].pCallback) pRef->callbacks[0].pCallback(pRef, pRef->callbacks[0].pUserData);
    }
    return 0;
}
