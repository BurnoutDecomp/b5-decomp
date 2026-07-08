#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestHeartbeat -- the MassiveAd client's keep-alive
// heartbeat request (vendor middleware). Reconstructed from the X360 ARTIST.XEX
// (no leak / DecFIGS) as the concrete CRequestObject CNetworkManager creates to
// keep the session alive.
//
// The heartbeat bodies are their own ledger TU; this header is the owning home so
// the CNetworkManager TU can create and submit it BY NAME. Only the surface
// CNetworkManager::CreateHeartbeat touches is modelled:
//   - CRequestHeartbeat(): direct bl on a fresh MassiveMalloc(80) block
//   - CreateRequest(builder): wires the heartbeat to its owning CRequestBuilder
//         (the CNetworkManager passes itself) -- direct bl from CreateHeartbeat
//   - submitted through the inherited CRequestObject::Submit
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CRequestHeartbeat class name) are PRESERVED VERBATIM --
// external middleware API, not project-owned code.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

namespace MassiveAdClient3
{

class CRequestBuilder;

class CRequestHeartbeat : public CRequestObject
{
public:
    // Constructs the heartbeat request (the CreateHeartbeat path bl's this on a
    // fresh MassiveMalloc(80) block). Body in the CRequestHeartbeat TU.
    CRequestHeartbeat();

    // Wires the heartbeat to its owning builder and builds the request block
    // (CNetworkManager::CreateHeartbeat passes the manager itself). Body in the
    // CRequestHeartbeat TU.
    int CreateRequest(CRequestBuilder* pBuilder);

    // The CRequestObject::Parse response override (slot 1). Concrete here so the
    // heartbeat is instantiable; its body (parsing the heartbeat response) is in
    // the CRequestHeartbeat TU -- not written here (never invent one).
    virtual int Parse();
};

} // namespace MassiveAdClient3
