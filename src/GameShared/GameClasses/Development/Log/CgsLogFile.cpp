#include "GameShared/GameClasses/Development/Log/CgsLogFile.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (flagged substitute for the StrStream assert path)

#include <cstring>     // (strlen replaced by the X360's inline NUL-walk below)
#include <windows.h>   // CreateFileA / WriteFile / CloseHandle / SetFilePointer

// CgsDev::Log::LogFile member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU bodies the three X360-emitted LogFile functions:
//
//   Open(const char*, bool) @ 0x82820880 -> CreateFileA (append: OPEN_ALWAYS + seek-to-end;
//                                            truncate: CREATE_ALWAYS), failure assert, returns true
//   Close()                 @ 0x828175C8 -> CloseHandle if open, reset miFile to -1
//   Append(const char*)     @ 0x82817608 -> WriteFile(miFile, str, strlen(str), &written, NULL)
//
// X360 store-for-store notes:
//   - Open: dwDesiredAccess = 0x40000000 (GENERIC_WRITE, `lis r4,0x4000`); dwShareMode = 2
//     (FILE_SHARE_WRITE); lpSecurityAttributes / dwFlagsAndAttributes / hTemplateFile = 0.
//     The branch on lbAppend selects dwCreationDisposition: append -> 4 (OPEN_ALWAYS) then
//     SetFilePointer(handle, 0, NULL, FILE_END=2); not-append -> 2 (CREATE_ALWAYS, truncate).
//     The handle is stored at `8(this)` (miFile) in both branches; on failure (== -1) the X360
//     fires "mFileHandle != INVALID_HANDLE_VALUE" (append asserts at line 85, truncate at 79).
//     The function hard-returns 1 (`li r3,1`).
//   - Close: loads miFile (`8(this)`); if != -1 CloseHandle(it) then stores -1 back.
//   - Append: if miFile != -1, walks lpcString to its NUL to compute the length (the X360 inlines
//     strlen: `r11 = end - start - 1`), then WriteFile(miFile, lpcString, length, &written, NULL).
//     (Hex-Rays mislabels the buffer pointer; the asm preserves r4 = lpcString as lpBuffer.)
//   - The X360 assert string is rodata, reproduced verbatim; the X360-baked file/line cites are
//     replaced by CGS_ASSERT's __FILE__/__LINE__ per project policy.

namespace CgsDev
{
namespace Log
{
    // X360 0x82820880.
    bool LogFile::Open(const char* lpcFileName, bool lbAppend)
    {
        if (lbAppend)
        {
            miFile = CreateFileA(lpcFileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                                 OPEN_ALWAYS, 0, NULL);
            CGS_ASSERT(miFile != INVALID_HANDLE_VALUE, "mFileHandle != INVALID_HANDLE_VALUE");
            SetFilePointer(miFile, 0, NULL, FILE_END);
        }
        else
        {
            miFile = CreateFileA(lpcFileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                                 CREATE_ALWAYS, 0, NULL);
            CGS_ASSERT(miFile != INVALID_HANDLE_VALUE, "mFileHandle != INVALID_HANDLE_VALUE");
        }
        return true;
    }

    // X360 0x828175C8.
    void LogFile::Close()
    {
        if (miFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(miFile);
            miFile = INVALID_HANDLE_VALUE;
        }
    }

    // X360 0x82817608. The virtual char* sink: write the NUL-terminated string to the file.
    void LogFile::Append(const char* lpcString)
    {
        if (miFile != INVALID_HANDLE_VALUE)
        {
            // X360 inlined NUL-walk to measure the string length (no trailing NUL written).
            const char* lpc = lpcString;
            while (*lpc)
                ++lpc;
            const DWORD luLength = static_cast<DWORD>(lpc - lpcString);

            DWORD luBytesWritten = 0;
            WriteFile(miFile, lpcString, luLength, &luBytesWritten, NULL);
        }
    }
}
}
