// =====================================================================================
// rw::core::debug::host -- low-level host file I/O shims used by the debug script host.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   rw::core::debug::host::Close @0x82BBC668   ->  b _close   (tail-jump)
//   rw::core::debug::host::Read  @0x82BBC670   ->  b _read    (tail-jump)
//   rw::core::debug::host::Write @0x82BBC678   ->  b _write   (tail-jump)
//
// Each entry is a single unconditional branch into the CRT POSIX low-level I/O
// routine (close/read/write over an integer file handle). On MSVC these live in
// <io.h> under the underscore-prefixed names. The shims are pass-through: same
// arguments, same return value (bytes transferred / 0 / -1 on error).
// =====================================================================================

#include "rw/core/debug/host.h"

#include <io.h>  // _close / _read / _write -- the CRT POSIX low-level file API

namespace rw { namespace core { namespace debug { namespace host {

    // X360 0x82BBC668:  b _close
    int Close(int FileHandle)
    {
        return _close(FileHandle);
    }

    // X360 0x82BBC670:  b _read
    int Read(int FileHandle, void* DstBuf, unsigned int MaxCharCount)
    {
        return _read(FileHandle, DstBuf, MaxCharCount);
    }

    // X360 0x82BBC678:  b _write
    int Write(int FileHandle, const void* Buf, unsigned int MaxCharCount)
    {
        return _write(FileHandle, Buf, MaxCharCount);
    }

} } } }  // namespace rw::core::debug::host
