#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestManager -- minimal owning home (vendor middleware).
//
// The full CRequestManager body (Initialize/Shutdown/Tick/GetNextRequest/
// PrepareForShutdown/...) is a SEPARATE ledger TU. This header only pins the
// surface the committed CRequestObject TU (MassiveAdClient3Request.h/.cpp) is
// attested to touch, so the class can be grown here later:
//
//   - RemoveRequest(manager, request): direct bl from CRequestObject::Cancel
//         @ 0x82BD0070 (its -697 result routes into the network manager)
//   - SubmitRequest(manager, request): direct bl from CRequestObject::Submit
//         @ 0x82BCFF70 (0 == accepted)
//   - the process-wide manager pointer off_8327F368, read (never written) by
//     both call sites. Named spRequestManager to match the vendor's own
//     spNetworkManager .data slot naming; DEFINED by the CRequestManager TU
//     that owns its lifetime.
// ===========================================================================

namespace MassiveAdClient3
{

class CRequestObject;

class CRequestManager
{
public:
    // Direct bl target of CRequestObject::Cancel. Returns -697 when the request
    // was not queued here (it may be the network manager's in-flight request).
    // Body in the CRequestManager TU.
    int RemoveRequest(CRequestObject* pRequest);

    // Direct bl target of CRequestObject::Submit. Returns 0 when the request was
    // accepted into the queue. Body in the CRequestManager TU.
    int SubmitRequest(CRequestObject* pRequest);
};

// off_8327F368 -- the live CRequestManager singleton pointer (null before init /
// after shutdown). Defined by the CRequestManager TU.
extern CRequestManager* spRequestManager;

} // namespace MassiveAdClient3
