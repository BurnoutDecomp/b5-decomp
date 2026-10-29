#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"

namespace CgsFileSystem
{
    // ARTIST830EA574; static AtomicInt32 storage is zero-initialized before startup.
    EA::Thread::AtomicInt32 FileSystem::mDiskErrorOccured;

    // Inlined in ResourceModule::Update82907948; CgsFileSystem.h:246.
    bool FileSystem::HasDiskErrorOccured() const
    {
        return mDiskErrorOccured.GetValueRaw() != 0;
    }

    // ARTIST828E8B20: ignore the callback index, atomically latch1, return false.
    bool FileSystem::RWDiskErrorHandler(s32)
    {
        mDiskErrorOccured.SetValue(1);
        return false;
    }
}
