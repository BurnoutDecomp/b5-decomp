#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestExitZone -- the "exit zone" protocol request
// (vendor middleware). A concrete CRequestObject: when the game leaves an ad
// zone the MassiveAd zone layer (CMassiveZoneManager::Tick) builds one of these
// through CreateRequest, WriteExitZoneRequest seals the outgoing block (zone
// name + client session/player IDs + optional bandwidth-usage triple), and
// Parse() consumes the server response. Sibling of CRequestObject /
// CRequestBuilder / CRequestManager / CRequestImpressionUpdate (all committed).
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CRequestExitZone::CRequestExitZone         @ 0x82BDC488
//     MassiveAdClient3::CRequestExitZone::`vector deleting destructor' @ 0x82BDC4E0
//     MassiveAdClient3::CRequestExitZone::WriteExitZoneRequest     @ 0x82BDC538
//     MassiveAdClient3::CRequestExitZone::Parse                    @ 0x82BDC700 (BLOCKED)
//     MassiveAdClient3::CRequestExitZone::CreateRequest            @ 0x82BDC8A0
//
// Parse @ 0x82BDC700 is NOT reconstructed here (BLOCKED, exactly as the sibling
// CRequestImpressionUpdate::Parse): after ReadRemoveSignature its body issues a
// `bl STUB` (Hex-Rays `STUB(this, mpSignature@+0x30, 20)`) into a function that
// is neither homed nor named in this TU's dossier, and it also calls
// CRequestObject::ReadRemoveSignature, whose signature the committed base header
// deliberately leaves undeclared as un-attested. The Parse override is DECLARED
// (so the class stays a concrete override of the pure base slot and the compile
// gate is clean) but its body is left for the ledger slice that homes those two
// collaborators. Reproducing the STUB side-effect without a real symbol would be
// fabrication.
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CRequestExitZone class / its methods) are PRESERVED VERBATIM
// -- external middleware API, not project-owned code.
//
// This class adds NO members over the CRequestObject base: the X360 ctor stores
// only the base subobject state plus the class vftable (off_82187850, modelled
// by the virtuals), and every body works through the inherited buffer/state.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

namespace MassiveAdClient3
{

class CRequestBuilder;

class CRequestExitZone : public CRequestObject
{
public:
    // @ 0x82BDC488. Chains CRequestObject(67, "RequestExitZone") and installs
    // this class's vftable (off_82187850 -- modelled by the virtuals). No own
    // state.
    CRequestExitZone();

    // @ 0x82BDC4E0 (vector deleting destructor). No own teardown: the X360 thunk
    // rewrites the vftable, chains ~CRequestObject, and conditionally frees the
    // object through CMassiveBaseObject::operator delete when its low bit is set.
    virtual ~CRequestExitZone();

    // @ 0x82BDC700 (BLOCKED -- see the file header). Vftable Parse override the
    // response path (CRequestObject::Complete) dispatches; body lives in the
    // ReadRemoveSignature/STUB slice.
    int Parse() override;

    // @ 0x82BDC8A0. Called by CMassiveZoneManager::Tick. Rejects a null builder
    // or a null zone name (returns -1100); otherwise chains the base
    // CreateRequest(pBuilder, 256, 256), writes the exit-zone block, sets status
    // 2 (ready to submit) and logs. Returns 0 on success.
    int CreateRequest(CRequestBuilder* pBuilder, const char* pcZoneName,
                      int nBandwidthTotalSize, int nBandwidthTotalTime,
                      unsigned short sBandwidthTotalItems);

    // @ 0x82BDC538. Builds the outgoing exit-zone block: zone name (tag 71), the
    // client player ID (tag 42) and session ID (tag 43), and -- only when all
    // three bandwidth values are non-zero -- the bandwidth-usage triple
    // (total size tag 16, total time tag 17, total items tag 18). Seals it with
    // FinishBaseBlock(207, timestamp, sign). Returns 1.
    int WriteExitZoneRequest(const char* pcZoneName, int nBandwidthTotalSize,
                             int nBandwidthTotalTime,
                             unsigned short sBandwidthTotalItems);
};

} // namespace MassiveAdClient3
