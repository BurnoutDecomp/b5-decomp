#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestManager.h"

// ===========================================================================
// MassiveAdClient3::CRequestBuilder -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE and BODIES both from the X360 asm (no leak / DecFIGS). The builder owns
// the collection of outstanding CRequestObjects (an embedded CMassiveList at
// +0x14) and the default submit status (+0x24). Suspend / Resume fan a suspend /
// resume out to every pending request while holding the request manager's shared
// critical section (spRequestManager + 0x2C). See MassiveAdClient3RequestBuilder.h
// for the layout map; the vftable (off_82186008) is modelled by the virtual dtor
// + the two callback virtuals, so no vftable store is written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CRequestBuilder::CRequestBuilder @ 0x82BD4688
//
//   CMassiveBaseObject::CMassiveBaseObject(this, a2);   (a2 = derived name)
//   *this = off_82186008;                               (vftable -- implicit)
//   +0x14 = +0x18 = +0x1C = +0x20 = 0;                  (list zeroed in place)
//   +0x24 = 16;
// ---------------------------------------------------------------------------
CRequestBuilder::CRequestBuilder(const char* pcName)
    : CMassiveBaseObject(pcName)  // bl CMassiveBaseObject::CMassiveBaseObject
    , mRequestList()              // stw 0, 0x14 / 0x18 / 0x1C / 0x20
    , mnSubmitStatus(16)          // stw 0x10, 0x24
{
}

// ---------------------------------------------------------------------------
// CRequestBuilder::~CRequestBuilder @ 0x82BD46E0
//
//   *this = off_82186008;                               (vftable rewrite -- implicit)
//   for (cur = list.GoToStart()/GetCurrData(); cur; GoToNext())
//   {   CRequestObject::Cancel(cur); (**cur)(cur, 1); } (cancel + delete each)
//   list.RemoveAll();
//   ~CMassiveList(list); ~CMassiveBaseObject(this);     (both implicit)
//
// The scalar deleting destructor @ 0x82BD49C0 is the compiler-emitted thunk
// around this dtor (conditional CMassiveBaseObject::operator delete), modelled
// by the virtual dtor + the base's static operator delete.
// ---------------------------------------------------------------------------
CRequestBuilder::~CRequestBuilder()
{
    mRequestList.GoToStart();

    CRequestObject* lpRequest;
    while ((lpRequest = static_cast<CRequestObject*>(mRequestList.GetCurrData())) != 0)
    {
        lpRequest->Cancel();
        delete lpRequest;  // (**v4)(v4, 1) -- virtual deleting destructor
        mRequestList.GoToNext();
    }

    mRequestList.RemoveAll();
}

// ---------------------------------------------------------------------------
// CRequestBuilder::RemoveFromRequestCollect @ 0x82BD47E0
//
// Walks the collection for pRequest; on a match removes the current node
// (Remove(list, list.mpCurrent, 1)) and deletes the request. Returns 0 when
// removed, -999 when the request is not in the collection.
// ---------------------------------------------------------------------------
int CRequestBuilder::RemoveFromRequestCollect(CRequestObject* pRequest)
{
    mRequestList.GoToStart();

    CRequestObject* lpCurrent;
    while ((lpCurrent = static_cast<CRequestObject*>(mRequestList.GetCurrData())) != 0)
    {
        if (lpCurrent == pRequest)
        {
            mRequestList.Remove(mRequestList.GetCurrent(), 1);  // Remove(list, *(this+0x1C), 1)
            delete lpCurrent;  // (**CurrData)(CurrData, 1)
            return 0;
        }
        mRequestList.GoToNext();
    }

    return -999;  // -0x3E7
}

// ---------------------------------------------------------------------------
// CRequestBuilder::Suspend @ 0x82BD4868
//
// Guards on the request-manager singleton, then tries its shared critical
// section; on success suspends every pending request in a non-empty collection
// (returns 0), reports -998 for an empty collection, and releases the section.
// -696 when the manager is absent or the section could not be acquired.
// ---------------------------------------------------------------------------
int CRequestBuilder::Suspend()
{
    if (spRequestManager &&
        spRequestManager->GetSharedCriticalSection()->TryEnter(GetName()))
    {
        int lnResult;
        if (mRequestList.GetCount() > 0)  // *(this + 0x20) = list.mnCount
        {
            mRequestList.GoToStart();

            CRequestObject* lpRequest;
            while ((lpRequest = static_cast<CRequestObject*>(mRequestList.GetCurrData())) != 0)
            {
                if (lpRequest->IsPending())
                    lpRequest->Suspend();
                mRequestList.GoToNext();
            }

            lnResult = 0;
        }
        else
        {
            lnResult = -998;  // -0x3E6
        }

        spRequestManager->GetSharedCriticalSection()->Exit(GetName());
        return lnResult;
    }

    return -696;  // -0x2B8
}

// ---------------------------------------------------------------------------
// CRequestBuilder::Resume @ 0x82BD4918
//
// Symmetric to Suspend. NOTE: the asm does not null-check spRequestManager here
// before adding 0x2C to reach the section (only Suspend does); reproduced
// verbatim -- the singleton pointer is dereferenced for the section directly.
// ---------------------------------------------------------------------------
int CRequestBuilder::Resume()
{
    if (spRequestManager->GetSharedCriticalSection()->TryEnter(GetName()))
    {
        int lnResult;
        if (mRequestList.GetCount() > 0)  // *(this + 0x20) = list.mnCount
        {
            mRequestList.GoToStart();

            CRequestObject* lpRequest;
            while ((lpRequest = static_cast<CRequestObject*>(mRequestList.GetCurrData())) != 0)
            {
                if (lpRequest->IsPending())
                    lpRequest->Resume();
                mRequestList.GoToNext();
            }

            lnResult = 0;
        }
        else
        {
            lnResult = -998;  // -0x3E6
        }

        spRequestManager->GetSharedCriticalSection()->Exit(GetName());
        return lnResult;
    }

    return -696;  // -0x2B8
}

} // namespace MassiveAdClient3
