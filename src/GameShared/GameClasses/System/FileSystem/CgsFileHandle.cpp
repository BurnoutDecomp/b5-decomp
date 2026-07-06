#include "GameShared/GameClasses/System/FileSystem/CgsFileHandle.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"

// CgsFileSystem::FileHandle — see CgsFileHandle.h. The two attested methods (Read/Write) are
// thin locked delegators onto FileSystem::{Read,Write}Internal. The X360 asm takes/releases the
// FileSystem's mFileSystemFutex directly around the delegated call (no RAII helper object was
// constructed), so this mirrors with explicit Lock()/Unlock().
namespace CgsFileSystem
{
    // @0x8264AEE0.
    bool FileHandle::Read(void* lpOutputBuffer, u64 luFilePosition, u64 lnSizeToRead)
    {
        FileSystem* lpFileSystem = mpFileSystem;
        const u32 luFileId = muFileId;

        lpFileSystem->mFileSystemFutex.Lock();
        const bool lbResult =
            lpFileSystem->ReadInternal(luFileId, lpOutputBuffer, luFilePosition, lnSizeToRead);
        lpFileSystem->mFileSystemFutex.Unlock();

        return lbResult;
    }

    // @0x8264AF40.
    bool FileHandle::Write(const void* lpInputBuffer, u64 luFilePosition, u64 lnSizeToWrite)
    {
        FileSystem* lpFileSystem = mpFileSystem;
        const u32 luFileId = muFileId;

        lpFileSystem->mFileSystemFutex.Lock();
        const bool lbResult =
            lpFileSystem->WriteInternal(luFileId, lpInputBuffer, luFilePosition, lnSizeToWrite);
        lpFileSystem->mFileSystemFutex.Unlock();

        return lbResult;
    }
}
