#pragma once

// CgsFileHandle.h — canonical home for CgsFileSystem::FileHandle, the lightweight value
// handle the game holds onto a FileSystem file slot with. It is NOT the buffered File/BaseFile
// object (those live in CgsFile.h); it is a trivial {FileSystem*, fileId} pair whose Read/Write
// take the FILE-SYSTEM-WIDE lock and delegate to FileSystem::Read/WriteInternal.
//
// SOURCES: the DecFIGS DWARF CgsFileHandle.h is GROUND TRUTH for the member layout + method
// shape; the X360 ARTIST asm (FileHandle::Read @ 0x8264AEE0, FileHandle::Write @ 0x8264AF40)
// is authority for behaviour. Both attest the exact same 2-member layout:
//   +0  mpFileSystem  (asm: lwz r31, 0(r3))
//   +4  muFileId      (asm: lwz r26, 4(r3))
// and both methods:
//   lock  mpFileSystem->mFileSystemFutex   (asm: RtlEnterCriticalSection(*a1 + 0x9920))
//   ret = mpFileSystem->{Read,Write}Internal(muFileId, buf, filePos, size)
//   unlock mpFileSystem->mFileSystemFutex  (asm: RtlLeaveCriticalSection)
//   return ret
// The +0x9920 critical section is the FileSystem's mFileSystemFutex member, reached BY NAME
// (project layout policy — DWARF member order + natural PC widths, not exact X360 byte offsets).

#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/CgsFile.h"  // FileState vocabulary

namespace CgsFileSystem
{
    class FileSystem;

    // ---- DWARF CgsFileHandle.h:41 ----
    struct FileHandle
    {
    public:
        // ---- DWARF CgsFileHandle.h:48-79 ----
        void Construct(FileSystem* lpFileSystem, u32 luFileId);
        FileHandle& operator=(const FileHandle& lOtherHandle);
        bool operator==(const FileHandle& lOtherHandle) const;

        // @0x8264AEE0. Takes the file-system-wide lock, delegates to FileSystem::ReadInternal,
        // releases the lock, returns its result.
        bool Read(void* lpOutputBuffer, u64 luFilePosition, u64 lnSizeToRead);

        // @0x8264AF40. Takes the file-system-wide lock, delegates to FileSystem::WriteInternal,
        // releases the lock, returns its result.
        bool Write(const void* lpInputBuffer, u64 luFilePosition, u64 lnSizeToWrite);

        FileState GetStatus() const;
        u32 GetFileId() const;
        u64 GetLastOperationSize();

    private:
        // ---- instance layout (DWARF CgsFileHandle.h:82-83) ----
        FileSystem* mpFileSystem;  // :82  (asm +0)
        u32         muFileId;      // :83  (asm +4)
    };
}
