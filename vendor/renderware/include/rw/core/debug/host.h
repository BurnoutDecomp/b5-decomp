#pragma once

// rw::core::debug::host -- low-level host file I/O shims used by the debug script
// host (CgsDev::DebugUI::ScriptInterface SaveState / ExecuteScript). Each is a
// pass-through onto the CRT POSIX low-level I/O API over an integer file handle.
// Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82BBC668 / @0x82BBC670 / @0x82BBC678.

namespace rw { namespace core { namespace debug { namespace host {

    // Open a host file by name (ScriptInterface ExecuteScript/SaveState). Returns an integer file
    // handle (< 0 on failure). OpenFlag is the CRT low-level _open flag set.
    int Open(const char* FileName, int OpenFlag);
    int Close(int FileHandle);
    int Read(int FileHandle, void* DstBuf, unsigned int MaxCharCount);
    int Write(int FileHandle, const void* Buf, unsigned int MaxCharCount);

} } } }  // namespace rw::core::debug::host
