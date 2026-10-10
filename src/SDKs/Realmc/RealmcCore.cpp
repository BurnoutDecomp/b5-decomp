#include "SDKs/Realmc/RealmcCore.h"
#include "SDKs/Realmc/RealmcMemcardState.h"  // RealmcCore::MemcardState -- IRunnableTask's
                                             // Start/Stop dispatch target (minimal home)
#include "SDKs/Realmc/RealmcMessageQueue.h"   // RealmcCore::MessageQueue -- IRunnableTask::SendMessage's queue
#include <eathread/eathread.h>                // EA::Thread::GetThreadTime (SendMessage's send time)

#include <cstring>   // std::memcpy -- the string assign body is a sized copy.
#include <intrin.h>  // _Interlocked* (MSVC) -- portable stand-in for the X360
                     // lwarx/stwcx. reservation idiom.

// ===========================================================================
// RealmcCore core primitives -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcCore.h for the layout and the flagged platform/vendor externs.
// ===========================================================================

namespace RealmcCore
{

// The global allocator backend pointer (X360 off_832BE204). Defined here as a
// null-initialised pointer; the platform Realmc heap layer installs the real
// backend object at boot. (Owning definition for the `extern` in the header.)
IRealmcAllocatorBackend* g_pRealmcAllocator = nullptr;

// ---------------------------------------------------------------------------
// allocator::allocate @ 0x82C44BC8
//
//   lis  r11, off_832BE204@ha ; lwz r3, off_832BE204@l(r11)  -> r3 = backend
//   mr   r6, r5                                              -> r6 = nExtra
//   addi r5, r11, aRealmccoreAllo                            -> r5 = tag string
//   lwz  r10, 0(r3) ; lwz r11, 8(r10) ; mtctr r11 ; bctr     -> vtable slot +8
//
// Tail-call: backend->[+8](backend, r4=nSize, r5=tag, r6=nExtra).
// ---------------------------------------------------------------------------
void* allocator::allocate(std::size_t nSize, int nExtra)
{
    return g_pRealmcAllocator->Allocate(nSize, "RealmcCore::allocator", nExtra);
}

// ---------------------------------------------------------------------------
// allocator::deallocate @ 0x82C44BF0
//
//   lis r11, off_832BE204@ha ; lwz r3, off_832BE204@l(r11)   -> r3 = backend
//   lwz r11, 0(r3) ; lwz r11, 0xC(r11) ; mtctr r11 ; bctr    -> vtable slot +12
//
// Tail-call: backend->[+12](backend, block, size). Only the allocator object is
// replaced by the backend; the block and its byte size arrive as the second and
// third arguments and pass through untouched to the sized Free.
// ---------------------------------------------------------------------------
void allocator::deallocate(void* pBlock, std::size_t nSize)
{
    g_pRealmcAllocator->Free(pBlock, nSize);
}

// ---------------------------------------------------------------------------
// AllocateMem @ 0x82C44B70
//
//   lis  r11, off_832BE204@ha
//   mr   r5, r3                              -> r5 = szTag  (AllocateMem arg 1)
//   li   r8, 0 ; li r7, 0 ; li r6, 0         -> align/alignOffset/flags = 0
//   lwz  r11, off_832BE204@l(r11)            -> r11 = backend
//   mr   r3, r11                             -> r3 = backend (this)
//   lwz  r10, 0(r11) ; lwz r10, 4(r10)       -> vtable slot +4
//   mtctr r10 ; bctr                         -> tail-call
//
// Tail-call: backend->[+4](backend, r4=nSize, r5=szTag, 0, 0, 0). r4 (nSize,
// AllocateMem's second arg) is passed through untouched; only szTag is shuffled
// into r5, so the wrapper is AllocateMem(szTag, nSize) forwarding to the backend's
// extended allocate with a zero flags/align/alignOffset tail.
// ---------------------------------------------------------------------------
void* AllocateMem(const char* szTag, std::size_t nSize)
{
    return g_pRealmcAllocator->Allocate(nSize, szTag, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// FreeMemSize @ 0x82C44BA0
//
//   lis  r11, off_832BE204@ha
//   mr   r5, r4                              -> r5 = luSize (arg 2)
//   mr   r4, r3                              -> r4 = lpBlock (arg 1)
//   lwz  r11, off_832BE204@l(r11)            -> r11 = backend
//   mr   r3, r11                             -> r3 = backend (this)
//   lwz  r10, 0(r11) ; lwz r11, 0xC(r10)     -> vtable slot +12
//   mtctr r11 ; bctr                         -> tail-call
//
// Tail-call: backend->[+12](backend, lpBlock, luSize) == Free(block, size). This
// is the sized free the Realmc deleting destructors call as FreeMemSize(this, N).
// ---------------------------------------------------------------------------
void FreeMemSize(void* lpBlock, u32 luSize)
{
    g_pRealmcAllocator->Free(lpBlock, luSize);
}

// ---------------------------------------------------------------------------
// GetMemAllocator @ 0x82C44B50
//
//   cmplwi cr6, r3, 0                        -> pAllocator == 0 ?
//   lis    r11, off_832BE204@ha
//   beq    cr6, loc_82C44B64                 -> if null, go read
//   stw    r3, off_832BE204@l(r11) ; blr     -> else store + return pAllocator
//   loc:
//   lwz    r3, off_832BE204@l(r11) ; blr     -> return the current global backend
//
// A get-or-set accessor over the global backend pointer: a non-null argument
// installs the backend (and is returned); a null argument queries the current one.
// ---------------------------------------------------------------------------
IRealmcAllocatorBackend* GetMemAllocator(IRealmcAllocatorBackend* pAllocator)
{
    if (pAllocator == nullptr)
    {
        return g_pRealmcAllocator;
    }
    g_pRealmcAllocator = pAllocator;
    return pAllocator;
}

// ===========================================================================
// Message -- the Realmc message base (a RefCount with Apply at vtable +8).
// ===========================================================================

// ---------------------------------------------------------------------------
// Message::Message
//
// Store the RefCount vtable, atomically zero the count at +4 (the interrupt-
// masked reservation store, retried until it sticks), then store the Message
// vtable. The two vtable stores are MSVC's base-then-final ctor sequence; the
// reservation store is modelled portably as an atomic exchange with 0.
// ---------------------------------------------------------------------------
Message::Message()
{
    _InterlockedExchange(reinterpret_cast<volatile long*>(&miRefCount), 0);
}

// ---------------------------------------------------------------------------
// Message::~Message
//
// The vector deleting destructor restores the RefCount vtable and, when the
// delete flag bit0 is set, frees 8 bytes through backend slot +0xC (Message's
// class operator delete). Nothing to do in the body itself.
// ---------------------------------------------------------------------------
Message::~Message()
{
}

// ---------------------------------------------------------------------------
// Message::Apply (vtable +8)
//
// Swap the two arguments so the processor becomes `this`, load the processor's
// vtable slot +0x54 and tail-call it with the message: the plain-Message
// handler.
// ---------------------------------------------------------------------------
void Message::Apply(IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

// ===========================================================================
// RefCount -- shared atomically-refcounted base.
// ===========================================================================

// ---------------------------------------------------------------------------
// RefCount::Release
//
// Atomically decrement the count at +4 (interrupt-masked reservation loop);
// when the post-decrement count is zero, call vtable slot +4 (Unreferenced) on
// the object. Returns the post-decrement count. The reservation loop is
// modelled portably with _InterlockedDecrement.
// ---------------------------------------------------------------------------
int RefCount::Release(RefCount* pThis)
{
    int iCount = static_cast<int>(
        _InterlockedDecrement(reinterpret_cast<volatile long*>(&pThis->miRefCount)));
    if (iCount == 0)
    {
        pThis->Unreferenced();  // vtable slot +4
    }
    return iCount;
}

// ---------------------------------------------------------------------------
// RefCount::Unreferenced (vtable slot +4)
//
// Return at once for a null object; otherwise tail-call vtable slot +0 with the
// delete flag set: the deleting destructor runs the destructor and frees the
// object -- "delete this", whose own null guard is the leading test.
// ---------------------------------------------------------------------------
void RefCount::Unreferenced()
{
    delete this;  // vtable[+0](this, 1): deleting destructor
}

// ---------------------------------------------------------------------------
// RefCount::AddRef  (the inlined Realmc reference-bump idiom)
//
// Every Realmc smart pointer over a RefCount object raises the count at +4 with
// the interrupt-masked reservation increment inlined at its bind site (the
// MessagePtr constructors and operator=). Modelled portably with
// _InterlockedIncrement, the mirror of Release's _InterlockedDecrement.
// ---------------------------------------------------------------------------
void RefCount::AddRef()
{
    _InterlockedIncrement(reinterpret_cast<volatile long*>(&miRefCount));
}

// ---------------------------------------------------------------------------
// RefCount::~RefCount
//
// The vector deleting destructor restores the RefCount vtable and calls the
// global operator delete when the delete flag is set. MSVC synthesises that
// wrapper from this dtor; the body itself does nothing.
// ---------------------------------------------------------------------------
RefCount::~RefCount()
{
}

// ===========================================================================
// Response -- a Message carrying one result word.
// ===========================================================================

// ---------------------------------------------------------------------------
// Response::Response
//
// Store the RefCount vtable, atomically zero the count, store the result word
// at +8, store the Response vtable. The Message base ctor is folded inline (its
// own vtable store is dropped as dead); the atomic zero is that base ctor.
// ---------------------------------------------------------------------------
Response::Response(int iValue)
    : Message(), miValue(iValue)
{
}

// ---------------------------------------------------------------------------
// Response::Apply (vtable +8)
//
// The same swap-and-tail-call thunk as Message::Apply, into the processor's
// slot +0x50: the Response handler.
// ---------------------------------------------------------------------------
void Response::Apply(IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

// ---------------------------------------------------------------------------
// Response::~Response
//
// The vector deleting destructor restores the RefCount vtable and, when the
// delete flag is set, frees 12 bytes (the console sizeof) through backend slot
// +0xC. The dtor body is empty; MSVC emits the deleting-destructor wrapper.
// ---------------------------------------------------------------------------
Response::~Response()
{
}

// ===========================================================================
// RealmcString -- the owned string inside MessageString.
// ===========================================================================

// ---------------------------------------------------------------------------
// RealmcString::Assign  (X360 sub_82B562A0 + the reserve helper sub_82B56228)
//
//   reserve(end - begin + 1):
//     if (n > 1) buf = allocator::allocate(n, 0); set {begin=buf, end=buf,
//                capEnd=buf+n}
//     else       point all three at the shared 1-byte empty singleton
//   memcpy(buf, srcBegin, end - begin)
//   mpEnd = buf + (end - begin) ; *mpEnd = '\0'
//
// The 1-byte empty-singleton branch (n <= 1) is the X360's shared empty-string
// object; modelled here with a function-static 1-byte buffer so an empty assign
// leaves the string non-owning (mpCapEnd - mpBegin == 1 -> Free() is a no-op),
// exactly matching the MessageString destructor's ownership guard.
// ---------------------------------------------------------------------------
void RealmcString::Assign(const char* pSrcBegin, const char* pSrcEnd)
{
    static char saEmpty[1] = { '\0' };

    const std::size_t nLen = static_cast<std::size_t>(pSrcEnd - pSrcBegin);
    const std::size_t nReserve = nLen + 1;

    if (nReserve > 1)
    {
        char* pBuf = static_cast<char*>(allocator::allocate(nReserve, 0));
        mpBegin  = pBuf;
        mpEnd    = pBuf;
        mpCapEnd = pBuf + nReserve;
    }
    else
    {
        mpBegin  = saEmpty;
        mpEnd    = saEmpty;
        mpCapEnd = saEmpty + 1;
    }

    std::memcpy(mpBegin, pSrcBegin, nLen);
    mpEnd = mpBegin + nLen;
    *mpEnd = '\0';
}

// ---------------------------------------------------------------------------
// RealmcString::RealmcString(const char*, const allocator&)
//
//   zero begin / end / capEnd
//   copy the allocator             (an empty body shared by many symbols)
//   n = strlen(pString)            (byte scan to the NUL)
//   Assign(pString, pString + n)   (the RangeInitialize helper)
//
// The eastl::basic_string<char, RealmcCore::allocator> C-string constructor.
// ---------------------------------------------------------------------------
RealmcString::RealmcString(const char* pString, const allocator& /*rAllocator*/)
    : mpBegin(nullptr), mpEnd(nullptr), mpCapEnd(nullptr), mpAllocatorName(nullptr)
{
    const char* pEnd = pString;
    while (*pEnd != '\0')
    {
        ++pEnd;
    }
    Assign(pString, pEnd);
}

// ---------------------------------------------------------------------------
// RealmcString::Free  (the guard inlined into MessageString::~MessageString)
//
//   n = mpCapEnd - mpBegin ; if (n > 1 && mpBegin) backend->Free(mpBegin, n)
//
// Free the buffer only when it is genuinely heap-owned (capacity > 1 and the
// begin pointer is non-null); the shared empty singleton is never freed. The
// byte capacity is the size handed to the sized Free.
// ---------------------------------------------------------------------------
void RealmcString::Free()
{
    const std::ptrdiff_t nCapacity = mpCapEnd - mpBegin;
    if (nCapacity > 1 && mpBegin != nullptr)
    {
        allocator::deallocate(mpBegin, static_cast<std::size_t>(nCapacity));
    }
}

// ===========================================================================
// MessageString -- a Message with an id word and an owned string.
// ===========================================================================

// ---------------------------------------------------------------------------
// MessageString::MessageString
//
// Store the RefCount vtable, atomically zero the count, store muId at +8, store
// the MessageString vtable, zero the RealmcString triple at +0xC, then Assign
// the source string's [begin, end) range into it -- the inlined basic_string
// copy constructor.
// ---------------------------------------------------------------------------
MessageString::MessageString(std::uint32_t uId, const RealmcString& rText)
    : Message(), muId(uId), maText()
{
    maText.Assign(rText.Begin(), rText.End());
}

// ---------------------------------------------------------------------------
// MessageString::~MessageString
//
// Reinstall the MessageString vtable, free the owned string buffer when it is
// heap-owned (capacity > 1 and begin non-null), restore the RefCount vtable.
// The scalar deleting destructor frees 28 bytes (the console sizeof).
// ---------------------------------------------------------------------------
MessageString::~MessageString()
{
    // maText's destructor (RealmcString::Free) releases the owned buffer as the
    // member is destroyed.
}

// ---------------------------------------------------------------------------
// MessageString::Apply (vtable +8)
//
// The swap-and-tail-call thunk into the processor's slot +0x4C: the
// MessageString handler.
// ---------------------------------------------------------------------------
void MessageString::Apply(IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

// ===========================================================================
// MessagePtr / ResponsePtr -- the intrusive smart pointers.
// ===========================================================================

// The three shared holders (owning definitions for the externs in the header).
// They start null; ObjectManager::Initialize creates the objects at boot and
// ObjectManager::Finalize deletes them.
MessagePtr*  g_pRealmcEmptyMessage       = nullptr;
ResponsePtr* g_pRealmcEmptyResponse      = nullptr;
ResponsePtr* g_pRealmcUnfilteredResponse = nullptr;

// ---------------------------------------------------------------------------
// MessagePtr::MessagePtr(Message*)
//
// Store the MessagePtr vtable, atomically increment the message's count (the
// inlined RefCount::AddRef), store the message at +4.
// ---------------------------------------------------------------------------
MessagePtr::MessagePtr(Message* pMessage)
    : mpMessage(pMessage)
{
    mpMessage->AddRef();
}

// ---------------------------------------------------------------------------
// MessagePtr::MessagePtr(const MessagePtr&)
//
// Store the MessagePtr vtable, atomically increment rOther's message count,
// store rOther's message at +4. The console keeps one out-of-line copy (used by
// GameCallbackProcessor's ctor); every other copy site inlines the same steps.
// ---------------------------------------------------------------------------
MessagePtr::MessagePtr(const MessagePtr& rOther)
    : mpMessage(rOther.mpMessage)
{
    mpMessage->AddRef();
}

// ---------------------------------------------------------------------------
// MessagePtr::~MessagePtr
//
// Reinstall the MessagePtr vtable, Release the held message, null the pointer.
// The scalar deleting destructor additionally frees 8 bytes through the backend
// when its delete flag is set.
// ---------------------------------------------------------------------------
MessagePtr::~MessagePtr()
{
    // RefCount::Release takes the target object explicitly (the console thunk
    // reads its first argument as the object), so it is invoked through the
    // message itself.
    mpMessage->Release(mpMessage);
    mpMessage = nullptr;
}

// ---------------------------------------------------------------------------
// MessagePtr::operator=
//
// When the held message differs from rOther's: Release the old message, store
// rOther's, atomically increment its count. Returns *this either way.
// ---------------------------------------------------------------------------
MessagePtr& MessagePtr::operator=(const MessagePtr& rOther)
{
    if (mpMessage != rOther.mpMessage)
    {
        mpMessage->Release(mpMessage);  // Release takes the target explicitly (see ~MessagePtr)
        mpMessage = rOther.mpMessage;
        mpMessage->AddRef();
    }
    return *this;
}

// ---------------------------------------------------------------------------
// MessagePtr::Apply (vtable +4)
//
// Load the held message, load its vtable slot +8 (Message::Apply and its
// overrides) and tail-call it with the processor passed through.
// ---------------------------------------------------------------------------
void MessagePtr::Apply(IMessageProcessor* pProcessor) const
{
    mpMessage->Apply(pProcessor);
}

// ---------------------------------------------------------------------------
// MessagePtr::EMPTY_MESSAGE
//
// Return the shared empty-message holder (a single global load).
// ---------------------------------------------------------------------------
const MessagePtr& MessagePtr::EMPTY_MESSAGE()
{
    return *g_pRealmcEmptyMessage;
}

// ---------------------------------------------------------------------------
// ResponsePtr::ResponsePtr
//
// Store the MessagePtr vtable, atomically increment the response's count, store
// it at +4, store the ResponsePtr vtable.
// ---------------------------------------------------------------------------
ResponsePtr::ResponsePtr(Response* pResponse)
    : MessagePtr(pResponse)
{
}

// ---------------------------------------------------------------------------
// ResponsePtr::~ResponsePtr
//
// Reinstall the ResponsePtr vtable and branch into ~MessagePtr (Release the
// held response, null the pointer) -- the base destructor runs after this empty
// body. The scalar deleting destructor frees 8 bytes through the backend.
// ---------------------------------------------------------------------------
ResponsePtr::~ResponsePtr()
{
}

// ---------------------------------------------------------------------------
// ResponsePtr::GetValue
//
// Load the held object at +4 and return its word at +8: the Response result.
// ---------------------------------------------------------------------------
int ResponsePtr::GetValue() const
{
    return static_cast<const Response*>(mpMessage)->GetValue();
}

// ---------------------------------------------------------------------------
// ResponsePtr::EMPTY_RESPONSE
//
// Return the shared empty-response holder (a single global load).
// ---------------------------------------------------------------------------
const ResponsePtr& ResponsePtr::EMPTY_RESPONSE()
{
    return *g_pRealmcEmptyResponse;
}

// ===========================================================================
// MessageFilter -- the IMessageProcessor a task's message is offered to before
// it is queued.
// ===========================================================================

// ---------------------------------------------------------------------------
// MessageFilter::MessageFilter
//
// Store mpHandler at +4 and the MessageFilter vtable, then build the embedded
// ResponsePtr at +8 over the empty-response holder's response (the inlined
// copy: MessagePtr vtable, AddRef, store, ResponsePtr vtable).
// ---------------------------------------------------------------------------
MessageFilter::MessageFilter(void* pHandler)
    : mpHandler(pHandler),
      maResponse(ResponsePtr::EMPTY_RESPONSE())
{
}

// ---------------------------------------------------------------------------
// MessageFilter::~MessageFilter
//
// Reinstall the MessageFilter vtable, tear down the embedded ResponsePtr
// (Release + null), restore the IMessageProcessor vtable. The vector deleting
// destructor frees 0x10 bytes through backend slot +0xC.
// ---------------------------------------------------------------------------
MessageFilter::~MessageFilter()
{
    // maResponse's destructor (Release + null) runs as the member is destroyed.
}

// ---------------------------------------------------------------------------
// MessageFilter::FilterMessage (vtable +0x58)
//
//   maResponse = *g_pRealmcUnfilteredResponse     (MessagePtr::operator=)
//   rMessage.Apply(this)                           (MessagePtr vtable +4)
//   return a ResponsePtr copy of maResponse        (the inlined copy, by value)
//
// The message reaches this filter's handler for its own type; a handler that
// answers stores its Response in maResponse. A filter that leaves the
// unfiltered holder in place did not answer.
// ---------------------------------------------------------------------------
ResponsePtr MessageFilter::FilterMessage(const MessagePtr& rMessage)
{
    maResponse = *g_pRealmcUnfilteredResponse;
    rMessage.Apply(this);
    return maResponse;
}

// ===========================================================================
// IRunnableTask -- the abstract, refcounted memory-card task base.
// ===========================================================================

// ---------------------------------------------------------------------------
// IRunnableTask::IRunnableTask
//
// Store the RefCount vtable, atomically zero the count, store the message queue
// at +8 and the MemcardState at +0xC, store the IRunnableTask vtable, then
// atomically increment the count: the task starts life holding one reference
// of its own (Release drops it to 0 -> Unreferenced deletes it).
// ---------------------------------------------------------------------------
IRunnableTask::IRunnableTask(MessageQueue* pMessageQueue, MemcardState* pMemcardState)
    : RefCount(), mpMessageQueue(pMessageQueue), mpMemcardState(pMemcardState)
{
    _InterlockedExchange(reinterpret_cast<volatile long*>(&miRefCount), 0);
    AddRef();  // the ctor's trailing atomic increment -> miRefCount = 1
}

// ---------------------------------------------------------------------------
// IRunnableTask::~IRunnableTask
//
// Restore the RefCount vtable; nothing else. The scalar deleting destructor
// additionally runs operator delete when its delete flag bit0 is set.
// ---------------------------------------------------------------------------
IRunnableTask::~IRunnableTask()
{
}

// ---------------------------------------------------------------------------
// IRunnableTask::Starting
//
// Call vtable +0x10 (the task type) and return mpMemcardState->StartTask(type).
// ---------------------------------------------------------------------------
int IRunnableTask::Starting()
{
    return mpMemcardState->StartTask(GetTaskType());  // vtable slot +0x10
}

// ---------------------------------------------------------------------------
// IRunnableTask::InvokeSynchronously
//
// Starting(); vtable +0x0C; vtable +0x08; StopTask(vtable +0x10); return 0.
// The +0x0C body runs first, then the +0x08 one (the runtime call order,
// independent of the slot order).
// ---------------------------------------------------------------------------
int IRunnableTask::InvokeSynchronously()
{
    Starting();
    OnTaskRun();       // vtable slot +0x0C (called first)
    OnTaskComplete();  // vtable slot +0x08 (called second)
    mpMemcardState->StopTask(GetTaskType());  // vtable slot +0x10
    return 0;
}

// ---------------------------------------------------------------------------
// IRunnableTask::operator()
//
//   filter = mpMemcardState->GetMessageFilter() ; filter->Reset()   (+0x5C)
//   this->OnTaskRun() (+0x0C) ; this->OnTaskComplete() (+0x08)
//   type = this->GetTaskType() (+0x10)
//   while (next = mpMemcardState->GetWaitingToStartTask()) {
//     mpMemcardState->StopAndStartTask(type, next->GetTaskType())
//     type = next->GetTaskType()
//     mpMemcardState->GetMessageFilter()->Reset()
//     next->OnTaskRun() ; next->OnTaskComplete()
//     RefCount::Release(next)
//   }
//   mpMemcardState->StopTask(type) ; return 0
//
// The filter is re-read from the MemcardState before every Reset, as the
// console does.
// ---------------------------------------------------------------------------
int IRunnableTask::operator()()
{
    mpMemcardState->GetMessageFilter()->Reset();  // vtable slot +0x5C
    OnTaskRun();                                   // vtable slot +0x0C
    OnTaskComplete();                              // vtable slot +0x08
    int liTaskType = GetTaskType();                // vtable slot +0x10

    for (IRunnableTask* lpNext = mpMemcardState->GetWaitingToStartTask();
         lpNext != nullptr;
         lpNext = mpMemcardState->GetWaitingToStartTask())
    {
        mpMemcardState->StopAndStartTask(liTaskType, lpNext->GetTaskType());
        liTaskType = lpNext->GetTaskType();
        mpMemcardState->GetMessageFilter()->Reset();
        lpNext->OnTaskRun();
        lpNext->OnTaskComplete();
        lpNext->Release(lpNext);
    }

    mpMemcardState->StopTask(liTaskType);
    return 0;
}

// ---------------------------------------------------------------------------
// IRunnableTask::SendMessage
//
//   if (lpuSentTime) *lpuSentTime = -1
//   response = mpMemcardState->GetMessageFilter()->FilterMessage(rMessage)  (+0x58)
//   if (response holds g_pRealmcUnfilteredResponse's response) {
//     response = mpMessageQueue->SendMessage(rMessage)   (temporary, then assigned)
//     if (lpuSentTime) *lpuSentTime = EA::Thread::GetThreadTime()
//   }
//   return a ResponsePtr copy of response
//
// A filter answer short-circuits the queue; otherwise the message goes to the
// game thread and the call blocks until its response arrives (or the queue
// shuts down or times out, which yields the empty response).
// ---------------------------------------------------------------------------
ResponsePtr IRunnableTask::SendMessage(const MessagePtr& rMessage, u32* lpuSentTime)
{
    if (lpuSentTime)
        *lpuSentTime = 0xFFFFFFFFu;

    ResponsePtr lResponse(mpMemcardState->GetMessageFilter()->FilterMessage(rMessage));
    if (lResponse.Get() == g_pRealmcUnfilteredResponse->Get())
    {
        lResponse = mpMessageQueue->SendMessage(rMessage);
        if (lpuSentTime)
            *lpuSentTime = static_cast<u32>(EA::Thread::GetThreadTime());
    }
    return lResponse;
}

} // namespace RealmcCore
