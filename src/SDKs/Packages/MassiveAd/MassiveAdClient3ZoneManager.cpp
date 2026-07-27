#include "SDKs/Packages/MassiveAd/MassiveAdClient3ZoneManager.h"

#include <new>      // placement new (raw heap-hook allocation + explicit construction)
#include <cstddef>  // std::size_t
#include <cstring>  // strlen, strncpy

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h" // CRequestObject::GetServerType
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Asset.h"      // CMassiveAsset (AssetFind / DeleteElements / Resume / ReportImpressions)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Objects.h"    // CMassiveOrder complete type (SetAssetExpiredByOrderID reads mnNumber)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"   // CMassiveAdObject (SubscriberAdd / MAOFind name / SetAssetExpired / ReportImpressions / Resume)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Subscriber.h" // CMassiveAdObjectSubscriber::mpcName (PreSubscriberAssignT)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestImpressionUpdate.h" // CRequestImpressionUpdate::Finish / CreateRequest
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"        // CMassiveClientCore::Instance / mBenchmark / ZoneManagerRemove
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestEnterZone.h"  // CRequestEnterZone (Tick state 1, HandleResponse server type 0x33)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestExitZone.h"   // CRequestExitZone (Tick state 17)

// External MassiveAd string-compare helper (`bl CompareStrings`). Returns 0 when
// the two NUL-terminated strings are equal -- the X360 branches on the result
// == 0 for a match, the strcmp convention. It demangles WITHOUT a namespace
// (a free vendor helper); declared at file scope, body in the MassiveAd string
// layer (another TU).
extern int CompareStrings(const char* pcA, const char* pcB);

// ===========================================================================
// MassiveAdClient3 -- CMassiveZoneManager.
//
// SHAPE and BODIES are reconstructed from BURNOUT_X360_ARTIST.XEX (there is no
// leak source / DecFIGS for this vendor middleware). Stores are reproduced
// member-for-member against the X360 disassembly; see MassiveAdClient3ZoneManager.h
// for the per-offset layout map.
//
// The TU is now COMPLETE -- the four bodies that were previously blocked on
// un-homed collaborators (CreateImpUpdateReque / HandleResponse / Tick /
// SetAssetExpiredByOrderID) are reconstructed here, now that CRequestEnterZone,
// CRequestExitZone, CRequestImpressionUpdate::CreateRequest, the client core's
// CRequestBuilder base (its state dword + embedded CMassiveBenchmark) and
// CMassiveAdObject's Tic vftable slot all have owning homes.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveZoneManager::CMassiveZoneManager @ 0x82BD2968
//
// Chains CRequestBuilder("CMassiveZoneManager") (which installs the request base
// + zeroes the request list), then zeroes this class's own members (the four
// lists default-construct empty; the name and impression-update pointers null),
// and -- when the zone name is valid -- copies it into a MassiveMalloc'd buffer.
// A bad name records -300; a failed allocation logs + records -99; both clear the
// base valid/state dword.
// ---------------------------------------------------------------------------
CMassiveZoneManager::CMassiveZoneManager(const char* pcZoneName)
    : CRequestBuilder("CMassiveZoneManager"),
      mPreSubscriberList(),
      mpcZoneName(0),        // a1[14] = 0 (set below on success)
      mAdObjectList(),
      mAssetList(),
      mOrderList(),
      mpImpressionUpdate(0)  // a1[27] = 0
{
    if (!CMassiveBaseObject::IsValidString(pcZoneName))
    {
        SetLastError(-300, reinterpret_cast<const char*>(0)); // &unk_820046A7
        SetValid(0);                                          // a1[4] = 0
        return;
    }

    std::size_t luLength = std::strlen(pcZoneName) + 1;               // v5
    char* lpcName = static_cast<char*>(MassiveMalloc(luLength));      // v6
    mpcZoneName = lpcName;                                            // a1[14] = v6
    if (!lpcName)
    {
        MassiveLog(2, GetName(), "ALLOCATION Failed for m_pName");
        SetLastError(-99, reinterpret_cast<const char*>(0)); // &unk_820046A7
        SetValid(0);                                          // a1[4] = 0
        return;
    }

    std::strncpy(lpcName, pcZoneName, luLength);
    MassiveLog(5, GetName(), "*ADDED ZONE: %s", pcZoneName);
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::~CMassiveZoneManager @ 0x82BD3580
//
// Logs the removal, tears down every list element (DeleteElements empties the
// ad-object / asset / order lists and the pre-subscriber queue), frees the zone-
// name buffer, and drops the pending impression update. The four embedded lists
// and the CRequestBuilder base then destruct in reverse-declaration order
// (mOrderList, mAssetList, mAdObjectList, mPreSubscriberList, then the base) --
// exactly the X360 teardown order (`~CMassiveList(a1+23/19/15/10)` then
// `~CRequestBuilder`). The vftable slot-0 rewrite is compiler-emitted for the
// virtual dtor.
// ---------------------------------------------------------------------------
CMassiveZoneManager::~CMassiveZoneManager()
{
    MassiveLog(5, GetName(), "*REMOVED ZONE: %s", mpcZoneName);

    DeleteElements();  // empties the four lists in place

    if (mpcZoneName)   // v4 = a1[14]; if (v4)
    {
        MassiveFree(mpcZoneName); // off_82F91C18(v4) -- the free hook
        mpcZoneName = 0;          // a1[14] = 0
    }

    mpImpressionUpdate = 0;       // a1[27] = 0
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::`vector deleting destructor' @ 0x82BD37B8
//
// The vftable slot-0 thunk: run the dtor, then free the object through the base
// operator delete when the low bit of the flag is set. Returns this.
// ---------------------------------------------------------------------------
void* CMassiveZoneManager::VectorDeletingDestructor(char bDelete)
{
    this->~CMassiveZoneManager();
    if (bDelete & 1)
        CMassiveBaseObject::operator delete(this);
    return this;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::HandleResponse @ 0x82BD3410
//
// CRequestBuilder vtable slot 1: a request owned by this zone completed. Switch
// on the request's server-type word:
//   0x33 '3' enter zone -- drain the parsed reply into this zone's own lists
//        (GetMAOs stamps each copied ad object's owner zone; GetOrders takes the
//        orders), then seed the asset list with ONE default-constructed
//        CMassiveAsset (all-zero; its zero crex skips the ctor's validation gate,
//        so the null URL/hash are safe) BEFORE draining the downloaded assets on
//        top of it, start the pending impression update, move the zone to state 2
//        (entered) and fold the client core's own state 13/15 -> 14.
//   0x43 'C' exit zone  -- move the zone to state 4 (exit complete).
//   0x65 'e' impression update -- log only.
// Then the request leaves the builder's collection and, when the zone reached
// state 4, the client core unlinks + DELETES this zone manager (so `this` is
// dead past that call -- the X360 returns the core's result unchanged).
// ---------------------------------------------------------------------------
int CMassiveZoneManager::HandleResponse(CRequestObject* pRequest)
{
    switch (pRequest->GetServerType())  // v4 = *(a2 + 20)
    {
        case 0x33:  // '3' -- enter zone
        {
            CRequestEnterZone* lpEnterZone = static_cast<CRequestEnterZone*>(pRequest);
            lpEnterZone->GetMAOs(&mAdObjectList, this);  // addi r4, r31, 0x3C
            lpEnterZone->GetOrders(&mOrderList);         // addi r4, r31, 0x5C

            void* lpAssetMem = CMassiveListNode::operator new(sizeof(CMassiveAsset)); // li r3,0x78
            CMassiveAsset* lpAsset =
                lpAssetMem ? ::new (lpAssetMem) CMassiveAsset(0, 0, 0, 0, 0, 0, 0,
                                                              0.0f, 0.0f,  // flt_82001CC0 twice
                                                              0, 0, 0, 0)
                           : 0;

            void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode)); // li r3,0xC
            CMassiveListNode* lpNode =
                lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(lpAsset) : 0;
            mAssetList.Append(lpNode);                   // the default asset goes on first
            lpEnterZone->GetAssets(&mAssetList);         // then the parsed/downloaded ones

            CreateImpUpdateReque();
            SetValid(2);  // *(a1 + 16) = 2 -- zone entered

            // The X360 re-fetches the singleton for each access (Instance() is an
            // opaque call, so the compiler could not fold them).
            if (CMassiveClientCore::Instance()->mbIsValid == 13
                || CMassiveClientCore::Instance()->mbIsValid == 15)
            {
                CMassiveClientCore::Instance()->mbIsValid = 14;
            }
            break;
        }
        case 0x43:  // 'C' -- exit zone
            SetValid(4);  // *(a1 + 16) = 4 -- exit complete
            break;
        case 0x65:  // 'e' -- impression update
            MassiveLog(5, GetName(), "Impression Update received successfully");
            break;
        default:
            break;
    }

    int lnResult = RemoveFromRequestCollect(pRequest);
    if (GetValid() == 4)  // *(a1 + 16) == 4
    {
        // Unlinks and DELETES this zone manager -- nothing may touch `this` after.
        lnResult = CMassiveClientCore::Instance()->ZoneManagerRemove(this);
    }
    return lnResult;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::HandleError @ 0x82BD3368
//
// CRequestBuilder vtable slot 2: a request owned by this zone failed. Switch on
// the request's server-type word (EnterZone 0x33 / ExitZone 0x43 / Impression
// Update 0x65) to record the matching error against the zone name, clearing the
// zone state on a failed enter, then remove the request from the collection. The
// X360 leaves the error-code argument unused.
// ---------------------------------------------------------------------------
int CMassiveZoneManager::HandleError(CRequestObject* pRequest, int /*nErrorCode*/)
{
    switch (pRequest->GetServerType())  // v4 = *(a2 + 20)
    {
        case 0x33:  // '3' -- enter zone
            SetLastError(-298, "Enter Zone Failed: %s", mpcZoneName);
            SetValid(0);  // *(a1 + 16) = 0
            break;
        case 0x43:  // 'C' -- exit zone
            SetLastError(-297, "Exit Zone Failed: %s", mpcZoneName);
            break;
        case 0x65:  // 'e' -- impression update
            SetLastError(-391, "Impression Updated failed for Zone: %s", mpcZoneName);
            break;
        default:
            break;
    }

    return RemoveFromRequestCollect(pRequest);
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::PreSubscriberAdd @ 0x82BD2BA8
//
// Queues a subscriber for an ad object that has not been located yet: rejects a
// null subscriber (-500), otherwise allocates a list node over the subscriber and
// appends it to the pre-subscriber queue. Returns 0.
// ---------------------------------------------------------------------------
int CMassiveZoneManager::PreSubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber)
{
    if (!pSubscriber)
        return SetLastError(-500, reinterpret_cast<const char*>(0)); // &unk_820046A7

    void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode)); // li r3,0xC
    CMassiveListNode* lpNode =
        lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(pSubscriber) : 0;
    mPreSubscriberList.Append(lpNode);
    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::PreSubscriberRemove @ 0x82BD2C28
//
// Removes pSubscriber from the pre-subscriber queue: rejects a null subscriber
// (-500), walks the queue for the node carrying it, and removes (and deletes)
// that node. Returns 0 on removal, -500 when the subscriber was not queued.
// ---------------------------------------------------------------------------
int CMassiveZoneManager::PreSubscriberRemove(CMassiveAdObjectSubscriber* pSubscriber)
{
    if (!pSubscriber)
        return SetLastError(-500, reinterpret_cast<const char*>(0)); // &unk_820046A7

    mPreSubscriberList.GoToStart();
    while (mPreSubscriberList.GetCurrent())  // while (*(a1 + 48))
    {
        if (mPreSubscriberList.GetCurrData() == static_cast<void*>(pSubscriber))
        {
            mPreSubscriberList.Remove(mPreSubscriberList.GetCurrent(), 1);
            return 0;
        }
        mPreSubscriberList.GoToNext();
    }
    return -500;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::DeleteElements @ 0x82BD2A88
//
// Empties the zone's four owned lists. For the ad-object, asset and order lists
// each live element is destroyed through its own vftable slot 0 (the X360
// `(**CurrData)(CurrData, 1)` deleting-destructor dispatch, i.e. a polymorphic
// `delete` on the CMassiveBaseObject-derived payload) before the list nodes are
// released with RemoveAll. The pre-subscriber queue is NOT element-owning, so it
// is only RemoveAll'd (the subscribers are owned elsewhere). Returns 1.
// ---------------------------------------------------------------------------
int CMassiveZoneManager::DeleteElements()
{
    mAdObjectList.GoToStart();
    while (mAdObjectList.GetCurrent())  // while (*(a1 + 0x44)) -- mAdObjectList cursor
    {
        void* lpData = mAdObjectList.GetCurrData();
        if (lpData)
            delete static_cast<CMassiveBaseObject*>(lpData);  // (**CurrData)(CurrData, 1)
        mAdObjectList.GoToNext();
    }
    mAdObjectList.RemoveAll();

    mAssetList.GoToStart();
    while (mAssetList.GetCurrent())     // while (*(a1 + 0x54)) -- mAssetList cursor
    {
        void* lpData = mAssetList.GetCurrData();
        if (lpData)
            delete static_cast<CMassiveBaseObject*>(lpData);
        mAssetList.GoToNext();
    }
    mAssetList.RemoveAll();

    mOrderList.GoToStart();
    while (mOrderList.GetCurrent())     // while (*(a1 + 0x64)) -- mOrderList cursor
    {
        void* lpData = mOrderList.GetCurrData();
        if (lpData)
            delete static_cast<CMassiveBaseObject*>(lpData);
        mOrderList.GoToNext();
    }
    mOrderList.RemoveAll();

    mPreSubscriberList.RemoveAll();     // RemoveAll(a1 + 0x28) -- no per-element delete
    return 1;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::AssetFind @ 0x82BD2D40
//
// Walks the asset list for the asset whose id (CMassiveAsset::mnAssetId, X360
// +0x3C) equals nAssetId, returning it (or null when the list is exhausted). The
// X360 reads the id field directly on the cursor's payload -- reproduced here as
// a named-member read through the CMassiveAsset friendship.
// ---------------------------------------------------------------------------
CMassiveAsset* CMassiveZoneManager::AssetFind(int nAssetId)
{
    mAssetList.GoToStart();
    while (mAssetList.GetCurrent())  // while (*(a1 + 0x54)) -- mAssetList cursor
    {
        CMassiveAsset* lpAsset = static_cast<CMassiveAsset*>(mAssetList.GetCurrData());
        if (lpAsset->mnAssetId == nAssetId)  // *(GetCurrData + 0x3C) == a2
            return lpAsset;
        mAssetList.GoToNext();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::PreSubscriberAssignT @ 0x82BD3630
//
// A new ad object pAdObject named pcName has arrived: hand every queued pre-
// subscriber whose name matches to the ad object (CMassiveAdObject::SubscriberAdd,
// the object's vftable slot +0x1C) and dequeue it. A bad name records -300; a null
// ad object records -400. The X360 restarts the walk from the head after every
// removal (its `goto` back to GoToStart), reproduced by the outer loop.
// ---------------------------------------------------------------------------
int CMassiveZoneManager::PreSubscriberAssignT(CMassiveAdObject* pAdObject, const char* pcName)
{
    if (!CMassiveBaseObject::IsValidString(pcName))
        return SetLastError(-300, reinterpret_cast<const char*>(0)); // &unk_820046A7
    if (!pAdObject)
        return SetLastError(-400, "MAO is not valid");

    for (;;)
    {
        mPreSubscriberList.GoToStart();
        bool lbRemoved = false;
        while (mPreSubscriberList.GetCurrent())  // while (*(a1 + 0x30)) -- queue cursor
        {
            CMassiveAdObjectSubscriber* lpSubscriber =
                static_cast<CMassiveAdObjectSubscriber*>(mPreSubscriberList.GetCurrData());
            if (CompareStrings(pcName, lpSubscriber->mpcName) == 0)  // *(CurrData + 4)
            {
                pAdObject->SubscriberAdd(lpSubscriber);  // (*(*a2 + 0x1C))(a2, subscriber)
                PreSubscriberRemove(lpSubscriber);
                lbRemoved = true;
                break;  // goto LABEL_8: restart the walk from the head
            }
            mPreSubscriberList.GoToNext();
        }
        if (!lbRemoved)
            return 0;
    }
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::MAOFind @ 0x82BD2CB0
//
// Finds the ad object named pcName in this zone by comparing each ad object's
// own name (CMassiveAdObject::mpcAdObjectName, X360 +0x14) against pcName. A bad
// name records -300 and returns null; a miss returns null. Called as
// `currentZone->MAOFind(name)` by CMassiveAdObjectSubscriber's ctor to bind a
// subscriber to an already-present ad object.
// ---------------------------------------------------------------------------
CMassiveAdObject* CMassiveZoneManager::MAOFind(const char* pcName)
{
    if (!CMassiveBaseObject::IsValidString(pcName))
    {
        SetLastError(-300, reinterpret_cast<const char*>(0)); // &unk_820046A7
        return 0;
    }

    mAdObjectList.GoToStart();
    while (mAdObjectList.GetCurrent())  // while (*(a1 + 0x44)) -- mAdObjectList cursor
    {
        CMassiveAdObject* lpObject =
            static_cast<CMassiveAdObject*>(mAdObjectList.GetCurrData());
        if (CompareStrings(pcName, lpObject->mpcAdObjectName) == 0)  // *(CurrData + 0x14)
            return static_cast<CMassiveAdObject*>(mAdObjectList.GetCurrData());
        mAdObjectList.GoToNext();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::SetAssetExpired @ 0x82BD2DA0
//
// Marks the asset identified by nAssetExpiry expired on every ad object in the
// zone, dispatching through each ad object's vftable slot +0x08
// (CMassiveAdObject::SetAssetExpired). Returns 1.
// ---------------------------------------------------------------------------
int CMassiveZoneManager::SetAssetExpired(int nAssetExpiry)
{
    mAdObjectList.GoToStart();
    while (mAdObjectList.GetCurrent())  // while (*(a1 + 0x44)) -- mAdObjectList cursor
    {
        CMassiveAdObject* lpObject =
            static_cast<CMassiveAdObject*>(mAdObjectList.GetCurrData());
        lpObject->SetAssetExpired(nAssetExpiry);  // (*(*CurrData + 8))(CurrData, a2)
        mAdObjectList.GoToNext();
    }
    return 1;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::SetAssetExpiredByOrderID @ 0x82BD3710
//
// An order has expired server-side: walk the zone's asset list and, for every
// asset carrying a real id whose owning order number matches nOrderId, log the
// expiry and expire that asset id on every ad object (SetAssetExpired). Returns 1.
//
// The X360 guards ONLY on a non-null cursor payload (`mr. r31,r3; beq`) and a
// non-zero mnAssetId (`lwz r7,0x3C(r31); cmplwi; beq`) -- mpOrder is dereferenced
// WITHOUT a null test (`lwz r11,0x38(r31); lwz r11,0x14(r11)`), reproduced as-is.
// The log takes the already-loaded id in r7 while the SetAssetExpired call
// re-reads +0x3C (`lwz r4, 0x3C(r31)`), so both reads are kept distinct.
// ---------------------------------------------------------------------------
int CMassiveZoneManager::SetAssetExpiredByOrderID(int nOrderId)
{
    mAssetList.GoToStart();
    while (mAssetList.GetCurrent())  // while (*(a1 + 0x54)) -- mAssetList cursor
    {
        CMassiveAsset* lpAsset = static_cast<CMassiveAsset*>(mAssetList.GetCurrData());
        if (lpAsset)
        {
            int lnAssetId = lpAsset->mnAssetId;  // v7 = CurrData[15]
            if (lnAssetId)
            {
                if (lpAsset->mpOrder->mnNumber == nOrderId)  // *(CurrData[14] + 20) == a2
                {
                    MassiveLog(5, GetName(),
                               "Order Expired: %d.  Expiring Asset: %d Crex: %d on All MAOs.",
                               nOrderId, lnAssetId, lpAsset->mnCrex);  // r6/r7/r8
                    SetAssetExpired(lpAsset->mnAssetId);  // v6[15] -- re-read
                }
            }
        }
        mAssetList.GoToNext();
    }
    return 1;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::Resume @ 0x82BD3088
//
// Resumes the base request builder's outstanding requests (name-hides
// CRequestBuilder::Resume, so the base is called explicitly), then resumes every
// ad object (vftable slot +0x18) and every asset (CMassiveAsset::Resume).
// Returns 1.
// ---------------------------------------------------------------------------
int CMassiveZoneManager::Resume()
{
    CRequestBuilder::Resume();  // name-hidden base call

    mAdObjectList.GoToStart();
    while (mAdObjectList.GetCurrent())  // while (*(a1 + 0x44)) -- mAdObjectList cursor
    {
        CMassiveAdObject* lpObject =
            static_cast<CMassiveAdObject*>(mAdObjectList.GetCurrData());
        lpObject->Resume();  // (*(*CurrData + 0x18))(CurrData)
        mAdObjectList.GoToNext();
    }

    mAssetList.GoToStart();
    while (mAssetList.GetCurrent())  // while (*(a1 + 0x54)) -- mAssetList cursor
    {
        CMassiveAsset* lpAsset = static_cast<CMassiveAsset*>(mAssetList.GetCurrData());
        lpAsset->Resume();
        mAssetList.GoToNext();
    }
    return 1;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::CreateImpUpdateReque @ 0x82BD2E00
//
// Lazily allocates the zone's ONE pending CRequestImpressionUpdate. A live one
// short-circuits (0). A shutting-down client core (its base state dword == 4,
// read cross-object through the CMassiveBaseObject friendship) logs and returns
// -299 without allocating. The fresh update is stored into the member BEFORE the
// null test (the X360 `stw r3, 0x6C(r31)` precedes the branch), and the two
// failure paths differ: a failed allocation clears the zone state, while a failed
// CRequestImpressionUpdate::CreateRequest only drops the update (the X360 branch
// lands AFTER the `stw r30, 0x10(r31)`).
// ---------------------------------------------------------------------------
int CMassiveZoneManager::CreateImpUpdateReque()
{
    if (mpImpressionUpdate)  // a1[27]
        return 0;

    if (CMassiveClientCore::Instance()->mbIsValid == 4)  // lwz r11, 0x10(core)
    {
        MassiveLog(5, GetName(),
                   "Shutting Down Client, no Need to Allocate CRequestImpressionUpdate");
        return -299;
    }

    void* lpMem = CMassiveListNode::operator new(sizeof(CRequestImpressionUpdate)); // li r3,0x58
    CRequestImpressionUpdate* lpUpdate =
        lpMem ? ::new (lpMem) CRequestImpressionUpdate() : 0;
    mpImpressionUpdate = lpUpdate;  // a1[27] = v3, stored before the null test

    if (!lpUpdate)
    {
        MassiveLog(2, GetName(), "ALLOCATION Failed for CRequestImpressionUpdate");
        SetValid(0);  // stw r30, 0x10(r31) -- BEFORE the SetLastError call
        return SetLastError(-99, reinterpret_cast<const char*>(0)); // &unk_820046A7
    }

    if (lpUpdate->CreateRequest(this))
    {
        // The X360 reloads a1[27] and null-tests it before the deleting-destructor
        // dispatch `(**v5)(v5, 1)`.
        if (mpImpressionUpdate)
            delete mpImpressionUpdate;
        mpImpressionUpdate = 0;  // stw r30, 0x6C(r31); the zone state is NOT cleared here
        return SetLastError(-299, reinterpret_cast<const char*>(0)); // &unk_820046A7
    }

    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::ReportImpressions @ 0x82BD2F08
//
// Flushes the zone's queued impressions: ensures a pending CRequestImpression
// Update exists (CreateImpUpdateReque; a non-null one short-circuits, else its
// error is returned), reports every asset (CMassiveAsset::ReportImpressions) and
// every ad object (vftable slot +0x10) into it, seals it (Finish) and submits it,
// then clears the slot and lazily starts a fresh update. Returns 0 on the flush
// path, or the CreateImpUpdateReque error when no update could be allocated.
// ---------------------------------------------------------------------------
int CMassiveZoneManager::ReportImpressions()
{
    int lResult = 0;
    if (mpImpressionUpdate || (lResult = CreateImpUpdateReque()) == 0)
    {
        mAssetList.GoToStart();
        while (mAssetList.GetCurrent())  // while (*(a1 + 0x54)) -- mAssetList cursor
        {
            CMassiveAsset* lpAsset = static_cast<CMassiveAsset*>(mAssetList.GetCurrData());
            lpAsset->ReportImpressions();
            mAssetList.GoToNext();
        }

        mAdObjectList.GoToStart();
        while (mAdObjectList.GetCurrent())  // while (*(a1 + 0x44)) -- mAdObjectList cursor
        {
            CMassiveAdObject* lpObject =
                static_cast<CMassiveAdObject*>(mAdObjectList.GetCurrData());
            lpObject->ReportImpressions();  // (*(*v4 + 0x10))(v4)
            mAdObjectList.GoToNext();
        }

        mpImpressionUpdate->Finish();
        mpImpressionUpdate->Submit();  // CRequestObject::Submit on the pending update
        mpImpressionUpdate = 0;
        CreateImpUpdateReque();  // start a fresh pending update
        return 0;
    }
    return lResult;
}

// ---------------------------------------------------------------------------
// CMassiveZoneManager::Tick @ 0x82BD3130
//
// The zone state machine, pumped once per client tick. It is gated on a live
// session id (gnMassiveSessionID) and on a non-zero zone state; a live state
// first clears the base last-error dword, then dispatches:
//   state 1  -- build + submit a CRequestEnterZone carrying the zone name and the
//               client core's bandwidth-usage triple, moving BOTH the zone state
//               and the client-core state to 15 on success.
//   state 2  -- entered: tick every ad object in the zone (vftable slot +0x0C).
//   state 17 -- build a CRequestExitZone the same way, tear the zone's elements
//               down, submit, and move the zone state to 16.
// Every error path clears the zone state and returns 0 -- except the missing-
// client-core one on the enter path, which only records -200 (the X360 branches
// straight to the `return 0` epilogue, past the state store). The failed enter/
// exit requests are NOT freed on any of those paths: that leak is the X360's,
// reproduced here rather than silently fixed.
// ---------------------------------------------------------------------------
int CMassiveZoneManager::Tick()
{
    if (!gnMassiveSessionID)  // dword_8327F2CC
        return 0;

    int lnState = GetValid();  // v2 = a1[4]
    if (!lnState)
        return 0;

    ClearLastError();  // a1[1] = 0

    if (lnState == 1)
    {
        void* lpMem = CMassiveListNode::operator new(sizeof(CRequestEnterZone)); // li r3,0x90
        CRequestEnterZone* lpEnterZone = lpMem ? ::new (lpMem) CRequestEnterZone() : 0;
        if (!lpEnterZone)
        {
            MassiveLog(2, GetName(), "ALLOCATION Failed for CRequestEnterZone");
            SetLastError(-99, reinterpret_cast<const char*>(0)); // &unk_820046A7
            SetValid(0);
            return 0;
        }

        CMassiveClientCore* lpCore = CMassiveClientCore::Instance();
        if (!lpCore)
        {
            SetLastError(-200, reinterpret_cast<const char*>(0)); // &unk_820046A7
            return 0;  // NOTE: the zone state is deliberately NOT cleared here
        }

        // lwz r6/r7 = core +0x3C/+0x40, lwz r10 = core +0x44 narrowed to u16.
        int lnCreate = lpEnterZone->CreateRequest(
            this, mpcZoneName,
            lpCore->mBenchmark.mnTotalA,
            lpCore->mBenchmark.mnTotalB,
            static_cast<unsigned short>(lpCore->mBenchmark.mnCount));
        if (lnCreate)
        {
            MassiveLog(2, GetName(), "Failed to Create Request for Enter Zone: %d", lnCreate);
            SetValid(0);
            return 0;
        }

        int lnSubmit = lpEnterZone->Submit();
        if (lnSubmit)
        {
            SetLastError(lnSubmit, "Failed to Submit Request for Enter Zone: %d", lnSubmit);
            SetValid(0);
            return 0;
        }

        SetValid(15);              // stw r11, 0x10(r31)
        lpCore->mbIsValid = 15;    // stw r11, 0x10(r30) -- the client core follows
        return 1;
    }

    if (lnState == 2)
    {
        mAdObjectList.GoToStart();
        while (mAdObjectList.GetCurrent())  // while (*(a1 + 0x44))
        {
            CMassiveAdObject* lpObject =
                static_cast<CMassiveAdObject*>(mAdObjectList.GetCurrData());
            lpObject->Tic();  // (*(*CurrData + 0x0C))(CurrData)
            mAdObjectList.GoToNext();
        }
        return 1;
    }

    if (lnState == 17)
    {
        void* lpMem = CMassiveListNode::operator new(sizeof(CRequestExitZone)); // li r3,0x50
        CRequestExitZone* lpExitZone = lpMem ? ::new (lpMem) CRequestExitZone() : 0;
        if (!lpExitZone)
        {
            MassiveLog(2, GetName(), "ALLOCATION Failed for CRequestExitZone");
            SetLastError(-99, reinterpret_cast<const char*>(0)); // &unk_820046A7
            SetValid(0);
            return 0;
        }

        // The exit path does NOT null-check the singleton (the X360 reads the
        // benchmark counters straight off the Instance() result).
        CMassiveClientCore* lpCore = CMassiveClientCore::Instance();
        if (lpExitZone->CreateRequest(
                this, mpcZoneName,
                lpCore->mBenchmark.mnTotalA,
                lpCore->mBenchmark.mnTotalB,
                static_cast<unsigned short>(lpCore->mBenchmark.mnCount)))
        {
            SetLastError(-297, "Failed to Create Request to Exit Zone");
            SetValid(0);
            return 0;
        }

        DeleteElements();

        if (lpExitZone->Submit())
        {
            SetLastError(-297, "Failed to Submit Request to Exit Zone");
            SetValid(0);
            return 0;
        }

        SetValid(16);  // stw r11, 0x10(r31)
        return 1;
    }

    return 1;  // any other live state: nothing to pump this tick
}

} // namespace MassiveAdClient3
