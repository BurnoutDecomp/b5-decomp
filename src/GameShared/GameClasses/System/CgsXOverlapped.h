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
// platform .cpp; this header carries only declarations + the XDK slice.
//
// The embedded XOVERLAPPED is exactly 7 DWORDs (28 bytes); Construct zero-fills
// those 7 DWORDs and GetResultString maps an XGetOverlappedResult() code to a
// printable string. Platform-specific glue (mirrors System/PS3): the XDK entry
// points are forward-declared here as extern "C" free functions.
// ----------------------------------------------------------------------------

#include "types.hpp"

// Minimal XDK XOVERLAPPED slice (real definition lives in the Xbox 360 XDK
// <xtl.h>). 7 DWORDs == 28 bytes, matching Construct's zero-fill loop (v1 = 7).
struct XOVERLAPPED
{
    u32 InternalLow;          // 0x00
    u32 InternalHigh;         // 0x04
    u32 InternalContext;      // 0x08
    u32 dwExtendedError;      // 0x0C  (read via XGetOverlappedExtendedError)
    u32 hEvent;               // 0x10
    u32 dwCompletionContext;  // 0x14  (completion routine context)
    u32 dwReserved;           // 0x18
};
typedef XOVERLAPPED* PXOVERLAPPED;

// XDK entry points (real prototypes live in the Xbox 360 XDK <xapi.h>/<xtl.h>);
// declared here as extern "C" free functions, mirroring the System/PS3 glue
// precedent (CgsHardwareLanguagePS3.cpp's `extern "C" int XTLGetLanguage();`).
extern "C" u32 XGetOverlappedResult(PXOVERLAPPED lpOverlapped, u32* lpdwResult, u32 bWait);
extern "C" u32 XGetOverlappedExtendedError(PXOVERLAPPED lpOverlapped);

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

private:
    XOVERLAPPED mOverlapped;  // 0x00, 28 bytes -- the only member the TU touches
};

}
