#include "SDKs/Realmc/RealmcMessageQueue.h"

// ===========================================================================
// RealmcCore::MessageQueue -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The list nodes go through RealmcCore::MessageList / ResponseList
// (RealmcContainers.cpp); the locking and waking through the linked EAThread
// Mutex / Condition. See the header banner for the layout and the flow.
// ===========================================================================

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// MessageQueue::MessageQueue
//
// Both lists start empty (sentinel next/prev pointing at itself), both flags
// clear, the mutex and condition are constructed with (null parameters, no
// default init) -- Initialize() Init's them -- and the wait timeout is 6000ms.
// ---------------------------------------------------------------------------
MessageQueue::MessageQueue()
    : mbInitialized(false)
    , mbShuttingDown(false)
    , mMutex(nullptr, false)
    , mCondition(nullptr, false)
    , miWaitTimeoutMs(6000)
{
    // mMessageList / mResponseList self-initialise to the empty state through
    // their own constructors.
}

// ---------------------------------------------------------------------------
// MessageQueue::Initialize
//
//   if (!mbInitialized) {
//     if (iTimeoutMs >= 0) miWaitTimeoutMs = iTimeoutMs
//     mCondition.Init(ConditionParameters(intraProcess = true, name = null))
//     mMutex.Init(MutexParameters(intraProcess = true, name = null))
//     mbInitialized = true
//   }
//   return 0
// ---------------------------------------------------------------------------
int MessageQueue::Initialize(int iTimeoutMs)
{
    if (!mbInitialized)
    {
        if (iTimeoutMs >= 0)
        {
            miWaitTimeoutMs = iTimeoutMs;
        }

        EA::Thread::ConditionParameters lConditionParams(true, nullptr);
        mCondition.Init(&lConditionParams);

        EA::Thread::MutexParameters lMutexParams(true, nullptr);
        mMutex.Init(&lMutexParams);

        mbInitialized = true;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// MessageQueue::ShutDown
//
//   if (mbInitialized) {
//     lock ; mbShuttingDown = true ; unlock
//     mCondition.Signal(broadcast)
//     up to 10 times, while mbInitialized:
//       lock ; mbInitialized = !mResponseList.empty() ; unlock ; sleep 10ms
//   }
//   return 0
//
// The console recomputes the flag branchlessly as "response list non-empty":
// ShutDown keeps mbInitialized set (and keeps sleeping) until the waiters have
// drained every queued response, then falls out.
// ---------------------------------------------------------------------------
int MessageQueue::ShutDown()
{
    if (mbInitialized)
    {
        mMutex.Lock();
        mbShuttingDown = true;
        mMutex.Unlock();

        mCondition.Signal(true);       // broadcast

        int iRetries = 10;
        while (mbInitialized)
        {
            if (iRetries-- <= 0)
            {
                break;
            }

            mMutex.Lock();
            mbInitialized = !mResponseList.empty();
            mMutex.Unlock();

            EA::Thread::ThreadSleep(10);
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// MessageQueue::GetMessage
//
//   result = copy of the empty-message holder          (AddRef)
//   lock
//   if (!mMessageList.empty()) {
//     result = front node's MessagePtr                  (operator=)
//     mMessageList.DoErase(front node)
//   }
//   unlock ; return result
// ---------------------------------------------------------------------------
MessagePtr MessageQueue::GetMessage()
{
    MessagePtr lMessage(MessagePtr::EMPTY_MESSAGE());

    mMutex.Lock();
    if (!mMessageList.empty())
    {
        MessageListNode* const lpFront = mMessageList.front();
        lMessage = lpFront->maValue;
        mMessageList.DoErase(lpFront);
    }
    mMutex.Unlock();

    return lMessage;
}

// ---------------------------------------------------------------------------
// MessageQueue::PostResponse
//
//   lock
//   pair = { copy of rMessage, copy of rResponse }      (two AddRefs)
//   mResponseList.push_back(pair)                       (DoCreateNode + link at the back)
//   ~pair                                               (two Releases)
//   unlock
//   mCondition.Signal(broadcast)
//   return 0
// ---------------------------------------------------------------------------
int MessageQueue::PostResponse(MessagePtr& rMessage, ResponsePtr& rResponse)
{
    mMutex.Lock();
    {
        const MessageResponsePair lPair = { rMessage, rResponse };
        mResponseList.push_back(lPair);
    }
    mMutex.Unlock();

    mCondition.Signal(true);       // broadcast
    return 0;
}

// ---------------------------------------------------------------------------
// MessageQueue::SendMessage
//
//   lock
//   if (!mbShuttingDown)
//     mMessageList.push_back(rMessage)                  (DoCreateNode + link at the back)
//   unlock
//   return _WaitForResponse(rMessage)
// ---------------------------------------------------------------------------
ResponsePtr MessageQueue::SendMessage(const MessagePtr& rMessage)
{
    mMutex.Lock();
    if (!mbShuttingDown)
    {
        mMessageList.push_back(rMessage);
    }
    mMutex.Unlock();

    return _WaitForResponse(rMessage);
}

// ---------------------------------------------------------------------------
// MessageQueue::_ExtractResponse
//
//   key = copy of rMessage                              (the find_if predicate,
//                                                        message_equal, by value)
//   it = find_if(mResponseList, entry.first's message == key's message)
//   ~key
//   if (it == end) return false
//   rResponse = it->second                              (operator=)
//   mResponseList.erase(it)
//   return true
// ---------------------------------------------------------------------------
bool MessageQueue::_ExtractResponse(const MessagePtr& rMessage, ResponsePtr& rResponse)
{
    MessageListNodeBase* const lpEnd = mResponseList.end();
    MessageListNodeBase* lpNode = mResponseList.begin();
    {
        const MessagePtr lKey(rMessage);
        while (lpNode != lpEnd &&
               static_cast<ResponseListNode*>(lpNode)->maValue.first.Get() != lKey.Get())
        {
            lpNode = lpNode->mpNext;
        }
    }

    if (lpNode == lpEnd)
        return false;

    rResponse = static_cast<ResponseListNode*>(lpNode)->maValue.second;
    mResponseList.erase(lpNode);
    return true;
}

// ---------------------------------------------------------------------------
// MessageQueue::_WaitForResponse
//
//   result = copy of the empty-response holder ; found = false
//   lock
//   while (!mbShuttingDown && !found) {
//     if (!mResponseList.empty() && !mbShuttingDown && _ExtractResponse(rMessage, result))
//       found = true
//     if (!mbShuttingDown && !mResponseList.empty())
//       mCondition.Signal(broadcast)                    (other waiters' responses)
//     if (!found && mCondition.Wait(&mMutex, GetThreadTime() + miWaitTimeoutMs) times out)
//       mbShuttingDown = true
//   }
//   unlock ; return result
//
// The console's Condition::Wait reports a timeout as -1 (its semaphore's timeout
// code); the linked EAThread Condition reports it as kResultTimeout.
// ---------------------------------------------------------------------------
ResponsePtr MessageQueue::_WaitForResponse(const MessagePtr& rMessage)
{
    ResponsePtr lResponse(ResponsePtr::EMPTY_RESPONSE());
    bool lbFound = false;

    mMutex.Lock();
    while (!mbShuttingDown)
    {
        if (lbFound)
            break;

        if (!mResponseList.empty() && !mbShuttingDown)
        {
            if (_ExtractResponse(rMessage, lResponse))
                lbFound = true;
        }

        if (!mbShuttingDown && !mResponseList.empty())
            mCondition.Signal(true);

        if (!lbFound)
        {
            const EA::Thread::ThreadTime ltDeadline =
                EA::Thread::GetThreadTime() + static_cast<EA::Thread::ThreadTime>(miWaitTimeoutMs);
            if (mCondition.Wait(&mMutex, ltDeadline) == EA::Thread::Condition::kResultTimeout)
                mbShuttingDown = true;
        }
    }
    mMutex.Unlock();

    return lResponse;
}

} // namespace RealmcCore
