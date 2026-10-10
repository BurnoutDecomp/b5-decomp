#pragma once

// ===========================================================================
// SDKs/Realmc/RealmcMessageQueue.h
//
// RealmcCore::MessageQueue -- the cross-thread request/response queue the Realmc
// memory-card worker uses to hand messages between the card thread and the game
// thread. It embeds two eastl::list instances over the Realmc allocator (a
// "message" list of RealmcCore::MessagePtr and a "response" list of
// pair<MessagePtr, ResponsePtr>), an EA::Thread::Mutex, an
// EA::Thread::Condition, and a wait timeout in milliseconds.
//
// The card thread posts a message with SendMessage and blocks in
// _WaitForResponse; the game thread drains messages with GetMessage, applies
// each to its processor, and answers through PostResponse, which pairs the
// response with its message on the response list and wakes the waiters. A
// waiter takes its own pair off the list with _ExtractResponse.
//
// No Feb-2007 leak source and no DWARF for this TU: the SHAPE and BODIES below
// come purely from the console pseudocode + asm. `Realmc` is a vendor library
// boundary, so its identifiers are preserved verbatim per the naming rules.
//
// LAYOUT (console offsets, from the ctor stores):
//   +0x00  mMessageList   -- eastl::list<MessagePtr> (sentinel + allocator word)
//   +0x0C  mResponseList  -- eastl::list<pair<MessagePtr,ResponsePtr>>
//   +0x18  mbInitialized  -- byte flag: the mutex+condition have been Init'd (and,
//                            during ShutDown, "the response list still has work")
//   +0x19  mbShuttingDown -- byte flag: a shutdown/quit has been requested (also
//                            set when a response wait times out)
//   +0x20  mMutex         -- EA::Thread::Mutex guarding both lists + the flags
//   +0x50  mCondition     -- EA::Thread::Condition the waiters block on
//   +0xB0  miWaitTimeoutMs-- the per-wait deadline offset (default 6000ms)
//
// The mutex and condition are the linked EAThread ones (the same Mutex
// MemcardState holds); their host sizes differ from the console's, so every
// field is reached BY NAME (semantic parity, not byte-matching).
// ===========================================================================

#include "types.hpp"

#include "SDKs/Realmc/RealmcCore.h"         // RealmcCore::MessagePtr / ResponsePtr (the list element types)
#include "SDKs/Realmc/RealmcContainers.h"   // RealmcCore::MessageList / ResponseList
#include <eathread/eathread.h>              // EA::Thread::ThreadTime / GetThreadTime / ThreadSleep
#include <eathread/eathread_mutex.h>        // EA::Thread::Mutex / MutexParameters
#include <eathread/eathread_condition.h>    // EA::Thread::Condition / ConditionParameters

// Keep the member spellings when <windows.h> came in through the includes above
// (see the same guard in RealmcCore.h).
#ifdef SendMessage
#undef SendMessage
#endif
#ifdef GetMessage
#undef GetMessage
#endif

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// RealmcCore::MessageQueue -- see the file banner for the layout and the flow.
// ---------------------------------------------------------------------------
class MessageQueue
{
public:
    // Empty-init both lists, clear both flags, construct the mutex + condition
    // without initialising them (two-phase: Initialize() Init's them), default
    // the wait timeout to 6000ms.
    MessageQueue();

    // One-shot init: when not already initialised, latch the wait timeout (when
    // iTimeoutMs >= 0), Init the condition and mutex from intra-process
    // parameters, and mark initialised. Returns 0.
    int Initialize(int iTimeoutMs);

    // Request shutdown: set the quit flag, wake every waiter, then spin (up to
    // 10 sleeps of 10ms) until the response list has drained. Returns 0.
    int ShutDown();

    // Take the oldest message off the message list (game thread). Returns the
    // empty message when the list is empty.
    MessagePtr GetMessage();

    // Pair rResponse with rMessage on the response list and wake every waiter.
    // Returns 0.
    int PostResponse(MessagePtr& rMessage, ResponsePtr& rResponse);

    // Post rMessage on the message list (unless the queue is shutting down) and
    // block until its response arrives, the queue shuts down, or a wait times
    // out; the last two yield the empty response. Card thread.
    ResponsePtr SendMessage(const MessagePtr& rMessage);

private:
    // Under the held lock: find the response list entry whose message is
    // rMessage's message; when found, copy its response into rResponse, erase
    // the entry and return true.
    bool _ExtractResponse(const MessagePtr& rMessage, ResponsePtr& rResponse);

    // Under the lock, loop until rMessage's response has been extracted or the
    // queue is shutting down: extract when the response list has entries, wake
    // the other waiters while entries remain, and wait on the condition with a
    // miWaitTimeoutMs deadline (a timed-out wait marks the queue shutting down).
    // Returns the extracted response, or the empty response.
    ResponsePtr _WaitForResponse(const MessagePtr& rMessage);

    MessageList           mMessageList;    // +0x00  eastl::list<MessagePtr>
    ResponseList          mResponseList;   // +0x0C  eastl::list<pair<MessagePtr,ResponsePtr>>
    bool                  mbInitialized;   // +0x18
    bool                  mbShuttingDown;  // +0x19
    EA::Thread::Mutex     mMutex;          // +0x20
    EA::Thread::Condition mCondition;      // +0x50
    int                   miWaitTimeoutMs; // +0xB0  (default 6000)

    // Non-copyable (embedded Mutex/Condition are non-copyable).
    MessageQueue(const MessageQueue&);
    MessageQueue& operator=(const MessageQueue&);
};

} // namespace RealmcCore
