#pragma once

#include "types.hpp"

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

    int   BeginAssert();
    int   FireAssert(const char* lpcExpression, const char* lpcFile, int liLine);
    void* EndAssert();
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
