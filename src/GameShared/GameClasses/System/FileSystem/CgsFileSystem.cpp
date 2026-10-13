#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevicePhysicalPC.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"

// CgsFileSystem::FileSystem - see the header. The async device engine bring-up lives in
// EnsureDeviceManagerUp(); the rw filesys layer + BaseFile stream machinery are follow-on.
namespace CgsFileSystem
{
    // Zero-initialized global disk-error latch (ARTIST dword_830EA574; DecFIGS
    // CgsFileSystem.h:262). EA::Thread::AtomicInt32 preserves the native atomic API.
    EA::Thread::AtomicInt32 mDiskErrorOccured(0);

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
            lpManager->AddPhysicalDevice(&gPhysicalDevice, "p_dvd", &FileSystem::RWDiskErrorHandler);
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
        , mbReleasing(false)
    {
    }

    // ARTIST @0x828E8B20. The X360 callback atomically stores 1 into
    // dword_830EA574 and returns 0. This host callback's typed contract is
    // int(int); keep the original error argument even though the native body
    // does not inspect it.
    int FileSystem::RWDiskErrorHandler(s32 /*liError*/)
    {
        mDiskErrorOccured.SetValue(1);
        return 0;
    }

    // ARTIST's ResourceModule::Update reads the same process-wide latch after
    // response processing, while holding its OutputBuffer write lock.
    bool FileSystem::HasDiskErrorOccured() const
    {
        return mDiskErrorOccured.GetValue() != 0;
    }

    ReadStream FileSystem::OpenReadStream(
        const char* lpcName, void* lpBuffer, u32 luBufferSize, u32 luNumBlocks,
        s32 liNormalPriority, s32 liHighPriority, bool lbUseHDCache)
    {
        mFileSystemFutex.Lock();
        ReadStream lStream = OpenReadStreamInternal(
            lpcName, lpBuffer, luBufferSize, luNumBlocks,
            liNormalPriority, liHighPriority, lbUseHDCache);
        mFileSystemFutex.Unlock();
        return lStream;
    }

    // ARTIST @0x82906950. Reject exhaustion, select the first stream whose
    // effective status is CLOSED, configure its async stream device and return
    // the one-pointer ReadStream handle.
    ReadStream FileSystem::OpenReadStreamInternal(
        const char* lpcName, void* lpBuffer, u32 luBufferSize, u32 luNumBlocks,
        s32 liNormalPriority, s32 liHighPriority, bool /*lbUseHDCache*/)
    {
        CGS_ASSERT(muNumUsedReadStreams < KU_MAX_READ_STREAMS,
                   "Out of read streams\n");

        ReadStream lResult;
        lResult.Construct(nullptr);
        for (u32 luIndex = 0; luIndex < KU_MAX_READ_STREAMS; ++luIndex)
        {
            StreamDeviceDiskRead& lrDevice = maReadStreams[luIndex];
            if (GetEffectiveStreamStatus(&lrDevice) !=
                StreamDeviceDiskRead::E_STATUS_CLOSED)
                continue;

            lrDevice.Open(lpcName, lpBuffer, luBufferSize, luNumBlocks,
                          liNormalPriority, liHighPriority, false, -1);
            lResult.Construct(&lrDevice);
            ++muNumUsedReadStreams;
            break;
        }
        CGS_ASSERT(lResult.IsValid(), "Failed to set device on stream\n");
        return lResult;
    }

    void FileSystem::CloseReadStream(ReadStream lStream)
    {
        mFileSystemFutex.Lock();
        CloseReadStreamInternal(lStream);
        mFileSystemFutex.Unlock();
    }

    // ARTIST @0x82903EC8. Only a handle into the owned eight-stream table is
    // closable; the used count drops as soon as the asynchronous close begins.
    void FileSystem::CloseReadStreamInternal(ReadStream lStream)
    {
        for (u32 luIndex = 0; luIndex < KU_MAX_READ_STREAMS; ++luIndex)
        {
            if (lStream.mpStreamDevice != &maReadStreams[luIndex])
                continue;
            maReadStreams[luIndex].Close();
            CGS_ASSERT(muNumUsedReadStreams > 0, "muNumUsedReadStreams > 0");
            --muNumUsedReadStreams;
            return;
        }
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

    // FLAG PC-platform leaf: ARTIST leaves failed opens closed (OnOpen82900740)
    // while its resource poll waits only for OPEN. Preserve native load-failure
    // replies rather than retaining a pending record and stream buffer forever.
    bool FileSystem::HasReadStreamFailedPC(ReadStream lStream) const
    {
        if (!lStream.mpStreamDevice) return true;
        const auto leStatus = GetEffectiveStreamStatus(lStream.mpStreamDevice);
        return leStatus == StreamDeviceDiskRead::E_STATUS_CLOSED
            || leStatus == StreamDeviceDiskRead::E_STATUS_ERROR;
    }

    // @0x828D6708. Receives the slot index; resolves it to the stream engine, then tests CLOSED.
    bool FileSystem::IsReadStreamClosed(s32 liIndex)
    {
        CGS_ASSERT(static_cast<u32>(liIndex) < KU_MAX_READ_STREAMS, "Index out of range");
        const StreamDeviceDiskRead* lpStream = &maReadStreams[liIndex];
        CGS_ASSERT(lpStream != nullptr, "No device found");
        return GetEffectiveStreamStatus(lpStream) == StreamDeviceDiskRead::E_STATUS_CLOSED;
    }

    // The native disk-cache drive is intentionally unused here: DiskCache and
    // the rw::core::filesys backend remain follow-on source paths.
    void FileSystem::Construct(rw::IResourceAllocator* lpFileSystemAllocator,
                               rw::IResourceAllocator* lpDebugAllocator,
                               char* lpInitialSearchPath,
                               DiskLayout* lpDiskLayout,
                               const char* /*lpcDiskCacheDrive*/)
    {
        for (u32 luIndex = 0; luIndex < KU_MAX_READ_STREAMS; ++luIndex)
            maReadStreams[luIndex].Construct(this, &mLog);
        muNumUsedReadStreams = 0;
        mbReleasing = false;

        // ARTIST stores these allocators before constructing the embedded patch
        // overlay; PatchManager retains the first allocator for its map arrays
        // and the native directory-search scratch allocation.
        mpFileSystemAllocator = lpFileSystemAllocator;
        mpDebugAllocator = lpDebugAllocator;
        mPatchManager.Construct(lpFileSystemAllocator);

        // FLAG PC-platform leaf: the host DeviceManager uses its process-lifetime
        // allocator sentinel and physical-device adapter. ARTIST initializes the
        // native device manager from mpFileSystemAllocator here.
        EnsureDeviceManagerUp();

        mpDiskLayout = lpDiskLayout;
        CgsCore::StrCpy(maFilePrefix, sizeof(maFilePrefix), lpInitialSearchPath);
    }

    bool FileSystem::Prepare()
    {
        EnsureDeviceManagerUp();
        // Construct retained the X360 initial search path in maFilePrefix. The PC
        // physical device maps this qualified root to the game's working directory.
        GetDeviceManager()->SetDefaultPath("p_dvd:");
        mbPrepared = true;
        mbReleasing = false;
        return true;
    }

    bool FileSystem::Release()
    {
        // FLAG PC-platform leaf: the native async device retains pointers into
        // these rings. Request close once, and let callbacks finish before the
        // owning resource arena can be retired.
        if (!mbReleasing)
        {
            for (auto& lrStream : maReadStreams)
                if (GetEffectiveStreamStatus(&lrStream) != StreamDeviceDiskRead::E_STATUS_CLOSED)
                    lrStream.Close();
            mbReleasing = true;
        }
        for (const auto& lrStream : maReadStreams)
            if (GetEffectiveStreamStatus(&lrStream) != StreamDeviceDiskRead::E_STATUS_CLOSED)
                return false;
        muNumUsedReadStreams = 0;
        mbPrepared = false;
        return true;
    }
    void FileSystem::Destruct() {}
}
