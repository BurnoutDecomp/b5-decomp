// DirtySDK 5.5.3 core/source/proto/protohttp.c; layout corroborated by ARTIST 82B24CE0/82B25050.
// XDK size optimization matches ARTIST at 0x82B255A8 and 0x82B25780.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "protohttp.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include "platform.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
typedef struct ProtoSSLRefT ProtoSSLRefT;
typedef void (ProtoHttpCustomHeaderCbT)(ProtoHttpRefT*, char*, u32, const char*, u32, void*);
struct ProtoHttpRefT
{
    ProtoSSLRefT* pSsl;
    ProtoHttpCustomHeaderCbT* pCustomHeaderCb;
    void* pCallbackRef;
    s32 iMemGroup, iPort, iSecure;
    enum
    {
        ST_IDLE,
        ST_CONN,
        ST_SEND,
        ST_RESP,
        ST_HEAD,
        ST_BODY,
        ST_DONE,
        ST_FAIL
    } eState;
    s32 iHdrCode, iHdrDate, iHeadSize, iBodySize, iRecvSize;
    char* pInpBuf;
    s32 iInpMax, iInpOff, iInpLen, iInpCnt, iChkLen, iNumRedirect, iMaxRedirect;
    char* pLocation;
    u32 uTimeout, uTimer;
    s32 iKeepAlive, iKeepAliveDflt;
    char strHdr[1024], strRequestHdr[1024], strAppendHdr[1024], strHost[256];
    u8 bTimeout, bChunked, bHeadOnly, bCloseHdr, iVerbose, bVerifyHdr, bHttp1_0, bCompactRecv;
};
typedef char HttpSizeCheck[(sizeof(ProtoHttpRefT) == 3436) ? 1 : -1];
typedef char HttpHeaderCheck[(offsetof(ProtoHttpRefT, strHdr) == 100) ? 1 : -1];
typedef char HttpStateCheck[(offsetof(ProtoHttpRefT, bTimeout) == 3428) ? 1 : -1];
ProtoSSLRefT* ProtoSSLCreate(void);
void ProtoSSLDestroy(ProtoSSLRefT* pSsl);
s32 ProtoSSLStat(ProtoSSLRefT* pSsl, s32 iSelect, void* pBuffer, s32 iLength);
s32 ProtoSSLDisconnect(ProtoSSLRefT* pSsl);
s32 ProtoHttpRecv(ProtoHttpRefT* pState, char* pBuffer, s32 iBufMin, s32 iBufMax);
char* ds_strnzcpy(char* pDest, const char* pSource, s32 iLength);
void NetPrintMemCode(const void* pMem, s32 iSize, const char* pTitle);
s32 ds_stricmp(const char*, const char*);
char* ds_stristr(const char*, const char*);
s32 ds_strnzcat(char*, const char*, s32);
u32 ds_strtotime(const char*);
void NetPrintWrapCode(const char*, s32);
u32 NetTick(void);
s32 ProtoSSLConnect(ProtoSSLRefT*, s32, const char*, u32, s32);
void ProtoSSLUpdate(ProtoSSLRefT*);
s32 ProtoSSLSend(ProtoSSLRefT*, const char*, s32);
s32 ProtoSSLRecv(ProtoSSLRefT*, char*, s32);

typedef enum ProtoHttpRequestTypeE
{
    PROTOHTTP_REQUESTTYPE_HEAD,
    PROTOHTTP_REQUESTTYPE_GET,
    PROTOHTTP_REQUESTTYPE_POST,
    PROTOHTTP_REQUESTTYPE_PUT,
    PROTOHTTP_NUMREQUESTTYPES
} ProtoHttpRequestTypeE;

static const char* _ProtoHttpParseUrl(const char* pUrl, char* pKind, s32 iKindSize, char* pHost, s32 iHostSize, s32* pPort)
{
    // Locals s/i/iPort from DWARF (protohttp.c:162-163); statement order from the PS3 -O0 body (0xECB4B8).
    const char* s;
    s32 i, iPort;
    while (*pUrl && *pUrl <= ' ') ++pUrl;
    *pKind = 0;
    for (s = pUrl; *s >= 'a' && *s <= 'z'; ++s)
        ;
    if (*s == ':' && s - pUrl < iKindSize)
    {
        for (i = 0; pUrl[i] != ':'; ++i)
            pKind[i] = pUrl[i];
        pUrl += i + 1;
        pKind[i] = 0;
    }
    while (*pUrl && *pUrl <= ' ') ++pUrl;
    if (pUrl[0] == '/' && pUrl[1] == '/') pUrl += 2;
    for (i = 0; i < iHostSize - 1; ++i)
    {
        if (*pUrl <= ' ' || *pUrl == '/' || *pUrl == '?' || *pUrl == ':') break;
        pHost[i] = *pUrl++;
    }
    pHost[i] = 0;
    iPort = 0;
    if (*pUrl == ':')
    {
        // PS3 -O0 (0xECB800): pUrl is advanced, then iPort cleared again, as a for initialiser.
        for (++pUrl, iPort = 0; *pUrl >= '0' && *pUrl <= '9'; ++pUrl)
            iPort = iPort * 10 + (*pUrl & 15);
    }
    *pPort = iPort;
    while (*pUrl && *pUrl <= ' ') ++pUrl;
    return pUrl;
}

static void _ProtoHttpClose(ProtoHttpRefT* pState, const char* pReason)
{
    if (ProtoSSLStat(pState->pSsl, 0x73746174, 0, 0) >= 0)
    {
        NetPrintfCode("protohttp: closing connection: %s\n", pReason);
        ProtoSSLDisconnect(pState->pSsl);
        pState->bCloseHdr = 0;
    }
}

static void _ProtoHttpReset(ProtoHttpRefT* pState)
{
    memset(pState->strHdr, 0, sizeof(pState->strHdr));
    memset(pState->strRequestHdr, 0, sizeof(pState->strRequestHdr));
    pState->eState = ST_IDLE;
    pState->iHdrCode = -1;
    pState->iHdrDate = 0;
    pState->iBodySize = 0;
    pState->iHeadSize = 0;
    pState->iRecvSize = 0;
    pState->iInpOff = 0;
    pState->iChkLen = 0;
    pState->iNumRedirect = 0;
    pState->pLocation = 0;
    pState->bTimeout = 0;
    pState->bChunked = 0;
    pState->bHeadOnly = 0;
}

static s32 _ProtoHttpFormatRequest(ProtoHttpRefT* pState, const char* pUrl, const char* pHost, s32 iPort, const char* pRequest, const char* pData, u32 iDataSize)
{
    s32 iLength;
    ds_snzprintf(pState->pInpBuf, pState->iInpMax, "%s %s HTTP/1.1\r\nHost: %s:%d\r\nContent-Length: %d\r\n%sUser-Agent: Custom/1.1\r\nAccept: */*\r\n",
        pRequest, pUrl, pHost, iPort, iDataSize, pState->iKeepAlive == 0 ? "Connection: Close\r\n" : "");
    if (pState->strAppendHdr[0])
    {
        char* pAgent = ds_stristr(pState->pInpBuf, "User-Agent");
        if (pAgent) { *pAgent = 0; ds_strnzcat(pState->pInpBuf, pState->strAppendHdr, pState->iInpMax); }
    }
    if (pState->pCustomHeaderCb) pState->pCustomHeaderCb(pState, pState->pInpBuf, pState->iInpMax, pData, iDataSize, pState->pCallbackRef);
    iLength = ds_strnzcat(pState->pInpBuf, "\r\n", pState->iInpMax);
    ds_strnzcpy(pState->strRequestHdr, pState->pInpBuf, sizeof(pState->strRequestHdr));
    if (pState->iVerbose) { NetPrintfCode("protohttp: sending request:\n"); NetPrintWrapCode(pState->pInpBuf, 80); }
    pState->iInpLen = iLength;
    return 0;
}

static s32 _ProtoHttpRequest(ProtoHttpRefT* pState, const char* pUrl, const char* pData, s32 iDataSize, ProtoHttpRequestTypeE eRequestType)
{
    const s32 eState = pState->eState;
    static const char requests[PROTOHTTP_NUMREQUESTTYPES][6] = {"HEAD", "GET", "POST", "PUT"};
    char strHost[256], strKind[8];
    s32 iPort = 0, bSecure;
    u8 bNewConnection = 0;
    const char* pPath;

    _ProtoHttpReset(pState);
    pPath = _ProtoHttpParseUrl(pUrl, strKind, sizeof(strKind), strHost, sizeof(strHost), &iPort);
    bSecure = ds_stricmp(strKind, "https") == 0;
    if (!iPort) iPort = bSecure ? 443 : 80;
    if (bSecure != pState->iSecure || ds_stricmp(strHost, pState->strHost) || iPort != pState->iPort)
    {
        NetPrintfCode("protohttp: requesting new connection -- url change\n");
        pState->iKeepAlive = pState->iKeepAliveDflt;
        ds_strnzcpy(pState->strHost, strHost, sizeof(pState->strHost));
        bNewConnection = 1; pState->iPort = iPort; pState->iSecure = bSecure;
    }
    if (!bNewConnection && ProtoSSLStat(pState->pSsl, 0x73746174, 0, 0) < 0)
    { NetPrintfCode("protohttp: requesting new connection -- previous connection was closed\n"); bNewConnection = 1; }
    if (!bNewConnection && eState != ST_IDLE && eState != ST_DONE)
    { NetPrintfCode("protohttp: requesting new connection -- current state of %d does not allow connection reuse\n", eState); bNewConnection = 1; }
    if (!bNewConnection)
    {
        u8 uTest;
        s32 iRecv = ProtoSSLRecv(pState->pSsl, (char*)&uTest, 1);
        if (iRecv > 0) { NetPrintfCode("protohttp: warning, recv on previous connection returned data (0x%02x)\n", uTest); iRecv = -1; }
        if (iRecv < 0) { NetPrintfCode("protohttp: requesting new connection -- received %d from previous connection\n", iRecv); bNewConnection = 1; }
    }
    if (pData && iDataSize <= 0) iDataSize = (s32)(strlen(pData));
    if (_ProtoHttpFormatRequest(pState, pPath, strHost, iPort, requests[eRequestType], pData, iDataSize) < 0) return -2;
    if (pData && iDataSize)
    {
        if (iDataSize > pState->iInpMax - pState->iInpLen) iDataSize = pState->iInpMax - pState->iInpLen;
        memcpy(pState->pInpBuf + pState->iInpLen, pData, iDataSize);
        pState->iInpLen += iDataSize;
    }
    pState->bHeadOnly = eRequestType == PROTOHTTP_REQUESTTYPE_HEAD;
    pState->uTimer = NetTick() + pState->uTimeout;
    if (bNewConnection == 1)
    {
        _ProtoHttpClose(pState, "new connection");
        ProtoSSLConnect(pState->pSsl, bSecure, strHost, 0, pState->iPort);
        pState->eState = ST_CONN;
    }
    else { NetPrintfCode("protohttp: reusing previous connection (keep-alive)\n"); pState->eState = ST_SEND; }
    ++pState->iKeepAlive;
    ProtoHttpUpdate(pState);
    return iDataSize;
}

static s32 _ProtoHttpParseHeader(ProtoHttpRefT* pState)
{
    char* pEnd = pState->pInpBuf + pState->iInpLen;
    char* p = pState->pInpBuf;
    pEnd -= 3;
    while (p != pEnd && !(p[0] == 13 && p[1] == 10 && p[2] == 13 && p[3] == 10)) ++p;
    if (p == pEnd) return -1;
    pState->iHeadSize = (s32)(p - pState->pInpBuf) + 4;
    p[2] = p[3] = 0;
    if (pState->bVerifyHdr && strncmp(pState->pInpBuf, "HTTP", 4))
    {
        NetPrintfCode("protohttp: invalid result type\n"); pState->eState = ST_FAIL; return -2;
    }
    pState->bHttp1_0 = strncmp(pState->pInpBuf, "HTTP/1.0", 8) == 0;
    if (pState->iVerbose) { NetPrintfCode("protohttp: received response header:\n"); NetPrintWrapCode(pState->pInpBuf, 80); }
    p = pState->pInpBuf;
    while (*p && *p > ' ') ++p;
    while (*p && *p <= ' ') ++p;
    pState->iHdrCode = 0;
    while (*p >= '0' && *p <= '9') { pState->iHdrCode = 10 * pState->iHdrCode + (*p & 15); ++p; }
    pState->iBodySize = -1;
    p = ds_stristr(pState->pInpBuf, "\nContent-Length:");
    if (p)
    {
        ++p;
        while (*p >= ' ' && (*p < '0' || *p > '9')) ++p;
        pState->iBodySize = 0;
        while (*p >= '0' && *p <= '9') { pState->iBodySize = 10 * pState->iBodySize + (*p & 15); ++p; }
    }
    pState->iHdrDate = 0;
    p = ds_stristr(pState->pInpBuf, "\nLast-Modified:");
    if (p) pState->iHdrDate = ds_strtotime(p + 15);
    if (ds_stristr(pState->pInpBuf, "\nTransfer-Encoding: chunked")) pState->bChunked = 1;
    pState->pLocation = ds_stristr(pState->pInpBuf, "\nLocation:");
    pState->bCloseHdr = ds_stristr(pState->pInpBuf, "\nConnection: close") != 0;
    ds_strnzcpy(pState->strHdr, pState->pInpBuf, sizeof(pState->strHdr));
    pState->iInpOff = pState->iHeadSize;
    pState->iInpCnt = pState->iInpLen - pState->iHeadSize;
    return 0;
}

static void _ProtoHttpRedirect(ProtoHttpRefT* pState)
{
    const char* pStr = pState->pLocation;
    const char* pUrl;
    s32 iUrl;

    if (!pStr)
    {
        NetPrintfCode("protohttp: no location included in redirect header\n");
        pState->eState = ST_FAIL;
        return;
    }
    if (++pState->iNumRedirect > pState->iMaxRedirect)
    {
        NetPrintfCode("protohttp: maximum number of redirections (%d) exceeded\n", pState->iMaxRedirect);
        pState->eState = ST_FAIL;
        return;
    }
    while (*pStr && *pStr != ' ') ++pStr;
    while (*pStr && *pStr <= ' ') ++pStr;
    for (iUrl = 0; pStr[iUrl] > ' '; ++iUrl)
        ;
    ++iUrl;
    pUrl = &pState->pInpBuf[pState->iInpMax - iUrl];
    while (iUrl > 0)
    {
        pState->pInpBuf[pState->iInpMax - iUrl] = *pStr++;
        --iUrl;
    }
    pState->pInpBuf[pState->iInpMax - 1] = 0;
    pState->iKeepAlive = 0;
    ProtoHttpGet(pState, pUrl, pState->bHeadOnly);
}

static s32 _ProtoHttpChunkProcess(ProtoHttpRefT* pState, s32 iBufMax)
{
    // Statement order and locals from DWARF (protohttp.c:732-744) and the PS3 -O0 body (0xECCEF8).
    s32 iChkSize, iLen;
    if (pState->iInpLen == pState->iInpOff) return 0;
    if (pState->iChkLen == 0)
    {
        char *s, *s2, *t;
        s = pState->pInpBuf + pState->iInpOff;
        t = pState->pInpBuf + pState->iInpLen - 1;
        for (s2 = s; s2 < t; s2++)
        {
            if (s2[0] == '\r' && s2[1] == '\n') break;
        }
        if (s2 == t) return 0;
        pState->iChkLen = strtol(s, NULL, 16);
        if (pState->iChkLen == 0) { pState->iInpOff = pState->iInpLen; return 0; }
        pState->iInpOff += (s32)(s2 - s) + 2;
    }
    iLen = pState->iInpLen - pState->iInpOff;
    if (iLen > iBufMax) iLen = iBufMax;
    if (iLen >= pState->iChkLen)
    {
        if (iLen >= pState->iChkLen + 2)
        {
            iChkSize = pState->iChkLen;
            pState->iChkLen = 0;
        }
        else if (pState->iInpLen == pState->iInpMax)
        {
            iChkSize = iLen / 2;
            pState->iChkLen -= iChkSize;
            pState->bCompactRecv = 1;
        }
        else
        {
            iChkSize = 0;
        }
    }
    else
    {
        iChkSize = iLen;
        pState->iChkLen -= iChkSize;
    }
    return iChkSize;
}

static s32 _ProtoHttpSend(ProtoHttpRefT* pState, const char* pStrBuf, s32 iSize)
{
    s32 iResult = ProtoSSLSend(pState->pSsl, pStrBuf, iSize);
    if (iResult > 0)
        pState->uTimer = NetTick() + pState->uTimeout;
    else if (iResult < 0)
    {
        NetPrintfCode("protohttp: error %d sending %d bytes\n", iResult, iSize);
        pState->eState = ST_FAIL;
    }
    return iResult;
}

static s32 _ProtoHttpRecv(ProtoHttpRefT* pState, char* pStrBuf, s32 iSize)
{
    s32 iResult = ProtoSSLRecv(pState->pSsl, pStrBuf, iSize);
    if (iResult > 0)
    {
        if (pState->iVerbose > 3)
            NetPrintMemCode(pStrBuf, iResult, "http-recv");
        pState->uTimer = NetTick() + pState->uTimeout;
    }
    return iResult;
}

ProtoHttpRefT* ProtoHttpCreate(s32 iBufSize)
{
    ProtoHttpRefT* pState;
    const s32 iMemGroup = DirtyMemGroupQuery();
    if (iBufSize < 4096) iBufSize = 4096;
    pState = (ProtoHttpRefT*)(DirtyMemAlloc(sizeof(ProtoHttpRefT), 0x70687470, iMemGroup));
    if (!pState) { NetPrintfCode("protohttp: unable to allocate module state\n"); return 0; }
    memset(pState, 0, sizeof(*pState));
    pState->pSsl = ProtoSSLCreate();
    if (!pState->pSsl)
    {
        NetPrintfCode("protohttp: unable to allocate ssl module\n");
        ProtoHttpDestroy(pState);
        return 0;
    }
    pState->pInpBuf = (char*)DirtyMemAlloc(iBufSize, 0x70687470, iMemGroup);
    if (!pState->pInpBuf)
    {
        NetPrintfCode("protohttp: unable to allocate protohttp buffer\n");
        ProtoHttpDestroy(pState);
        return 0;
    }
    pState->iMemGroup = iMemGroup;
    pState->iInpMax = iBufSize;
    pState->bVerifyHdr = 1;
    pState->eState = ST_IDLE;
    pState->uTimeout = 30000;
    pState->iVerbose = 1;
    pState->iMaxRedirect = 3;
    return pState;
}

void ProtoHttpDestroy(ProtoHttpRefT* pState)
{
    if (pState->pSsl) ProtoSSLDestroy(pState->pSsl);
    if (pState->pInpBuf) DirtyMemFree(pState->pInpBuf, 0x70687470, pState->iMemGroup);
    DirtyMemFree(pState, 0x70687470, pState->iMemGroup);
}

s32 ProtoHttpGet(ProtoHttpRefT* pState, const char* pUrl, u32 bHeadOnly)
{
    return _ProtoHttpRequest(pState, pUrl, 0, 0, bHeadOnly ? PROTOHTTP_REQUESTTYPE_HEAD : PROTOHTTP_REQUESTTYPE_GET);
}

s32 ProtoHttpRecv(ProtoHttpRefT* pState, char* pBuffer, s32 iBufMin, s32 iBufMax)
{
    s32 iLen;
    if (pState->eState == ST_FAIL) return -2;
    if (pState->eState != ST_BODY && pState->eState != ST_DONE) return -3;
    if (pState->bHeadOnly == 1) return -4;
    if (iBufMax < 1) return 0;
    if (iBufMin < 1) iBufMin = 1;
    if (iBufMax < iBufMin) iBufMax = iBufMin;
    if (iBufMin > pState->iInpMax) iBufMin = pState->iInpMax;
    if (iBufMax > pState->iInpMax) iBufMax = pState->iInpMax;
    if (iBufMin > pState->iInpMax - pState->iInpOff || pState->bCompactRecv == 1)
    {
        memmove(pState->pInpBuf, pState->pInpBuf + pState->iInpOff, pState->iInpLen - pState->iInpOff);
        pState->iInpLen -= pState->iInpOff;
        pState->iInpOff = 0;
        pState->bCompactRecv = 0;
        ProtoHttpUpdate(pState);
    }
    if (pState->bChunked == 1) iLen = _ProtoHttpChunkProcess(pState, iBufMax);
    else { iLen = pState->iInpLen - pState->iInpOff; if (iLen > iBufMax) iLen = iBufMax; }
    if (!iLen && pState->eState == ST_DONE) return -1;
    if (iLen < iBufMin && pState->iInpCnt != pState->iBodySize) return 0;
    if (pBuffer)
    {
        memcpy(pBuffer, pState->pInpBuf + pState->iInpOff, iLen);
        if (pState->iVerbose > 3) NetPrintMemCode(pBuffer, iLen, "http-read");
    }
    pState->iInpOff += iLen;
    if (pState->bChunked == 1 && !pState->iChkLen) pState->iInpOff += 2;
    return iLen;
}

s32 ProtoHttpRecvAll(ProtoHttpRefT* pState, char* pBuffer, s32 iBufSize)
{
    const s32 iRecvMax = iBufSize - 1;
    s32 iResult;
    while ((iResult = ProtoHttpRecv(pState, pBuffer + pState->iRecvSize, 1, iRecvMax - pState->iRecvSize)) > 0)
    {
        pState->iRecvSize += iResult;
        if (pState->iRecvSize == iRecvMax) { iResult = -5; break; }
    }
    if (iResult == -1) { pBuffer[pState->iRecvSize] = 0; return pState->iRecvSize; }
    if (iResult >= 0 || iResult == -3) return 0;
    NetPrintfCode("protohttp: error %d receiving response\n", iResult);
    return iResult;
}

void ProtoHttpAbort(ProtoHttpRefT* pState)
{
    _ProtoHttpClose(pState, "abort");
    _ProtoHttpReset(pState);
}

// DirtySDK 5.5.3 protohttp.c:1314. ARTIST retains these diagnostics at
// 0x82F7F20C..0x82F7F2A7; there is no linked entry for the control dispatch.
static s32 ProtoHttpControl(ProtoHttpRefT* pState, s32 iSelect, s32 iValue, s32 iValue2, void* pValue)
{
    if (iSelect == 'apnd')
    {
        ds_strnzcpy(pState->strAppendHdr, (const char*)pValue, sizeof(pState->strAppendHdr));
        return 0;
    }
    if (iSelect == 'disc')
    {
        _ProtoHttpClose(pState, "user request");
        return 0;
    }
    if (iSelect == 'hver')
    {
        const char* pVerify = iValue ? "enabled" : "disabled";
        NetPrintfCode("protohttp: header type verification %s\n", pVerify);
        pState->bVerifyHdr = (u8)iValue;
    }
    if (iSelect == 'keep')
    {
        NetPrintfCode("protohttp: setting keep-alive to %d\n", iValue);
        pState->iKeepAliveDflt = iValue;
        pState->iKeepAlive = iValue;
        return 0;
    }
    if (iSelect == 'rmax')
    {
        pState->iMaxRedirect = iValue;
        return 0;
    }
    if (iSelect == 'spam')
    {
        pState->iVerbose = (u8)iValue;
        return 0;
    }
    if (iSelect == 'time')
    {
        NetPrintfCode("protohttp: setting timeout to %d ms\n", iValue);
        pState->uTimeout = iValue;
        return 0;
    }
    return -1;
}

s32 ProtoHttpStatus(ProtoHttpRefT* pState, s32 iSelect, void* pBuffer, s32 iBufSize)
{
    if (iSelect == 0x73717565) return pState->pSsl ? ProtoSSLStat(pState->pSsl, iSelect, 0, 0) : -1;
    if (iSelect == 0x686f7374) { ds_strnzcpy((char*)(pBuffer), pState->strHost, iBufSize); return 0; }
    if (iSelect == 0x72747874) { ds_strnzcpy((char*)(pBuffer), pState->strRequestHdr, iBufSize); return 0; }
    if (iSelect == 0x646f6e65) return pState->eState == ST_FAIL ? -1 : pState->eState == ST_DONE;
    if (iSelect == 0x64617461)
    {
        if (pState->eState == ST_FAIL)
            return -1;
        if (pState->eState == ST_BODY || pState->eState == ST_DONE)
            return pState->iInpLen;
        return 0;
    }
    if (iSelect == 0x636f6465) return pState->iHdrCode;
    if (iSelect == 0x74696d65) return pState->bTimeout;
    if (pState->eState == ST_FAIL) return -1;
    if (pState->eState != ST_BODY && pState->eState != ST_DONE) return -2;
    if (iSelect == 0x68656164) return pState->iHeadSize;
    if (iSelect == 0x626f6479) return pState->iBodySize;
    if (iSelect == 0x64617465) return pState->iHdrDate;
    if (iSelect == 0x68747874) { ds_strnzcpy((char*)(pBuffer), pState->strHdr, iBufSize); return 0; }
    return -1;
}

void ProtoHttpUpdate(ProtoHttpRefT* pState)
{
    s32 iResult;
    char strData[256];

    ProtoSSLUpdate(pState->pSsl);
    if (pState->eState && pState->eState != ST_DONE && pState->eState != ST_FAIL && (s32)(NetTick() - pState->uTimer) >= 0)
    { NetPrintfCode("protohttp: server timeout (state=%d)\n", pState->eState); pState->eState = ST_FAIL; pState->bTimeout = 1; }
    if (pState->eState == ST_CONN)
    {
        iResult = ProtoSSLStat(pState->pSsl, 0x73746174, 0, 0);
        if (iResult > 0) { pState->uTimer = NetTick() + pState->uTimeout; pState->eState = ST_SEND; }
        if (iResult < 0) { NetPrintfCode("protohttp: ST_CONN got ST_FAIL (err=%d)\n", iResult); pState->eState = ST_FAIL; }
    }
    if (pState->eState == ST_SEND)
    {
        iResult = _ProtoHttpSend(pState, pState->pInpBuf + pState->iInpOff, pState->iInpLen);
        if (iResult > 0)
        {
            pState->iInpOff += iResult; pState->iInpLen -= iResult;
            if (pState->iInpLen == 0) { pState->eState = ST_RESP; pState->iInpOff = 0; }
        }
    }
    if (pState->eState == ST_RESP)
    {
        iResult = _ProtoHttpRecv(pState, strData, 1);
        if (iResult > 0)
        {
            pState->pInpBuf[0] = strData[0]; pState->iInpLen = 1; pState->eState = ST_HEAD;
        }
        if (iResult < 0) { NetPrintfCode("protohttp: ST_HEAD byte0 got ST_FAIL (err=%d)\n", iResult); pState->iInpLen = 0; pState->eState = ST_FAIL; }
    }
    if (pState->eState == ST_HEAD)
    {
        iResult = _ProtoHttpRecv(pState, pState->pInpBuf + pState->iInpLen, pState->iInpMax - pState->iInpLen);
        if (iResult > 0)
            pState->iInpLen += iResult;
        if (iResult < 0) { NetPrintfCode("protohttp: ST_HEAD append got ST_FAIL (err=%d, len=%d)\n", iResult, pState->iInpLen); pState->eState = ST_FAIL; }
    }
    if (pState->eState == ST_HEAD && pState->iInpLen > 4)
    {
        if (_ProtoHttpParseHeader(pState) >= 0)
        {
            if (pState->bHttp1_0 == 1 && !pState->iBodySize && pState->iInpCnt > 0) pState->iBodySize = -1;
            if (pState->bHeadOnly == 1)
                pState->eState = ST_DONE;
            else if (pState->iBodySize >= 0 && pState->iInpCnt >= pState->iBodySize)
                pState->eState = ST_DONE;
            else
                pState->eState = ST_BODY;
            if (100 * (pState->iHdrCode / 100) == 300) _ProtoHttpRedirect(pState);
        }
        else return;
    }
    while (pState->eState == ST_BODY)
    {
        if (pState->iInpLen > 0 && pState->iInpOff == pState->iInpLen) pState->iInpOff = pState->iInpLen = 0;
        iResult = pState->iInpMax - pState->iInpLen;
        if (iResult <= 0)
            iResult = 0;
        else
            iResult = _ProtoHttpRecv(pState, pState->pInpBuf + pState->iInpLen, iResult);
        if (iResult == 0) break;
        if (iResult == -1 && (pState->iBodySize == -1 || pState->iBodySize == pState->iInpCnt))
        { pState->iBodySize = pState->iInpCnt; pState->bCloseHdr = 1; pState->eState = ST_DONE; break; }
        if (iResult < 0) { NetPrintfCode("protohttp: ST_FAIL (err=%d)\n", iResult); pState->eState = ST_FAIL; break; }
        pState->iInpLen += iResult; pState->iInpCnt += iResult;
        if (pState->iBodySize >= 0 && pState->iInpCnt >= pState->iBodySize) { pState->eState = ST_DONE; break; }
    }
    if (pState->eState == ST_FAIL) _ProtoHttpClose(pState, "error");
    if (pState->eState == ST_DONE && pState->bCloseHdr) _ProtoHttpClose(pState, "server request");
}
