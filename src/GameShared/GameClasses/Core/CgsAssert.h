#pragma once

#include "types.hpp"
namespace EA { namespace Thread { class Semaphore; } }

// The assert front-end. CgsDev::Assert is a NAMESPACE of free functions (not a
// class of statics): the X360 build attests BeginAssert / FireAssert / EndAssert
// as the three call-site entry points used by CGS_ASSERT, and they forward into
// CgsDev::Assert::Manager (reconstructed separately in
// GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.cpp).
//
//   BeginAssert @ 0x82817548 enters the native assert mutex.
//   FireAssert  @ 0x82820810 captures the first stack, normalizes null strings
//                           and calls Manager::HandleAssert.
//   EndAssert   @ 0x82817558 leaves the native assert mutex.
// DecFIGS declares all three void; incidental r3 values are not return contracts.
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

    // ARTIST82820758 seeds the actual manager's once flag, mutex and font,
    // then enters the pending thread-assert render handoff.
    void  Construct();

    void  BeginAssert();
    void  FireAssert(const char* lpcExpression, const char* lpcFile, int liLine);
    void  EndAssert();
    void  PrintStringed(const char* lpcFile, const char* lpcFunction, const int liLine, const char* lpcMessage);

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
