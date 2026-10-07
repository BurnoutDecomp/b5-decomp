#include "GameShared/GameClasses/System/FileSystem/CgsFileHandle.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"

// CgsFileSystem::FileHandle — see CgsFileHandle.h. The two attested methods (Read/Write) are
// thin locked delegators onto FileSystem::{Read,Write}Internal. The X360 asm takes/releases the
// FileSystem's mFileSystemFutex directly around the delegated call (no RAII helper object was
// constructed), so this mirrors with explicit Lock()/Unlock().
namespace CgsFileSystem
{
    // Original inline copy in ReplayModule::WaitForOpenReplayFiles
    // 8264E978..8264E988. Preserve the canonical host pointer and 32-bit id.
    FileHandle& FileHandle::operator=(const FileHandle& lOtherHandle)
    {
        mpFileSystem = lOtherHandle.mpFileSystem;
        muFileId = lOtherHandle.muFileId;
        return *this;
    }

    // Original inline accessor at 8264E9C4..8264E9CC: the FileSystem method
    // owns its real status/lock behavior; the value handle adds no guard.
    FileState FileHandle::GetStatus() const
    {
        return mpFileSystem->GetStatus(muFileId);
    }

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
