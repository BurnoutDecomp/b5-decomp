#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestBuilder -- minimal owning home (vendor middleware).
//
// The full CRequestBuilder body is a SEPARATE ledger TU (~CRequestBuilder
// @ 0x82BD0xxx, Suspend/Resume, AddToRequestCollection, ...). This header only
// pins the surface that the committed CRequestObject TU (MassiveAdClient3Request
// .h/.cpp) is store-for-store attested to touch, so the class can be grown here
// later without moving its home:
//
//   - vtable slot 1 (+0x04): called by CRequestObject::Complete @ 0x82BD0468 as
//         (*(*builder + 4))(builder, request)
//   - vtable slot 2 (+0x08): called by CRequestObject::Error @ 0x82BD0008 as
//         (*(*builder + 8))(builder, request, errorCode)
//   - AddToRequestCollection(builder, request): direct bl from
//         CRequestObject::CreateRequest @ 0x82BD03E0
//   - a dword at builder+0x24: read by CRequestObject::Submit @ 0x82BCFF70 as
//         SetStatus(*(builder + 0x24))
//
// FLAG (shape, not fabrication-free): slot 0 is modelled as the virtual dtor
// (~CRequestBuilder exists in the X360 call graph and MSVC puts the deleting
// dtor in slot 0); the slot-1/slot-2 callback NAMES are supplied (only the slot
// ORDER and argument shapes are attested); the +0x24 read is exposed through the
// declared accessor GetSubmitStatus() because the builder's own layout is not
// attested in the CRequestObject TU. The CRequestBuilder TU must home the real
// member and implement the accessor as its direct load.
// ===========================================================================

namespace MassiveAdClient3
{

class CRequestObject;

class CRequestBuilder
{
public:
    // Slot 0. ~CRequestBuilder body lives in the CRequestBuilder TU.
    virtual ~CRequestBuilder();

    // Slot 1 (+0x04). Invoked by CRequestObject::Complete after a successful
    // virtual Parse() on a non-cancelled request. FLAG: name supplied; the slot
    // index and (builder, request) argument shape are the attested facts.
    virtual int OnRequestComplete(CRequestObject* pRequest);

    // Slot 2 (+0x08). Invoked by CRequestObject::Error on a non-cancelled
    // request. FLAG: name supplied; the slot index and (builder, request,
    // errorCode) argument shape are the attested facts.
    virtual int OnRequestError(CRequestObject* pRequest, int nErrorCode);

    // Direct (non-virtual) bl target of CRequestObject::CreateRequest. Body in
    // the CRequestBuilder TU.
    int AddToRequestCollection(CRequestObject* pRequest);

    // FLAG additive accessor (not its own X360 function): CRequestObject::Submit
    // reads the dword at builder+0x24 directly as the status value to install on
    // the request. Exposed BY NAME here; the CRequestBuilder TU homes the member
    // and defines this as its direct load.
    int GetSubmitStatus() const;
};

} // namespace MassiveAdClient3
