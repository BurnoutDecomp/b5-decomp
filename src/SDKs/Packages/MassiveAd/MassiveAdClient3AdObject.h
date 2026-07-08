#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveAdObject -- minimal owning home (vendor middleware).
//
// The full CMassiveAdObject body (Initialize / Tick / the State* download state
// machine / GetBestImpression / Subscriber{Add,Find,Remove} / AssetID* / ...,
// ~30 X360 functions) is a SEPARATE ledger TU. This header only pins the small
// surface the committed CMassiveAdObjectSubscriber TU
// (MassiveAdClient3Subscriber.h/.cpp) is attested to touch, so the class can be
// grown here later (same "surface pin" pattern as MassiveAdClient3ClientCore.h):
//
//   - SubscriberAdd(subscriber): the subscriber's constructor, on finding the
//     matching ad object in the current zone, dispatches through the object's
//     vftable slot at +0x1C -- `(*(*pAdObject + 0x1C))(pAdObject, subscriber)`
//     @ 0x82BCEA44 -- to hand itself to the ad object. Modelled as a virtual
//     method (the X360 call is a vtable dispatch); the exact slot index is not
//     byte-asserted (semantic parity by name, not vtable byte order).
//   - GetCrexID(): tail-called directly (non-virtual) by
//     CMassiveAdObjectSubscriber::GetCrexID @ 0x82BCEC64.
//   - mnInvElementID: the delivered ad's inventory-element id, read directly
//     from the ad object at X360 +0x48 by
//     CMassiveAdObjectSubscriber::GetInvElementID @ 0x82BCEC44.
//
// This is vendor/SDK code reconstructed in its canonical vendor home; the
// MassiveAdClient3 namespace and CMassiveAdObject class name are external
// middleware identifiers PRESERVED VERBATIM.
// ===========================================================================

namespace MassiveAdClient3
{

class CMassiveAdObjectSubscriber;

class CMassiveAdObject
{
public:
    // Polymorphic base -- the X360 object carries a vftable (the subscriber's
    // ctor dispatches SubscriberAdd through it). The full dtor body lives in the
    // CMassiveAdObject TU; declared virtual here to model the vftable.
    virtual ~CMassiveAdObject();

    // @ vftable slot +0x1C. Registers a subscriber with this ad object. Body in
    // the CMassiveAdObject TU.
    virtual int SubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber);

    // @ 0x82BCE... (non-virtual; tail-called by the subscriber). Returns the
    // delivered creative id. Body in the CMassiveAdObject TU.
    int GetCrexID();

    // The delivered ad's inventory-element id (X360 +0x48). Read directly by
    // CMassiveAdObjectSubscriber::GetInvElementID; exposed here BY NAME (the full
    // preceding layout is recovered in the CMassiveAdObject TU) rather than as a
    // fabricated accessor. Public because the subscriber reads the field
    // directly on the X360, not through a getter.
    int mnInvElementID;  // +0x48
};

} // namespace MassiveAdClient3
