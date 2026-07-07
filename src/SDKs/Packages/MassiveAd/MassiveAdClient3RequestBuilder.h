#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestBuilder -- owning home + real layout (vendor
// middleware). Reconstructed from the X360 ARTIST.XEX (no leak / DecFIGS):
//     MassiveAdClient3::CRequestBuilder::CRequestBuilder              @ 0x82BD4688
//     MassiveAdClient3::CRequestBuilder::~CRequestBuilder             @ 0x82BD46E0
//     MassiveAdClient3::CRequestBuilder::`scalar deleting destructor' @ 0x82BD49C0
//     MassiveAdClient3::CRequestBuilder::RemoveFromRequestCollect     @ 0x82BD47E0
//     MassiveAdClient3::CRequestBuilder::Suspend                      @ 0x82BD4868
//     MassiveAdClient3::CRequestBuilder::Resume                       @ 0x82BD4918
//
// CRequestBuilder is the request-owning base of every MassiveAd client
// subsystem (CMassiveClientCore / CNetworkManager / CMassiveZoneManager /
// CMassiveAsset all chain its ctor / dtor): it keeps the collection of
// outstanding CRequestObjects and the default submit status stamped onto each.
// It derives from CMassiveBaseObject (the ctor chains the base ctor and installs
// off_82186008) -- the earlier minimal home did not model the base or members;
// this is the real CRequestBuilder TU that homes them.
//
// Layout over the 0x14-byte CMassiveBaseObject base (offsets X360-relative,
// reproduced BY NAME -- the host base and list are wider):
//   +0x00  vftable        (off_82186008; slot 0 dtor, slot 1 OnRequestComplete,
//                          slot 2 OnRequestError)
//   +0x14  mRequestList   (CMassiveList, embedded by value; the ctor zeroes its
//                          four dwords in place, the dtor Cancels+deletes every
//                          entry then RemoveAll's + destroys it)
//   +0x24  mnSubmitStatus (16 at ctor; the status CRequestObject::Submit installs
//                          on a request, read as GetSubmitStatus / builder+0x24)
//
// The slot-1 / slot-2 callback NAMES are supplied; the attested facts are the
// slot ORDER and the (builder, request[, errorCode]) argument shapes taken by
// CRequestObject::Complete / Error. No CRequestBuilder base body for either
// exists in the X360 ledger (every subsystem overrides them), so they are pure
// virtual here.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

namespace MassiveAdClient3
{

class CRequestObject;

class CRequestBuilder : public CMassiveBaseObject
{
public:
    // @ 0x82BD4688. Chains the base ctor with pcName, installs the builder
    // vftable (off_82186008 -- modelled by the virtuals), default-constructs the
    // empty request list, and defaults the submit status to 16.
    CRequestBuilder(const char* pcName);

    // @ 0x82BD46E0 (slot 0; the scalar deleting destructor @ 0x82BD49C0 is the
    // compiler-emitted thunk around it). Cancels and deletes every outstanding
    // request, then RemoveAll's the collection before the embedded list and the
    // base destruct.
    virtual ~CRequestBuilder();

    // Slot 1 (+0x04). Invoked by CRequestObject::Complete on a successfully
    // parsed, non-cancelled request. Pure in the base; every request-owning
    // subsystem overrides it in its own TU.
    virtual int OnRequestComplete(CRequestObject* pRequest) = 0;

    // Slot 2 (+0x08). Invoked by CRequestObject::Error on a non-cancelled
    // request. Pure in the base (overridden per subsystem).
    virtual int OnRequestError(CRequestObject* pRequest, int nErrorCode) = 0;

    // @ 0x82BD4868. Under the request manager's critical section, suspends every
    // pending request in the collection. Returns 0 on success, -998 on an empty
    // collection, -696 when the section could not be acquired (or the manager
    // singleton is absent).
    int Suspend();

    // @ 0x82BD4918. Symmetric to Suspend: resumes every pending request. Unlike
    // Suspend it does NOT null-check the request-manager singleton before taking
    // its section -- reproduced verbatim from the asm.
    int Resume();

    // @ 0x82BD47E0. Finds pRequest in the collection, removes its node, and
    // deletes the request. Returns 0 when removed, -999 when not found.
    int RemoveFromRequestCollect(CRequestObject* pRequest);

    // Direct (non-virtual) bl target of CRequestObject::CreateRequest -- registers
    // a request into the collection. Separate ledger TU; declared here so this
    // TU's bodies and the committed CRequestObject TU compile against the shape.
    int AddToRequestCollection(CRequestObject* pRequest);

    // CRequestObject::Submit reads the default submit status at builder+0x24 as a
    // direct load; homed here as mnSubmitStatus.
    int GetSubmitStatus() const { return mnSubmitStatus; }

protected:
    // The request-owning subsystems derive from this base and walk the same
    // collection, so the members are protected rather than private.
    CMassiveList mRequestList;   // +0x14 (outstanding CRequestObjects)
    int          mnSubmitStatus; // +0x24 (16 at ctor)
};

} // namespace MassiveAdClient3
