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
//     MassiveAdClient3::CRequestOpenSession::Parse                      @ 0x82BD4E58 (BLOCKED)
//     MassiveAdClient3::CRequestOpenSession::WriteOpenSessionRequest    @ 0x82BD50A0
//     MassiveAdClient3::CRequestOpenSession::CreateRequest              @ 0x82BD53C8
//
// Parse @ 0x82BD4E58 is NOT reconstructed here (BLOCKED, exactly as the sibling
// CRequestCloseSession::Parse / CRequestExitZone::Parse / CRequestImpressionUpdate
// ::Parse): inside its HMAC-signature branch (wire tag 30) its body calls
// CRequestObject::ReadRemoveSignature -- whose signature the committed base header
// deliberately leaves undeclared as un-attested -- and then issues a `bl STUB`
// (Hex-Rays `STUB(this, mpSignature@+0x30, 20)`) into a function that is neither
// homed nor named in this TU's dossier. The Parse override is DECLARED (so the
// class stays a concrete override of the pure base slot and the compile gate is
// clean) but its body is left for the ledger slice that homes those two
// collaborators. Reproducing the STUB side-effect without a real symbol would be
// fabrication.
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

    // @ 0x82BD4E58 (BLOCKED -- see the file header). Vftable Parse override the
    // response path (CRequestObject::Complete) dispatches; body lives in the
    // ReadRemoveSignature/STUB slice.
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
