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
// forwards to CgsDev::Assert::Manager::HandleAssert, which logs the failure). The message is
// passed to FireAssert as it is: ARTIST passes a plain message string straight in r3 wherever
// the original message is one literal (e.g. "Array used before Construct/Clear was called" in
// CgsContainers::Array<int,20>::Append @0x821FBCD0).
#if defined(CGS_MATCH_ASSERT_LOCATIONS) && CGS_MATCH_ASSERT_LOCATIONS
#define CGS_ASSERT_LOCATION(artistFile, artistLine) artistFile, artistLine
#else
#define CGS_ASSERT_LOCATION(artistFile, artistLine) __FILE__, __LINE__
#endif

#define CGS_ASSERT(condition, msg) CGS_ASSERT_AT(condition, msg, __FILE__, __LINE__)
#define CGS_ASSERT_AT(condition, msg, artistFile, artistLine)    \
    do                                                          \
    {                                                           \
        if (!(condition))                                       \
        {                                                       \
            CgsDev::Assert::BeginAssert();                      \
            CgsDev::Assert::FireAssert((msg), CGS_ASSERT_LOCATION(artistFile, artistLine)); \
            CgsDev::Assert::EndAssert();                        \
        }                                                       \
    } while (0)

// CGS_ASSERT_STREAM: the streamed-message form. On failure the message is built by streaming
// `stream` (one or more `<<` operands) into the shared CgsDev::Assert::gpcMessageBuffer through
// a CgsDev::StrStream, then fired with this file and line. ARTIST shows the expansion inline
// wherever the original streams a message, e.g. CgsContainers::Array<int,20>::GetItem
// @0x821FF680: BeginAssert; the StrStream over gpcMessageBuffer / giMessageBufferSize; the
// streamed pieces; FireAssert(gpcMessageBuffer, file, line); EndAssert. The same sequence
// written out by hand is exact in CgsVariableEventQueue.h. A caller includes CgsStrStream.h.
#define CGS_ASSERT_STREAM(condition, stream) CGS_ASSERT_STREAM_AT(condition, stream, __FILE__, __LINE__)
#define CGS_ASSERT_STREAM_AT(condition, stream, artistFile, artistLine)                         \
    do                                                                                         \
    {                                                                                          \
        if (!(condition))                                                                      \
        {                                                                                      \
            CgsDev::Assert::BeginAssert();                                                     \
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer,                     \
                                         CgsDev::Assert::giMessageBufferSize);                 \
            lStrStream << stream;                                                              \
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, CGS_ASSERT_LOCATION(artistFile, artistLine)); \
            CgsDev::Assert::EndAssert();                                                       \
        }                                                                                      \
    } while (0)
