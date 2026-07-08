#pragma once

// ===========================================================================
// MassiveAdClient3::CTransactionHTTP -- the MassiveAd client's HTTP transaction
// object (vendor middleware). Reconstructed from the X360 ARTIST.XEX (no leak /
// DecFIGS) as the type CNetworkManager owns by pointer at manager+0x78.
//
// The transaction bodies are their own ledger TU(s); this header is the owning
// home so the CNetworkManager TU can hold it by pointer and drive it BY NAME.
// Only the surface CNetworkManager touches is modelled. It is polymorphic and
// CNetworkManager calls three of its virtuals, plus destroys it:
//   - vtable slot 0 (called on a freshly popped request, no args) -> OnRequestStarted
//   - vtable slot 1 (called before the request block is sent, no args) -> BuildRequestBlock
//   - vtable slot 2 (called with the received buffer + byte count) -> ProcessReceivedData;
//         returns non-zero once the whole response has been consumed
//   - destroyed by CNetworkManager (Initialize failure paths and ~CNetworkManager)
//         through the heap hook -- reproduced as `delete`
//
// FLAG (layout not asserted): on the X360 CNetworkManager::~CNetworkManager deletes
// the transaction through a polymorphic subobject at transaction+0x0C (it forms
// `transaction+0x0C` and calls that vtable's slot-0 scalar deleting destructor),
// i.e. CTransactionHTTP carries the deletable base as a SECONDARY base while its
// three-slot interface vtable sits at +0x00. That multiple-inheritance shape is
// the transaction's own to home; here it is modelled as a single CMassiveBaseObject
// subclass (so `delete` routes teardown through the MassiveAd heap hook, the
// attested effect) with the three interface methods declared as virtuals. The
// exact vtable slot ORDER / secondary-base offset is documented, not byte-asserted
// (semantic parity). The whole object is a MassiveMalloc(68) block.
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CTransactionHTTP class name) are PRESERVED VERBATIM --
// external middleware API, not project-owned code. The three virtual method names
// are reconstructed from their call-site roles (the X360 renders them as indirect
// vtable dispatches, not named symbols).
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

namespace MassiveAdClient3
{

class CTransactionHTTP : public CMassiveBaseObject
{
public:
    // Constructs the transaction (the CNetworkManager Initialize path bl's this on
    // a fresh MassiveMalloc(68) block). Body in the CTransactionHTTP TU.
    CTransactionHTTP();

    // Destroys the transaction. Virtual so `delete` on a CTransactionHTTP* routes
    // teardown through CMassiveBaseObject::operator delete (the X360 heap hook) --
    // see the layout FLAG above. Body in the CTransactionHTTP TU.
    virtual ~CTransactionHTTP();

    // Vtable slot invoked when a new request has just been popped for service
    // (CNetworkManager::Tick, on the fresh mpCurrentRequest). Resets the
    // transaction for the new request. Body in the CTransactionHTTP TU.
    virtual void OnRequestStarted();

    // Vtable slot invoked immediately before the request block is sent
    // (CNetworkManager::Tick, sending state). Builds/frames the outgoing HTTP
    // request. Body in the CTransactionHTTP TU.
    virtual void BuildRequestBlock();

    // Vtable slot invoked with each received chunk (CNetworkManager::Receive).
    // Consumes nLength bytes of pData; returns non-zero once the whole response
    // has been assembled (the manager then clears the in-flight request). Body in
    // the CTransactionHTTP TU.
    virtual int ProcessReceivedData(const void* pData, int nLength);
};

} // namespace MassiveAdClient3
