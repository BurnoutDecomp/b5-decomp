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

// The ad zone this object belongs to (its own ledger TU; owning home is
// MassiveAdClient3ZoneManager.h). Pointer-only use here -- no new include.
class CMassiveZoneManager;

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
    // FILED, NOT FIXED (wave O): the 4th parameter FEEDS mpZone (`stw r26, 0x18`
    // @ 0x82BD5928), so its true type is CMassiveZoneManager*, not int -- on the
    // 64-bit host an int here truncates a pointer slot. Retyping it breaks the
    // four derived ctors that forward it (AudioDynamic/ModelDynamic/Texture/Video,
    // all of which type their own a8 as int), so it needs ONE coordinated commit
    // across those sibling TUs rather than a drive-by. Left as int deliberately.
    CMassiveAdObject(const char* pcName, int a3, int a7, int a8);

    // Polymorphic base -- the X360 object carries a vftable (the subscriber's
    // ctor dispatches SubscriberAdd through it). The full dtor body lives in the
    // CMassiveAdObject TU; declared virtual here to model the vftable.
    virtual ~CMassiveAdObject();

    // ----- vftable slots the owning CMassiveZoneManager forwards through -------
    //
    // The zone manager fans its own per-ad-object operations out across every
    // CMassiveAdObject on its mAdObjectList by dispatching through these vftable
    // slots (X360 `(*(*obj + N))(obj, ...)`):
    //   +0x08  SetAssetExpired(nAssetExpiry)  <- CMassiveZoneManager::SetAssetExpired
    //                                             @ 0x82BD2DA0 (one int arg)
    //   +0x10  ReportImpressions()            <- CMassiveZoneManager::ReportImpressions
    //                                             @ 0x82BD2F08 (no arg)
    //   +0x18  Resume()                       <- CMassiveZoneManager::Resume
    //                                             @ 0x82BD3088 (no arg)
    // The ATTESTED fact is the slot offset + argument shape at each call site; the
    // method NAMES are inferred from the zone manager's identically-named forwarders
    // (semantic parity by name, the slot byte order is not asserted -- same pattern
    // as SubscriberAdd below). The bodies live in the full CMassiveAdObject TU.

    // @ vftable slot +0x08. Expires the asset identified by nAssetExpiry on this ad
    // object. Body in the CMassiveAdObject TU.
    virtual int SetAssetExpired(int nAssetExpiry);

    // @ vftable slot +0x10. Reports this ad object's impressions into the zone's
    // pending impression update. Body in the CMassiveAdObject TU.
    virtual int ReportImpressions();

    // @ vftable slot +0x18. Resumes this ad object after a suspend. Body in the
    // CMassiveAdObject TU.
    virtual int Resume();

    // @ vftable slot +0x0C. Per-frame tick on this ad object; returns the first
    // non-zero sub-result (a stop/error code) or 0. Attested as a virtual by the
    // composite CMassiveAdObjectAudioDynamic::Tic @ 0x82BDE098, which fans its own
    // tick out across its slave ad objects via `(*(*slave + 0x0C))(slave)`. Body
    // in the full CMassiveAdObject TU.
    // NAME (measured wave O): the BASE symbol is intact at 40 chars and reads
    // `MassiveAdClient3::CMassiveAdObject::Tick` @ 0x82BD7900 (slot 3) -- the
    // derived `Tic` ledger spellings are 51-char IDA clips, not the real name.
    // Kept as Tic here only to avoid a cascade into the committed AudioDynamic TU
    // and the ledger identity strings; the correct name is Tick. Dispatch is
    // unaffected either way.
    virtual int Tic();

    // @ vftable slot +0x14. Suspends this ad object. Attested as a virtual by the
    // composite CMassiveAdObjectAudioDynamic::Suspend @ 0x82BDE188, which fans its
    // suspend out across its slave ad objects via `(*(*slave + 0x14))(slave)`.
    // Body in the full CMassiveAdObject TU.
    virtual int Suspend();

    // @ vftable slot +0x20. Advances/returns the ad object's "current asset" id.
    // Attested as a virtual by CMassiveAdObjectAudioDynamic::CreateSlaveM
    // @ 0x82BDDE20, which reads it back on `this` through `(*(*this + 0x20))(this)`
    // to seed the newly-created slave. Body in the full CMassiveAdObject TU.
    virtual int GetNextAssetID();

    // @ vftable slot +0x1C. Registers a subscriber with this ad object. Body in
    // the CMassiveAdObject TU.
    virtual int SubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber);

    // @ 0x82BCE... (non-virtual; tail-called by the subscriber). Returns the
    // delivered creative id. Body in the CMassiveAdObject TU.
    int GetCrexID();

    // The ad object's own identifying name (X360 +0x14 -- the first member past
    // the 0x14-byte CMassiveBaseObject base, distinct from the base debug name at
    // +0x0C). CMassiveZoneManager::MAOFind @ 0x82BD2CB0 compares each list entry's
    // name at +0x14 (`CompareStrings(pcName, *(obj + 0x14))`) to locate an ad
    // object by name. Exposed here BY NAME (the full CMassiveAdObject layout is
    // recovered in that TU) rather than as an offset read. Public because the zone
    // manager reads the field directly on the X360, not through a getter.
    char* mpcAdObjectName;  // +0x14

    // The delivered ad's inventory-element id (X360 +0x48). Read directly by
    // CMassiveAdObjectSubscriber::GetInvElementID; exposed here BY NAME (the full
    // preceding layout is recovered in the CMassiveAdObject TU) rather than as a
    // fabricated accessor. Public because the subscriber reads the field
    // directly on the X360, not through a getter.
    int mnInvElementID;  // +0x48

    // @ vftable slot +0x04 (off_82186474 slot 1, base impl @ 0x82BD6FD0 --
    // `mr r4, r3; b BaseFindPreSubscribers`). Post-construction initialisation.
    // Overridden by every *Dynamic composite (CMassiveAdObjectModelDynamic::
    // Initialize @ 0x82BDEEF8 is slot 1 of off_821872B4; TextureDynamic's
    // 0x82BDECE8 is slot 1 of off_8218723C). Body in the CMassiveAdObject TU.
    virtual int Initialize();

    // @ 0x82BD5C60 (non-virtual; direct bl). Records one delivered asset id:
    // rejects a zero id (SetLastError -600, returns 0); otherwise MassiveMalloc(4)'s
    // a copy of the id (store @ 0x82BD5CCC), wraps it in a CMassiveListNode and
    // appends it to mAssetIDList (Append(this+0x50) @ 0x82BD5CF0; logs + returns 0
    // on allocation failure), returns 1. Public: the master composites call it ON
    // THE SLAVE they just spawned (CreateSlaveM @ 0x82BDE900: `mr r3, slave`).
    int AssetIDAdd(int nAssetId);

    // The spawning composite ("master") of a slave ad object, or 0 (zeroed by
    // CMassiveAdObject::CMassiveAdObject @ 0x82BD58F8, `stw r30, 0x34` @
    // 0x82BD595C). Written cross-object by every CreateSlave* (`stw master,
    // 0x34(slave)` @ 0x82BDE8DC / 0x82BDD98C / 0x82BDDEAC) -- public BY NAME like
    // mpcAdObjectName / mnInvElementID (the X360 writes the field directly).
    CMassiveAdObject* mpMaster;  // +0x34

protected:
    // The zone this ad object belongs to (base ctor arg5, `stw r26, 0x18` @
    // 0x82BD5928; when the caller passes 0 the ctor falls back to
    // Instance()->GetCurrentZone() @ 0x82BD59F0). Re-written by the composite
    // Initialize overrides (`stw r3, 0x18(this)` @ 0x82BDEF1C).
    CMassiveZoneManager* mpZone;  // +0x18

    // Base ctor arg4 (`stw r27, 0x38` @ 0x82BD5960). ReadIEBlock passes 0x10 for
    // the dynamics and a parsed word for the leaves; every CreateSlave* forwards
    // the master's value to the spawned slave. Role beyond that not exercised by
    // recovered code.
    int mnField38;  // +0x38

    // Count of delivered asset ids (u16 -- sth/lhz; zeroed by
    // CMassiveAdObject::CMassiveAdObject @ 0x82BD58F8, `sth r30, 0x4C(r31)` @
    // 0x82BD5978). Read by the composite GetNextAssetID overrides (lhz +0x4C @
    // 0x82BDE970 / 0x82BDE99C / 0x82BDE9B4).
    unsigned short muAssetIDCount;  // +0x4C

    // The delivered asset-id list (X360 +0x50; head/tail/cursor/count at
    // +0x50/+0x54/+0x58/+0x5C -- the CMassiveList layout exactly). MEASURED: nodes
    // carry a MassiveMalloc(4)'d int* asset id (AssetIDAdd @ 0x82BD5C60:
    // MassiveMalloc(4) @ 0x82BD5C94-A4, `stw r30, 0(r31)` @ 0x82BD5CCC, Append to
    // this+0x50 @ 0x82BD5CF0). Walked by the composite GetNextAssetID overrides.
    CMassiveList mAssetIDList;  // +0x50
};

} // namespace MassiveAdClient3
