#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestOpenSession -- the "/adsrv/4/openSession" open-
// session protocol request (vendor middleware). A concrete CRequestObject: when
// the MassiveAd client core (CMassiveClientCore::RequestSessionOpen) opens its
// session it builds one of these through CreateRequest, WriteOpenSessionRequest
// seals the outgoing block (SKU name/version, session type, optional third-party
// id/service, optional multiplayer GUID, the freshly-generated session token and
// -- on request -- the MD5-hashed hardware id), and Parse() consumes the server
// response (the Massive player / session IDs). Sibling of CRequestObject /
// CRequestBuilder / CRequestManager / CRequestCloseSession / CRequestExitZone /
// CRequestImpressionUpdate / CRequestLocateService / CRequestDownloadBinary /
// CRequestHeartbeat (all committed).
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CRequestOpenSession::CRequestOpenSession        @ 0x82BD4D98
//     MassiveAdClient3::CRequestOpenSession::GetRequestURL              @ 0x82BD4DF0
//     MassiveAdClient3::CRequestOpenSession::`scalar deleting destructor'@ 0x82BD4E00
//     MassiveAdClient3::CRequestOpenSession::Parse                      @ 0x82BD4E58 (body pending)
//     MassiveAdClient3::CRequestOpenSession::WriteOpenSessionRequest    @ 0x82BD50A0
//     MassiveAdClient3::CRequestOpenSession::CreateRequest              @ 0x82BD53C8
//
// Parse @ 0x82BD4E58 is DECLARED here but its body is still to be reconstructed.
// Its two once-blocking collaborators are now both resolved (measured 2026-08-05
// against the ARTIST .i64):
//   * CRequestObject::ReadRemoveSignature @ 0x82BD02D0 is declared in the
//     committed base header and DEFINED in MassiveAdClient3Request.cpp (its full
//     0x5C-byte body re-verified by a fresh headless dump).
//   * the `bl STUB` after the signature read (Hex-Rays `STUB(this,
//     mpSignature@+0x30, 20)`) resolves to STUB @ 0x82AD5078 -- a single `blr`
//     shared by 150 call sites: the ICF-folded, compiled-out trace hook. Per the
//     committed CRequestImpressionUpdate::Parse / CRequestExitZone::Parse
//     precedent it is DOCUMENTED at the call site, not modelled (the fold
//     destroyed the original name; inventing one would be fabrication).
// The one remaining compile dependency is a declaration for
// CMassiveClientCore::ZoneNameAdd @ 0x82BCCE68 (the zone-name field, wire tag
// 71, is registered on the Instance() singleton) in the shared
// MassiveAdClient3ClientCore.h -- owned by the CMassiveClientCore TU.
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CRequestOpenSession class / its methods) are PRESERVED
// VERBATIM -- external middleware API, not project-owned code.
//
// Layout over the committed CRequestObject base (members BY NAME; X360-absolute
// offsets are reproduced by member NAME because the base subobject and the
// pointers are wider on the 64-bit host). Unlike the sibling CRequestExitZone /
// CRequestCloseSession this class DOES add its own state: the ctor zeroes two
// trailing response dwords the Parse override fills from the server reply:
//   +0x00  vftable            (off_82186064; slot 0 = scalar deleting destructor,
//                              the Parse override, GetRequestURL)
//   ...    CRequestObject base body
//   +0x50  mnMassivePlayerID   (u32; server-assigned player id, wire tag 42;
//                              zeroed by the ctor, filled by Parse)
//   +0x54  mnMassiveSessionID  (u32; server-assigned session id, wire tag 43;
//                              zeroed by the ctor, filled by Parse)
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

namespace MassiveAdClient3
{

class CRequestBuilder;

// ---------------------------------------------------------------------------
// MassiveAd token PRNG (separate MassiveAd platform TU).
//
// MassivePRNG(tokenBuffer) fills the client session-token buffer (gacHMACKey,
// byte_8327F2A0) with fresh pseudo-random bytes ahead of each open-session write.
// Attested by the named `bl MassivePRNG` in WriteOpenSessionRequest's asm (r3 =
// the token buffer). The X360 symbol demangles WITHOUT the MassiveAdClient3
// namespace, but it is declared inside the namespace here so the whole vendor
// package stays self-contained (link-name fidelity is not a gate). The X360 r3
// return is discarded at the single call site; body lives in the MassiveAd
// platform TU.
// ---------------------------------------------------------------------------
int MassivePRNG(char* pacTokenBuffer);

// ---------------------------------------------------------------------------
// MassiveAd MD5 helper (separate crypto TU).
//
// CalculateMD5Hash(data, dataLength) returns a freshly MassiveMalloc'd string
// holding the MD5 digest of the input (owned by the wire-buffer copy WriteString
// makes; the source hardware-id string is MassiveFree'd here), or null on
// failure. Attested by the named `bl CalculateMD5Hash` in
// WriteOpenSessionRequest's asm (r3 = the hardware-id string, r4 = its strlen).
// Also declared -- identically -- by the sibling CRequestLocateService header
// (its shared home); redeclared here so this TU is self-contained. Body lives in
// the MassiveAd crypto TU.
// ---------------------------------------------------------------------------
char* CalculateMD5Hash(const void* pData, int nDataLength);

class CRequestOpenSession : public CRequestObject
{
public:
    // @ 0x82BD4D98. Chains CRequestObject(35, "RequestOpenSession"), installs this
    // class's vftable (off_82186064 -- modelled by the virtuals), and zeroes the
    // two trailing response IDs (+0x50 / +0x54).
    CRequestOpenSession();

    // @ 0x82BD4E00 (scalar deleting destructor). No own teardown: the X360 thunk
    // rewrites the vftable, chains ~CRequestObject, and conditionally frees the
    // object through CMassiveBaseObject::operator delete when its low bit is set.
    virtual ~CRequestOpenSession();

    // @ 0x82BD4DF0. Returns the open-session server endpoint. The X360 loads it
    // through the .data pointer off_82F91CAC -> "/adsrv/4/openSession". Overrides
    // the base slot-2 per-request-type endpoint getter.
    const char* GetRequestURL() override;

    // @ 0x82BD4E58 (body pending -- see the file header for the resolved
    // collaborator facts). Vftable slot-1 Parse override the response path
    // (CRequestObject::Complete) dispatches: walks the 204-response block and
    // fills the two response IDs below.
    int Parse() override;

    // @ 0x82BD53C8. Called by CMassiveClientCore::RequestSessionOpen. Rejects a
    // null builder, SKU name or SKU version (returns -1100); otherwise chains the
    // base CreateRequest(pBuilder, 512, 512), writes the open-session block, sets
    // status 2 (ready to submit) and logs. Returns 0 on success.
    int CreateRequest(CRequestBuilder* pBuilder, const char* pcSKUName,
                      const char* pcSKUVersion, int nSessionType,
                      const char* pcMultiplayerGUID, int bWriteHardwareID);

    // @ 0x82BD50A0. Builds the outgoing open-session block: SKU name (tag 61),
    // SKU version (tag 62), session type (tag 60 then the type byte), the optional
    // third-party id (tag 65) / service (tag 66), the optional multiplayer GUID
    // (tag 28, only for a multiplayer session type == 2), the session token
    // (tag 68, freshly generated by MassivePRNG) and -- when bWriteHardwareID is
    // set and the platform hardware address is available -- its MD5 hash (tag 29),
    // finally the client version string "3.3.0.22" (tag 22). Seals it with
    // FinishBaseBlock(203, timestamp, sign) and returns that result.
    int WriteOpenSessionRequest(const char* pcSKUName, const char* pcSKUVersion,
                                int nSessionType, int bWriteHardwareID,
                                const char* pcMultiplayerGUID);

private:
    unsigned int mnMassivePlayerID;   // +0x50 (server player id; tag 42; 0 at ctor)
    unsigned int mnMassiveSessionID;  // +0x54 (server session id; tag 43; 0 at ctor)
};

} // namespace MassiveAdClient3
