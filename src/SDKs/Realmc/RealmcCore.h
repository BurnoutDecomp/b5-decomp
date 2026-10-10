#pragma once

// ===========================================================================
// Realmc core -- vendor memory-card library (the "Realmc" / RealmcIface family
// in BURNOUT_X360_ARTIST.XEX). This header is the canonical OWNING home for the
// two Realmc core primitives that the X360 binary defines as standalone thunks:
//
//     RealmcCore::allocator::allocate    @ 0x82C44BC8
//     RealmcCore::allocator::deallocate  @ 0x82C44BF0
//     RealmcCore::Message::Apply         @ 0x82C44C08
//     RealmcCore::Message::Message       @ 0x82C456D8
//     RealmcCore::Message::`vector deleting destructor' @ 0x82C45718
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcCore, allocator, allocate,
// deallocate, Message, Apply) are preserved verbatim per the naming convention.
//
// PLATFORM/VENDOR EXTERNS (flagged):
//   * g_pRealmcAllocator (X360 off_832BE204) -- a global pointer to the Realmc
//     allocator backend object. The object exposes an abstract interface whose
//     vtable holds, at slot +8, an Allocate(size, tag, extra) call and, at slot
//     +12, a Free(...) call. The concrete backend and its vtable are installed
//     by another (platform) Realmc TU; a forward declaration of the interface
//     and the global pointer is all this TU needs to compile and link.
//   * Message base/derived vtables (X360 off_821BA2CC / off_821BA2E8) -- the C++
//     vtables MSVC emits for RealmcCore::Message. They are produced by the
//     compiler from the class definition below, not hand-authored data.
//
// ---------------------------------------------------------------------------
// WAVE-EXTENSION (additive). The following Realmc-core primitives share this
// home (same vendor library boundary, same off_821BA2CC base vtable installed
// first in every ctor and restored in every deleting destructor):
//
//     RealmcCore::RefCount::Release                       @ 0x82C45108
//     RealmcCore::RefCount::Unreferenced                  @ 0x82C44D18
//     RealmcCore::RefCount::`vector deleting destructor'  @ 0x82C450C0
//     RealmcCore::Response::Response                      @ 0x82C458B0
//     RealmcCore::Response::Apply                         @ 0x82C44D38
//     RealmcCore::Response::`vector deleting destructor'  @ 0x82C458F0
//     RealmcCore::MessageString::MessageString            @ 0x82C46338
//     RealmcCore::MessageString::~MessageString           @ 0x82C46028
//     RealmcCore::MessageString::`scalar deleting destructor' @ 0x82C463C0
//
// The base RefCount vtable holds the deleting dtor at +0 and the Unreferenced
// hook Release() fires when the count hits zero at +4; Response and
// MessageString each install their own final vtable after the base, exactly as
// MSVC emits for a derived ctor.
// ===========================================================================

#include <cstddef>
#include <cstdint>

#include "types.hpp"   // u32 -- matches the FreeMemSize declaration in RealmcIfaceMessages.h

// <windows.h>, which some Realmc includers drag in, maps SendMessage and
// GetMessage to their A/W variants. IRunnableTask::SendMessage and the
// MessageQueue members of those names must keep one spelling in every TU.
#ifdef SendMessage
#undef SendMessage
#endif
#ifdef GetMessage
#undef GetMessage
#endif

// The RealmcIface message types the shared message processor (IMessageProcessor,
// below) has a handler slot for. Their homes are the RealmcIface message headers.
namespace RealmcIface
{
class MessageSetActiveCardDone;
class MessageCardRemoved;
class MessageCheckLoadedData;
class MessageSetAutosaveDone;
class MessageLoadDone;
class MessageSaveDone;
class MessageBootupDone;
class MessageShowAutosaveIcon;
} // namespace RealmcIface

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// RefCount -- the shared, atomically-refcounted Realmc base object. It is the
// base of Message (and through it every message and Response) and of
// IRunnableTask.
//
// LAYOUT (from asm):
//   +0  vtable pointer
//   +4  miRefCount -- the 32-bit reference count, decremented under the
//                     interrupt-masked lwarx/stwcx. idiom in Release().
//
// VTABLE (two slots, dumped from the image):
//   slot +0  the deleting destructor.
//   slot +4  Unreferenced() -- Release() fires it when the count reaches zero.
//            Every Realmc vtable that derives RefCount (IRunnableTask,
//            XenonRunnableTask, Message, Response, MessageString, MessageTrc,
//            MessageLoadDone) keeps the RefCount body here: delete the object
//            through slot +0.
// ---------------------------------------------------------------------------
class RefCount
{
public:
    RefCount() : miRefCount(0) {}

    // Atomically decrement miRefCount; when it reaches 0, fire the virtual
    // Unreferenced() hook (vtable slot +4). Returns the post-decrement count.
    int Release(RefCount* pThis);

    // Additive accessor (FLAG: not its own console function). Every Realmc smart
    // pointer over a RefCount object bumps the count with the same interrupt-
    // masked lwarx/addi+1/stwcx. idiom inlined at its construction / assignment
    // site. Exposed here by NAME so those sites raise the count through RefCount
    // rather than reaching the +4 field via a raw offset. Atomic increment, no
    // layout change.
    void AddRef();

    // slot +0 -- the deleting destructor (restore the RefCount vtable, then
    //            operator delete when the delete flag bit0 is set).
    virtual ~RefCount();

    // slot +4 -- the "count reached zero" hook fired by Release(): delete the
    //            object through its slot +0 deleting destructor. The console body
    //            null-checks the object first, then calls slot +0 with the
    //            delete flag set.
    virtual void Unreferenced();

protected:
    std::int32_t miRefCount;  // +4
};

class allocator;  // RealmcCore::allocator, defined below (RealmcString's allocator)

// ---------------------------------------------------------------------------
// RealmcString -- the small Realmc string the console keeps inside MessageString
// (eastl::basic_string<char, RealmcCore::allocator>).
//
// LAYOUT (from MessageString's ctor/dtor + the assign/reserve helpers):
//   +0  mpBegin  -- start of the character buffer (or the shared empty
//                   singleton when the string is empty)
//   +4  mpEnd    -- one past the last character (points at the NUL terminator)
//   +8  mpCapEnd -- one past the end of the allocated buffer
//   +C  the stateless allocator subobject (its copy constructor is an empty
//       body, so the word is never written or read)
//
// The buffer is heap-owned only when (mpCapEnd - mpBegin) > 1 and mpBegin is
// non-null; otherwise mpBegin aliases a shared 1-byte empty singleton and must
// NOT be freed -- exactly the guard the X360 destructor evaluates.
// ---------------------------------------------------------------------------
class RealmcString
{
public:
    RealmcString()
        : mpBegin(nullptr), mpEnd(nullptr), mpCapEnd(nullptr), mpAllocatorName(nullptr) {}

    // Construct from a NUL-terminated string: zero the three pointers, copy the
    // allocator (an empty body), then Assign(pString, pString + strlen(pString)).
    RealmcString(const char* pString, const allocator& rAllocator);

    // Copy constructor (basic_string(const basic_string&)): a fresh buffer over
    // rOther's range, never a shared one.
    RealmcString(const RealmcString& rOther)
        : mpBegin(nullptr), mpEnd(nullptr), mpCapEnd(nullptr), mpAllocatorName(nullptr)
    {
        Assign(rOther.mpBegin, rOther.mpEnd);
    }
    RealmcString& operator=(const RealmcString&) = delete;

    // The inlined basic_string destructor: Free().
    ~RealmcString() { Free(); }

    // Assign from a [pBegin, pEnd) character range (the basic_string
    // RangeInitialize): reserve (pEnd - pBegin + 1) bytes, copy the range,
    // NUL-terminate.
    void Assign(const char* pBegin, const char* pEnd);

    // Release the heap buffer if it is owned: when capacity > 1 and mpBegin is
    // non-null, hand (mpBegin, mpCapEnd - mpBegin) back to the backend.
    void Free();

    // The [Begin(), End()) character range, for copy-construction by owners.
    const char* Begin() const { return mpBegin; }
    const char* End()   const { return mpEnd; }

private:
    char*       mpBegin;         // +0
    char*       mpEnd;           // +4
    char*       mpCapEnd;        // +8
    const char* mpAllocatorName; // +C (stateless allocator word)
};

// ---------------------------------------------------------------------------
// IRealmcAllocatorBackend -- the abstract allocator object reached through the
// global g_pRealmcAllocator pointer (X360 off_832BE204).
//
// The backend exposes two Allocate entry points plus a sized Free. From the asm
// dispatch sites (byte offset == slot*4 for 4-byte X360 pointers):
//   slot +4  the aligned/extended allocate (size, tag, flags, align, alignOffset)
//            -- AllocateMem @0x82C44B70 tail-calls it with (size, tag, 0, 0, 0).
//   slot +8  the plain allocate (size, tag, flags) -- allocator::allocate
//            @0x82C44BC8 and MessagePtr's node-alloc tail-call it.
//   slot +12 the sized Free (block, size) -- deallocate/FreeMemSize tail-call it.
// Modelled here as virtual methods in that exact slot order (declaration order
// == vtable slot order under MSVC).
// ---------------------------------------------------------------------------
class IRealmcAllocatorBackend
{
public:
    virtual ~IRealmcAllocatorBackend() {}            // vtable slot +0
    // slot +4 -- the extended allocate AllocateMem forwards to. AllocateMem's
    // asm passes (r4=size, r5=tag, r6=0, r7=0, r8=0), i.e. the plain allocate's
    // (size, tag, flags) plus a trailing (align, alignOffset) pair, defaulted to
    // zero at the AllocateMem call site.
    virtual void* Allocate(std::size_t nSize,
                           const char* szTag,
                           int nFlags,
                           int nAlign,
                           int nAlignOffset) = 0;     // vtable slot +4
    virtual void* Allocate(std::size_t nSize,
                           const char* szTag,
                           int nExtra) = 0;           // vtable slot +8
    virtual void  Free(void* pBlock, std::size_t nSize) = 0; // vtable slot +12
};

// The global Realmc allocator backend (X360 off_832BE204). Installed by the
// platform Realmc heap layer (another TU); declared here for compile/link.
extern IRealmcAllocatorBackend* g_pRealmcAllocator;

// ---------------------------------------------------------------------------
// Realmc core memory free-functions (X360 thin thunks over g_pRealmcAllocator).
// These are the RealmcCore-namespace memory entry points the Realmc/RealmcIface
// tasks use for allocation. Reconstructed from BURNOUT_X360_ARTIST.XEX (no leak
// source / DWARF).
//   AllocateMem     @ 0x82C44B70 -- backend->Allocate(size, tag, 0, 0, 0) (slot +4)
//   FreeMemSize     @ 0x82C44BA0 -- backend->Free(block, size)            (slot +12)
//   GetMemAllocator @ 0x82C44B50 -- get-or-set the g_pRealmcAllocator pointer
// FreeMemSize is also (redundantly) declared in RealmcIfaceMessages.h with the
// identical signature; its owning body lives in this TU (RealmcCore.cpp).
// ---------------------------------------------------------------------------

// @ 0x82C44B70 -- allocate luSize bytes tagged with szTag through the backend's
//                 slot +4 (extended allocate, flags/align/alignOffset = 0).
void* AllocateMem(const char* szTag, std::size_t nSize);

// @ 0x82C44BA0 -- free a sized block through the backend's slot +12.
void FreeMemSize(void* lpBlock, u32 luSize);

// @ 0x82C44B50 -- when pAllocator is non-null, install it as the global backend
//                 and return it; when null, return the current global backend.
//                 (The X360 uses the null argument as the "just read it" query.)
IRealmcAllocatorBackend* GetMemAllocator(IRealmcAllocatorBackend* pAllocator);

// ---------------------------------------------------------------------------
// RealmcCore::allocator -- a thin stateless adaptor over g_pRealmcAllocator.
// Both methods forward to the global backend; allocate() stamps the allocation
// with the "RealmcCore::allocator" tag string.
// ---------------------------------------------------------------------------
class allocator
{
public:
    // @ 0x82C44BC8 -- forwards to g_pRealmcAllocator->Allocate(nSize,
    //                 "RealmcCore::allocator", nExtra).
    static void* allocate(std::size_t nSize, int nExtra);

    // @ 0x82C44BF0 -- forwards to g_pRealmcAllocator->Free(...).
    // The block and its byte size pass straight through to the backend's sized
    // Free; only the allocator object is swapped for the backend.
    static void deallocate(void* pBlock, std::size_t nSize);
};

// ---------------------------------------------------------------------------
// The message family. Every Realmc message is a RefCount with one extra virtual,
// Apply(processor), and every message target is the one IMessageProcessor below:
// Apply is a double dispatch that hands the message to the processor handler
// slot for its own type. MessagePtr / ResponsePtr are the intrusive smart
// pointers that carry messages and responses across the card-thread queue, and
// MessageFilter is the processor a task's outgoing message is offered to before
// it is queued.
// ---------------------------------------------------------------------------
class IMessageProcessor;
class Response;
class MessageString;
class MessageTrc;    // home: SDKs/Realmc/RealmcTrc.h
class MessageClear;  // home: SDKs/Realmc/RealmcCoreMessageClear.h
class MessageQueue;  // home: SDKs/Realmc/RealmcMessageQueue.h

// ---------------------------------------------------------------------------
// RealmcCore::Message -- the Realmc message base.
//
// LAYOUT: +0 vtable, +4 miRefCount (inherited from RefCount). The console
// sizeof is 8: the deleting destructor frees 8 bytes through the backend.
//
// VTABLE (dumped from the image): [+0 deleting destructor,
// +4 RefCount::Unreferenced, +8 Message::Apply]. Every message type derives
// Message and overrides Apply at +8.
// ---------------------------------------------------------------------------
class Message : public RefCount
{
public:
    // Install the vtable and atomically zero the inherited refcount.
    Message();

    // slot +0 -- restore the RefCount vtable; the deleting form frees the
    //            object through the Realmc backend (operator delete below).
    ~Message() override;

    // slot +8 -- pProcessor->ProcessMessage(this), the processor's +0x54 slot
    //            (the plain-Message handler). The console thunk swaps its two
    //            arguments so the processor becomes `this` and tail-calls the
    //            slot; every override below has the same one-line shape.
    virtual void Apply(IMessageProcessor* pProcessor);

    // Messages are allocated through the Realmc backend (AllocateMem) and the
    // deleting destructors hand them back to it with their size (backend slot
    // +0xC, i.e. FreeMemSize). Every class derived from Message inherits this
    // routing. The size is the host sizeof, supplied by the compiler.
    static void operator delete(void* lpBlock, std::size_t luSize)
    {
        FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }
};

// ---------------------------------------------------------------------------
// IMessageProcessor -- the single target every message's Apply dispatches
// into. Its vtable (dumped: 22 slots) is the deleting destructor followed by 21
// pure handler slots, one per message type. The image's symbols name two of
// the overrides ProcessMessage (XenonMessageFilter's MessageTrc handler and
// GameCallbackProcessor's Response handler), so every handler is a
// ProcessMessage overload. Slot -> message type, read off each message's Apply
// thunk:
//   +0x04 MessageSetActiveCardDone   +0x0C MessageCardRemoved
//   +0x10 MessageCheckLoadedData     +0x18 MessageSetAutosaveDone
//   +0x30 MessageLoadDone            +0x34 MessageSaveDone
//   +0x3C MessageBootupDone          +0x40 MessageShowAutosaveIcon
//   +0x44 MessageClear               +0x48 MessageTrc
//   +0x4C MessageString              +0x50 Response
//   +0x54 Message
// The other eight slots (+0x08, +0x14, +0x1C, +0x20, +0x24, +0x28, +0x2C,
// +0x38) serve message types this build never constructs: the image holds no
// Apply thunk and no vtable for any of them, so their parameter types are not
// known and they are not declared here. Nothing on the host dispatches by slot
// offset (each Apply resolves its handler by overload), so the host vtable
// order does not need to match the console's.
//
// The handlers return nothing the callers read: Apply tail-calls them and every
// Apply caller (MessageFilter::FilterMessage, the interface's update loop)
// discards the result.
// ---------------------------------------------------------------------------
class IMessageProcessor
{
public:
    virtual ~IMessageProcessor() {}                                                    // +0x00
    virtual void ProcessMessage(RealmcIface::MessageSetActiveCardDone* pMessage) = 0;  // +0x04
    virtual void ProcessMessage(RealmcIface::MessageCardRemoved* pMessage) = 0;        // +0x0C
    virtual void ProcessMessage(RealmcIface::MessageCheckLoadedData* pMessage) = 0;    // +0x10
    virtual void ProcessMessage(RealmcIface::MessageSetAutosaveDone* pMessage) = 0;    // +0x18
    virtual void ProcessMessage(RealmcIface::MessageLoadDone* pMessage) = 0;           // +0x30
    virtual void ProcessMessage(RealmcIface::MessageSaveDone* pMessage) = 0;           // +0x34
    virtual void ProcessMessage(RealmcIface::MessageBootupDone* pMessage) = 0;         // +0x3C
    virtual void ProcessMessage(RealmcIface::MessageShowAutosaveIcon* pMessage) = 0;   // +0x40
    virtual void ProcessMessage(MessageClear* pMessage) = 0;                           // +0x44
    virtual void ProcessMessage(MessageTrc* pMessage) = 0;                             // +0x48
    virtual void ProcessMessage(MessageString* pMessage) = 0;                          // +0x4C
    virtual void ProcessMessage(Response* pMessage) = 0;                               // +0x50
    virtual void ProcessMessage(Message* pMessage) = 0;                                // +0x54
};

// ---------------------------------------------------------------------------
// Response -- the reply to a message: a Message carrying one result word.
//
// LAYOUT: +0 vtable, +4 miRefCount, +8 miValue (the ctor argument). The console
// sizeof is 12 (the deleting destructor frees 12 bytes through the backend).
// VTABLE (dumped): [+0 deleting destructor, +4 RefCount::Unreferenced,
// +8 Response::Apply].
// ---------------------------------------------------------------------------
class Response : public Message
{
public:
    // Install the vtable, atomically zero the refcount, store the result word.
    explicit Response(int iValue);

    // slot +0
    ~Response() override;

    // slot +8 -- pProcessor->ProcessMessage(this), the processor's +0x50 slot.
    void Apply(IMessageProcessor* pProcessor) override;

    // The result word ResponsePtr::GetValue reads.
    int GetValue() const { return miValue; }

private:
    int miValue;  // +8
};

// ---------------------------------------------------------------------------
// MessageString -- a Message that carries one id word and one owned string.
//
// LAYOUT (from the ctor/dtor asm):
//   +0    vtable pointer
//   +4    miRefCount (inherited, atomically zeroed in the ctor)
//   +8    muId      -- the id word passed to the ctor
//   +0xC  maText    -- a RealmcString (begin/end/capEnd + allocator word),
//                      copy-constructed in the ctor, freed (if owned) in the dtor.
// The console sizeof is 0x1C (28): the scalar deleting destructor frees 28 bytes.
// VTABLE (dumped): [+0 deleting destructor, +4 RefCount::Unreferenced,
// +8 MessageString::Apply].
// ---------------------------------------------------------------------------
class MessageString : public Message
{
public:
    // Install the vtable, atomically zero the refcount, store muId, then
    // copy-construct maText from rText (Assign over rText's [begin, end) range).
    MessageString(std::uint32_t uId, const RealmcString& rText);

    // Free the owned string buffer (the RealmcString member's destructor).
    ~MessageString() override;

    // slot +8 -- pProcessor->ProcessMessage(this), the processor's +0x4C slot.
    void Apply(IMessageProcessor* pProcessor) override;

private:
    std::uint32_t muId;    // +8
    RealmcString  maText;  // +0xC
};

// ---------------------------------------------------------------------------
// MessagePtr -- the intrusive smart pointer over a Message.
//
// LAYOUT: +0 vtable, +4 mpMessage (the held message, AddRef'd on bind and
// Released on rebind/teardown). The console sizeof is 8.
// VTABLE (dumped): [+0 scalar deleting destructor, +4 MessagePtr::Apply].
// ---------------------------------------------------------------------------
class MessagePtr
{
public:
    // Install the vtable, AddRef the message (the inlined interrupt-masked
    // increment of its refcount), store it.
    explicit MessagePtr(Message* pMessage);

    // Copy: install the vtable, AddRef rOther's message, store it.
    MessagePtr(const MessagePtr& rOther);

    // Rebind to rOther's message: when it differs from the current one, Release
    // the old message, store the new one and AddRef it. Returns *this.
    MessagePtr& operator=(const MessagePtr& rOther);

    // slot +0 -- Release the held message and null the pointer; the scalar
    //            deleting form frees the 8-byte object through the backend.
    virtual ~MessagePtr();

    // slot +4 -- mpMessage->Apply(pProcessor): dispatch into the held message's
    //            own vtable slot +8 with the processor passed through.
    virtual void Apply(IMessageProcessor* pProcessor) const;

    // The shared empty-message holder (a MessagePtr over a bare Message,
    // created by ObjectManager::Initialize).
    static const MessagePtr& EMPTY_MESSAGE();

    // Additive accessor (FLAG: not its own console function): the held message,
    // read by name where the console reads +4 of a MessagePtr.
    Message* Get() const { return mpMessage; }

    // Same backend routing as Message (the scalar deleting destructor frees the
    // object through backend slot +0xC with its size).
    static void operator delete(void* lpBlock, std::size_t luSize)
    {
        FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

protected:
    Message* mpMessage;  // +0x04
};

// ---------------------------------------------------------------------------
// ResponsePtr -- the MessagePtr over a Response. The ctor installs the
// MessagePtr vtable first and then its own; the destructor reinstalls its own
// vtable and branches into ~MessagePtr; the held pointer is the inherited +4.
// VTABLE (dumped): [+0 scalar deleting destructor, +4 MessagePtr::Apply].
// ---------------------------------------------------------------------------
class ResponsePtr : public MessagePtr
{
public:
    // AddRef the response and store it (the base MessagePtr bind).
    explicit ResponsePtr(Response* pResponse);

    // Copy: the inlined MessagePtr copy plus the ResponsePtr vtable store.
    ResponsePtr(const ResponsePtr& rOther) : MessagePtr(rOther) {}

    // Reinstall the ResponsePtr vtable, then the shared MessagePtr teardown.
    ~ResponsePtr() override;

    // The held response's result word (Response +8).
    int GetValue() const;

    // The shared empty-response holder (a ResponsePtr over Response(0), created
    // by ObjectManager::Initialize).
    static const ResponsePtr& EMPTY_RESPONSE();
};

// The three shared holders ObjectManager::Initialize creates and Finalize
// deletes. g_pRealmcEmptyMessage backs MessagePtr::EMPTY_MESSAGE and
// g_pRealmcEmptyResponse backs ResponsePtr::EMPTY_RESPONSE.
// g_pRealmcUnfilteredResponse holds Response(5): MessageFilter::FilterMessage
// rebinds the filter's held response to it before offering the message, so a
// filter that still holds it afterwards did not answer the message (the console
// accesses this holder directly; it has no accessor of its own).
extern MessagePtr*  g_pRealmcEmptyMessage;
extern ResponsePtr* g_pRealmcEmptyResponse;
extern ResponsePtr* g_pRealmcUnfilteredResponse;

// ---------------------------------------------------------------------------
// MessageFilter -- the IMessageProcessor a task's outgoing message is offered
// to before it is queued for the game thread (IRunnableTask::SendMessage).
// A filter that answers a message stores the answer in its held ResponsePtr.
//
// LAYOUT (from the ctor / dtor / FilterMessage asm):
//   +0x00  vtable pointer
//   +0x04  mpHandler   -- the owner the ctor stores (the MemcardState that
//                         creates the filter passes itself)
//   +0x08  maResponse  -- an embedded ResponsePtr (its vtable at +8, the held
//                         response at +0xC), bound in the ctor to the empty
//                         response and rebound in FilterMessage.
// The console sizeof is 16: the deleting destructor frees 0x10 bytes through
// the backend.
//
// VTABLE (dumped: 24 slots): the deleting destructor, all 21 handler slots
// pointing at the shared empty function (a bare MessageFilter answers
// nothing), FilterMessage at +0x58 and Reset at +0x5C (also the empty
// function here; RealmcIface::XenonMessageFilter overrides it).
// ---------------------------------------------------------------------------
class MessageFilter : public IMessageProcessor
{
public:
    // Store mpHandler, then bind maResponse to the empty response (AddRef).
    explicit MessageFilter(void* pHandler);

    // Tear down maResponse (Release + null); restore the IMessageProcessor
    // vtable.
    ~MessageFilter() override;

    void ProcessMessage(RealmcIface::MessageSetActiveCardDone*) override {}
    void ProcessMessage(RealmcIface::MessageCardRemoved*) override {}
    void ProcessMessage(RealmcIface::MessageCheckLoadedData*) override {}
    void ProcessMessage(RealmcIface::MessageSetAutosaveDone*) override {}
    void ProcessMessage(RealmcIface::MessageLoadDone*) override {}
    void ProcessMessage(RealmcIface::MessageSaveDone*) override {}
    void ProcessMessage(RealmcIface::MessageBootupDone*) override {}
    void ProcessMessage(RealmcIface::MessageShowAutosaveIcon*) override {}
    void ProcessMessage(MessageClear*) override {}
    void ProcessMessage(MessageTrc*) override {}
    void ProcessMessage(MessageString*) override {}
    void ProcessMessage(Response*) override {}
    void ProcessMessage(Message*) override {}

    // slot +0x58 -- rebind maResponse to the unfiltered-response holder, let
    //               the message apply itself to this filter (MessagePtr slot +4,
    //               which reaches this filter's handler for the message's type),
    //               then return a copy of the response the filter now holds.
    virtual ResponsePtr FilterMessage(const MessagePtr& rMessage);

    // slot +0x5C -- clear any per-task filter state before a task body runs
    //               (IRunnableTask::operator()). A bare MessageFilter keeps none.
    virtual void Reset() {}

    // The deleting destructor frees the filter through backend slot +0xC with
    // its size, i.e. FreeMemSize (the size is the host sizeof).
    static void operator delete(void* lpBlock, std::size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

    void*       mpHandler;   // +0x04 (owner/target pointer)
    ResponsePtr maResponse;  // +0x08 (vtable at +8, held response at +0xC)
};

// ---------------------------------------------------------------------------
// RealmcCore::IRunnableTask -- the abstract, refcounted base of every Realmc
// memory-card task (the XenonRunnableTask family that
// MemcardInterfaceImpl::RunAsync<T> / ThrFunction<T> drive).
//
// LAYOUT (from the ctor stores):
//   +0x00  vtable pointer
//   +0x04  miRefCount     -- inherited from RefCount; atomically zeroed then
//                            bumped to 1 in the ctor (the task holds one
//                            self-reference)
//   +0x08  mpMessageQueue -- the ctor's first argument: the cross-thread queue
//                            SendMessage posts the task's messages on (it reads
//                            this word as the MessageQueue `this`)
//   +0x0C  mpMemcardState -- the owning MemcardState the task reports Start/Stop
//                            to, and whose message filter SendMessage consults
//
// VTABLE (dumped; slots after RefCount's +0 deleting destructor and
// +4 Unreferenced):
//   +0x08  the second task-body virtual (called after the +0x0C one in every
//          run); IRunnableTask's own entry is the shared empty function
//   +0x0C  the first task-body virtual (pure)
//   +0x10  the task-type id the MemcardState tracks (pure)
// Their names are inferred from their dispatch role (FLAG); their SLOT order is
// fixed by the image, and the runtime CALL order (+0x0C before +0x08) is
// spelled out at each call site.
// ---------------------------------------------------------------------------
class MemcardState;  // fwd -- home SDKs/Realmc/RealmcMemcardState.h

class IRunnableTask : public RefCount
{
public:
    // Atomically zero the inherited refcount, store the message queue (+8) and
    // the owning MemcardState (+0xC), install the vtable, then atomically bump
    // the refcount to 1 (the task's own self-reference).
    IRunnableTask(MessageQueue* pMessageQueue, MemcardState* pMemcardState);

    // Register this task as running on its MemcardState:
    // return mpMemcardState->StartTask(GetTaskType()).
    int Starting();

    // Run the task once, synchronously: Starting(); the two task-body virtuals
    // (+0x0C then +0x08); then StopTask(GetTaskType()). Returns 0.
    int InvokeSynchronously();

    // The async run loop a worker thread enters through ThrFunction<T>: reset
    // the message filter and run this task's two body virtuals, then hand the
    // MemcardState over to every task waiting to start (each one runs the same
    // way and is Released), and finally stop the last running task type.
    // Returns 0.
    int operator()();

    // Offer rMessage to the MemcardState's message filter; when the filter does
    // not answer it, post it on the message queue and wait for the game
    // thread's response. When lpuSentTime is non-null it is set to 0xFFFFFFFF
    // first and, for a queued message, to the thread time after the response
    // arrived. Returns the response by value.
    ResponsePtr SendMessage(const MessagePtr& rMessage, u32* lpuSentTime);

    // slot +0 -- restore the RefCount vtable; the scalar deleting form runs
    //            operator delete when the delete flag bit0 is set.
    virtual ~IRunnableTask();

    // slot +0x08 -- second of the two task-body virtuals dispatched per run.
    //               IRunnableTask's entry is empty; concrete tasks override it.
    //               FLAG: name inferred from dispatch role.
    virtual void OnTaskComplete() {}

    // slot +0x0C -- first task-body virtual dispatched per run. Pure; concrete
    //               tasks implement it. FLAG: name inferred from dispatch role.
    virtual void OnTaskRun() = 0;

    // slot +0x10 -- the task-type id the MemcardState tracks (passed to
    //               StartTask / StopTask / StopAndStartTask). Pure; concrete
    //               tasks implement it. FLAG: name inferred from dispatch role.
    virtual int GetTaskType() = 0;

private:
    MessageQueue* mpMessageQueue;  // +0x08 (the task's message queue)
    MemcardState* mpMemcardState;  // +0x0C (owning card-task state machine)
};

// ---------------------------------------------------------------------------
// ThrFunction<TTask> -- the worker-thread entry point
// RealmcIface::MemcardInterfaceImpl::RunAsync<TTask> hands to
// EA::Thread::Thread::Begin, with the task object as the thread argument. Every
// instantiation (SetActiveCardTask, CheckCardTask, BootupCheckTask,
// SaveCheckTask, SaveTask, SetAutosaveTask, LoadTask) compiles to the same body:
//
//   IRunnableTask::operator()(pArg)   ; run the task and drain the waiting queue
//   RefCount::Release(pArg)           ; drop the reference RunAsync handed over
//   return 0
//
// The thread argument is used as the task pointer unadjusted: each TTask derives
// IRunnableTask at offset 0.
// ---------------------------------------------------------------------------
template <class TTask>
int ThrFunction(void* pArg)
{
    IRunnableTask* pTask = static_cast<TTask*>(pArg);
    (*pTask)();
    pTask->Release(pTask);
    return 0;
}

} // namespace RealmcCore
