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
//
// RECONCILE (W28, CMassiveAdObjectTexture): the ad-object subtype
// CMassiveAdObjectTexture chains this class's constructor and, in its vector
// deleting destructor @ 0x82BDBCC8, frees through CMassiveBaseObject::operator
// delete; CMassiveAdObject::GetBestImpression @ 0x82BD6898 calls
// CMassiveBaseObject::SetLastError(this, ...). Both prove CMassiveAdObject
// derives from CMassiveBaseObject (the two calls resolve the base's static
// operator delete and the base SetLastError on the ad-object `this`). The base
// is therefore pinned in additively below (deriving from CMassiveBaseObject +
// the name-taking ctor the subtype chains); the full CMassiveAdObject body stays
// its own separate ledger TU.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"  // CMassiveBaseObject

namespace MassiveAdClient3
{

class CMassiveAdObjectSubscriber;

class CMassiveAdObject : public CMassiveBaseObject
{
public:
    // The base constructor CMassiveAdObjectTexture::CMassiveAdObjectTexture
    // @ 0x82BDAB18 chains: `bl CMassiveAdObject::CMassiveAdObject` with r3=this,
    // r4=name, r5=a3, r6=a7, r7=a8 -- i.e. (pcName, a3, a7, a8) (the subtype's a6
    // is clobbered before the chain and never reaches the base). The first arg is
    // a const char* name (it flows into the CMassiveBaseObject name copy); the
    // three trailing ints' roles are recovered in the CMassiveAdObject TU. Body
    // lives in that separate TU; declared here so the subtype ctor can chain it.
    CMassiveAdObject(const char* pcName, int a3, int a7, int a8);

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
