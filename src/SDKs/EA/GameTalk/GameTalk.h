#ifndef SDKS_EA_GAMETALK_GAMETALK_H
#define SDKS_EA_GAMETALK_GAMETALK_H

#include "types.hpp"

// ============================================================================
// SDKs/EA/GameTalk/GameTalk.h
//
// EA::GameTalk -- EA's in-game <-> external-tool messaging channel (the debug
// "GameTalk" wire used by GameExplorer and the other authoring tools). A
// GameTalkMessage is a keyed bag of named content blobs addressed to a tool
// "channel"; GameTalkManager is the process-wide singleton that ferries a
// finished message out to a named tool endpoint.
//
// HOMED HERE because the EA GameTalk SDK is not modelled anywhere else in the
// tree and ICE::ICEFileHandler::FileClose (@0x8252C960) is the first
// reconstructed caller -- it ships the accumulated take buffer to the
// "Tool.GameExplorer" endpoint under the "SaveTake" key.
//
// MINIMAL SLICE -- declaration-only, modelling exactly the three entry points
// FileClose touches (from the X360 asm):
//
//   GameTalkMessage::GameTalkMessage(const char* lpcChannel)
//       ctor; r4 = "Camera" channel string.
//   GameTalkMessage::AddKeyContent(const char* lpcKey, s32 liType,
//                                  const void* lpContent, s32 liLength)
//       r4 = "SaveTake", r5 = 0 (type/flags), r6 = the content buffer (the
//       ICEFileHandler `this`/macWriteBuffer), r7 = its StringLength. The asm
//       captures the return in r3 but FileClose discards it; the X360 Hex-Rays
//       renders it as a `_DWORD*`, so it is modelled as a pointer return.
//   GameTalkManager::GetInstance()
//       static singleton accessor (no args in the asm; the r3 it reads is the
//       leftover AddKeyContent result, not a real argument).
//   GameTalkManager::SendMessage(const char* lpcEndpoint, GameTalkMessage& rMsg)
//       STATIC: r3 = "Tool.GameExplorer", r4 = &msg (no `this`). The asm calls
//       GetInstance() then overwrites r3 with the endpoint string before the call,
//       discarding GetInstance's result -- so SendMessage takes the endpoint in r3,
//       which is only possible if it is a static/free function.
//
// The message is a stack temporary in FileClose (BYREF, 24 bytes of frame),
// constructed and destructed in place, and passed to SendMessage by reference.
//
// DECLARATION-ONLY: the real bodies live in the EA GameTalk SDK TU and the
// per-TU `cl /c` compile gate does not link. When the GameTalk SDK surface is
// reconstructed, GROW this header in place -- do NOT fork a parallel type.
// ============================================================================

namespace EA
{
namespace GameTalk
{
    // ------------------------------------------------------------------------
    // GameTalk namespace-level helpers (X360 .XEX, BURNOUT_X360_ARTIST.XEX).
    //
    //   Alloc(s32 liSize)              @ 0x82838000
    //   Free(void* lpBlock)            @ 0x82836E18
    //   StrIEqual(const char*, const char*) @ 0x82836C60
    //
    // Alloc/Free are GameTalk's allocation shims: they route every GameTalk
    // allocation through the AttribSys GameTalk package allocator. The X360 asm
    // inlines CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator() (it
    // reads the manager's sbHasLinearAllocator flag and the static GameTalk
    // package-allocator instance &dword_83011B64 directly, asserting the manager
    // has been Prepare'd against CgsAttribSysMemoryManager.h:192) and forwards to
    // that allocator's Malloc(size, 0) / Free(block). A NULL size / NULL block is
    // a no-op (Alloc returns 0, Free returns its argument).
    //
    // StrIEqual is a case-insensitive C-string equality test (used by
    // GameTalkManager::IsMessageMatching / ConfigHandler to match channel keys).
    void* Alloc(s32 liSize);
    void* Free(void* lpBlock);
    bool  StrIEqual(const char* lpcLhs, const char* lpcRhs);

    class GameTalkMessage
    {
    public:
        // Construct a message bound to the named tool channel ("Camera").
        explicit GameTalkMessage(const char* lpcChannel);
        ~GameTalkMessage();

        // Attach a named content blob to the message. liType is the content
        // type/flags word (0 from FileClose); lpContent/liLength are the raw
        // payload (the take buffer + its length). Returns an opaque slot
        // pointer (discarded by FileClose).
        void* AddKeyContent(const char* lpcKey, s32 liType, const void* lpContent, s32 liLength);
    };

    class GameTalkManager
    {
    public:
        // Process-wide GameTalk singleton.
        static GameTalkManager* GetInstance();

        // Ferry a finished message out to the named tool endpoint
        // ("Tool.GameExplorer"). STATIC (the asm passes the endpoint in r3, not a
        // `this`); FileClose's `GetInstance()->SendMessage(...)` evaluates
        // GetInstance() for its side effect then calls this static method.
        static s32 SendMessage(const char* lpcEndpoint, GameTalkMessage& rMessage);
    };
}
}

#endif // SDKS_EA_GAMETALK_GAMETALK_H
