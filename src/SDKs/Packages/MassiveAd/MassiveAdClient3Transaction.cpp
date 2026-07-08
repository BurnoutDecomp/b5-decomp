#include "SDKs/Packages/MassiveAd/MassiveAdClient3Transaction.h"

#include <cstdio>   // sscanf
#include <cstring>  // memcpy, memset, strlen, strtok, strstr

#include "SDKs/Packages/MassiveAd/MassiveAdClient3NetworkManager.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

// ===========================================================================
// MassiveAdClient3::CTransactionHTTP -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE and BODIES both from the X360 asm (no leak / DecFIGS). The transaction
// wraps an owned CRequestObject in HTTP: ProcessRequest frames the outgoing
// request block (request line + headers), and ProcessResponse re-assembles the
// response into an internal buffer, parses the status/headers, and streams the
// body (chunked or unchunked) back into the request.
//
// The X360 renders the vendor logger (`STUB(level, name, fmt, ...)`) as the
// already-homed MassiveLog(level, GetName(), fmt, ...) -- the +0x18 second
// argument is the CMassiveBaseObject name (mpcName), i.e. GetName(). The heap is
// reached through the MassiveMalloc / MassiveFree hooks, and the size-less /
// sized vendor formatters are MassiveSprintf (sub_82C0CB70) / MassiveFormatString
// (sub_82C10930).
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CTransactionHTTP::CTransactionHTTP @ 0x82BD8878
//
// Chains the CMassiveBaseObject base ("TransactionHTTP") and zeroes the whole
// per-request HTTP state. (The X360 installs the object's two vftables around the
// base ctor; that is compiler-emitted here by the virtual destructor.)
// ---------------------------------------------------------------------------
CTransactionHTTP::CTransactionHTTP()
    : CMassiveBaseObject("TransactionHTTP")
    , mpRequest(0)          // stw 0, 0x04
    , mbStatusReceived(0)   // stw 0, 0x08
    , mpHTTPBuffer(0)       // stw 0, 0x20
    , mpHTTPWritePos(0)     // stw 0, 0x24
    , mnHTTPUsed(0)         // stw 0, 0x28
    , mnHTTPCapacity(0)     // stw 0, 0x2C
    , mbHeadersComplete(0)  // stw 0, 0x30
    , mbChunked(0)          // stw 0, 0x34
    , mnChunkLength(0)      // stw 0, 0x38
    , mnChunkReceived(0)    // stw 0, 0x3C
    , mnContentLength(0)    // stw 0, 0x40
{
}

// ---------------------------------------------------------------------------
// CTransactionHTTP::~CTransactionHTTP @ 0x82BD8900
//
// Frees the HTTP assembly buffer through the MassiveAd heap hook (when present),
// then chains the base dtor. (The vftable rewrites the X360 dtor performs are
// compiler-emitted by the virtual destructor.)
// ---------------------------------------------------------------------------
CTransactionHTTP::~CTransactionHTTP()
{
    if (mpHTTPBuffer)
        MassiveFree(mpHTTPBuffer);
}

// ---------------------------------------------------------------------------
// CTransactionHTTP::AllocateHTTPDataBuffer @ 0x82BD9048
// ---------------------------------------------------------------------------
int CTransactionHTTP::AllocateHTTPDataBuffer(int nSize)
{
    unsigned char* lpBuffer = static_cast<unsigned char*>(MassiveMalloc(nSize));
    mpHTTPBuffer = lpBuffer;
    mpHTTPWritePos = lpBuffer;
    if (lpBuffer)
    {
        mnHTTPCapacity = nSize;
        mnHTTPUsed = 0;
        return 0;
    }

    mnHTTPUsed = 0;
    mnHTTPCapacity = 0;
    MassiveLog(1, GetName(), "ALLOCATION Failed, could not allocate HTTP buffer.");
    return -99;
}

// ---------------------------------------------------------------------------
// CTransactionHTTP::ReallocateHTTPDataBuff @ 0x82BD91D0
// ---------------------------------------------------------------------------
int CTransactionHTTP::ReallocateHTTPDataBuff(int nNewSize)
{
    if (mpHTTPBuffer && mnHTTPCapacity)
    {
        unsigned char* lpBuffer = static_cast<unsigned char*>(MassiveMalloc(nNewSize));
        if (lpBuffer)
        {
            memcpy(lpBuffer, mpHTTPBuffer, mnHTTPUsed);
            MassiveFree(mpHTTPBuffer);
            mpHTTPBuffer = lpBuffer;
            mnHTTPCapacity = nNewSize;
            mpHTTPWritePos = lpBuffer + mnHTTPUsed;
            return 0;
        }

        MassiveLog(1, GetName(), "ALLOCATION Failed, could not reallocate HTTP buffer.");
        return -99;
    }

    return AllocateHTTPDataBuffer(nNewSize);
}

// ---------------------------------------------------------------------------
// CTransactionHTTP::SetRequest @ 0x82BD9130
// ---------------------------------------------------------------------------
int CTransactionHTTP::SetRequest(CRequestObject* pRequest)
{
    if (!pRequest)
    {
        mpRequest = 0;
        MassiveLog(1, GetName(), "Can not set null request");
        return -900;
    }

    if (pRequest != mpRequest)
    {
        if (!mpHTTPBuffer || !mnHTTPCapacity)
            AllocateHTTPDataBuffer(1024);
        mpRequest = pRequest;
        Reset();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CTransactionHTTP::ParseHeaders @ 0x82BD8BC0
//
// Tokenises the captured header block by CRLF. The first token is the status
// line ("HTTP/1.1 <code> <reason>"); the remaining tokens are headers, from which
// Content-Length and Transfer-Encoding: chunked are extracted.
// ---------------------------------------------------------------------------
int CTransactionHTTP::ParseHeaders()
{
    if (!mbHeadersComplete || !mpHTTPBuffer)
    {
        MassiveLog(1, GetName(), "Can't parse headers if they haven't been fully captured");
        return 0;
    }

    char* lpStatusLine = strtok(reinterpret_cast<char*>(mpHTTPBuffer), "\r\n");
    if (!lpStatusLine)
    {
        MassiveLog(2, GetName(), "Can not read response header");
        return 0;
    }

    int  lnResponseCode = 0;
    char lacResponseString[336];
    memset(lacResponseString, 0, 256);
    sscanf(lpStatusLine, "%*s %d %256[ a-zA-Z0-9]", &lnResponseCode, lacResponseString);

    if (lnResponseCode == 200)
    {
        MassiveLog(5, GetName(), "HTTP %d %s", 200, lacResponseString);
        mbStatusReceived = 1;
        for (char* lpHeader = strtok(0, "\r\n"); lpHeader; lpHeader = strtok(0, "\r\n"))
        {
            MassiveLog(5, GetName(), "HTTP HEADER %s", lpHeader);
            if (strstr(lpHeader, "Content-Length:"))
                sscanf(lpHeader, "%*s %d", &mnContentLength);
            if (strstr(lpHeader, "Transfer-Encoding: chunked"))
            {
                mbChunked = 1;
                MassiveLog(4, GetName(), "Response is chunked.", lpHeader);
            }
        }
        return 1;
    }

    if (lnResponseCode >= 400)
    {
        if (lnResponseCode < 500)
        {
            MassiveLog(2, GetName(), "Client Error HTTP %d %s", lnResponseCode, lacResponseString);
            mbStatusReceived = 1;
            for (char* lpHeader = strtok(0, "\r\n"); lpHeader; lpHeader = strtok(0, "\r\n"))
                MassiveLog(2, GetName(), lpHeader);
            return 0;
        }
        if (lnResponseCode < 600)
        {
            MassiveLog(2, GetName(), "Server Error HTTP %d %s", lnResponseCode, lacResponseString);
            mbStatusReceived = 1;
            for (char* lpHeader = strtok(0, "\r\n"); lpHeader; lpHeader = strtok(0, "\r\n"))
                MassiveLog(2, GetName(), lpHeader);
            return 0;
        }
    }

    MassiveLog(5, GetName(), "Unknown HTTP Status %d %s", lnResponseCode, lacResponseString);
    mbStatusReceived = 1;
    return 0;
}

// ---------------------------------------------------------------------------
// CTransactionHTTP::ForwardChunkedToRequest @ 0x82BD8E90
//
// Streams a chunked-transfer body: each chunk is preceded by a hex length line;
// the payload is appended to the request, and the terminating zero-length chunk
// completes the request.
// ---------------------------------------------------------------------------
int CTransactionHTTP::ForwardChunkedToRequest(const void* pData, int nLength)
{
    const unsigned char* lpData = static_cast<const unsigned char*>(pData);
    int lnOffset = 0;

    if (nLength > 0)
    {
        do
        {
            int lnChunkLen = mnChunkLength;
            if (lnChunkLen == 0 || mnChunkReceived == lnChunkLen)
            {
                // Start of a new chunk: read its hex length line.
                if (sscanf(reinterpret_cast<const char*>(lpData + lnOffset), "%x\r\n",
                           reinterpret_cast<unsigned int*>(&mnChunkLength)) == -1)
                {
                    MassiveLog(1, GetName(), "Could not get chunk length.");
                    break;
                }

                // Skip past the end of the chunk-size line (up to and over the LF).
                for (int lcByte = lpData[lnOffset];
                     lcByte != 10 && lnOffset < nLength;
                     lcByte = lpData[lnOffset])
                    ++lnOffset;
                if (++lnOffset > nLength)
                {
                    MassiveLog(1, GetName(), "Buffer overrun while processing chunked data");
                    break;
                }

                int lnThisChunk = mnChunkLength;
                mnChunkReceived = 0;
                mnContentLength += lnThisChunk;
                if (lnThisChunk == 0)
                {
                    // The terminating zero-length chunk: the response is complete.
                    mpRequest->Complete();
                    Reset();
                    mpRequest = 0;
                    return 1;
                }
            }
            else
            {
                // Continue the current chunk: append as much as is available.
                int lnAvailable = lnChunkLen - mnChunkReceived;
                if (lnAvailable > nLength - lnOffset)
                    lnAvailable = nLength - lnOffset;

                mpRequest->AppendToRequestBuffer(lpData + lnOffset, lnAvailable);
                lnOffset += lnAvailable;

                int lnReceived = mnChunkReceived + lnAvailable;
                bool lbChunkDone = (lnReceived == mnChunkLength);
                mnChunkReceived = lnReceived;
                if (lbChunkDone)
                {
                    // Step over the chunk's trailing CRLF.
                    lnOffset += 2;
                    if (lnOffset > nLength)
                    {
                        MassiveLog(1, GetName(), "Buffer overrun while processing chunked data");
                        break;
                    }
                }
            }
        }
        while (lnOffset < nLength);
    }

    return 0;
}

// ---------------------------------------------------------------------------
// CTransactionHTTP::ForwardUnChunkedToRequest @ 0x82BD8FE0
// ---------------------------------------------------------------------------
int CTransactionHTTP::ForwardUnChunkedToRequest(const void* pData, int nLength)
{
    mpRequest->AppendToRequestBuffer(pData, nLength);
    if (mpRequest->GetDataLength() != mnContentLength)
        return 0;

    mpRequest->Complete();
    Reset();
    mpRequest = 0;
    return 1;
}

// ---------------------------------------------------------------------------
// CTransactionHTTP::ForwardToHTTPData @ 0x82BD9288
//
// Accumulates received bytes into the HTTP assembly buffer until the end-of-header
// CRLFCRLF is seen, parses the headers, then streams the body to the chunked or
// unchunked forwarder.
// ---------------------------------------------------------------------------
int CTransactionHTTP::ForwardToHTTPData(const void* pData, int nLength)
{
    int lnResult = 0;

    if (!pData
        || nLength < 0
        || ((mnHTTPUsed + nLength > mnHTTPCapacity)
            && ReallocateHTTPDataBuff(mnHTTPUsed + nLength) != 0)
        || mpHTTPWritePos == 0)
    {
        return 1;
    }

    memcpy(mpHTTPWritePos, pData, nLength);
    mpHTTPWritePos += nLength;
    mnHTTPUsed += nLength;
    if (mnHTTPUsed <= 0)
        return lnResult;

    // Scan for the "\r\n\r\n" header terminator.
    int  lnState = 0;
    int  lnIndex = 0;
    bool lbHeadersFound = false;
    while (lnIndex < mnHTTPUsed)
    {
        unsigned char lcByte = mpHTTPBuffer[lnIndex];
        if (lnState == 0)
            lnState = (lcByte == 13) ? 1 : 0;
        else if (lnState == 1)
            lnState = (lcByte == 10) ? 2 : 0;
        else if (lnState == 2)
            lnState = (lcByte == 13) ? 3 : 0;
        else // lnState == 3
        {
            if (lcByte == 10)
            {
                lbHeadersFound = true;
                break;
            }
            lnState = 0;
        }
        ++lnIndex;
    }

    if (!lbHeadersFound)
        return lnResult;

    int lnHeaderEnd  = lnIndex + 1;
    int lnBodyLength = mnHTTPUsed - lnHeaderEnd;
    mnHTTPUsed = lnHeaderEnd;
    mbHeadersComplete = 1;
    if (lnHeaderEnd > 0)
        mpHTTPBuffer[lnIndex] = 0;

    if (!ParseHeaders())
    {
        mpRequest->Error(6);
        Reset();
        mpRequest = 0;
        return 1;
    }

    const unsigned char* lpBody = mpHTTPBuffer + lnHeaderEnd;
    if (mbChunked)
        lnResult = ForwardChunkedToRequest(lpBody, lnBodyLength);
    else
        lnResult = ForwardUnChunkedToRequest(lpBody, lnBodyLength);
    return lnResult;
}

// ---------------------------------------------------------------------------
// CTransactionHTTP::ProcessRequest @ 0x82BD8980
//
// Wraps the current request's wire block with the outgoing HTTP request, exactly
// once (gated on the request's "HTTP wrapped" flag). Builds the Host string from
// the resolved server host-name (falling back to the dotted address) plus the
// port, formats the header block, prepends it, then prepends the request URL and
// the GET/POST method.
// ---------------------------------------------------------------------------
int CTransactionHTTP::ProcessRequest()
{
    CRequestObject* lpRequest = mpRequest;
    if (!lpRequest)
    {
        MassiveLog(1, GetName(), "Can not process null request.");
        return -900;
    }

    if (!lpRequest->IsHTTPWrapped())
    {
        MassiveLog(5, GetName(), "Processing Request(Wrapping with HTTP Headers)...");

        int lnContentLength = lpRequest->GetDataLength();

        char lacHost[128];
        char lacHeaders[560];
        memset(lacHost, 0, sizeof(lacHost));
        memset(lacHeaders, 0, 512);

        const char* lpcHostName =
            CNetworkManager::GetServerHostName(lpRequest->GetServerIndex());
        MassiveSprintf(lacHost, "%s", lpcHostName);
        if (!strlen(lacHost))
        {
            int lacAddr[4];
            lacAddr[0] = CNetworkManager::GetServerAddressU32(mpRequest->GetServerIndex());
            MassiveInet_ntop(2, lacAddr, lacHost, 16);
        }

        unsigned short lnPort =
            CNetworkManager::GetServerPortU16(mpRequest->GetServerIndex());
        MassiveSprintf(lacHost, "%s:%d", lacHost, lnPort);

        if (MassiveFormatString(lacHeaders, 0x200,
                                " HTTP/1.1\r\n"
                                "Host:%s\r\n"
                                "Content-Length:%d\r\n"
                                "Content-Type:application/massive\r\n"
                                "User-Agent:%s%s\r\n"
                                "\r\n",
                                lacHost, lnContentLength,
                                "Adclient Massive Inc./", "3.3.0.22") < 0)
        {
            MassiveLog(1, GetName(), "HTTP Header Buffer too small");
        }

        mpRequest->PrependToRequestBuffer(lacHeaders, strlen(lacHeaders));
        MassiveLog(5, GetName(), "HTTP Request Headers:%s", lacHeaders);

        const char* lpcURL = mpRequest->GetRequestURL();
        mpRequest->PrependToRequestBuffer(lpcURL, strlen(lpcURL));
        MassiveLog(4, GetName(), "HTTP Request URL:%s", lpcURL);

        if (mpRequest->GetServerType() == 84)  // 'T' -> GET
        {
            mpRequest->PrependToRequestBuffer("GET ", 4);
            MassiveLog(5, GetName(), "HTTP Method: GET");
        }
        else
        {
            mpRequest->PrependToRequestBuffer("POST ", 5);
            MassiveLog(5, GetName(), "HTTP Method: POST");
        }

        mpRequest->SetHTTPWrapped(1);
        MassiveLog(5, GetName(), "Done Processing Request(Wrapping with HTTP Headers)");
    }

    return 0;
}

// ---------------------------------------------------------------------------
// CTransactionHTTP::ProcessResponse @ 0x82BD9430
//
// Drives one received chunk of the response. A null/zero final call finalises the
// request (content-length verification -> Complete or Error); otherwise the data
// is routed to the header collector or the chunked/unchunked body forwarders.
// ---------------------------------------------------------------------------
int CTransactionHTTP::ProcessResponse(const void* pData, int nLength)
{
    CRequestObject* lpRequest = mpRequest;
    if (!lpRequest)
    {
        MassiveLog(1, GetName(), "Can not process response for null request object.");
        Reset();
        return 1;
    }

    if (lpRequest->GetStatus() != 0x300)
    {
        MassiveLog(1, GetName(), "Trying to process response, but request is not in the proper state.");
        Reset();
        return 1;
    }

    if (!nLength || !pData)
    {
        int lnExpected = mnContentLength;
        mbStatusReceived = 1;
        int lnActual = lpRequest->GetDataLength();
        if (lnActual == lnExpected)
        {
            lpRequest->Complete();
        }
        else
        {
            MassiveLog(1, GetName(),
                       "Expected Content Length(%d) does not match Actual Content Length(%d)",
                       lnExpected, lnActual);
            mpRequest->Error(6);
        }
        Reset();
        mpRequest = 0;
        return 1;
    }

    if (!mbHeadersComplete)
        return ForwardToHTTPData(pData, nLength);
    if (mbChunked)
        return ForwardChunkedToRequest(pData, nLength);
    return ForwardUnChunkedToRequest(pData, nLength);
}

} // namespace MassiveAdClient3
