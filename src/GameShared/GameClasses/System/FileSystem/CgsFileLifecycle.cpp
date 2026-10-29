#include "GameShared/GameClasses/System/FileSystem/CgsFileRW.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsFileSystem
{
    // Inlined ARTIST829033C0,828F04E8,82903880,828E8A88. Last-operation size
    // and priority are not touched by these reset sequences.
    void BaseFile::Construct(FileSystem* lpSystem, FileLog* lpLog, s32 liIndex)
    {
        FileFutexHelper lGuard(mFutex);
        CGS_ASSERT(lpLog != 0, "lpLog");
        meFileAccess = E_FILEACCESS_CLOSED;
        mpcFileName = 0;
        meFileState = E_FILESTATE_CLOSED;
        muSeekPosition = 0;
        muPendingOperationCount = 0;
        mpLog = lpLog;
        mpFileSystem = lpSystem;
        miIndex = liIndex;
    }
    void BaseFile::Prepare()
    {
        FileFutexHelper lGuard(mFutex);
        meFileAccess = E_FILEACCESS_CLOSED;
        mpcFileName = 0;
        meFileState = E_FILESTATE_CLOSED;
        muSeekPosition = 0;
        muPendingOperationCount = 0;
    }
    void BaseFile::Release()
    {
        FileFutexHelper lGuard(mFutex);
        CGS_ASSERT(static_cast<FileState>(meFileAccess) == E_FILESTATE_CLOSED,
                   "static_cast<CgsFileSystem::FileState> ( meFileAccess ) == E_FILESTATE_CLOSED");
        meFileAccess = E_FILEACCESS_CLOSED;
        mpcFileName = 0;
        meFileState = E_FILESTATE_CLOSED;
        muSeekPosition = 0;
        muPendingOperationCount = 0;
    }
    void BaseFile::Destruct()
    {
        FileFutexHelper lGuard(mFutex);
        CGS_ASSERT(static_cast<FileState>(meFileAccess) == E_FILESTATE_CLOSED,
                   "static_cast< CgsFileSystem::FileState >(meFileAccess) == E_FILESTATE_CLOSED");
        meFileAccess = E_FILEACCESS_CLOSED;
        mpcFileName = 0;
        meFileState = E_FILESTATE_CLOSED;
        muSeekPosition = 0;
        muPendingOperationCount = 0;
    }
    void File::Construct(FileSystem* lpSystem, FileLog* lpLog, s32 liIndex)
    {
        FileFutexHelper lGuard(mFutex);
        CGS_ASSERT(lpLog != 0, "lpLog");
        meFileAccess = E_FILEACCESS_CLOSED;
        mpcFileName = 0; meFileState = E_FILESTATE_CLOSED;
        muSeekPosition = 0; muPendingOperationCount = 0;
        mpLog = lpLog; mpFileSystem = lpSystem; miIndex = liIndex;
        mpHandle = 0; muCacheId = 0xffffffffu;
    }
    void File::Prepare()
    {
        FileFutexHelper lGuard(mFutex);
        meFileAccess = E_FILEACCESS_CLOSED;
        mpcFileName = 0; meFileState = E_FILESTATE_CLOSED;
        muSeekPosition = 0; muPendingOperationCount = 0;
        mpHandle = 0; muCacheId = 0xffffffffu;
    }
    void File::Release()
    {
        FileFutexHelper lGuard(mFutex);
        mpHandle = 0; muCacheId = 0xffffffffu;
        CGS_ASSERT(static_cast<FileState>(meFileAccess) == E_FILESTATE_CLOSED,
                   "static_cast<CgsFileSystem::FileState> ( meFileAccess ) == E_FILESTATE_CLOSED");
        meFileAccess = E_FILEACCESS_CLOSED;
        mpcFileName = 0; meFileState = E_FILESTATE_CLOSED;
        muSeekPosition = 0; muPendingOperationCount = 0;
    }
    void File::Destruct()
    {
        FileFutexHelper lGuard(mFutex);
        mpHandle = 0; muCacheId = 0xffffffffu;
        CGS_ASSERT(static_cast<FileState>(meFileAccess) == E_FILESTATE_CLOSED,
                   "static_cast< CgsFileSystem::FileState >(meFileAccess) == E_FILESTATE_CLOSED");
        meFileAccess = E_FILEACCESS_CLOSED;
        mpcFileName = 0; meFileState = E_FILESTATE_CLOSED;
        muSeekPosition = 0; muPendingOperationCount = 0;
    }
}
