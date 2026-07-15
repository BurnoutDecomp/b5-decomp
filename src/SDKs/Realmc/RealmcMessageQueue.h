#pragma once

// ===========================================================================
// SDKs/Realmc/RealmcMessageQueue.h
//
// RealmcCore::MessageQueue -- the cross-thread request/response queue the Realmc
// memory-card worker uses to hand messages between the game thread and the card
// thread. It embeds two intrusive eastl::list sentinels (a "message" list of
// RealmcCore::MessagePtr and a "response" list of
// eastl::pair<MessagePtr, ResponsePtr>), an EA::Thread::Mutex, an
// EA::Thread::Condition, and a wait-timeout in milliseconds. This is the
// intrusive-list MessageQueue Wave-1 RealmcCore blocked on.
//
// No Feb-2007 leak source and no DWARF for this TU: the SHAPE and BODIES below
// come purely from the X360 pseudocode + asm (BURNOUT_X360_ARTIST.XEX, per-addr
// exports under .ida-exports/BURNOUT_X360_ARTIST.XEX/). `Realmc` is a vendor
// library boundary, so its identifiers (RealmcCore, MessageQueue, GetMessage,
// PostResponse, SendMessage, ...) are preserved verbatim per the naming rules.
//
// LAYOUT (X360 offsets, from the ctor stores @ 0x82C47168):
//   +0x00  mMessageList   -- eastl::list<MessagePtr> sentinel (next/prev self-
//                            referential when empty) + its allocator-name word
//   +0x0C  mResponseList  -- eastl::list<pair<MessagePtr,ResponsePtr>> sentinel
//   +0x18  mbInitialized  -- byte flag: the mutex+condition have been Init'd (and,
//                            during ShutDown, "the response list still has work")
//   +0x19  mbShuttingDown -- byte flag: a shutdown/quit has been requested
//   +0x20  mMutex         -- EA::Thread::Mutex guarding both lists + the flags
//   +0x50  mCondition     -- EA::Thread::Condition the worker waits/signals on
//   +0xB0  miWaitTimeoutMs-- Condition::Wait deadline offset (default 6000ms)
//
// Because the host EA::Thread::Mutex / Condition are sized for the Win32 kernel
// objects (not the X360 ones), the PC member offsets differ from the X360 ones
// above -- every field is reached BY NAME (semantic parity, not byte-matching),
// exactly as the rest of the Realmc reconstruction does.
//
// ---------------------------------------------------------------------------
// BLOCKED (not homed in this TU -- honest gaps, NOT fabricated):
//   RealmcCore::MessageQueue::GetMessage        @ 0x82C46BF8
//   RealmcCore::MessageQueue::PostResponse      @ 0x82C46C98
//   RealmcCore::MessageQueue::SendMessage       @ 0x82C47510
//   RealmcCore::MessageQueue::_ExtractResponse  @ 0x82C47018
//   RealmcCore::MessageQueue::_WaitForResponse  @ 0x82C471F0
//
// All five manipulate the eastl::list NODES (allocate / insert / find_if / erase),
// which route through the still-un-homed custom EASTL allocator + its helpers --
// RealmcCore::allocator_::RangeInitialize and RealmcCore::allocator_::DoErase (a
// SEPARATE todo TU), the node-allocate thunks sub_82C46178 /
// RealmcCore::MessagePtr_clas, the `find_if` ListIterator specialization, and the
// erase helper (RealmcCore::ResponsePtr__struc). Reconstructing the 24-byte
// {next, prev, pair<MessagePtr,ResponsePtr>} node type and those allocator bodies
// here would be a guess; per the reconstruction rules they are left BLOCKED and
// added additively to this header when the allocator TU lands. The three
// functions homed below touch only the list-head sentinel (self-init +
// empty-test) and the EA::Thread primitives, so they need none of that.
// ===========================================================================

#include "types.hpp"

#include "SDKs/Realmc/RealmcCore.h"                    // RealmcCore::MessagePtr / ResponsePtr (the list element types)
#include "SDKs/EATech/eathread/eathread.h"             // EA::Thread facade (ThreadSleep + the umbrella)
#include "SDKs/EATech/eathread/eathread_mutex.h"       // EA::Thread::Mutex / MutexParameters
#include "SDKs/EATech/eathread/eathread_condition.h"   // EA::Thread::Condition / ConditionParameters

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// RealmcListHead -- the embedded intrusive-list sentinel the X360 MessageQueue
// carries twice. It is the head node of an eastl::list: {mpNext, mpPrev} that
// point at the head itself while the list is empty (the exact self-referential
// state the ctor writes -- `stw this,0(this); stw this,4(this)`), followed by the
// EASTL allocator's name word (the +0x08 / +0x14 slot the ctor leaves untouched).
//
// Modeled minimally here for the functions this TU homes (which only need the
// empty-test and the self-init). The list ELEMENT/NODE type and the node
// allocate/erase machinery live with the still-un-homed RealmcCore::allocator_
// TU; mpNext/mpPrev are typed as RealmcListHead* only for the head-sentinel
// self-reference and address comparison -- never to walk real nodes here.
// ---------------------------------------------------------------------------
struct RealmcListHead
{
    RealmcListHead* mpNext;          // sentinel next (points at self when empty)
    RealmcListHead* mpPrev;          // sentinel prev (points at self when empty)
    const char*     mpAllocatorName; // EASTL allocator name word (ctor-untouched)

    // Empty (self-referential) init -- the X360 ctor's two self-stores per list.
    RealmcListHead()
        : mpNext(this)
        , mpPrev(this)
        , mpAllocatorName(nullptr)
    {
    }

    // The list holds no elements when its sentinel still points at itself. Backs
    // the X360 ShutDown branchless test (cntlzw(sentinel - next) & 0x20) == 0,
    // i.e. "next != &sentinel".
    bool IsEmpty() const { return mpNext == this; }
};

// ---------------------------------------------------------------------------
// RealmcCore::MessageQueue -- see the file banner for layout + the blocked set.
// ---------------------------------------------------------------------------
class MessageQueue
{
public:
    // @ 0x82C47168 -- empty-init both list sentinels, clear both flags, construct
    //                 the mutex + condition without auto-initialising them
    //                 (two-phase: Initialize() Init's them), default the wait
    //                 timeout to 6000ms.
    MessageQueue();

    // @ 0x82C44C50 -- one-shot init: when not already initialised, latch the wait
    //                 timeout (when iTimeoutMs >= 0), Init the condition and mutex
    //                 from intra-process parameters, and mark initialised.
    //                 Returns 0.
    int Initialize(int iTimeoutMs);

    // @ 0x82C45000 -- request shutdown: set the quit flag, wake the worker, then
    //                 spin (up to 10 sleeps of 10ms) until the response list has
    //                 drained (mbInitialized tracks "still has pending responses").
    //                 Returns 0.
    int ShutDown();

    // @ 0x82C46C98 -- enqueue a (message, response) pair onto the response list for
    //                 the waiting requester to pick up. DECLARATION-ONLY here: the
    //                 body is still BLOCKED (see the banner above -- it allocates an
    //                 eastl::list node through the un-homed custom allocator) and is
    //                 homed additively when the allocator TU lands. Declared so
    //                 consumers (RealmcIface::GameCallbackProcessor::ProcessMessage)
    //                 can call it by name -- the compile gate is `cl /c` (no link).
    //                 Signature from the X360 asm: r3 = this (queue), r4 = &message
    //                 (MessagePtr), r5 = &response (ResponsePtr).
    int PostResponse(MessagePtr& rMessage, ResponsePtr& rResponse);

private:
    RealmcListHead        mMessageList;    // +0x00  eastl::list<MessagePtr>
    RealmcListHead        mResponseList;   // +0x0C  eastl::list<pair<MessagePtr,ResponsePtr>>
    bool                  mbInitialized;   // +0x18
    bool                  mbShuttingDown;  // +0x19
    EA::Thread::Mutex     mMutex;          // +0x20
    EA::Thread::Condition mCondition;      // +0x50
    int                   miWaitTimeoutMs; // +0xB0  (default 6000)

    // Non-copyable (embedded Mutex/Condition are non-copyable) -- implicit, but
    // spelled out so the intent is clear.
    MessageQueue(const MessageQueue&);
    MessageQueue& operator=(const MessageQueue&);
};

} // namespace RealmcCore
