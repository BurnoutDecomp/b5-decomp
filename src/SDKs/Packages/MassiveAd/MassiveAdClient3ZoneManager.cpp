#include "SDKs/Packages/MassiveAd/MassiveAdClient3ZoneManager.h"

#include <new>      // placement new (raw heap-hook allocation + explicit construction)
#include <cstddef>  // std::size_t
#include <cstring>  // strlen, strncpy

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h" // CRequestObject::GetServerType
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Asset.h"      // CMassiveAsset (AssetFind / DeleteElements / Resume / ReportImpressions)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"   // CMassiveAdObject (SubscriberAdd / MAOFind name / SetAssetExpired / ReportImpressions / Resume)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Subscriber.h" // CMassiveAdObjectSubscriber::mpcName (PreSubscriberAssignT)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestImpressionUpdate.h" // CRequestImpressionUpdate::Finish (ReportImpressions)

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
// for the per-offset layout map and the list of BLOCKED bodies left for a later
// ledger slice (they dispatch through collaborators -- CRequestEnterZone /
// CRequestImpressionUpdate / CMassiveClientCore internal state / un-exposed
// CMassiveAdObject vftable slots -- whose owning layout is not yet reconstructed
// and must not be guessed).
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

} // namespace MassiveAdClient3
