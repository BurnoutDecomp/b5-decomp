#ifndef SDKS_PACKAGES_ICE_ICEFILE_HPP
#define SDKS_PACKAGES_ICE_ICEFILE_HPP

#include "types.hpp"

// ============================================================================
// SDKs/Packages/ICE/ICEFile.hpp
//
// ICE::ICEFileHandler -- the ICE camera-take text sink. Despite the "File" name
// it does NOT wrap an OS file handle: it accumulates formatted text into a
// fixed 32 KiB write buffer (macWriteBuffer) and, on FileClose, ships the
// buffer out over EA::GameTalk to the GameExplorer authoring tool (the asm sends
// it under the "SaveTake" key to the "Tool.GameExplorer" endpoint). The handler
// IS its buffer: the X360 build passes `this` straight to the rwcore string
// routines, so macWriteBuffer sits at offset 0 (it is the only data member).
//
// FilePrintf is the printf-style append used by ICETakeData::SaveData
// (@0x82532CF8) to emit its XML-ish take dump.
//
// REAL LAYOUT recovered from the X360 asm + DecFIGS DWARF (ICEFile.hpp):
//   private char macWriteBuffer[KI_WRITEBUFFER_SIZE]  (32768)
//   Construct / Destruct / FileOpen / FileClose / FilePrintf / FilePrintfAtEnd
//
// This TU (ICEFile.cpp) reconstructs FileClose + FilePrintf; the remaining
// methods are declaration-only here (their bodies are out of this TU's scope).
// ============================================================================

namespace ICE
{
    // ICEFile.hpp:28 (DWARF) -- the per-FilePrintf scratch/format buffer size.
    const u32 KU_NORMAL_BUFFER = 1024;

    // ICEFile.hpp:29 (DWARF) -- write-buffer block count (24 * 1024 == 24 KiB of
    // the 32 KiB macWriteBuffer is the nominal working region).
    const u32 KU_BUFFER_BLOCKS = 24;

    class ICEFileHandler
    {
    public:
        // ICEFile.hpp:63 (DWARF) -- the fixed accumulation-buffer size.
        static const s32 KI_WRITEBUFFER_SIZE = 32768;

    public:
        void Construct();
        void Destruct();

        ICEFileHandler* FileOpen(const char* lpcFilename, const char* lpcGuid);

        // Flush: append the closing </TAKE> tag and ship the accumulated buffer to
        // the GameExplorer tool over EA::GameTalk. Does NOT close any OS handle.
        void FileClose();

        // printf-style formatted append into macWriteBuffer.
        //
        // SIGNATURE NOTE: the DecFIGS DWARF spells the format parameter `char*`
        // (FilePrintf(char*, ...)), but every call site -- ICETakeData::SaveData's
        // ~30 FilePrintf("string literal", ...) calls -- passes a string literal,
        // which is `const char*` under MSVC /permissive-. The format string is
        // read-only, so `const char*` is the correct type and keeps every call
        // site compiling unchanged. FLAG: DWARF says char*; modelled const char*.
        s32 FilePrintf(const char* lpcFormat, ...);

        // As FilePrintf, but appends at the current end of the buffer (DWARF
        // ICEFile.cpp:126). Same const char* format-param decision. Not reconstructed
        // in this TU -- declaration-only.
        s32 FilePrintfAtEnd(const char* lpcFormat, ...);

    private:
        // ICEFile.hpp:65 (DWARF) -- the 32 KiB text accumulation buffer. Offset 0:
        // the asm passes `this` directly to rw::core::stdc::String{Cat,Length}.
        char macWriteBuffer[KI_WRITEBUFFER_SIZE];
    };
}

#endif // SDKS_PACKAGES_ICE_ICEFILE_HPP
