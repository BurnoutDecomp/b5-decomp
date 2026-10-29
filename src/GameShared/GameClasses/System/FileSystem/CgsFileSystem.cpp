#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceMemFileSystem.h"
#include <new>
#include <cstddef>

// Native filesystem lifetime and stream dispatch; ARTIST assembly is authoritative.
namespace CgsFileSystem
{

    // @0x827DF320 (boot trace). The X360 ctor walks the 16 file handles and 8 read-stream engines
    // calling RtlInitializeCriticalSection on each embedded lock, constructs the FileLog, inits the
    // subsystem-wide lock, and installs the embedded subobjects' vtables. In faithful C++ that is
    // exactly the default member-subobject construction: each File / StreamDeviceDiskRead / FileLog /
    // Futex constructs itself (installing its own lock + vtable). mbPrepared starts false.
    FileSystem::FileSystem()
        : mbPrepared(false)
        , muNumUsedReadStreams(0)
        , mpFileSystemAllocator(0)
        , mpDebugAllocator(0)
        , mpDiskLayout(0)
        , mpMemFileSystem(0)
    {
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
        lResult.Construct(0);
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

    // ARTIST @0x828F07F8. The native bounds assertion is non-terminal; execution then indexes maFiles.
    bool FileSystem::ReadInternal(u32 luFileID, void* lpOutputBuffer,
                                  u64 luFilePosition, u64 luSizeToRead)
    {
        if (luFileID >= KU_MAXNOOFFILES)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Invalid file id\n",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsFileSystem.cpp",
                481);
            CgsDev::Assert::EndAssert();
        }

        maFiles[luFileID].Read(lpOutputBuffer, luFilePosition, luSizeToRead);
        return true;
    }

    // ARTIST @0x828E8B50. As above, keep the assertion non-terminal and return the native constant 1.
    bool FileSystem::WriteInternal(u32 luFileID, const void* lpInputBuffer,
                                   u64 luFilePosition, u64 luSizeToWrite)
    {
        if (luFileID >= KU_MAXNOOFFILES)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Invalid file id\n",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsFileSystem.cpp",
                500);
            CgsDev::Assert::EndAssert();
        }

        maFiles[luFileID].Write(lpInputBuffer, luFilePosition, luSizeToWrite);
        return true;
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
        CGS_ASSERT(lStream.mpStreamDevice != 0, "No device found");
        return GetEffectiveStreamStatus(lStream.mpStreamDevice) ==
               StreamDeviceDiskRead::E_STATUS_OPEN;
    }

    // @0x828D6708. Receives the slot index; resolves it to the stream engine, then tests CLOSED.
    bool FileSystem::IsReadStreamClosed(s32 liIndex)
    {
        CGS_ASSERT(static_cast<u32>(liIndex) < KU_MAX_READ_STREAMS, "Index out of range");
        const StreamDeviceDiskRead* lpStream = &maReadStreams[liIndex];
        CGS_ASSERT(lpStream != 0, "No device found");
        return GetEffectiveStreamStatus(lpStream) == StreamDeviceDiskRead::E_STATUS_CLOSED;
    }

    // ARTIST829033C0: native per-instance DVD/HDD devices and their logical remaps.
    void FileSystem::Construct(rw::IResourceAllocator* lpAllocator,
                               rw::IResourceAllocator* lpDebugAllocator,
                               const char* lpcSearchPath, DiskLayout* lpDiskLayout)
    {
        for (u32 lu = 0; lu < KU_MAXNOOFFILES; ++lu)
            maFiles[lu].Construct(this, &mLog, static_cast<s32>(lu));
        for (u32 lu = 0; lu < KU_MAX_READ_STREAMS; ++lu)
            maReadStreams[lu].Construct(this, &mLog);
        mpFileSystemAllocator = lpAllocator;
        mpDebugAllocator = lpDebugAllocator;
        mPatchManager.Construct(lpAllocator);
        DeviceManager::InitializeDeviceManager(lpAllocator);
        mX360DVDDrive.Construct(0);
        mX360HardDrive.Construct(1);
        GetDeviceManager()->AddPhysicalDevice(&mX360DVDDrive, "p_dvd", RWDiskErrorHandler);
        GetDeviceManager()->AddPhysicalDevice(&mX360HardDrive, "p_hdd", 0);
        mX360RemapDVDDrive.Construct("/d");
        mX360RemapHardDrive.Construct("/e");
        GetDeviceManager()->AddVirtualDevice(&mX360RemapDVDDrive, "d", "p_dvd", 0);
        GetDeviceManager()->AddVirtualDevice(&mX360RemapHardDrive, "e", "p_hdd", 0);
        mbPrepared = false;
        mpDiskLayout = lpDiskLayout;
        CgsCore::StrCpy(maFilePrefix, sizeof(maFilePrefix), lpcSearchPath);
        mDiskCache.Construct();
    }

    // ARTIST828F04E8: allocator-backed RenderWare manager, native memory driver,
    // and the original search path. Construction does not report prepared early.
    bool FileSystem::Prepare()
    {
        if (mbPrepared) return true;
        if (!mpFileSystemAllocator) return false;
        rw::core::filesys::FileSystemConfig lOptions;
        lOptions.muMaxSearchPaths = 2;
        lOptions.muScratchSize = 1024;
        lOptions.mpAllocator = mpFileSystemAllocator;
        rw::core::filesys::Manager::CreateInstance(&lOptions);
        rw::core::filesys::Manager* lpManager =
            static_cast<rw::core::filesys::Manager*>(rw::core::filesys::Manager::GetInstance());
        EA::Thread::ThreadParameters lParameters = lpManager->mThreadParameters;
        lParameters.mnPriority = 350;
        lParameters.mpName = "RwFileSystem";
        lpManager->mThreadParameters = lParameters;
        for (u32 lu = 0; lu < KU_MAXNOOFFILES; ++lu) maFiles[lu].Prepare();
        lpManager->SetSearchPath(maFilePrefix);
        GetDeviceManager()->SetDefaultPath(maFilePrefix);
        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(DeviceMemFileSystem);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        void* lpMemory = mpFileSystemAllocator->DoAllocate(lDescriptor, 0).m_baseResources[0];
        mpMemFileSystem = lpMemory ? ::new (lpMemory) DeviceMemFileSystem("MEM:") : 0;
        mpMemFileSystem->Construct();
        lpManager->RegisterDevice(mpMemFileSystem, 0);
        lpManager->mpErrorHandler = RWDiskErrorHandler;
        mbPrepared = true;
        muNumUsedReadStreams = 0;
        mReleasing.SetValue(0);
        return true;
    }

    // ARTIST82903880: start closes, retry while anything remains pending, then
    // release files before retiring the RW manager and device workers.
    bool FileSystem::Release()
    {
        mReleasing.SetValue(1);
        if (!mbPrepared) return true;
        bool lbClosed = true;
        for (u32 lu = 0; lu < KU_MAXNOOFFILES; ++lu)
        {
            if (maFiles[lu].GetStatus() == E_FILESTATE_CLOSED) continue;
            if (maFiles[lu].GetStatus() == E_FILESTATE_OPEN) maFiles[lu].Close();
            lbClosed = false;
        }
        for (u32 lu = 0; lu < KU_MAX_READ_STREAMS; ++lu)
        {
            if (GetEffectiveStreamStatus(&maReadStreams[lu]) == StreamDeviceDiskRead::E_STATUS_CLOSED) continue;
            maReadStreams[lu].Close();
            lbClosed = false;
        }
        if (!lbClosed) return false;
        for (u32 lu = 0; lu < KU_MAXNOOFFILES; ++lu) maFiles[lu].Release();
        rw::core::filesys::Manager::DestroyInstance();
        GetDeviceManager()->Shutdown();
        mbPrepared = false;
        return true;
    }

    // ARTIST828E8A88: only the sixteen file records are destructed here.
    void FileSystem::Destruct()
    {
        for (u32 lu = 0; lu < KU_MAXNOOFFILES; ++lu) maFiles[lu].Destruct();
    }

    void FileSystem::AssertNativeLayout()
    {
        CGS_STATIC_ASSERT(offsetof(FileSystem, maFiles) == 1024, "ARTIST file table");
        CGS_STATIC_ASSERT(offsetof(FileSystem, mbPrepared) == 27648, "ARTIST prepared flag");
        CGS_STATIC_ASSERT(offsetof(FileSystem, maReadStreams) == 27656, "ARTIST read-stream table");
        CGS_STATIC_ASSERT(offsetof(FileSystem, mpFileSystemAllocator) == 39180, "ARTIST filesystem allocator");
        CGS_STATIC_ASSERT(offsetof(FileSystem, mpMemFileSystem) == 39232, "ARTIST memory driver");
        CGS_STATIC_ASSERT(offsetof(FileSystem, mPatchManager) == 39240, "ARTIST patch manager");
        CGS_STATIC_ASSERT(offsetof(FileSystem, mX360DVDDrive) == 41456, "ARTIST DVD device");
        CGS_STATIC_ASSERT(offsetof(FileSystem, mX360HardDrive) == 45856, "ARTIST HDD device");
        CGS_STATIC_ASSERT(offsetof(FileSystem, mX360RemapDVDDrive) == 50256, "ARTIST DVD remap");
        CGS_STATIC_ASSERT(offsetof(FileSystem, mX360RemapHardDrive) == 50332, "ARTIST HDD remap");
        CGS_STATIC_ASSERT(sizeof(FileSystem) == 50408, "ARTIST filesystem size");
    }
}
