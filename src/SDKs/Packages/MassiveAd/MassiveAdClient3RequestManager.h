#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestManager -- the MassiveAd client's outstanding-request
// registry (vendor middleware). Reconstructed from the X360 ARTIST.XEX (no leak /
// DecFIGS):
//     MassiveAdClient3::CRequestManager::CRequestManager               @ 0x82BD06D0
//     MassiveAdClient3::CRequestManager::~CRequestManager              @ 0x82BD0748
//     MassiveAdClient3::CRequestManager::`scalar deleting destructor'  @ 0x82BD0BF8
//     MassiveAdClient3::CRequestManager::Initialize                    @ 0x82BD0798
//     MassiveAdClient3::CRequestManager::Shutdown                      @ 0x82BD07F0
//     MassiveAdClient3::CRequestManager::PrepareForShutdown            @ 0x82BD0860
//     MassiveAdClient3::CRequestManager::SubmitRequest                 @ 0x82BD0900
//     MassiveAdClient3::CRequestManager::RemoveRequest                 @ 0x82BD0A18
//     MassiveAdClient3::CRequestManager::SuspendAll                    @ 0x82BD0AD0
//     MassiveAdClient3::CRequestManager::GetNextRequest                @ 0x82BD0B40
//
// The manager is a lazy process singleton (Initialize allocates + constructs it
// into off_8327F368, Shutdown tears it down and clears the slot). It owns the
// queue of pending CRequestObjects and a critical section that serialises every
// access to that queue; CRequestObject::Submit / Cancel and CRequestBuilder::
// Suspend / Resume drive it through the singleton pointer.
//
// This is vendor/SDK code reconstructed in its canonical vendor home; per the
// naming convention the MassiveAdClient3 namespace and the CRequestManager class
// / method names are PRESERVED VERBATIM (external middleware API).
//
// Layout over the 0x14-byte CMassiveBaseObject base (offsets X360-relative,
// reproduced BY NAME -- the host base, list and critical section are wider; the
// ctor allocates 0x60 bytes on the X360):
//   +0x00  vftable             (off_82184BF8; slot 0 = scalar deleting destructor)
//   +0x14  mRequestQueue       (CMassiveList of outstanding CRequestObjects,
//                               embedded by value; the ctor zeroes its four dwords
//                               in place)
//   +0x24  mbAcceptingRequests (1 at ctor; PrepareForShutdown clears it, and
//                               SubmitRequest rejects with -699 once clear)
//   +0x28  mbSuspended         (0 at ctor; SuspendAll sets it, GetNextRequest
//                               skips dispatch while it is set)
//   +0x2C  mCriticalSection    (CMassiveCriticalSection "RequestQueue")
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

namespace MassiveAdClient3
{

// Defined in MassiveAdClient3Request.h; only referenced here through pointers, so
// a forward declaration is enough (and keeps the two headers acyclic).
class CRequestObject;

// ---------------------------------------------------------------------------
// CRequestManager -- the process-wide outstanding-request registry.
// ---------------------------------------------------------------------------
class CRequestManager : public CMassiveBaseObject
{
public:
    // @ 0x82BD06D0. Chains the base ctor (name "CRequestManager"), installs this
    // class's vftable (off_82184BF8 -- modelled by the virtual dtor), default-
    // constructs the empty request queue, defaults mbAcceptingRequests to 1 /
    // mbSuspended to 0, and constructs the "RequestQueue" critical section.
    CRequestManager();

    // @ 0x82BD0748 (slot 0; the scalar deleting destructor @ 0x82BD0BF8 is the
    // compiler-emitted thunk around it). Rewrites the vftable, then destructs the
    // critical section, the request queue and the base -- all compiler-emitted, so
    // the reconstructed body is empty.
    virtual ~CRequestManager();

    // @ 0x82BD0BF8. Vftable slot 0: runs the destructor, then frees the object
    // through CMassiveBaseObject::operator delete when the low bit of bDelete is
    // set. Returns this. (Shutdown reaches it as the virtual slot-0 dispatch.)
    void* ScalarDeletingDestructor(char bDelete);

    // ----- lazy process singleton -----------------------------------------

    // @ 0x82BD0798. Lazily allocates (through the MassiveAd heap hook) and
    // constructs the singleton into off_8327F368 on first call; a prior instance
    // is returned untouched. Returns null on allocation failure.
    static CRequestManager* Initialize();

    // @ 0x82BD07F0. Clears the client third-party ID / service state, tears the
    // singleton down (virtual scalar deleting destructor) and clears off_8327F368.
    // Returns the (now-freed) former singleton pointer -- the X360 r3 carry-over.
    static void* Shutdown();

    // @ 0x82BD0860. Clears mbAcceptingRequests, then -- under the queue lock --
    // cancels every outstanding request except those whose stored index/type word
    // is 101 or 115, restarting the walk after each cancel (Cancel mutates the
    // queue). Returns 0, or -696 when the lock could not be taken.
    int PrepareForShutdown();

    // ----- request lifecycle (all under the queue lock) --------------------

    // @ 0x82BD0900. Submits pRequest: rejected with -699 when not accepting
    // requests, -700 when the request is not pending. Otherwise wraps it in a new
    // list node and appends it to the queue. Returns 0 on success, -698 on a
    // failed append, or -696 when the lock could not be taken. Direct bl target of
    // CRequestObject::Submit.
    int SubmitRequest(CRequestObject* pRequest);

    // @ 0x82BD0A18. Removes pRequest from the queue (deleting its node). Returns 0
    // when removed, -697 when it was not queued here, or -696 when the lock could
    // not be taken. Direct bl target of CRequestObject::Cancel.
    int RemoveRequest(CRequestObject* pRequest);

    // @ 0x82BD0B40. Pops the first active request out of the queue (removing its
    // node and stamping status 0x100 onto it); returns null while suspended or
    // when no active request is queued, or when the lock could not be taken.
    // Called by CNetworkManager::Tick.
    CRequestObject* GetNextRequest();

    // @ 0x82BD0AD0. Sets mbSuspended under the queue lock. Returns 0, or -696 when
    // the lock could not be taken.
    int SuspendAll();

    // Additive accessor (FLAG: not its own X360 function): CRequestBuilder::
    // Suspend / Resume lock the manager's queue section (the X360 loads the
    // singleton pointer and adds 0x2C to reach it). Exposes the embedded section
    // BY NAME so the builder does not reach into the private member.
    CMassiveCriticalSection* GetSharedCriticalSection() { return &mCriticalSection; }

private:
    CMassiveList            mRequestQueue;       // +0x14 (outstanding requests)
    int                     mbAcceptingRequests; // +0x24 (1 at ctor)
    int                     mbSuspended;         // +0x28 (0 at ctor)
    CMassiveCriticalSection mCriticalSection;    // +0x2C ("RequestQueue")
};

// off_8327F368 -- the live CRequestManager singleton pointer (null before init /
// after shutdown). Defined in MassiveAdClient3RequestManager.cpp.
extern CRequestManager* spRequestManager;

} // namespace MassiveAdClient3
