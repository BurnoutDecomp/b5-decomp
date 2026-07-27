#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestManager.h"

#include <new>  // placement new (raw heap-hook allocation + explicit construction)

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

// ===========================================================================
// MassiveAdClient3::CRequestManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE and BODIES both from the X360 asm (no leak / DecFIGS). The manager owns
// an embedded CMassiveList of outstanding CRequestObjects (+0x14) guarded by an
// embedded CMassiveCriticalSection (+0x2C); see MassiveAdClient3RequestManager.h
// for the layout map. The vftable (off_82184BF8) is modelled by the virtual dtor,
// so no vftable store is written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

// off_8327F368 -- the live singleton pointer (null before init / after shutdown).
CRequestManager* spRequestManager = 0;

// ---------------------------------------------------------------------------
// dword_8327F2CC / dword_8327F2D0 -- the client-wide MassiveAd session / player
// IDs. Their real homes are gnMassiveSessionID / gnMassivePlayerID
// (MassiveAdClient3Request.h, defined in MassiveAdClient3Request.cpp); the
// earlier anonymous-namespace `gpMassiveClientState_*` placeholder pair this TU
// carried was a duplicate model of the SAME two .data slots (it would have made
// Shutdown clear file-local copies instead of the live IDs) and was removed
// when the globals were homed.
// ---------------------------------------------------------------------------
// CRequestManager::CRequestManager @ 0x82BD06D0
//
//   CMassiveBaseObject::CMassiveBaseObject(this, "CRequestManager");
//   vftable = off_82184BF8;
//   +0x14..+0x20 = 0 (empty request list); +0x24 = 1; +0x28 = 0;
//   CMassiveCriticalSection::CMassiveCriticalSection(this+0x2C, "RequestQueue");
// ---------------------------------------------------------------------------
CRequestManager::CRequestManager()
    : CMassiveBaseObject("CRequestManager") // bl CMassiveBaseObject::CMassiveBaseObject
    , mRequestQueue()                       // stw 0, 0x14..0x20 (empty list)
    , mbAcceptingRequests(1)                // stw 1, 0x24
    , mbSuspended(0)                        // stw 0, 0x28
    , mCriticalSection("RequestQueue")      // bl CMassiveCriticalSection ctor @ +0x2C
{
}

// ---------------------------------------------------------------------------
// CRequestManager::~CRequestManager @ 0x82BD0748
//
// The X360 rewrites the vftable back to off_82184BF8 (compiler-emitted), then
// destructs the critical section (+0x2C), the request queue (+0x14) and the base
// -- all compiler-emitted member/base destruction, in reverse declaration order.
// The reconstructed body is therefore empty.
// ---------------------------------------------------------------------------
CRequestManager::~CRequestManager()
{
}

// ---------------------------------------------------------------------------
// CRequestManager::`scalar deleting destructor' @ 0x82BD0BF8
//
// The compiler-emitted vftable slot-0 thunk: run the destructor, then -- iff
// (bDelete & 1) -- free the object through the base operator delete. Returns this.
// ---------------------------------------------------------------------------
void* CRequestManager::ScalarDeletingDestructor(char bDelete)
{
    this->~CRequestManager();                  // bl ...~CRequestManager
    if ((bDelete & 1) != 0)                     // clrlwi. r11, r30, 31; beq
        CMassiveBaseObject::operator delete(this); // bl ...operator delete(a1)
    return this;                                // r3 = r31
}

// ---------------------------------------------------------------------------
// CRequestManager::Initialize @ 0x82BD0798
//
// Lazy singleton: if the slot is empty, allocate 0x60 bytes through the MassiveAd
// heap hook (CMassiveListNode::operator new) and construct a CRequestManager into
// them; an existing instance is returned untouched. A failed allocation stores
// (and returns) null.
// ---------------------------------------------------------------------------
CRequestManager* CRequestManager::Initialize()
{
    CRequestManager* lpInstance = spRequestManager;         // lwz r3, off_8327F368
    if (!lpInstance)                                        // bne -> skip
    {
        void* lpMemory = CMassiveListNode::operator new(96); // li r3, 0x60; bl operator new
        lpInstance = lpMemory ? ::new (lpMemory) CRequestManager() // bl CRequestManager ctor
                              : 0;                            // li r3, 0
        spRequestManager = lpInstance;                        // stw r3, off_8327F368
    }
    return lpInstance;
}

// ---------------------------------------------------------------------------
// CRequestManager::Shutdown @ 0x82BD07F0
//
// Clear the client third-party ID / service state and two adjacent .data slots,
// then -- if the singleton exists -- run its virtual scalar deleting destructor
// (bDelete = 1) and clear the slot. Returns the former singleton pointer (the
// X360 r3 carry-over from the destructor call).
// ---------------------------------------------------------------------------
void* CRequestManager::Shutdown()
{
    CRequestObject::ClearThirdPartyID();       // bl ...ClearThirdPartyID
    CRequestObject::ClearThirdPartyService();  // bl ...ClearThirdPartyService (r3 noise dropped)
    gnMassivePlayerID = 0;                     // stw 0, dword_8327F2D0
    void* lpResult = spRequestManager;         // lwz r3, off_8327F368
    gnMassiveSessionID = 0;                    // stw 0, dword_8327F2CC
    if (spRequestManager)                      // beq -> skip
        lpResult = spRequestManager->ScalarDeletingDestructor(1); // vtable[0](this, 1)
    spRequestManager = 0;                      // stw 0, off_8327F368
    return lpResult;
}

// ---------------------------------------------------------------------------
// CRequestManager::PrepareForShutdown @ 0x82BD0860
//
// Stop accepting requests, then -- under the queue lock -- cancel every queued
// request whose stored index/type word (mnServerIndex) is not one of the two
// keep-states 101 / 115. Cancel mutates the queue, so the walk restarts from the
// head after each cancel. Returns 0, or -696 when the lock could not be taken.
// ---------------------------------------------------------------------------
int CRequestManager::PrepareForShutdown()
{
    mbAcceptingRequests = 0;                                       // stw 0, 0x24
    if (!mCriticalSection.TryEnter("CRequestManager::PrepareForShutdown"))
        return -696;

    mRequestQueue.GoToStart();
    while (mRequestQueue.GetCurrent())                            // while mpCurrent != 0
    {
        CRequestObject* lpRequest =
            static_cast<CRequestObject*>(mRequestQueue.GetCurrData());
        if (lpRequest->mnServerIndex == 101 || lpRequest->mnServerIndex == 115)
        {
            mRequestQueue.GoToNext();                             // keep it, advance
        }
        else
        {
            lpRequest->Cancel();                                 // cancel + restart
            mRequestQueue.GoToStart();
        }
    }

    mCriticalSection.Exit("CRequestManager::PrepareForShutdown");
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestManager::SubmitRequest @ 0x82BD0900
//
// Reject when not accepting requests (-699) or when the request is not pending
// (-700); otherwise wrap it in a fresh list node and append it under the queue
// lock. Returns 0 on success, -698 on a failed append, or -696 when the lock
// could not be taken.
//
// (The TryEnter trace string is verbatim X360 rodata -- the shipped code reuses
//  "CRequestManager::PrepareForShutdown" here; reproduced exactly.)
// ---------------------------------------------------------------------------
int CRequestManager::SubmitRequest(CRequestObject* pRequest)
{
    if (!mbAcceptingRequests)                  // lwz 0x24; beq
        return -699;
    if (!pRequest->IsPending())                // bl IsPending; beq
        return -700;
    if (!mCriticalSection.TryEnter("CRequestManager::PrepareForShutdown"))
        return -696;

    void* lpMemory = CMassiveListNode::operator new(12); // li r3, 0xC; bl operator new
    CMassiveListNode* lpNode =
        lpMemory ? ::new (lpMemory) CMassiveListNode(pRequest) // bl CMassiveListNode ctor
                 : 0;                                          // li r4, 0
    int lnResult = mRequestQueue.Append(lpNode) ? 0 : -698;   // bl Append; cmpwi

    mCriticalSection.Exit("CRequestManager::SubmitRequest");
    return lnResult;
}

// ---------------------------------------------------------------------------
// CRequestManager::RemoveRequest @ 0x82BD0A18
//
// Remove pRequest from the queue under the lock (deleting its node). Returns 0
// when removed, -697 when it was not queued here, or -696 when the lock could not
// be taken.
// ---------------------------------------------------------------------------
int CRequestManager::RemoveRequest(CRequestObject* pRequest)
{
    if (!mCriticalSection.TryEnter("CRequestManager::RemoveRequest"))
        return -696;

    MassiveLog(5, GetName(), "Removing Request", pRequest); // STUB(5, mpcName, fmt, req)

    mRequestQueue.GoToStart();
    int lnResult;
    for (;;)
    {
        void* lpCurr = mRequestQueue.GetCurrData();
        if (!lpCurr)                                        // walked off the end
        {
            lnResult = -697;
            break;
        }
        if (lpCurr == pRequest)                             // found it
        {
            mRequestQueue.Remove(mRequestQueue.GetCurrent(), 1); // Remove(mpCurrent, delete)
            lnResult = 0;
            break;
        }
        mRequestQueue.GoToNext();
    }

    mCriticalSection.Exit("CRequestManager::RemoveRequest");
    return lnResult;
}

// ---------------------------------------------------------------------------
// CRequestManager::SuspendAll @ 0x82BD0AD0
//
// Set mbSuspended under the queue lock. Returns 0, or -696 when the lock could not
// be taken.
// ---------------------------------------------------------------------------
int CRequestManager::SuspendAll()
{
    MassiveLog(5, GetName(), "Suspended.");    // STUB(5, mpcName, "Suspended.")
    if (!mCriticalSection.TryEnter("CRequestManager::SuspendAll"))
        return -696;

    mbSuspended = 1;                           // stw 1, 0x28
    mCriticalSection.Exit("CRequestManager::SuspendAll");
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestManager::GetNextRequest @ 0x82BD0B40
//
// Under the queue lock, and only while not suspended, walk the queue for the first
// active request: remove its node, stamp status 0x100 onto it, and return it.
// Returns null while suspended, when no active request is queued, or when the lock
// could not be taken.
//
// (The TryEnter trace string is verbatim X360 rodata -- the shipped code reuses
//  "CRequestManager::RemoveRequest" here; reproduced exactly.)
// ---------------------------------------------------------------------------
CRequestObject* CRequestManager::GetNextRequest()
{
    if (!mCriticalSection.TryEnter("CRequestManager::RemoveRequest"))
        return 0;

    CRequestObject* lpRequest = 0;
    if (!mbSuspended)                                       // lwz 0x28; bne
    {
        mRequestQueue.GoToStart();
        while ((lpRequest =
                    static_cast<CRequestObject*>(mRequestQueue.GetCurrData())) != 0)
        {
            if (lpRequest->IsActive())                     // bl IsActive; bne
            {
                mRequestQueue.Remove(mRequestQueue.GetCurrent(), 1);
                lpRequest->SetStatus(0x100);               // li r4, 0x100; bl SetStatus
                break;
            }
            mRequestQueue.GoToNext();
        }
    }

    mCriticalSection.Exit("CRequestManager::GetNextRequest");
    return lpRequest;
}

} // namespace MassiveAdClient3
