#pragma once

// ============================================================================
// b5-decomp/src/GameShared/GameClasses/System/CgsXOverlapped.h
// ============================================================================
// Owning header for CgsSystem::CgsXOverlapped -- the thin X360-only wrapper
// around the XDK XOVERLAPPED async-I/O structure. The type was originally
// declared inline inside its single platform .cpp
// (System/X360/CgsXOverlappedX360.cpp); it is extracted here so callers that
// EMBED it by value (e.g. BrnGameState::AchievementManagerX360::mOverLapped @
// this+0x38, which calls Construct() from Prepare/Release) can #include the real
// layout instead of forking an opaque byte blob. The method bodies stay in the
// platform .cpp; this header uses the XDK definition directly.
//
// The embedded XOVERLAPPED is exactly 7 DWORDs (28 bytes); Construct zero-fills
// those 7 DWORDs and GetResultString maps an XGetOverlappedResult() code to a
// printable string.
// ----------------------------------------------------------------------------

#include "types.hpp"
#include <cstddef>
#include <xtl.h>

CGS_STATIC_ASSERT((sizeof(XOVERLAPPED) == 28), "Original X360 overlapped width");
CGS_STATIC_ASSERT((offsetof(XOVERLAPPED, hEvent) == 0x0c), "ARTIST event word");
CGS_STATIC_ASSERT((offsetof(XOVERLAPPED, pCompletionRoutine) == 0x10), "ARTIST callback word");
CGS_STATIC_ASSERT((offsetof(XOVERLAPPED, dwExtendedError) == 0x18), "ARTIST extended error word");

namespace CgsSystem
{

class CgsXOverlapped
{
public:
    // Zero-initialises the embedded XOVERLAPPED (7 DWORDs). The X360 build
    // inlines this as a 7-iteration zero-fill loop over the object; modelled as
    // a constructor-style reset. The binary's `return result` is a
    // register/memset artifact on what is logically a void initialiser.
    void Construct();

    // Calls XGetOverlappedResult(pOverlapped, 0, 0) and maps the completion code
    // to a human-readable string. Known codes return a static literal; any other
    // code is formatted (with the extended error) into a shared static buffer.
    // Static utility -- takes the XOVERLAPPED by pointer, does not touch *this*.
    static const char* GetResultString(PXOVERLAPPED lpOverlapped);

    // Header inline (the achievement manager's assert names it): the asynchronous call is still
    // running while XGetOverlappedResult answers 997 (pending) or 996 (incomplete).
    bool IsOperationInProgress()
    {
        const u32 luResult = XGetOverlappedResult(&mOverlapped, 0, 0);
        return luResult == 997u || luResult == 996u;
    }

private:
    XOVERLAPPED mOverlapped;  // 0x00, 28 bytes -- the only member the TU touches
};

}
