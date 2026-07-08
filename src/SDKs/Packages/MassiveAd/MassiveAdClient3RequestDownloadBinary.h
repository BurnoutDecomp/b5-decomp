#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestDownloadBinary -- the binary-asset download request
// (vendor middleware). A concrete CRequestObject: when the MassiveAd asset layer
// needs the raw bytes of an ad media asset (CMassiveAsset::RequestDownloadBinary)
// it builds one of these through CreateRequest -- which records the asset URL,
// the expected 32-byte MD5 digest and the expected byte size -- submits it, and
// Parse() validates the downloaded response (size then MD5) before it is
// accepted. Sibling of CRequestObject / CRequestBuilder / CRequestManager /
// CRequestImpressionUpdate / CRequestExitZone / CRequestLocateService (all
// committed).
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CRequestDownloadBinary::CRequestDownloadBinary          @ 0x82BDD280
//     MassiveAdClient3::CRequestDownloadBinary::~CRequestDownloadBinary         @ 0x82BDD2D8
//     MassiveAdClient3::CRequestDownloadBinary::SetRequestURL                   @ 0x82BDD338
//     MassiveAdClient3::CRequestDownloadBinary::CreateRequest                   @ 0x82BDD3A8
//     MassiveAdClient3::CRequestDownloadBinary::Parse                           @ 0x82BDD470
//     MassiveAdClient3::CRequestDownloadBinary::`vector deleting destructor'    @ 0x82BDD518
//
// Unlike the sibling CRequestExitZone / CRequestImpressionUpdate, this request's
// Parse @ 0x82BDD470 is FULLY reconstructed: it does not touch the un-homed
// ReadRemoveSignature / STUB signature path, it only compares the received size
// against the expected size and the received MD5 against the expected MD5 (both
// attested named calls). The `vector deleting destructor' @ 0x82BDD518 is the
// compiler-emitted thunk (runs ~CRequestDownloadBinary, then conditionally frees
// through CMassiveBaseObject::operator delete) and is modelled by the virtual
// destructor below rather than written by hand.
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CRequestDownloadBinary class / its methods) are PRESERVED
// VERBATIM -- external middleware API, not project-owned code.
//
// Layout over the committed CRequestObject base (members BY NAME; the X360-
// absolute offsets are reproduced by member name because the base subobject is
// wider on the 64-bit host):
//   +0x00  vftable        (off_82187AD0; slot 0 = vector deleting destructor,
//                          slot 1 = the Parse override)
//   ...    CRequestObject base body
//   +0x30  mpSignature    (BASE slot reused): CreateRequest MassiveMalloc's the
//                          32-byte expected-MD5 block into the inherited
//                          mpSignature member (attested `stw r3, 0x30`), so the
//                          base ~CRequestObject frees it. Parse memcmp's the
//                          received digest against it.
//   +0x48  mnField48      (BASE): CreateRequest stores the expected byte size
//                          here (attested `stw r29, 0x48`, and again via the
//                          base CreateRequest's nField48 param); Parse reads it
//                          back as the expected size.
//   +0x50  mpRequestURL   (u32; owned MassiveMalloc'd copy of the asset URL, or
//                          0; 0 at ctor, freed + nulled by the destructor)
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

namespace MassiveAdClient3
{

class CRequestBuilder;

class CRequestDownloadBinary : public CRequestObject
{
public:
    // @ 0x82BDD280. Chains CRequestObject(84, "RequestDownloadBinary"), installs
    // this class's vftable (off_82187AD0 -- modelled by the virtuals), and nulls
    // the owned URL pointer.
    CRequestDownloadBinary();

    // @ 0x82BDD2D8 (the `vector deleting destructor' @ 0x82BDD518 is the
    // compiler-emitted thunk over this). Rewrites the vftable, frees the owned
    // URL copy through the MassiveAd heap hook and nulls it, then chains
    // ~CRequestObject (which frees the reused mpSignature MD5 block and the wire
    // buffer).
    virtual ~CRequestDownloadBinary();

    // @ 0x82BDD470. Vftable Parse override the response path (CRequestObject::
    // Complete) dispatches. Rejects the response when the received payload length
    // (mnDataLength) does not equal the expected size (mnField48), or when the
    // MD5 of the received payload does not match the expected 32-byte digest
    // (mpSignature). Returns 1 on a valid response, 0 otherwise.
    int Parse() override;

    // @ 0x82BDD3A8. Called by CMassiveAsset::RequestDownloadBinary. Rejects a
    // null builder, URL or expected-MD5 pointer (-100); records the URL, copies
    // the expected 32-byte MD5 (into the reused mpSignature slot; -99 on a failed
    // allocation) and the expected size, chains the base CreateRequest(pBuilder,
    // 256, nExpectedSize), marks the request ready to submit (SetStatus 2) and
    // logs. Returns 0 on success.
    int CreateRequest(CRequestBuilder* pBuilder, const char* pcURL,
                      const void* pExpectedMD5, int nExpectedSize);

    // @ 0x82BDD338. Installs an owned MassiveMalloc'd copy of pcURL into
    // mpRequestURL. On a failed allocation records error -99 and returns the
    // SetLastError result; otherwise returns the copied buffer.
    char* SetRequestURL(const char* pcURL);

private:
    char* mpRequestURL; // +0x50 (owned URL copy, or 0)
};

} // namespace MassiveAdClient3
