#ifndef SDKS_PACKAGES_ICE_ICEFILE_HPP
#define SDKS_PACKAGES_ICE_ICEFILE_HPP

#include "types.hpp"

// ============================================================================
// SDKs/Packages/ICE/ICEFile.hpp
//
// ICE::ICEFileHandler -- the ICE debug/serialisation text sink. The only member
// the reconstructed code uses so far is the printf-style FilePrintf, called by
// ICETakeData::SaveData (@0x82532CF8) to emit its XML-ish take dump (the asm
// symbol is ICE::ICEFileHandler::FilePrintf).
//
// MINIMAL SLICE -- declaration-only. The full ICEFileHandler (its file-handle
// state + open/close/read/write) belongs to the SDKs/Packages/ICE/ICEFile.cpp TU;
// GROW this header in place when that TU lands -- do NOT fork a parallel type.
// ============================================================================

namespace ICE
{
    class ICEFileHandler
    {
    public:
        // printf-style formatted write to the handler's stream. (SaveData passes the
        // handler straight in as the stream `this`.)
        void FilePrintf(const char* lpcFormat, ...);
    };
}

#endif // SDKS_PACKAGES_ICE_ICEFILE_HPP
