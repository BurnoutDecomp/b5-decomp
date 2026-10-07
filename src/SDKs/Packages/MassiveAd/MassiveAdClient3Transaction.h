#pragma once

// ===========================================================================
// MassiveAdClient3::CTransactionHTTP -- the MassiveAd client's HTTP transaction
// object (vendor middleware). Reconstructed from the X360 ARTIST.XEX (no leak /
// DecFIGS) as the type CNetworkManager owns by pointer at manager+0x78.
//
// The transaction bodies are their own ledger TU(s); this header is the owning
// home so the CNetworkManager TU can hold it by pointer and drive it BY NAME.
// It is polymorphic; CNetworkManager drives it through its three interface
// virtuals (SetRequest on a freshly popped request, ProcessRequest before the
// block is sent, ProcessResponse with each received chunk) and destroys it.
//
// FLAG (layout not asserted): on the X360 CNetworkManager::~CNetworkManager deletes
// the transaction through a polymorphic subobject at transaction+0x0C (it forms
// `transaction+0x0C` and calls that vtable's slot-0 scalar deleting destructor),
// i.e. CTransactionHTTP carries the deletable base as a SECONDARY base while its
// three-slot interface vtable sits at +0x00. That multiple-inheritance shape is
// the transaction's own to home; here it is modelled as a single CMassiveBaseObject
// subclass (so `delete` routes teardown through the MassiveAd heap hook, the
// attested effect) with the three interface methods declared as virtuals. The
// whole object is a MassiveMalloc(68) block.
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CTransactionHTTP class name) are PRESERVED VERBATIM --
// external middleware API, not project-owned code.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

namespace MassiveAdClient3
{

class CRequestObject;

class CTransactionHTTP : public CMassiveBaseObject
{
public:
    // Constructs the transaction (the CNetworkManager Initialize path bl's this on
    // a fresh MassiveMalloc(68) block). @ 0x82BD8878. Body in the CTransactionHTTP TU.
    CTransactionHTTP();

    // Destroys the transaction. Virtual so `delete` on a CTransactionHTTP* routes
    // teardown through CMassiveBaseObject::operator delete (the X360 heap hook) --
    // see the layout FLAG above. @ 0x82BD8900. Body in the CTransactionHTTP TU.
    virtual ~CTransactionHTTP();

    // @ 0x82BD9130. Binds pRequest as the in-flight request: on a fresh request it
    // (lazily) allocates the 1 KiB HTTP assembly buffer and Reset()s. A null
    // request clears mpRequest, logs, and returns -900; an unchanged request is a
    // no-op. Returns 0 on success.
    virtual int SetRequest(CRequestObject* pRequest);

    // @ 0x82BD8980. Wraps the current request's wire block with the outgoing HTTP
    // request (request line + Host/Content-Length/Content-Type/User-Agent headers),
    // prepending the URL and the GET/POST method, exactly once per request. Returns
    // 0, or -900 when there is no request.
    virtual int ProcessRequest();

    // @ 0x82BD9430. Consumes nLength bytes of received response data (pData). A
    // null/zero final call finalises the request (content-length check -> Complete
    // or Error); otherwise the data is routed to the header collector or the
    // chunked/unchunked body forwarders. Returns non-zero once the response is fully
    // processed.
    virtual int ProcessResponse(const void* pData, int nLength);

    // @ 0x82BD9288. Appends received bytes to the HTTP assembly buffer, detects the
    // end-of-headers CRLFCRLF, parses the headers, then forwards the body to the
    // chunked/unchunked path. Returns the forwarder's result (1 when the response
    // completed), or 1 on a rejected/failed input.
    int ForwardToHTTPData(const void* pData, int nLength);

    // @ 0x82BD8E90. Forwards a chunked-transfer body: parses each hex chunk-size
    // line, appends the chunk payload to the request, and completes the request on
    // the terminating zero-length chunk. Returns 1 when the response completed,
    // else 0.
    int ForwardChunkedToRequest(const void* pData, int nLength);

    // @ 0x82BD8FE0. Forwards an unchunked body: appends the bytes to the request
    // and, once the accumulated length reaches the expected content length,
    // completes the request. Returns 1 when the response completed, else 0.
    int ForwardUnChunkedToRequest(const void* pData, int nLength);

    // @ 0x82BD8BC0. Parses the captured HTTP response header block: reads the status
    // line, logs each header, and picks out Content-Length and Transfer-Encoding:
    // chunked. Returns 1 on a 200 response, 0 otherwise (or on a malformed header).
    int ParseHeaders();

    // @ 0x82BD9048. Allocates the HTTP assembly buffer (nSize bytes). Returns 0 on
    // success, -99 on allocation failure.
    int AllocateHTTPDataBuffer(int nSize);

    // @ 0x82BD91D0. Grows the HTTP assembly buffer to nNewSize (copying the live
    // bytes and freeing the old block); falls back to AllocateHTTPDataBuffer when no
    // buffer exists yet. Returns 0 on success, -99 on failure.
    int ReallocateHTTPDataBuff(int nNewSize);

    // Clears the per-request HTTP state: header/status/chunk flags and counters,
    // and empties the assembly buffer (zero-filled, write cursor back at its start).
    void Reset();

private:
    // Per-request HTTP transaction state. On the X360 these dwords sit at the object
    // offsets noted; they are accessed BY NAME here (the absolute offsets are the
    // MI-shape's and are not asserted on the host -- semantic parity, see the FLAG).
    CRequestObject* mpRequest;        // +0x04  in-flight request (or 0)
    int             mbStatusReceived; // +0x08  set once a response status line parsed
    unsigned char*  mpHTTPBuffer;     // +0x20  HTTP response assembly buffer (or 0)
    unsigned char*  mpHTTPWritePos;   // +0x24  write cursor (= mpHTTPBuffer + used)
    int             mnHTTPUsed;       // +0x28  bytes currently in the buffer
    int             mnHTTPCapacity;   // +0x2C  allocated buffer capacity
    int             mbHeadersComplete;// +0x30  full header block captured
    int             mbChunked;        // +0x34  Transfer-Encoding: chunked
    int             mnChunkLength;    // +0x38  current chunk length
    int             mnChunkReceived;  // +0x3C  bytes received in the current chunk
    int             mnContentLength;  // +0x40  expected content length (accumulated)
};

} // namespace MassiveAdClient3
