#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestHeartbeat -- the "/impsrv/4/heartbeat" keep-alive
// heartbeat request (vendor middleware). A concrete CRequestObject: while a
// session is live the CNetworkManager transport periodically calls
// CreateHeartbeat, which allocates one of these on a fresh MassiveMalloc(80)
// block, wires it to itself through CreateRequest, WriteHeartbeatRequest seals
// the outgoing block (client session ID + player ID, both PREPENDED), and Parse()
// consumes the server response. Sibling of CRequestObject / CRequestBuilder /
// CRequestManager / CRequestDownloadBinary / CRequestCloseSession /
// CRequestLocateService / CRequestExitZone / CRequestImpressionUpdate
// (all committed).
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CRequestHeartbeat::CRequestHeartbeat        @ 0x82BD8608
//     MassiveAdClient3::CRequestHeartbeat::GetRequestURL            @ 0x82BD8650
//     MassiveAdClient3::CRequestHeartbeat::`vector deleting destructor' @ 0x82BD8660
//     MassiveAdClient3::CRequestHeartbeat::Parse                    @ 0x82BD86B8
//     MassiveAdClient3::CRequestHeartbeat::WriteHeartbeatRequest    @ 0x82BD8798
//     MassiveAdClient3::CRequestHeartbeat::CreateRequest            @ 0x82BD8818
//
// Parse @ 0x82BD86B8 (long parked as BLOCKED) is now reconstructed in the .cpp:
// its two collaborators are both resolved in the committed tree.
// CRequestObject::ReadRemoveSignature @ 0x82BD02D0 is declared in the base
// header and defined in MassiveAdClient3Request.cpp, and the `bl STUB` inside
// the signature branch targets 0x82AD5078 -- measured as a single `blr` shared
// by ~150 callers, an ICF-folded debug/trace hook compiled out of the retail
// build (an attested no-op, documented at the call site rather than modelled).
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CRequestHeartbeat class / its methods) are PRESERVED
// VERBATIM -- external middleware API, not project-owned code.
//
// This class adds NO members over the CRequestObject base: the X360 ctor stores
// only the base subobject state plus the class vftable (off_82186AD4, modelled
// by the virtuals), and every body works through the inherited buffer/state.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

namespace MassiveAdClient3
{

class CRequestBuilder;

class CRequestHeartbeat : public CRequestObject
{
public:
    // @ 0x82BD8608. Chains CRequestObject(133, "RequestHeartbeat") and installs
    // this class's vftable (off_82186AD4 -- modelled by the virtuals). No own
    // state. (The CreateHeartbeat path bl's this on a fresh MassiveMalloc(80)
    // block.)
    CRequestHeartbeat();

    // @ 0x82BD8660 (vector deleting destructor). No own teardown: the X360 thunk
    // rewrites the vftable, chains ~CRequestObject, and conditionally frees the
    // object through CMassiveBaseObject::operator delete when its low bit is set.
    virtual ~CRequestHeartbeat();

    // @ 0x82BD8650. Returns the heartbeat server endpoint. The X360 loads it
    // through the .data pointer off_82F91D10 -> "/impsrv/4/heartbeat". Overrides
    // the base slot-2 per-request-type endpoint getter.
    const char* GetRequestURL() override;

    // @ 0x82BD86B8. Vftable Parse override the response path
    // (CRequestObject::Complete) dispatches. Walks the heartbeat response:
    // protocol version, response-type byte 215, remaining-byte total, then the
    // fields -- exactly one signature block (wire tag 30, pulled back out of
    // the buffer by ReadRemoveSignature) and a matching HMAC make it succeed.
    int Parse() override;

    // @ 0x82BD8818. Called by CNetworkManager::CreateHeartbeat (the manager
    // passes itself). Rejects a null builder (returns -1100); otherwise chains
    // the base CreateRequest(pBuilder, 256, 256), writes the heartbeat block,
    // sets status 2 (ready to submit), and returns 0.
    int CreateRequest(CRequestBuilder* pBuilder);

    // @ 0x82BD8798. Builds the outgoing heartbeat block. Write order (each
    // write PREPENDS, so the wire order is the reverse): session ID u32, tag
    // 43, player ID u32, tag 42 -- landing front-to-back on the wire as
    // [42][player ID u32][43][session ID u32]. Seals it with
    // FinishBaseBlock(214, timestamp, sign). Returns the FinishBaseBlock
    // result.
    int WriteHeartbeatRequest();
};

} // namespace MassiveAdClient3
