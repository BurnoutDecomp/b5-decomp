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

#include <io.h>  // _open / _close / _read / _write -- the CRT POSIX low-level file API
#include <fcntl.h>
#include <sys/stat.h>

namespace rw { namespace core { namespace debug { namespace host {

    // The host API uses its own compact flag word. Mode 1 is read-only; the remaining bits map to
    // the CRT write/read-write/append/create/truncate/binary flags. SaveState passes 0x32
    // (write + create + truncate), while ExecuteScript passes 1.
    int Open(const char* FileName, int OpenFlag)
    {
        int liFlags = 0;
        int liPermission = 0;
        if (OpenFlag & 0x02) liFlags |= _O_WRONLY;
        if (OpenFlag & 0x04) liFlags |= _O_RDWR;
        if (OpenFlag & 0x08) liFlags |= _O_APPEND;
        if (OpenFlag & 0x10)
        {
            liFlags |= _O_CREAT;
            liPermission = _S_IWRITE;
        }
        if (OpenFlag & 0x20) liFlags |= _O_TRUNC;
        if (OpenFlag & 0x40) liFlags |= _O_BINARY;
        return _open(FileName, liFlags, liPermission);
    }

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
