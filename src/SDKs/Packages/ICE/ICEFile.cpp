// ============================================================================
// SDKs/Packages/ICE/ICEFile.cpp
//
// ICE::ICEFileHandler -- the camera-take text sink. Reconstructed from the X360
// asm (FileClose @0x8252C960, FilePrintf @0x8252C9F0). The handler accumulates
// formatted text into its 32 KiB macWriteBuffer (offset 0, so `this` is the
// buffer) and, on close, ships it to the GameExplorer tool over EA::GameTalk.
//
// This TU reconstructs FileClose + FilePrintf; the other ICEFileHandler methods
// (Construct/Destruct/FileOpen/FilePrintfAtEnd) are declared in ICEFile.hpp and
// reconstructed elsewhere.
// ============================================================================

#include "SDKs/Packages/ICE/ICEFile.hpp"             // ICE::ICEFileHandler + KU_NORMAL_BUFFER
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "SDKs/EA/GameTalk/GameTalk.h"               // EA::GameTalk::GameTalkMessage / GameTalkManager
#include "rw/core/stdc/stdc.h"                        // rw::core::stdc::String{Cat,Length} / Vsprintf

#include <cstdarg>   // va_list / va_start / va_end (FilePrintf varargs)

namespace ICE
{
// ----------------------------------------------------------------------------
// ICE::ICEFileHandler::FileClose @ 0x8252C960
//
// Terminate the take dump and flush it to the GameExplorer tool. The asm:
//   StringCat(this, "</TAKE>");              // close the root XML tag in-buffer
//   len = StringLength(this);                // total accumulated length
//   GameTalkMessage msg("Camera");           // message on the "Camera" channel
//   msg.AddKeyContent("SaveTake", 0, this, len);   // attach the take buffer
//   GameTalkManager::GetInstance()->SendMessage("Tool.GameExplorer", msg);
//   ~msg;                                    // stack temporary, destructed here
// No OS file handle is involved -- "FileClose" finalises and ships the buffer.
// ----------------------------------------------------------------------------
void ICEFileHandler::FileClose()
{
    // `this` is the macWriteBuffer (offset 0); the rwcore string routines operate
    // on it directly, exactly as the X360 build passes r3.
    char* lpcBuffer = reinterpret_cast<char*>(this);

    rw::core::stdc::StringCat(lpcBuffer, "</TAKE>");
    s32 liLength = rw::core::stdc::StringLength(lpcBuffer);

    EA::GameTalk::GameTalkMessage lMessage("Camera");
    lMessage.AddKeyContent("SaveTake", 0, lpcBuffer, liLength);
    EA::GameTalk::GameTalkManager::GetInstance()->SendMessage("Tool.GameExplorer", lMessage);
    // lMessage destructed here (stack temporary), mirroring the trailing
    // ~GameTalkMessage in the asm.
}

// ----------------------------------------------------------------------------
// ICE::ICEFileHandler::FilePrintf @ 0x8252C9F0
//
// printf-style append into macWriteBuffer. Formats into a 1 KiB stack scratch
// buffer, bounds-checks, then StringCats it onto `this`. The asm:
//   assert( StringLength(lpFormat) < KU_NORMAL_BUFFER );       // line 99
//   len = Vsprintf(buf, lpFormat, args);
//   assert( len >= 0 );                                        // line 106
//   assert( len < KU_NORMAL_BUFFER - 1 );                      // line 107
//   StringCat(this, buf);
//   return len;
//
// SIGNATURE NOTE: lpcFormat is const char* (not the DWARF's char*) so the
// SaveData string-literal call sites bind under /permissive-. See ICEFile.hpp.
// ----------------------------------------------------------------------------
s32 ICEFileHandler::FilePrintf(const char* lpcFormat, ...)
{
    char lacBuffer[KU_NORMAL_BUFFER];   // ICEFile.cpp:100 -- the 1 KiB format scratch
    va_list lArgList;                   // ICEFile.cpp:101

    // The format string itself must fit the scratch buffer (line 99).
    CGS_ASSERT(static_cast<u32>(rw::core::stdc::StringLength(lpcFormat)) < KU_NORMAL_BUFFER,
               "static_cast<uint32_t> ( ICEMath::StrLen( lpFormat ) ) < KU_NORMAL_BUFFER");

    va_start(lArgList, lpcFormat);
    s32 liStrLength = rw::core::stdc::Vsprintf(lacBuffer, lpcFormat, lArgList);   // ICEFile.cpp:103
    va_end(lArgList);

    CGS_ASSERT(liStrLength >= 0, "lStrLength >= 0");
    CGS_ASSERT(liStrLength < static_cast<s32>(KU_NORMAL_BUFFER - 1),
               "lStrLength < static_cast<int32_t>(KU_NORMAL_BUFFER-1)");

    // Append the formatted text onto the accumulation buffer (`this`).
    rw::core::stdc::StringCat(reinterpret_cast<char*>(this), lacBuffer);

    return liStrLength;
}
}
