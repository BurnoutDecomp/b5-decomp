#pragma once

// CgsFileRW.h — the full definition of CgsFileSystem::File, the concrete buffered file
// handle the FileSystem owns 16 of (maFiles). It is the lock-guarded BaseFile (CgsFile.h)
// plus the raw rw::core::filesys backing state (the open Handle, the disk-cache id and the
// pending read op's position/size/address) and the async Open/Read/Write/Close request
// machinery that drives rw::core::filesys::AsyncOp.
//
// SOURCES: BURNOUT_X360_ARTIST.XEX is ground truth (the PowerPC asm — every field access
// below was mapped from the addis/ld/std/stw displacements to the DWARF-ordered member it
// lands on; the Hex-Rays 32-bit-pair view of the u64 position/size args was NOT trusted —
// the asm passes them as single PPC64 registers). The DecFIGS DWARF CgsFileRW.h fixes the
// member names/types and the public/private split.
//   Open  0x828F0090   Read  0x828F0248   Write 0x828E81F8   Close 0x828F03E0
//   OnOpen 0x828E83E0  OnRead 0x828E8670  OnWrite 0x828DCBA0 OnClose 0x828E8868
//   OpenCallback 0x828E8A58  ReadCallback 0x828E8A68
//   WriteCallback 0x828DCD98 CloseCallback 0x828E8A78
//
// LAYOUT POLICY: faithful DWARF member ORDER with natural PC widths (the subsystem
// convention set by CgsFile.h / CgsFileSystem.h — File is embedded by value and reached
// BY NAME, never by raw offset, so exact X360 byte offsets are not pinned here).

#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/CgsFile.h"  // BaseFile / FileAccess / FileFutexHelper

// The raw RenderWare-core filesystem primitives this handle drives. Referenced by pointer
// only from the class body, so a forward declaration keeps the heavy include in the .cpp.
namespace rw { namespace core { namespace filesys { struct AsyncOp; struct Handle; } } }

namespace CgsFileSystem
{
    // ---- DWARF CgsFileRW.h:45 ----
    // The concrete buffered file handle. Every public entry point takes the inherited
    // BaseFile::mFutex, asserts the file state, allocates one AsyncOp through the filesys
    // Manager, submits the async request (completion -> the matching static *Callback ->
    // On*), then bumps the outstanding-operation count. The On* handlers run under the same
    // lock, fold the op's status into meFileState, decrement the count, and free the op.
    class File : public BaseFile
    {
    public:
        // ---- lifecycle (DWARF CgsFileRW.h:48-60) ----
        // DECLARE-ONLY: no standalone X360 symbol (fully inlined into the FileSystem ctor /
        // Construct/Prepare/Release/Destruct). BaseFile already declares the shared shape;
        // bodies are not attested in this TU's asm.
        void Construct(FileSystem* lpFileSystem, FileLog* lpLog, s32 liIndex);
        void Prepare();
        void Release();
        void Destruct();

        // ---- async request entry points (this TU) ----
        // @0x828F0090. Begin an async open of lpcFileName in the given access mode; latches
        // the name/access/priority, marks the handle OPENING and queues the device open.
        bool Open(const char* lpcFileName, FileAccess leFileAccess, s32 liPriority,
                  bool lbUseHDCache);

        // @0x828F0248. Queue an async read of luSize bytes at luPosition into lpBuffer.
        bool Read(void* lpBuffer, u64 luPosition, u64 luSize);

        // @0x828E81F8. Queue an async write of luSize bytes at luPosition from lpBuffer.
        bool Write(const void* lpBuffer, u64 luPosition, u64 luSize);

        // @0x828F03E0. Queue an async close of the open handle.
        bool Close();

    private:
        // ---- device-completion handlers (this TU; run under mFutex) ----
        void OnOpen(rw::core::filesys::AsyncOp* lpOp);   // @0x828E83E0
        void OnRead(rw::core::filesys::AsyncOp* lpOp);   // @0x828E8670
        void OnWrite(rw::core::filesys::AsyncOp* lpOp);  // @0x828DCBA0
        void OnClose(rw::core::filesys::AsyncOp* lpOp);  // @0x828E8868

        // ---- completion trampolines (this TU) ----
        // Static: installed as rw::core::filesys::CompletionCallback (void(*)(AsyncOp*)).
        // Each recovers the owning File from the op's user word and forwards to its On*.
        static void OpenCallback(rw::core::filesys::AsyncOp* lpOp);   // @0x828E8A58
        static void ReadCallback(rw::core::filesys::AsyncOp* lpOp);   // @0x828E8A68
        static void WriteCallback(rw::core::filesys::AsyncOp* lpOp);  // @0x828DCD98
        static void CloseCallback(rw::core::filesys::AsyncOp* lpOp);  // @0x828E8A78

        // ---- instance layout (DWARF CgsFileRW.h:80-85, appended to BaseFile) ----
        rw::core::filesys::Handle* mpHandle;                 // :80  (asm this+0x60) open handle
        u32                        muCacheId;                // :81  (asm this+0x64) disk-cache id (-1 none)
        u64                        muAttemptedReadFilePos;   // :82  (asm this+0x68) pending read position
        u64                        muAttemptedReadOpSize;    // :83  (asm this+0x70) pending read size
        void*                      mpAttemptedReadOpAddress; // :84  (asm this+0x78) pending read dst
        char                       macFileName[256];         // :85  (asm this+0x7C) latched name copy
    };
}
