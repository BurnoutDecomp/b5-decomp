#include "SDKs/Packages/MassiveAd/MassiveAdClient3ZoneManager.h"

#include <new>      // placement new (raw heap-hook allocation + explicit construction)
#include <cstddef>  // std::size_t
#include <cstring>  // strlen, strncpy

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h" // CRequestObject::GetServerType

// ===========================================================================
// MassiveAdClient3 -- CMassiveZoneManager.
//
// SHAPE and BODIES are reconstructed from BURNOUT_X360_ARTIST.XEX (there is no
// leak source / DecFIGS for this vendor middleware). Stores are reproduced
// member-for-member against the X360 disassembly; see MassiveAdClient3ZoneManager.h
// for the per-offset layout map and the list of BLOCKED bodies left for a later
// ledger slice (they dispatch through collaborators -- CMassiveAsset /
// CRequestEnterZone / CMassiveClientCore internal state / un-exposed CMassiveAdObject
// vftable slots -- whose owning layout is not yet reconstructed and must not be
// guessed).
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

} // namespace MassiveAdClient3
