#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveZoneManager -- owning home + real layout (vendor
// middleware). Reconstructed from the X360 ARTIST.XEX (no leak / DecFIGS).
//
// A CMassiveZoneManager owns ONE ad zone the client has entered: it drives the
// enter/exit protocol handshake (Tick builds a CRequestEnterZone / CRequestExit
// Zone and submits it), holds the zone's live ad objects (CMassiveAdObject),
// downloaded media assets (CMassiveAsset) and ad orders (CMassiveOrder), plus a
// queue of pre-subscribers waiting for an ad object to appear, and it aggregates
// impression reports into a single pending CRequestImpressionUpdate. It derives
// from CRequestBuilder (its request-collection base): the ctor chains the base
// ctor with name "CMassiveZoneManager" and installs this class's own vftable
// (off_82185780), and it overrides the builder's HandleResponse / HandleError
// completion callbacks to advance the zone state machine.
//
// Per-function X360 addresses (17 ledger functions):
//     MassiveAdClient3::CMassiveZoneManager::CMassiveZoneManager       @ 0x82BD2968
//     MassiveAdClient3::CMassiveZoneManager::~CMassiveZoneManager      @ 0x82BD3580
//     MassiveAdClient3::CMassiveZoneManager::`vector deleting destructor' @ 0x82BD37B8
//     MassiveAdClient3::CMassiveZoneManager::PreSubscriberAdd          @ 0x82BD2BA8
//     MassiveAdClient3::CMassiveZoneManager::PreSubscriberRemove       @ 0x82BD2C28
//     MassiveAdClient3::CMassiveZoneManager::HandleError               @ 0x82BD3368
//     MassiveAdClient3::CMassiveZoneManager::AssetFind                 @ 0x82BD2D40
//     MassiveAdClient3::CMassiveZoneManager::DeleteElements            @ 0x82BD2A88
//     MassiveAdClient3::CMassiveZoneManager::PreSubscriberAssignT      @ 0x82BD3630
//     MassiveAdClient3::CMassiveZoneManager::MAOFind                   @ 0x82BD2CB0
//     MassiveAdClient3::CMassiveZoneManager::CreateImpUpdateReque      @ 0x82BD2E00 (BLOCKED)
//     MassiveAdClient3::CMassiveZoneManager::HandleResponse            @ 0x82BD3410 (BLOCKED)
//     MassiveAdClient3::CMassiveZoneManager::ReportImpressions         @ 0x82BD2F08
//     MassiveAdClient3::CMassiveZoneManager::Resume                    @ 0x82BD3088
//     MassiveAdClient3::CMassiveZoneManager::SetAssetExpired           @ 0x82BD2DA0
//     MassiveAdClient3::CMassiveZoneManager::SetAssetExpiredByOrderID  @ 0x82BD3710 (BLOCKED)
//     MassiveAdClient3::CMassiveZoneManager::Tick                      @ 0x82BD3130 (BLOCKED)
//
// W38 UNBLOCK: DeleteElements, AssetFind and PreSubscriberAssignT are now
// implemented -- their collaborator layouts have since been homed. DeleteElements
// needs only CMassiveList + the polymorphic CMassiveBaseObject deleting-destructor
// (no concrete payload layout); AssetFind reads CMassiveAsset::mnAssetId (now a
// friend); PreSubscriberAssignT reads CMassiveAdObjectSubscriber::mpcName (now a
// friend) and dispatches CMassiveAdObject::SubscriberAdd (a now-homed virtual).
//
// W41 UNBLOCK: MAOFind, SetAssetExpired, Resume and ReportImpressions are now
// implemented -- the CMassiveAdObject surface-pin home was grown ADDITIVELY with
// the ad object's own name field (mpcAdObjectName @ +0x14, read by MAOFind) and
// the three vftable slots the zone manager forwards through (SetAssetExpired
// @ +0x08, ReportImpressions @ +0x10, Resume @ +0x18 -- their bodies stay in the
// CMassiveAdObject TU). Resume also calls the now-homed CRequestBuilder::Resume
// and CMassiveAsset::Resume; ReportImpressions calls the now-homed CMassiveAsset::
// ReportImpressions and CRequestImpressionUpdate::Finish + CRequestObject::Submit
// on the pending update (its CreateImpUpdateReque helper stays blocked below, but
// is declared, so the flush body compiles and is faithful).
//
// The 4 still-BLOCKED bodies are DECLARED here (so the class shape / vftable is
// complete and the compile gate is clean) but left for a later ledger slice:
// they read/dispatch through collaborators whose owning layout is not yet
// reconstructed and must not be guessed --
//   - CRequestEnterZone (Tick / HandleResponse build + query one): no home.
//   - CMassiveClientCore internal state at +0x10 and player/session fields at
//     +0x3C..+0x44 (CreateImpUpdateReque / HandleResponse / Tick read them):
//     ClientCore is a memberless surface-pin here (no state field homed), and
//     CRequestImpressionUpdate::CreateRequest is not attested here either.
//   - CMassiveClientCore::ZoneManagerRemove (HandleResponse tail): no home.
//   - CMassiveAsset::mnField38's un-homed order-record pointee (SetAssetExpired
//     ByOrderID reads asset.mnField38->+0x14 for the CMassiveOrder number): the
//     committed CMassiveAsset stores +0x38 as a 32-bit int, so reading it as a
//     CMassiveOrder* would need a NON-ADDITIVE int->pointer width change on that
//     committed member -- left blocked rather than fork the asset layout.
//
// Layout over the CRequestBuilder base (base = CMassiveBaseObject[+0x00..+0x13]
// + CMassiveList mRequestList[+0x14] + int mnSubmitStatus[+0x24]; own members
// begin at +0x28). Absolute offsets are X360-relative and reproduced BY NAME
// (the host base, lists and pointers are wider); the base valid/state dword at
// +0x10 doubles as this zone's state machine (0 idle/invalid, 1 enter-requested,
// 2 entered, 4 exit-complete, 15/16/17 transition states), written through the
// base SetValid helper:
//   +0x28  mPreSubscriberList  (CMassiveList; subscribers awaiting an ad object)
//   +0x38  mpcZoneName         (MassiveMalloc'd copy of the zone name, or 0)
//   +0x3C  mAdObjectList       (CMassiveList of CMassiveAdObject)
//   +0x4C  mAssetList          (CMassiveList of CMassiveAsset)
//   +0x5C  mOrderList          (CMassiveList of CMassiveOrder)
//   +0x6C  mpImpressionUpdate  (the pending CRequestImpressionUpdate, or 0)
//
// This is vendor/SDK code reconstructed in its canonical vendor home; the
// MassiveAdClient3 namespace and the CMassiveZoneManager class / method names
// (including the IDA-truncated CreateImpUpdateReque / PreSubscriberAssignT) are
// external middleware identifiers PRESERVED VERBATIM.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"

namespace MassiveAdClient3
{

class CRequestObject;
class CRequestImpressionUpdate;
class CMassiveAdObject;
class CMassiveAdObjectSubscriber;
class CMassiveAsset;

class CMassiveZoneManager : public CRequestBuilder
{
public:
    // @ 0x82BD2968. Chains CRequestBuilder("CMassiveZoneManager"), installs this
    // class's vftable (off_82185780), default-constructs the four lists, and --
    // when pcZoneName is valid -- copies it into a MassiveMalloc'd buffer. On a
    // bad name (-300) or a failed allocation (-99) it records the error and
    // clears the base valid/state dword.
    CMassiveZoneManager(const char* pcZoneName);

    // @ 0x82BD3580 (slot 0; the vector deleting destructor @ 0x82BD37B8 is the
    // compiler-emitted thunk around it). Logs "*REMOVED ZONE", tears down every
    // list element (DeleteElements), frees the zone-name buffer, drops the pending
    // impression update, then the embedded lists and the CRequestBuilder base
    // destruct.
    virtual ~CMassiveZoneManager();

    // @ 0x82BD37B8. Vector deleting destructor (the vftable slot-0 thunk): runs
    // the dtor, then frees the object through CMassiveBaseObject::operator delete
    // when the low bit of bDelete is set. Returns this.
    void* VectorDeletingDestructor(char bDelete);

    // ----- CRequestBuilder completion callbacks (vftable slots 1 / 2) --------

    // @ 0x82BD3410 (slot 1 override). BLOCKED -- advances the zone state on a
    // completed Enter/Exit/Impression request: parses the enter-zone reply into
    // the MAO/asset/order lists (CRequestEnterZone getters + a CMassiveAsset),
    // touches CMassiveClientCore state, then RemoveFromRequestCollect. Declared so
    // the class overrides the pure base slot; body left for the CRequestEnterZone
    // / CMassiveAsset / ClientCore-state slice.
    int HandleResponse(CRequestObject* pRequest) override;

    // @ 0x82BD3368 (slot 2 override). On a failed Enter (-298) / Exit (-297) /
    // Impression-update (-391) request -- discriminated by the request's server-
    // type word -- records the error (clearing the zone state on a failed enter)
    // and removes the request from the collection.
    int HandleError(CRequestObject* pRequest, int nErrorCode) override;

    // ----- pre-subscriber queue ----------------------------------------------

    // @ 0x82BCEA30 / 0x82BD2BA8. Queues a subscriber for an ad object that has not
    // been located yet (a null subscriber records -500). Called as
    // `currentZone->PreSubscriberAdd(subscriber)` by CMassiveAdObjectSubscriber's
    // ctor when no ad object matched.
    int PreSubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber);

    // @ 0x82BD2C28. Removes pSubscriber from the pre-subscriber queue (a null
    // subscriber records -500; -500 when it was not queued). Returns 0 on removal.
    int PreSubscriberRemove(CMassiveAdObjectSubscriber* pSubscriber);

    // @ 0x82BD3630. Walks the pre-subscriber queue and, for every subscriber whose
    // name matches pcName, hands it to pAdObject (SubscriberAdd) and dequeues it,
    // restarting the walk after each removal. A bad name records -300; a null ad
    // object records -400. Reads the subscriber's name (CMassiveAdObjectSubscriber
    // friend) and dispatches CMassiveAdObject::SubscriberAdd.
    int PreSubscriberAssignT(CMassiveAdObject* pAdObject, const char* pcName);

    // ----- ad-object / asset lookups -----------------------------------------

    // @ 0x82BCEA60 / 0x82BD2CB0. Finds the ad object named pcName in this zone by
    // comparing each object's own name (CMassiveAdObject::mpcAdObjectName @ +0x14)
    // against pcName; a bad name records -300, a miss returns null. Called as
    // `currentZone->MAOFind(name)` by the subscriber ctor.
    CMassiveAdObject* MAOFind(const char* pcName);

    // @ 0x82BD2D40. Finds the asset whose id (CMassiveAsset::mnAssetId, +0x3C)
    // equals nAssetId, or null when none match.
    CMassiveAsset* AssetFind(int nAssetId);

    // ----- impression aggregation (BLOCKED) ----------------------------------

    // @ 0x82BD2E00. BLOCKED -- lazily allocates the pending CRequestImpression
    // Update (rejects allocation while the client core is shutting down, state
    // +0x10 == 4). Reads CMassiveClientCore internal state.
    int CreateImpUpdateReque();

    // @ 0x82BD2F08. Flushes queued impressions: ensures a pending impression
    // update exists (CreateImpUpdateReque), reports every asset (CMassiveAsset::
    // ReportImpressions) and ad object (vftable slot +0x10) into it, seals (Finish)
    // + submits it, then starts a fresh one. Returns 0, or the CreateImpUpdateReque
    // error when none could be allocated.
    int ReportImpressions();

    // ----- zone lifecycle (BLOCKED) ------------------------------------------

    // @ 0x82BD3130. BLOCKED -- the zone state machine: builds and submits a
    // CRequestEnterZone (state 1) or CRequestExitZone (state 17), ticks every ad
    // object (state 2). Reads CMassiveClientCore player/session fields.
    int Tick();

    // @ 0x82BD3088. Resumes the base builder (name-hides CRequestBuilder::Resume,
    // called explicitly) then every ad object (vftable slot +0x18) and asset
    // (CMassiveAsset::Resume). Returns 1.
    int Resume();

    // @ 0x82BD2A88. Deletes every ad object, asset and order (a polymorphic
    // deleting-destructor dispatch through each element's vftable slot 0) and empties
    // the pre-subscriber queue (which is not element-owning). Returns 1.
    int DeleteElements();

    // @ 0x82BD2DA0. Marks nAssetExpiry expired on every ad object in the zone
    // (dispatch through CMassiveAdObject vftable slot +0x08). Returns 1.
    int SetAssetExpired(int nAssetExpiry);

    // @ 0x82BD3710. BLOCKED -- expires the asset carrying order id nOrderId across
    // all ad objects. Reads CMassiveAsset +0x38/+0x3C/+0x40.
    int SetAssetExpiredByOrderID(int nOrderId);

    // Additive accessor (FLAG: not its own X360 function). CMassiveAsset::SendReport
    // @ 0x82BDA688 aggregates each media-type impression into the CURRENT zone's
    // pending update, which it reaches through the client core (`zone = Instance()->
    // GetCurrentZone(); update = *(zone+0x6C)`). Exposes that pending update BY NAME
    // so the asset does not read the zone's private +0x6C slot by offset.
    CRequestImpressionUpdate* GetImpressionUpdate() const { return mpImpressionUpdate; }

private:
    CMassiveList             mPreSubscriberList; // +0x28
    char*                    mpcZoneName;        // +0x38 (owned zone-name copy)
    CMassiveList             mAdObjectList;      // +0x3C (CMassiveAdObject)
    CMassiveList             mAssetList;         // +0x4C (CMassiveAsset)
    CMassiveList             mOrderList;         // +0x5C (CMassiveOrder)
    CRequestImpressionUpdate* mpImpressionUpdate; // +0x6C (pending update, or 0)
};

} // namespace MassiveAdClient3
