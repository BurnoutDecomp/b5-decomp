#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevicePhysicalPC.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsFileSystem::FileSystem - see the header. The async device engine bring-up lives in
// EnsureDeviceManagerUp(); the rw filesys layer + BaseFile stream machinery are follow-on.
namespace CgsFileSystem
{
    // Marked PC sentinel allocator. The X360 passed the GameData allocator to
    // InitializeDeviceManager (which only stores it; DeviceManager::operator new mallocs on PC),
    // so any non-null value satisfies the "Allocator required" assert. Routing the manager through
    // the real GameData allocator is deferred.
    static char gcPcAllocatorSentinel;

    // The single PC physical device. A process-static (rather than a FileSystem member) so its
    // lifetime spans the whole run and the worker thread that references it never dangles, and so
    // a lazy bring-up from the bundle loader (before the FileSystem object prepares) has a device
    // to register.
    static DevicePhysicalPC gPhysicalDevice;

    void EnsureDeviceManagerUp()
    {
        if (!DeviceManager::GetIfInitialized())
            DeviceManager::InitializeDeviceManager(&gcPcAllocatorSentinel);

        DeviceManager* lpManager = GetDeviceManager();
        if (lpManager && !lpManager->FindDevice("p_dvd"))
            lpManager->AddPhysicalDevice(&gPhysicalDevice, "p_dvd", 0);
    }

    // @0x827DF320 (boot trace). The X360 ctor walks the 16 file handles and 8 read-stream engines
    // calling RtlInitializeCriticalSection on each embedded lock, constructs the FileLog, inits the
    // subsystem-wide lock, and installs the embedded subobjects' vtables. In faithful C++ that is
    // exactly the default member-subobject construction: each File / StreamDeviceDiskRead / FileLog /
    // Futex constructs itself (installing its own lock + vtable). mbPrepared starts false.
    FileSystem::FileSystem()
        : mbPrepared(false)
        , muNumUsedReadStreams(0)
        , mpFileSystemAllocator(nullptr)
        , mpDebugAllocator(nullptr)
        , mpDiskLayout(nullptr)
        , mpMemFileSystem(nullptr)
    {
    }

    // @0x8264AE28.
    FileState FileSystem::GetStatus(u32 luFileId)
    {
        CGS_ASSERT(luFileId < KU_MAXNOOFFILES, "Invalid file id");
        return maFiles[luFileId].GetStatus();
    }

    // @0x828D6860. The X360 asm computes (stream - &maReadStreams[0]) / sizeof(stream) via the
    // 1440-byte stride; in human C++ that is plain pointer subtraction over the array.
    u32 FileSystem::GetReadStreamIndex(ReadStream lStream) const
    {
        const u32 luIndex =
            static_cast<u32>(lStream.mpStreamDevice - &maReadStreams[0]);
        CGS_ASSERT(luIndex < KU_MAX_READ_STREAMS, "Index out of range");
        return luIndex;
    }

    // Shared effective-status read (X360 asm in Is{Open,Closed}): any outstanding operation
    // collapses the reported status to E_STATUS_PENDING.
    StreamDeviceDiskRead::EReadStreamStatus
        FileSystem::GetEffectiveStreamStatus(const StreamDeviceDiskRead* lpStream)
    {
        if (lpStream->miPendingOperationCount > 0)
            return StreamDeviceDiskRead::E_STATUS_PENDING;
        return lpStream->meStatus;
    }

    // @0x828D6650. Receives the ReadStream by value (= the StreamDeviceDiskRead*).
    bool FileSystem::IsReadStreamOpen(ReadStream lStream)
    {
        CGS_ASSERT(lStream.mpStreamDevice != nullptr, "No device found");
        return GetEffectiveStreamStatus(lStream.mpStreamDevice) ==
               StreamDeviceDiskRead::E_STATUS_OPEN;
    }

    // @0x828D6708. Receives the slot index; resolves it to the stream engine, then tests CLOSED.
    bool FileSystem::IsReadStreamClosed(s32 liIndex)
    {
        CGS_ASSERT(static_cast<u32>(liIndex) < KU_MAX_READ_STREAMS, "Index out of range");
        const StreamDeviceDiskRead* lpStream = &maReadStreams[liIndex];
        CGS_ASSERT(lpStream != nullptr, "No device found");
        return GetEffectiveStreamStatus(lpStream) == StreamDeviceDiskRead::E_STATUS_CLOSED;
    }

    void FileSystem::Construct() {}

    bool FileSystem::Prepare()
    {
        EnsureDeviceManagerUp();
        return true;
    }

    bool FileSystem::Release()  { return true; }
    void FileSystem::Destruct() {}
}
