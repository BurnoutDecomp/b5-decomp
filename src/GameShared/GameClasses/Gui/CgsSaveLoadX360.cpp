#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsGui::XenonFileInputStream member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:CgsGui::XenonFileInputStream) bodies the two
// X360-emitted stream methods:
//
//   GetSize @ 0x8284BD88  -> assert open, return miSize (this[1] == +4)
//   Read    @ 0x8284BE28  -> assert open, ReadFile(mhFile, buf, n, &read, NULL), return read
//
// X360 store-for-store notes:
//   - Both load mhFile from `*this` (+0) and assert it != -1 (INVALID_HANDLE_VALUE),
//     firing "File open failed!" otherwise.
//   - GetSize returns `lwz r3, 4(this)` == miSize.
//   - Read sets the bytes-read out-word to luBytesToRead first (stw r22, var_90), then calls
//     ReadFile(hFile=*this, lpBuffer=arg, nNumberOfBytesToRead=luBytesToRead,
//     lpNumberOfBytesRead=&var, lpOverlapped=NULL). On a FALSE return it fires "File read
//     failed!". The return value is the bytes-read out-word.
//   - The X360-baked CgsSaveLoadX360.h file/line (596/611/614) are discarded per project
//     convention; the assert strings are X360 rodata, reproduced verbatim.

namespace CgsGui
{
    // X360 0x8284BD88.
    s32 XenonFileInputStream::GetSize() const
    {
        CGS_ASSERT(mhFile != INVALID_HANDLE_VALUE, "File open failed!");
        return miSize;
    }

    // X360 0x8284BE28.
    s32 XenonFileInputStream::Read(void* lpBuffer, u32 luBytesToRead)
    {
        CGS_ASSERT(mhFile != INVALID_HANDLE_VALUE, "File open failed!");

        DWORD luBytesRead = static_cast<DWORD>(luBytesToRead);
        if (!::ReadFile(mhFile, lpBuffer, static_cast<DWORD>(luBytesToRead), &luBytesRead, 0))
        {
            CGS_ASSERT(false, "File read failed!");
        }

        return static_cast<s32>(luBytesRead);
    }
}
