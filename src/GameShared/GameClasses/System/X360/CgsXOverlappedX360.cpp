#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/System/CgsXOverlapped.h" // CgsSystem::CgsXOverlapped + XOVERLAPPED slice

// CgsXOverlapped -- thin X360-only wrapper around the XDK XOVERLAPPED async
// I/O structure. The embedded XOVERLAPPED is exactly 7 DWORDs (28 bytes); the
// X360 binary's Construct just zero-fills those 7 DWORDs, and GetResultString
// is a static helper that turns an XGetOverlappedResult() code into a printable
// string. This is platform-specific glue; the class DECLARATION + the XDK
// XOVERLAPPED slice were extracted to the shared CgsXOverlapped.h so callers that
// embed the wrapper by value can #include the real layout. The method BODIES
// stay here in the single platform .cpp.

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
