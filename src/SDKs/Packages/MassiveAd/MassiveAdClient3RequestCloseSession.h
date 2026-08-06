#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestCloseSession -- the "/adsrv/4/closeSession" close-
// session protocol request (vendor middleware). A concrete CRequestObject: when
// the MassiveAd client tears down its session (CMassiveClientCore::
// RequestSessionClose) it builds one of these through CreateRequest,
// WriteCloseSessionRequest seals the outgoing block (client session ID + player
// ID, both PREPENDED), and Parse() consumes the server response. Sibling of
// CRequestObject / CRequestBuilder / CRequestManager / CRequestExitZone /
// CRequestImpressionUpdate / CRequestLocateService / CRequestDownloadBinary
// (all committed).
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CRequestCloseSession::CRequestCloseSession @ 0x82BD3990
//     MassiveAdClient3::CRequestCloseSession::GetRequestURL        @ 0x82BD39D8
//     MassiveAdClient3::CRequestCloseSession::`vector deleting destructor' @ 0x82BD39E8
//     MassiveAdClient3::CRequestCloseSession::Parse                @ 0x82BD3A40
//     MassiveAdClient3::CRequestCloseSession::WriteCloseSessionRequest @ 0x82BD3B20
//     MassiveAdClient3::CRequestCloseSession::CreateRequest        @ 0x82BD3BA0
//
// Parse @ 0x82BD3A40 (formerly BLOCKED) is reconstructed in the .cpp: its two
// once-blocking callees resolved to already-settled symbols. CRequestObject::
// ReadRemoveSignature @ 0x82BD02D0 is declared in the committed base header and
// bodied in MassiveAdClient3Request.cpp; the Hex-Rays `STUB(this,
// mpSignature@+0x30, 20)` call targets 0x82AD5078, whose entire body is a single
// `blr` shared by ~150 call sites -- the ICF-folded empty debug/trace hook
// already documented by the CRequestExitZone / CRequestImpressionUpdate
// closures (attested no-op: documented at the call site, not modelled).
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CRequestCloseSession class / its methods) are PRESERVED
// VERBATIM -- external middleware API, not project-owned code.
//
// This class adds NO members over the CRequestObject base: the X360 ctor stores
// only the base subobject state plus the class vftable (off_82185A24, modelled
// by the virtuals), and every body works through the inherited buffer/state.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

namespace MassiveAdClient3
{

class CRequestBuilder;

class CRequestCloseSession : public CRequestObject
{
public:
    // @ 0x82BD3990. Chains CRequestObject(115, "RequestCloseSession") and
    // installs this class's vftable (off_82185A24 -- modelled by the virtuals).
    // No own state.
    CRequestCloseSession();

    // @ 0x82BD39E8 (vector deleting destructor). No own teardown: the X360 thunk
    // rewrites the vftable, chains ~CRequestObject, and conditionally frees the
    // object through CMassiveBaseObject::operator delete when its low bit is set.
    virtual ~CRequestCloseSession();

    // @ 0x82BD39D8. Returns the close-session server endpoint. The X360 loads it
    // through the .data pointer off_82F91C30 -> "/adsrv/4/closeSession". Overrides
    // the base slot-2 per-request-type endpoint getter.
    const char* GetRequestURL() override;

    // @ 0x82BD3A40. Vftable Parse override the response path
    // (CRequestObject::Complete) dispatches. Walks the close-session response
    // (protocol version, response type 210, then the field loop): the one
    // signature field (wire tag 30) is pulled out via ReadRemoveSignature and
    // every other field must SkipField; succeeds only with exactly one
    // signature block and a matching HMAC. Returns 1 on success, 0 on any
    // failure.
    int Parse() override;

    // @ 0x82BD3BA0. Called by CMassiveClientCore::RequestSessionClose. Rejects a
    // null builder (returns -1100); otherwise chains the base
    // CreateRequest(pBuilder, 256, 256), writes the close-session block, sets
    // status 2 (ready to submit), and returns 0.
    int CreateRequest(CRequestBuilder* pBuilder);

    // @ 0x82BD3B20. Builds the outgoing close-session block, PREPENDING (front-
    // to-back on disk): the client session ID (tag 43) then the client player ID
    // (tag 42). Seals it with FinishBaseBlock(209, timestamp, sign). Returns the
    // FinishBaseBlock result.
    int WriteCloseSessionRequest();
};

} // namespace MassiveAdClient3
