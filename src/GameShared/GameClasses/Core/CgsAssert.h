#pragma once

#include "types.hpp"
namespace EA { namespace Thread { class Semaphore; } }

// The assert front-end. CgsDev::Assert is a NAMESPACE of free functions (not a
// class of statics): the X360 build attests BeginAssert / FireAssert / EndAssert
// as the three call-site entry points used by CGS_ASSERT, and they forward into
// CgsDev::Assert::Manager (reconstructed separately in
// GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.cpp).
//
//   BeginAssert @ 0x82817548  -> int  (enters gAssertMutex)
//   FireAssert  @ 0x82820810  -> int  FireAssert(expr, file, line); forwards to
//                                Manager::HandleAssert(this, expr, file, line).
//                                Hex-Rays renders the first arg as `void *`; the
//                                call sites pass the assertion-expression string,
//                                so it is a `const char *`.
//   EndAssert   @ 0x82817558  -> void* (leaves gAssertMutex; returns the mutex's
//                                DebugCriticalSection::Leave result pointer)
namespace CgsDev
{
namespace Assert
{
    // CgsAssertManager.h:43 (DWARF) — assert message buffer size.
    const s32 KI_MESSAGEBUFFERSIZE = 256;

    // Original shared formatted-assert destination and its runtime capacity. Synchronous callers
    // use it under BeginAssert's mutex; deferred workers format into their own scoped storage.
    extern char* gpcMessageBuffer;
    extern s32 giMessageBufferSize;

    // FLAG PC-platform leaf: construct after BeginAssert. Keep the original shared buffer for
    // synchronous assertions, but own worker text until FireAssert has captured the deferred request.
    class MessageBufferPC
    {
    public:
        MessageBufferPC();
        MessageBufferPC(const MessageBufferPC&) = delete;
        MessageBufferPC& operator=(const MessageBufferPC&) = delete;
        char* GetBuffer() const { return mpcBuffer; }
        s32 GetSize() const { return miSize; }

    private:
        char macBuffer[KI_MESSAGEBUFFERSIZE];
        char* mpcBuffer;
        s32 miSize;
    };

    // X360 @0x82820758 — the once-guarded assert-system bring-up (the FIRST call
    // DebugManager::Construct @0x828332C0 makes): Create gAssertMutex + initialise the
    // assert-screen state. (The X360 body also sets the assert overlay's vector-font size
    // — {14,14} at glyph-cell scale 0.125 — and starts Manager::RenderThreadAsserts; in
    // this tree the font sizing lives with Manager::SetRenderer and the per-thread assert
    // render is the threading follow-on.)
    void  Construct();

    int   BeginAssert();
    int   FireAssert(const char* lpcExpression, const char* lpcFile, int liLine);
    void* EndAssert();
    void  PrintStringed(const char* lpcFile, const char* lpcFunction, const int liLine, const char* lpcMessage);

    // FLAG PC-platform leaf: assertions coordinate before taking the assert
    // mutex. Foreign workers publish captured failures for the joined owner.
    using FrameFencePC = void (*)(void*);
    void AttachFrameOwnerPC(void* lpOwner, FrameFencePC lpFence);
    void DetachFrameOwnerPC(void* lpOwner);
    void SetDispatchThreadPC(bool lbDispatch);
    void DispatchPendingAssertsPC(bool lbAllowDisplay = true);
    void ServiceWorkerAssertsWhileWaitingPC();
    int WaitForWorkerPC(EA::Thread::Semaphore& lrCompletion);
}
}

// Evaluate the condition; on failure run the Begin/Fire/End assert sequence (FireAssert
// forwards to CgsDev::Assert::Manager::HandleAssert, which logs the failure). The original
// streamed the message into the assert buffer via StrStream; the call sites all pass a plain
// string, so the message is forwarded directly.
#define CGS_ASSERT(condition, msg)                              \
    do                                                          \
    {                                                           \
        if (!(condition))                                       \
        {                                                       \
            CgsDev::Assert::BeginAssert();                      \
            CgsDev::Assert::FireAssert((msg), __FILE__, __LINE__); \
            CgsDev::Assert::EndAssert();                        \
        }                                                       \
    } while (0)
