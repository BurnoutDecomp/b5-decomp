// DirtySDK 5.5.3 native HTTP state; layout corroborated by ARTIST 82B24CE0/82B25050.
#include "protohttp.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include "platform.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
struct ProtoSSLRefT;
typedef void (ProtoHttpCustomHeaderCbT)(ProtoHttpRefT*, char*, u32, const char*, u32, void*);
struct ProtoHttpRefT
{
    ProtoSSLRefT* pSsl;
    ProtoHttpCustomHeaderCbT* pCustomHeaderCb;
    void* pCallbackRef;
    s32 iMemGroup, iPort, iSecure, eState, iHdrCode, iHdrDate, iHeadSize, iBodySize, iRecvSize;
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
extern "C" ProtoSSLRefT* ProtoSSLCreate(void);
extern "C" void ProtoSSLDestroy(ProtoSSLRefT* pSsl);
extern "C" s32 ProtoSSLStat(ProtoSSLRefT* pSsl, s32 iSelect, void* pBuffer, s32 iLength);
extern "C" s32 ProtoSSLDisconnect(ProtoSSLRefT* pSsl);
extern "C" s32 ProtoHttpRecv(ProtoHttpRefT* pState, char* pBuffer, s32 iBufMin, s32 iBufMax);
extern "C" char* ds_strnzcpy(char* pDest, const char* pSource, s32 iLength);

extern "C" void ProtoHttpDestroy(ProtoHttpRefT* pState)
{
    if (pState->pSsl) ProtoSSLDestroy(pState->pSsl);
    if (pState->pInpBuf) DirtyMemFree(pState->pInpBuf, 0x70687470, pState->iMemGroup);
    DirtyMemFree(pState, 0x70687470, pState->iMemGroup);
}

extern "C" ProtoHttpRefT* ProtoHttpCreate(s32 iBufSize)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    if (iBufSize < 4096) iBufSize = 4096;
    ProtoHttpRefT* pState = static_cast<ProtoHttpRefT*>(DirtyMemAlloc(sizeof(ProtoHttpRefT), 0x70687470, iMemGroup));
    if (!pState) { NetPrintfCode("protohttp: unable to allocate module state\n"); return 0; }
    memset(pState, 0, sizeof(*pState));
    pState->pSsl = ProtoSSLCreate();
    if (!pState->pSsl) NetPrintfCode("protohttp: unable to allocate ssl module\n");
    else
    {
        pState->pInpBuf = static_cast<char*>(DirtyMemAlloc(iBufSize, 0x70687470, iMemGroup));
        if (pState->pInpBuf)
        {
            pState->iMemGroup = iMemGroup;
            pState->iInpMax = iBufSize;
            pState->bVerifyHdr = 1;
            pState->eState = 0;
            pState->uTimeout = 30000;
            pState->iVerbose = 1;
            pState->iMaxRedirect = 3;
            return pState;
        }
        NetPrintfCode("protohttp: unable to allocate protohttp buffer\n");
    }
    ProtoHttpDestroy(pState);
    return 0;
}

extern "C" void ProtoHttpAbort(ProtoHttpRefT* pState)
{
    if (ProtoSSLStat(pState->pSsl, 0x73746174, 0, 0) >= 0)
    {
        NetPrintfCode("protohttp: closing connection: %s\n", "abort");
        ProtoSSLDisconnect(pState->pSsl);
        pState->bCloseHdr = 0;
    }
    memset(pState->strHdr, 0, sizeof(pState->strHdr));
    memset(pState->strRequestHdr, 0, sizeof(pState->strRequestHdr));
    pState->eState = 0; pState->iHdrCode = -1;
    pState->iHdrDate = pState->iBodySize = pState->iHeadSize = pState->iRecvSize = 0;
    pState->iInpOff = pState->iChkLen = pState->iNumRedirect = 0;
    pState->pLocation = 0;
    pState->bTimeout = pState->bChunked = pState->bHeadOnly = 0;
}

extern "C" s32 ProtoHttpStatus(ProtoHttpRefT* pState, s32 iSelect, void* pBuffer, s32 iBufSize)
{
    if (iSelect == 0x73717565) return pState->pSsl ? ProtoSSLStat(pState->pSsl, iSelect, 0, 0) : -1;
    if (iSelect == 0x686f7374) { ds_strnzcpy(static_cast<char*>(pBuffer), pState->strHost, iBufSize); return 0; }
    if (iSelect == 0x72747874) { ds_strnzcpy(static_cast<char*>(pBuffer), pState->strRequestHdr, iBufSize); return 0; }
    if (iSelect == 0x646f6e65) return pState->eState == 7 ? -1 : pState->eState == 6;
    if (iSelect == 0x64617461) return pState->eState == 7 ? -1 : (pState->eState == 5 || pState->eState == 6 ? pState->iInpLen : 0);
    if (iSelect == 0x636f6465) return pState->iHdrCode;
    if (iSelect == 0x74696d65) return pState->bTimeout;
    if (pState->eState == 7) return -1;
    if (pState->eState != 5 && pState->eState != 6) return -2;
    if (iSelect == 0x68656164) return pState->iHeadSize;
    if (iSelect == 0x626f6479) return pState->iBodySize;
    if (iSelect == 0x64617465) return pState->iHdrDate;
    if (iSelect == 0x68747874) { ds_strnzcpy(static_cast<char*>(pBuffer), pState->strHdr, iBufSize); return 0; }
    return -1;
}

extern "C" s32 ProtoHttpRecvAll(ProtoHttpRefT* pState, char* pBuffer, s32 iBufSize)
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

// _ProtoHttpChunkProcess, ARTIST 82B24B30.
static s32 ProtoHttpChunkProcess(ProtoHttpRefT* pState, s32 iBufMax)
{
    if (pState->iInpLen == pState->iInpOff) return 0;
    s32 iChunk = pState->iChkLen;
    if (!iChunk)
    {
        const char* pStart = pState->pInpBuf + pState->iInpOff;
        const char* pEnd = pState->pInpBuf + pState->iInpLen - 1;
        const char* pScan = pStart;
        while (pScan < pEnd && !(pScan[0] == 13 && pScan[1] == 10)) ++pScan;
        if (pScan == pEnd) return 0;
        iChunk = static_cast<s32>(strtol(pStart, 0, 16));
        pState->iChkLen = iChunk;
        if (!iChunk) { pState->iInpOff = pState->iInpLen; return 0; }
        pState->iInpOff += static_cast<s32>(pScan - pStart) + 2;
    }
    s32 iLen = pState->iInpLen - pState->iInpOff;
    if (iLen > iBufMax) iLen = iBufMax;
    if (iLen >= iChunk)
    {
        if (iLen >= iChunk + 2) { pState->iChkLen = 0; return iChunk; }
        if (pState->iInpLen != pState->iInpMax) return 0;
        iLen /= 2;
        pState->bCompactRecv = 1;
    }
    pState->iChkLen = iChunk - iLen;
    return iLen;
}
extern "C" void NetPrintMemCode(const void* pMem, s32 iSize, const char* pTitle);
extern "C" s32 ProtoHttpRecv(ProtoHttpRefT* pState, char* pBuffer, s32 iBufMin, s32 iBufMax)
{
    if (pState->eState == 7) return -2;
    if (pState->eState != 5 && pState->eState != 6) return -3;
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
    s32 iLen;
    if (pState->bChunked == 1) iLen = ProtoHttpChunkProcess(pState, iBufMax);
    else { iLen = pState->iInpLen - pState->iInpOff; if (iLen > iBufMax) iLen = iBufMax; }
    if (!iLen && pState->eState == 6) return -1;
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

extern "C" s32 ds_stricmp(const char*, const char*);
extern "C" char* ds_stristr(const char*, const char*);
extern "C" s32 ds_strnzcat(char*, const char*, s32);
extern "C" u32 ds_strtotime(const char*);
extern "C" void NetPrintWrapCode(const char*, s32);
extern "C" u32 NetTick(void);
extern "C" s32 ProtoSSLConnect(ProtoSSLRefT*, s32, const char*, u32, s32);
extern "C" void ProtoSSLUpdate(ProtoSSLRefT*);
extern "C" s32 ProtoSSLSend(ProtoSSLRefT*, const char*, s32);
extern "C" s32 ProtoSSLRecv(ProtoSSLRefT*, char*, s32);

static void ProtoHttpClose(ProtoHttpRefT* pState, const char* pReason)
{
    if (ProtoSSLStat(pState->pSsl, 0x73746174, 0, 0) >= 0)
    {
        NetPrintfCode("protohttp: closing connection: %s\n", pReason);
        ProtoSSLDisconnect(pState->pSsl);
        pState->bCloseHdr = 0;
    }
}
static void ProtoHttpReset(ProtoHttpRefT* pState)
{
    memset(pState->strHdr, 0, sizeof(pState->strHdr));
    memset(pState->strRequestHdr, 0, sizeof(pState->strRequestHdr));
    pState->eState = 0; pState->iHdrCode = -1;
    pState->iHdrDate = pState->iBodySize = pState->iHeadSize = pState->iRecvSize = 0;
    pState->iInpOff = pState->iChkLen = pState->iNumRedirect = 0;
    pState->pLocation = 0;
    pState->bTimeout = pState->bChunked = pState->bHeadOnly = 0;
}
static const char* ProtoHttpParseUrl(const char* pUrl, char* pKind, s32 iKindSize, char* pHost, s32 iHostSize, s32* pPort)
{
    while (*pUrl && *pUrl <= ' ') ++pUrl;
    const char* pScan = pUrl;
    *pKind = 0;
    while (*pScan >= 'a' && *pScan <= 'z') ++pScan;
    if (*pScan == ':' && pScan - pUrl < iKindSize)
    {
        s32 i = 0;
        while (*pUrl != ':') pKind[i++] = *pUrl++;
        pKind[i] = 0; ++pUrl;
    }
    while (*pUrl && *pUrl <= ' ') ++pUrl;
    if (pUrl[0] == '/' && pUrl[1] == '/') pUrl += 2;
    s32 i = 0;
    while (i < iHostSize - 1 && *pUrl > ' ' && *pUrl != '/' && *pUrl != '?' && *pUrl != ':') pHost[i++] = *pUrl++;
    pHost[i] = 0;
    s32 iPort = 0;
    if (*pUrl == ':')
    {
        ++pUrl;
        while (*pUrl >= '0' && *pUrl <= '9') { iPort = iPort * 10 + (*pUrl & 15); ++pUrl; }
    }
    *pPort = iPort;
    while (*pUrl && *pUrl <= ' ') ++pUrl;
    return pUrl;
}
static s32 ProtoHttpFormatRequest(ProtoHttpRefT* pState, const char* pUrl, const char* pHost, s32 iPort, const char* pRequest, const char* pData, s32 iDataSize)
{
    ds_snzprintf(pState->pInpBuf, pState->iInpMax, "%s %s HTTP/1.1\r\nHost: %s:%d\r\nContent-Length: %d\r\n%sUser-Agent: Custom/1.1\r\nAccept: */*\r\n",
        pRequest, pUrl, pHost, iPort, iDataSize, pState->iKeepAlive ? "" : "Connection: Close\r\n");
    if (pState->strAppendHdr[0])
    {
        char* pAgent = ds_stristr(pState->pInpBuf, "User-Agent");
        if (pAgent) { *pAgent = 0; ds_strnzcat(pState->pInpBuf, pState->strAppendHdr, pState->iInpMax); }
    }
    if (pState->pCustomHeaderCb) pState->pCustomHeaderCb(pState, pState->pInpBuf, pState->iInpMax, pData, iDataSize, pState->pCallbackRef);
    const s32 iLength = ds_strnzcat(pState->pInpBuf, "\r\n", pState->iInpMax);
    ds_strnzcpy(pState->strRequestHdr, pState->pInpBuf, sizeof(pState->strRequestHdr));
    if (pState->iVerbose) { NetPrintfCode("protohttp: sending request:\n"); NetPrintWrapCode(pState->pInpBuf, 80); }
    pState->iInpLen = iLength;
    return 0;
}
static s32 ProtoHttpRequest(ProtoHttpRefT* pState, const char* pUrl, const char* pData, s32 iDataSize, s32 eRequestType)
{
    const u32 uOldState = pState->eState;
    ProtoHttpReset(pState);
    char strKind[8], strHost[256];
    s32 iPort = 0;
    const char* pPath = ProtoHttpParseUrl(pUrl, strKind, sizeof(strKind), strHost, sizeof(strHost), &iPort);
    const s32 bSecure = ds_stricmp(strKind, "https") == 0;
    if (!iPort) iPort = bSecure ? 443 : 80;
    u8 bNewConnection = 0;
    if (bSecure != pState->iSecure || ds_stricmp(strHost, pState->strHost) || iPort != pState->iPort)
    {
        NetPrintfCode("protohttp: requesting new connection -- url change\n");
        pState->iKeepAlive = pState->iKeepAliveDflt;
        ds_strnzcpy(pState->strHost, strHost, sizeof(pState->strHost));
        bNewConnection = 1; pState->iPort = iPort; pState->iSecure = bSecure;
    }
    if (!bNewConnection && ProtoSSLStat(pState->pSsl, 0x73746174, 0, 0) < 0)
    { NetPrintfCode("protohttp: requesting new connection -- previous connection was closed\n"); bNewConnection = 1; }
    if (!bNewConnection && uOldState && uOldState != 6)
    { NetPrintfCode("protohttp: requesting new connection -- current state of %d does not allow connection reuse\n", uOldState); bNewConnection = 1; }
    if (!bNewConnection)
    {
        char c;
        s32 iRecv = ProtoSSLRecv(pState->pSsl, &c, 1);
        if (iRecv > 0) { NetPrintfCode("protohttp: warning, recv on previous connection returned data (0x%02x)\n", c); iRecv = -1; }
        if (iRecv < 0) { NetPrintfCode("protohttp: requesting new connection -- received %d from previous connection\n", iRecv); bNewConnection = 1; }
    }
    if (pData && iDataSize <= 0) iDataSize = static_cast<s32>(strlen(pData));
    static const char* const requests[] = {"HEAD", "GET", "POST", "PUT"};
    if (ProtoHttpFormatRequest(pState, pPath, strHost, iPort, requests[eRequestType], pData, iDataSize) < 0) return -2;
    if (pData && iDataSize)
    {
        if (iDataSize > pState->iInpMax - pState->iInpLen) iDataSize = pState->iInpMax - pState->iInpLen;
        memcpy(pState->pInpBuf + pState->iInpLen, pData, iDataSize);
        pState->iInpLen += iDataSize;
    }
    pState->bHeadOnly = eRequestType == 0;
    pState->uTimer = NetTick() + pState->uTimeout;
    if (bNewConnection == 1)
    {
        ProtoHttpClose(pState, "new connection");
        ProtoSSLConnect(pState->pSsl, bSecure, strHost, 0, pState->iPort);
        pState->eState = 1;
    }
    else { NetPrintfCode("protohttp: reusing previous connection (keep-alive)\n"); pState->eState = 2; }
    ++pState->iKeepAlive;
    ProtoHttpUpdate(pState);
    return iDataSize;
}
extern "C" s32 ProtoHttpGet(ProtoHttpRefT* pState, const char* pUrl, u32 bHeadOnly)
{
    return ProtoHttpRequest(pState, pUrl, 0, 0, bHeadOnly == 0);
}

// ARTIST 82B24888: response header decoding, with original logging and offsets.
static s32 ProtoHttpParseHeader(ProtoHttpRefT* pState)
{
    char* p = pState->pInpBuf;
    char* pEnd = p + pState->iInpLen - 3;
    while (p != pEnd && !(p[0] == 13 && p[1] == 10 && p[2] == 13 && p[3] == 10)) ++p;
    if (p == pEnd) return -1;
    pState->iHeadSize = static_cast<s32>(p - pState->pInpBuf) + 4;
    p[2] = p[3] = 0;
    if (pState->bVerifyHdr && strncmp(pState->pInpBuf, "HTTP", 4))
    {
        NetPrintfCode("protohttp: invalid result type\n"); pState->eState = 7; return -2;
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
static void ProtoHttpRedirect(ProtoHttpRefT* pState)
{
    char* p = pState->pLocation;
    if (!p) { NetPrintfCode("protohttp: no location included in redirect header\n"); pState->eState = 7; return; }
    if (++pState->iNumRedirect > pState->iMaxRedirect)
    { NetPrintfCode("protohttp: maximum number of redirections (%d) exceeded\n", pState->iMaxRedirect); pState->eState = 7; return; }
    while (*p && *p != ' ') ++p;
    while (*p && *p <= ' ') ++p;
    s32 iLength = 0;
    while (p[iLength] > ' ') ++iLength;
    ++iLength;
    char* pUrl = pState->pInpBuf + pState->iInpMax - iLength;
    for (s32 i = 0; i < iLength; ++i) pUrl[i] = p[i];
    pState->pInpBuf[pState->iInpMax - 1] = 0;
    pState->iKeepAlive = 0;
    ProtoHttpRequest(pState, pUrl, 0, 0, pState->bHeadOnly == 0);
}
extern "C" void ProtoHttpUpdate(ProtoHttpRefT* pState)
{
    ProtoSSLUpdate(pState->pSsl);
    if (pState->eState && pState->eState != 6 && pState->eState != 7 && static_cast<s32>(NetTick() - pState->uTimer) >= 0)
    { NetPrintfCode("protohttp: server timeout (state=%d)\n", pState->eState); pState->eState = 7; pState->bTimeout = 1; }
    if (pState->eState == 1)
    {
        const s32 iStatus = ProtoSSLStat(pState->pSsl, 0x73746174, 0, 0);
        if (iStatus > 0) { pState->eState = 2; pState->uTimer = NetTick() + pState->uTimeout; }
        if (iStatus < 0) { NetPrintfCode("protohttp: ST_CONN got ST_FAIL (err=%d)\n", iStatus); pState->eState = 7; }
    }
    if (pState->eState == 2)
    {
        const s32 iLength = pState->iInpLen;
        const s32 iSent = ProtoSSLSend(pState->pSsl, pState->pInpBuf + pState->iInpOff, iLength);
        if (iSent > 0) pState->uTimer = NetTick() + pState->uTimeout;
        else if (iSent < 0) { NetPrintfCode("protohttp: error %d sending %d bytes\n", iSent, iLength); pState->eState = 7; }
        if (iSent > 0)
        {
            pState->iInpLen -= iSent; pState->iInpOff += iSent;
            if (!pState->iInpLen) { pState->iInpOff = 0; pState->eState = 3; }
        }
    }
    if (pState->eState == 3)
    {
        char c;
        const s32 iRecv = ProtoSSLRecv(pState->pSsl, &c, 1);
        if (iRecv > 0)
        {
            if (pState->iVerbose > 3) NetPrintMemCode(&c, iRecv, "http-recv");
            pState->uTimer = NetTick() + pState->uTimeout;
            pState->pInpBuf[0] = c; pState->iInpLen = 1; pState->eState = 4;
        }
        if (iRecv < 0) { NetPrintfCode("protohttp: ST_HEAD byte0 got ST_FAIL (err=%d)\n", iRecv); pState->iInpLen = 0; pState->eState = 7; }
    }
    if (pState->eState == 4)
    {
        char* pBuf = pState->pInpBuf + pState->iInpLen;
        const s32 iRecv = ProtoSSLRecv(pState->pSsl, pBuf, pState->iInpMax - pState->iInpLen);
        if (iRecv > 0)
        {
            if (pState->iVerbose > 3) NetPrintMemCode(pBuf, iRecv, "http-recv");
            pState->uTimer = NetTick() + pState->uTimeout; pState->iInpLen += iRecv;
        }
        if (iRecv < 0) { NetPrintfCode("protohttp: ST_HEAD append got ST_FAIL (err=%d, len=%d)\n", iRecv, pState->iInpLen); pState->eState = 7; }
    }
    if (pState->eState == 4 && pState->iInpLen > 4)
    {
        if (ProtoHttpParseHeader(pState) >= 0)
        {
            if (pState->bHttp1_0 == 1 && !pState->iBodySize && pState->iInpCnt > 0) pState->iBodySize = -1;
            if (pState->bHeadOnly == 1 || (pState->iBodySize >= 0 && pState->iInpCnt >= pState->iBodySize)) pState->eState = 6;
            else pState->eState = 5;
            if (100 * (pState->iHdrCode / 100) == 300) ProtoHttpRedirect(pState);
        }
        else return;
    }
    while (pState->eState == 5)
    {
        if (pState->iInpLen > 0 && pState->iInpOff == pState->iInpLen) pState->iInpOff = pState->iInpLen = 0;
        const s32 iAvailable = pState->iInpMax - pState->iInpLen;
        if (iAvailable <= 0) break;
        char* pBuf = pState->pInpBuf + pState->iInpLen;
        const s32 iRecv = ProtoSSLRecv(pState->pSsl, pBuf, iAvailable);
        if (iRecv > 0)
        {
            if (pState->iVerbose > 3) NetPrintMemCode(pBuf, iRecv, "http-recv");
            pState->uTimer = NetTick() + pState->uTimeout;
        }
        if (!iRecv) break;
        if (iRecv == -1 && (pState->iBodySize == -1 || pState->iBodySize == pState->iInpCnt))
        { pState->bCloseHdr = 1; pState->iBodySize = pState->iInpCnt; pState->eState = 6; break; }
        if (iRecv < 0) { NetPrintfCode("protohttp: ST_FAIL (err=%d)\n", iRecv); pState->eState = 7; break; }
        pState->iInpCnt += iRecv; pState->iInpLen += iRecv;
        if (pState->iBodySize >= 0 && pState->iInpCnt >= pState->iBodySize) { pState->eState = 6; break; }
    }
    if (pState->eState == 7) ProtoHttpClose(pState, "error");
    if (pState->eState == 6 && pState->bCloseHdr) ProtoHttpClose(pState, "server request");
}
