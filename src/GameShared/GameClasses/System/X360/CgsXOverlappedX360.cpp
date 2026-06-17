#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

// CgsXOverlapped -- thin X360-only wrapper around the XDK XOVERLAPPED async
// I/O structure. The embedded XOVERLAPPED is exactly 7 DWORDs (28 bytes); the
// X360 binary's Construct just zero-fills those 7 DWORDs, and GetResultString
// is a static helper that turns an XGetOverlappedResult() code into a printable
// string. This is platform-specific glue, so (mirroring System/PS3) the class is
// declared inline in its single platform .cpp rather than in a shared header.
//
// XOVERLAPPED is a pure XDK type with no PS3/PC analogue; we forward-declare the
// XDK shape locally as a minimal slice (the real layout comes from the Xbox 360
// XDK <xtl.h>). Only the size (7 DWORDs) is load-bearing for Construct.

// Minimal XDK XOVERLAPPED slice (real definition lives in the Xbox 360 XDK).
// 7 DWORDs == 28 bytes, matching Construct's zero-fill loop (v1 = 7).
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

// Win32/XDK winerror.h numeric literals consumed by GetResultString.
#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS       0u
#endif
#ifndef ERROR_IO_INCOMPLETE
#define ERROR_IO_INCOMPLETE 996u
#endif
#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING    997u
#endif
#ifndef FALSE
#define FALSE 0
#endif

namespace CgsSystem
{

// Size of the shared static result buffer (X360-baked SPrintf length == 100).
static const u32 KU_RESULT_STRING_LENGTH = 100;

// byte_830EA388 (size 100) recovered as the file-static result string.
static char sacResultString[KU_RESULT_STRING_LENGTH];

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

void CgsXOverlapped::Construct()
{
    // X360 @ 0x828D72C0: zero-fills 7 DWORDs (the embedded XOVERLAPPED, 28 bytes).
    // Pseudocode was a `do { *result++ = 0; } while (--v1)` loop with v1 = 7,
    // returning the advanced pointer (a memset register artifact). Logically a
    // void zero-initialiser of the whole object.
    mOverlapped.InternalLow         = 0;
    mOverlapped.InternalHigh        = 0;
    mOverlapped.InternalContext     = 0;
    mOverlapped.dwExtendedError     = 0;
    mOverlapped.hEvent              = 0;
    mOverlapped.dwCompletionContext = 0;
    mOverlapped.dwReserved          = 0;
}

const char* CgsXOverlapped::GetResultString(PXOVERLAPPED lpOverlapped)
{
    // X360 @ 0x823557F0. XGetOverlappedResult returns the async completion code
    // (does NOT wait: bWait = 0, pdwResult = 0).
    const u32 luResult = XGetOverlappedResult(lpOverlapped, nullptr, FALSE);

    switch (luResult)
    {
        case ERROR_SUCCESS:        // 0
            return "ERROR_SUCCESS";
        case ERROR_IO_INCOMPLETE:  // 996
            return "ERROR_IO_INCOMPLETE";
        case ERROR_IO_PENDING:     // 997
            return "ERROR_IO_PENDING";
    }

    // Any other code: format result + extended error into the shared buffer.
    // The X360 pseudocode's LODWORD/HIDWORD(v4) and __SPAIR64__ packing are
    // varargs register-pair artifacts: the extended error is logically passed
    // twice (once as %d, once as 0x%x).
    const u32 luExtendedError = XGetOverlappedExtendedError(lpOverlapped);
    CgsCore::SPrintf(
        sacResultString,
        KU_RESULT_STRING_LENGTH,
        "Error code %d extended error %d(0x%x)",
        luResult,
        luExtendedError,
        luExtendedError);
    return sacResultString;
}

}
