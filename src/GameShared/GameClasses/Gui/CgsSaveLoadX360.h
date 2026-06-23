#pragma once

#include "types.hpp"

#include <Windows.h>   // HANDLE (the X360 stream wraps a Win32 file handle; host Win32 on PC)

// CgsGui::XenonFileInputStream - a read-only file input stream over a Win32 file HANDLE,
// used by the Xenon (X360) save/load path (CgsGui::SaveLoadSystem::Prepare opens a stream
// and reads its header through this). Recovered from the X360 ARTIST binary:
//   GetSize @ 0x8284BD88   Read @ 0x8284BE28
// (assert sites cite the X360 build's gui/CgsSaveLoadX360.h).
//
// Layout (X360 authoritative, from the GetSize/Read member loads):
//   mhFile  [+0x0]  HANDLE  -- the open file handle; INVALID_HANDLE_VALUE (-1) until/if open
//                              fails (both methods assert mhFile != -1, "File open failed!").
//   miSize  [+0x4]  s32     -- the file size (GetSize returns `this[1]` == +4).
namespace CgsGui
{
    class XenonFileInputStream
    {
    public:
        // X360 0x8284BD88. Asserts the file is open (mhFile != INVALID_HANDLE_VALUE),
        // returns the cached file size.
        s32 GetSize() const;

        // X360 0x8284BE28. Asserts the file is open, then reads luBytesToRead bytes from the
        // handle into lpBuffer via Win32 ReadFile (lpOverlapped == NULL). On a ReadFile
        // failure (returns FALSE) it fires "File read failed!". Returns the number of bytes
        // actually read.
        s32 Read(void* lpBuffer, u32 luBytesToRead);

    private:
        HANDLE mhFile; // +0x0 -- open Win32 file handle (-1 == INVALID_HANDLE_VALUE)
        s32    miSize; // +0x4 -- cached file size in bytes
    };
}
