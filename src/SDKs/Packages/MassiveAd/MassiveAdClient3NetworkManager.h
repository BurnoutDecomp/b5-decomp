#pragma once

// ===========================================================================
// MassiveAdClient3::CNetworkManager -- minimal owning home (vendor middleware).
//
// The full CNetworkManager body (Connect/Receive/Tick/CreateHeartbeat/
// PrepareForShutdown/...) is a SEPARATE ledger TU. This header only pins the
// surface the committed CRequestObject TU (MassiveAdClient3Request.h/.cpp) is
// attested to touch, so the class can be grown here later:
//
//   - RemoveCurrentRequest(manager): direct bl from CRequestObject::Cancel
//         @ 0x82BD0070
//   - a pointer at manager+0x70: CRequestObject::Cancel compares itself against
//         *(manager + 0x70) (the network manager's in-flight request)
//   - the process-wide manager pointer spNetworkManager (the X360 .data symbol
//     name, kept verbatim), read (never written) by Cancel. DEFINED by the
//     CNetworkManager TU that owns its lifetime.
//
// FLAG: the +0x70 in-flight-request read is exposed through the declared
// accessor GetCurrentRequest() because the network manager's own layout is not
// attested in the CRequestObject TU. The CNetworkManager TU must home the real
// member and implement the accessor as its direct load.
// ===========================================================================

namespace MassiveAdClient3
{

class CRequestObject;

class CNetworkManager
{
public:
    // Direct bl target of CRequestObject::Cancel (taken when the request manager
    // answered -697 and this request is the in-flight one). Body in the
    // CNetworkManager TU.
    int RemoveCurrentRequest();

    // FLAG additive accessor (not its own X360 function): CRequestObject::Cancel
    // reads the in-flight request pointer directly at manager+0x70. Exposed BY
    // NAME here; the CNetworkManager TU homes the member and defines this as its
    // direct load.
    CRequestObject* GetCurrentRequest() const;
};

// spNetworkManager -- the live CNetworkManager singleton pointer (the X360 .data
// symbol name, kept verbatim; null before init / after shutdown). Defined by the
// CNetworkManager TU.
extern CNetworkManager* spNetworkManager;

} // namespace MassiveAdClient3
