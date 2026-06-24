#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStreamBase

#include <windows.h>   // HANDLE / INVALID_HANDLE_VALUE (the X360 wraps a Win32 file handle; host Win32 on PC)

// CgsDev::Log::LogFile - the simplest log-file stream: a StrStreamBase whose char* sink writes
// text straight to an open Win32 file handle (no intermediate buffer, unlike its sibling
// LogFileBuffered). Reconstructed from the DecFIGS DWARF (Development/Log/CgsLogFile.h) + the
// X360 ARTIST bodies Open @0x82820880 / Close @0x828175C8 / Append @0x82817608.
//
// Layout (X360, from DWARF + the asm): StrStreamBase (vtable@0 + mePrintMode@4 = 8 bytes) then
//   miFile [+0x08]  the open file handle (-1 == INVALID_HANDLE_VALUE until/if Open succeeds)
// The asm loads/stores the handle at `8(this)` (stw, 32-bit on console); the DWARF types it as
// int32_t (CgsLogFile.h:67). On the PC host the handle is a Win32 HANDLE, so this owning slice
// models miFile as a HANDLE (same precedent as CgsGui::XenonFileInputStream in CgsSaveLoadX360.h).
namespace CgsDev
{
namespace Log
{
    struct LogFile : public StrStreamBase
    {
        // X360 0x82820880. Open the named file: in append mode (lbAppend) it OPENS_ALWAYS and seeks
        // to the end; otherwise it CREATE_ALWAYS (truncates). On a failed CreateFileA it fires the
        // "mFileHandle != INVALID_HANDLE_VALUE" assert. Always returns true (the X360 hard-returns 1).
        bool Open(const char* lpcFileName, bool lbAppend);

        // X360 0x828175C8. Close the handle if open and reset miFile to INVALID_HANDLE_VALUE.
        void Close();

        // CgsLogFile.h:139 (DWARF). The stream is open iff it holds a valid handle.
        bool IsOpen() const { return miFile != INVALID_HANDLE_VALUE; }

    protected:
        // X360 0x82817608. The char* sink (virtual; overrides StrStreamBase::operator<< via the base
        // forwarder). When open, writes strlen(lpcString) bytes of lpcString to miFile via WriteFile.
        virtual void Append(const char* lpcString);

        // StrStreamBase requires a concrete operator<<(const char*) sink; forward it to Append so the
        // class is instantiable and `stream << "text"` reaches the file (the X360 streams through the
        // virtual char* sink, which this concrete stream points at Append).
        StrStreamBase& operator<<(const char* lpcText) override
        {
            Append(lpcText);
            return *this;
        }

    protected:
        HANDLE miFile;   // [+0x08] open file handle (-1 == INVALID_HANDLE_VALUE) -- CgsLogFile.h:67
    };
}
}
