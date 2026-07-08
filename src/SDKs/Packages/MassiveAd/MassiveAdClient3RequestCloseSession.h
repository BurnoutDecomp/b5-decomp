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
//     MassiveAdClient3::CRequestCloseSession::Parse                @ 0x82BD3A40 (BLOCKED)
//     MassiveAdClient3::CRequestCloseSession::WriteCloseSessionRequest @ 0x82BD3B20
//     MassiveAdClient3::CRequestCloseSession::CreateRequest        @ 0x82BD3BA0
//
// Parse @ 0x82BD3A40 is NOT reconstructed here (BLOCKED, exactly as the sibling
// CRequestExitZone::Parse / CRequestImpressionUpdate::Parse): inside its
// signature-block branch its body calls CRequestObject::ReadRemoveSignature --
// whose signature the committed base header deliberately leaves undeclared as
// un-attested -- and then issues a `bl STUB` (Hex-Rays `STUB(this,
// mpSignature@+0x30, 20)`) into a function that is neither homed nor named in
// this TU's dossier. The Parse override is DECLARED (so the class stays a
// concrete override of the pure base slot and the compile gate is clean) but its
// body is left for the ledger slice that homes those two collaborators.
// Reproducing the STUB side-effect without a real symbol would be fabrication.
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

    // @ 0x82BD3A40 (BLOCKED -- see the file header). Vftable Parse override the
    // response path (CRequestObject::Complete) dispatches; body lives in the
    // ReadRemoveSignature/STUB slice.
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
