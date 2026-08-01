// ============================================================================
// SDKs/Packages/ICE/ICEFileClose.cpp
//
// ⚠️ THIS IS A FILE SPLIT, NOT A CODE CHANGE. ICE::ICEFileHandler::FileClose
// @0x8252C960 was moved here VERBATIM out of its real home, ICEFile.cpp, on
// 2026-08-01 (ICE take-runtime wave). Nothing about it was edited.
//
// WHY. ICEData.cpp's ICETakeData::SaveData -- the take debug dumper -- calls
// ICEFileHandler::FilePrintf, so mounting the ICE take runtime requires FilePrintf
// to link. FilePrintf needs only rwcore's string/format wrappers. FileClose, its
// TU-mate, is the only thing in the ICE package that touches EA::GameTalk, and
// mounting it MEASURED (2026-08-01, two builds) at:
//     + ICEFile.cpp                  ->  5 unresolved (GameTalkMessage ctor/dtor,
//                                        AddKeyContent, GameTalkManager::GetInstance,
//                                        GameTalkManager::SendMessage)
//     + ICEFile.cpp + GameTalk.cpp   ->  3 unresolved (GameTalkMessage::
//                                        AllocateDataBuffer, GameTalkManager::
//                                        ReceiverCallback, AttribSysPackageAllocator::Free)
// i.e. bringing the whole GameTalk tool-protocol stack (and its static manager
// instance) into the exe -- to serve a debug XML dumper that nothing on any runtime
// path calls. This split is strictly cheaper and changes no behaviour.
//
// This body is NOT stubbed: it is the real one, simply not in the exe source list.
// Nothing else in the tree calls FileClose today.
//
// DELETE-WHEN: the EA::GameTalk stack lands (GameTalk.cpp + its three remaining
// leaves); then paste this function back at the end of ICEFile.cpp, delete this
// file, and mount GameTalk.cpp beside it.
// ============================================================================

#include "SDKs/Packages/ICE/ICEFile.hpp"             // ICE::ICEFileHandler
#include "SDKs/EA/GameTalk/GameTalk.h"               // EA::GameTalk::GameTalkMessage / GameTalkManager
#include "rw/core/stdc/stdc.h"                        // rw::core::stdc::String{Cat,Length}

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
}
