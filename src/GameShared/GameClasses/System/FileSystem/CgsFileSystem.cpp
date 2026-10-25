#include <new>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SDKs/EATech/rwcore/filesys/device.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceMemFileSystem.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevicePhysicalPC.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <cstring>

// CgsFileSystem::FileSystem - see the header. The async device engine bring-up lives in
// EnsureDeviceManagerUp(); the rw filesys layer + BaseFile stream machinery are follow-on.
namespace CgsFileSystem
{
namespace
{
    // Original CgsStringUtils.h:65 copy expansion. Preserve the assert and
    // terminator-inclusive copy; no truncation or alternate path is introduced.
    void CopyConstructionString(char* lpcDest, u32 luBytes, const char* lpcSource)
    {
        const size_t luLength = std::strlen(lpcSource);
        if (luLength >= luBytes)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lBuffer;
            CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
            lStream << "String " << lpcSource << " is too long. Buffer size = "
                    << luBytes << ", string length = " << static_cast<u32>(luLength) << "\n";
            CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 65);
            CgsDev::Assert::EndAssert();
        }
        std::memcpy(lpcDest, lpcSource, luLength + 1);
    }

}
    EA::Thread::AtomicInt32 FileSystem::mDiskErrorOccured(0);
    EA::Thread::AtomicInt32 FileSystem::mReleasing(0); // primary raw830EA574

    bool FileSystem::HasDiskErrorOccured() const
    {
        return mDiskErrorOccured.GetValue() != 0;
    }

    bool FileSystem::RWDiskErrorHandler(s32 /*liError*/)
    {
        mDiskErrorOccured.SetValue(1);
        return false;
    }

    // ARTIST829035B0..CC binds the same828E8B20 store leaf to the typed device
    // callback. Keep the RW int32 callback's declaration and full body intact.
    bool FileSystem::DiskErrorHandler(EDeviceErrors /*leError*/)
    {
        mDiskErrorOccured.SetValue(1);
        return false;
    }

    // ARTIST828F04E8. This path requires the actual four-argument Construct.
    bool FileSystem::Prepare()
    {
        if (mbPrepared)
            return true;
        if (!mpFileSystemAllocator)
            return false;

        rw::core::filesys::FileSystemConfig lOptions;
        lOptions.muMaxSearchPaths = 2;
        lOptions.muScratchSize = 1024;
        lOptions.mpAllocator = mpFileSystemAllocator;
        rw::core::filesys::Manager::CreateInstance(&lOptions);
        auto* lpManager = static_cast<rw::core::filesys::Manager*>(rw::core::filesys::Manager::GetInstance());
        EA::Thread::ThreadParameters lThreadParameters = lpManager->mThreadParameters;
        lThreadParameters.mnPriority = 350;
        lThreadParameters.mpName = "RwFileSystem";
        lpManager = static_cast<rw::core::filesys::Manager*>(rw::core::filesys::Manager::GetInstance());
        lpManager->mThreadParameters = lThreadParameters;

        for (u32 luIndex = 0; luIndex < KU_MAXNOOFFILES; ++luIndex)
            maFiles[luIndex].Prepare();
        lpManager = static_cast<rw::core::filesys::Manager*>(rw::core::filesys::Manager::GetInstance());
        lpManager->SetSearchPath(maFilePrefix);
        GetDeviceManager()->SetDefaultPath(maFilePrefix);

        rw::ResourceDescriptor lDescriptor;
        // FLAG PC-platform leaf: retain the native pointer-width object sizeof;
        // original main-only allocation,alignment4,null name and all other lanes.
        lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(sizeof(DeviceMemFileSystem));
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        const rw::Resource lResource = mpFileSystemAllocator->DoAllocate(lDescriptor, nullptr);
        void* const lpMemory = lResource.m_baseResources[0];
        mpMemFileSystem = lpMemory ? ::new (lpMemory) DeviceMemFileSystem("MEM:") : nullptr;
        // Original allocation failure continues into Construct; no new skip policy.
        mpMemFileSystem->Construct();
        lpManager = static_cast<rw::core::filesys::Manager*>(rw::core::filesys::Manager::GetInstance());
        lpManager->RegisterDevice(mpMemFileSystem, 0);
        lpManager = static_cast<rw::core::filesys::Manager*>(rw::core::filesys::Manager::GetInstance());
        lpManager->mErrorHandler = &FileSystem::RWDiskErrorHandler;
        mbPrepared = true;
        muNumUsedReadStreams = 0;
        mReleasing.SetValue(0);
        return true;
    }

    // ARTIST82903880. Shutdown is retried until every original file/stream closes.
    bool FileSystem::Release()
    {
        mReleasing.SetValue(1);
        if (!mbPrepared)
            return true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Attempting to release file system...\n";
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Closing files\n";
        bool lbReleased = true;
        for (u32 luIndex = 0; luIndex < KU_MAXNOOFFILES; ++luIndex)
        {
            File& lrFile = maFiles[luIndex];
            if (lrFile.GetStatus() != E_FILESTATE_CLOSED)
            {
                if (lrFile.GetStatus() == E_FILESTATE_OPEN)
                {
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                        *CgsDev::Log::gpDebugPrint << "Closing file " << luIndex << "\n";
                    lrFile.Close();
                }
                else if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "Aborting close of file " << luIndex
                                             << " due to pending operations\n";
                lbReleased = false;
            }
            else if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "File " << luIndex << " already closed\n";
        }
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Closing streams\n";
        for (u32 luIndex = 0; luIndex < KU_MAX_READ_STREAMS; ++luIndex)
        {
            StreamDeviceDiskRead& lrStream = maReadStreams[luIndex];
            // ARTIST reads this pair directly before the stream's own Close lock.
            if (lrStream.miPendingOperationCount > 0 || lrStream.meStatus != StreamDeviceDiskRead::E_STATUS_CLOSED)
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "Closing stream " << luIndex << "\n";
                lrStream.Close();
                lbReleased = false;
            }
            else if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Stream " << luIndex << " already closed\n";
        }
        if (!lbReleased)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Failed to release file system this frame due to pending operations\n";
            return false;
        }
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Releasing files\n";
        for (u32 luIndex = 0; luIndex < KU_MAXNOOFFILES; ++luIndex)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Releasing file " << luIndex << "\n";
            maFiles[luIndex].Release();
        }
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Destroying file system\n";
        rw::core::filesys::Manager::DestroyInstance();
        GetDeviceManager()->Shutdown();
        mbPrepared = false;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "File system shutdown complete\n";
        return true;
    }

    // ARTIST828E8A88; it does not free the arenas or invent another shutdown.
    void FileSystem::Destruct()
    {
        for (u32 luIndex = 0; luIndex < KU_MAXNOOFFILES; ++luIndex)
            maFiles[luIndex].Destruct();
    }


    // FLAG PC-platform leaf: complete existing native lifecycle retained.
    bool FileSystem::PrepareNativePC()
    {
        EnsureDeviceManagerUp();
        // The X360 installs maFilePrefix here. The PC physical device maps this
        // qualified root to the game's working directory.
        GetDeviceManager()->SetDefaultPath("p_dvd:");
        mbPrepared = true;
        mbReleasing = false;
        return true;
    }

    // FLAG PC-platform leaf: complete existing native lifecycle retained.
    bool FileSystem::ReleaseNativePC()
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

    // FLAG PC-platform leaf: complete existing native lifecycle retained.
    void FileSystem::DestructNativePC() {}

    // The single PC physical device. A process-static (rather than a FileSystem member) so its
    // lifetime spans the whole run and the worker thread that references it never dangles, and so
    // a lazy bring-up from the bundle loader (before the FileSystem object prepares) has a device
    // to register.
    static DevicePhysicalPC gPhysicalDevice;

    void EnsureDeviceManagerUp()
    {
        if (!DeviceManager::GetIfInitialized())
            DeviceManager::InitializeDeviceManagerNativePC();

        DeviceManager* lpManager = GetDeviceManager();
        if (lpManager && !lpManager->FindDevice("p_dvd"))
            lpManager->AddPhysicalDeviceNativePC(&gPhysicalDevice, "p_dvd", 0);
    }

    // @0x827DF320 (boot trace). The X360 ctor walks the 16 file handles and 8 read-stream engines
    // calling RtlInitializeCriticalSection on each embedded lock, constructs the FileLog, inits the
    // subsystem-wide lock, and installs the embedded subobjects' vtables. In faithful C++ that is
    // exactly the default member-subobject construction: each File / StreamDeviceDiskRead / FileLog /
    // Futex constructs itself (installing its own lock + vtable). mbPrepared starts false.
    // DecFIGS h122; original public wrapper inlined at828F3D60..88.
    s32 FileSystem::Open(const char* lpcFileName, FileAccess leFileAccess,
                         s32 liPriority, bool lbUseHDCache)
    {
        mFileSystemFutex.Lock();
        const s32 liFileId = OpenInternal(lpcFileName, leFileAccess, liPriority, lbUseHDCache);
        mFileSystemFutex.Unlock();
        return liFileId;
    }

    // ARTIST828F0718; inspect actual per-file status before taking a free slot.
    s32 FileSystem::OpenInternal(const char* lpcFileName, FileAccess leFileAccess,
                                 s32 liPriority, bool lbUseHDCache)
    {
        u32 luFileId = 0;
        while (luFileId < KU_MAXNOOFFILES)
        {
            if (maFiles[luFileId].GetStatus() == E_FILESTATE_CLOSED)
                break;
            ++luFileId;
        }
        if (luFileId == KU_MAXNOOFFILES)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("No more files available to open.\n",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsFileSystem.cpp", 455);
            CgsDev::Assert::EndAssert();
            return -1;
        }
        maFiles[luFileId].Open(lpcFileName, leFileAccess, liPriority, lbUseHDCache);
        return static_cast<s32>(luFileId);
    }

    // DecFIGS h146; original caller inlines this lock/call/unlock sequence.
    bool FileSystem::Close(u32 luFileId)
    {
        mFileSystemFutex.Lock();
        const bool lbResult = CloseInternal(luFileId);
        mFileSystemFutex.Unlock();
        return lbResult;
    }

    // ARTIST828F08C8: range assertion continues into the real Close call.
    bool FileSystem::CloseInternal(u32 luFileId)
    {
        if (luFileId >= KU_MAXNOOFFILES)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lBuffer;
            CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
            lStream << "Invalid file id\n";
            CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsFileSystem.cpp", 520);
            CgsDev::Assert::EndAssert();
        }
        maFiles[luFileId].Close();
        return true;
    }

    // ARTIST828EC600 and828E8C20; the original public lock wrapper.
    bool FileSystem::RegisterMemFile(const char* lpcFileName, void* lpData, u64 luSize)
    {
        mFileSystemFutex.Lock();
        const bool lbResult = RegisterMemFileInternal(lpcFileName, lpData, luSize);
        mFileSystemFutex.Unlock();
        return lbResult;
    }

    // ARTIST828E8C20; assertion continuation calls the actual owned device.
    bool FileSystem::RegisterMemFileInternal(const char* lpcFileName, void* lpData, u64 luSize)
    {
        if (!mpMemFileSystem)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lBuffer;
            CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
            lStream << "No memory file system\n";
            CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                "d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\system\filesystem\CgsFileSystem.cpp", 663);
            CgsDev::Assert::EndAssert();
        }
        return mpMemFileSystem->RegisterMemFile(lpcFileName, lpData, luSize);
    }

    bool FileSystem::UnregisterMemFile(const char* lpcFileName)
    {
        mFileSystemFutex.Lock();
        const bool lbResult = UnregisterMemFileInternal(lpcFileName);
        mFileSystemFutex.Unlock();
        return lbResult;
    }

    // ARTIST828E8CE0; r4 is the name pointer, not a spurious64-bit third arg.
    bool FileSystem::UnregisterMemFileInternal(const char* lpcFileName)
    {
        if (!mpMemFileSystem)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lBuffer;
            CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
            lStream << "No memory file system\n";
            CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                "d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\system\filesystem\CgsFileSystem.cpp", 681);
            CgsDev::Assert::EndAssert();
        }
        return mpMemFileSystem->UnregisterMemFile(lpcFileName);
    }

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

    // ARTIST829033C0: X360 consumes allocator/debug/path/layout in r4..r7.
    // DecFIGS's additional cache-drive input is not consumed by this build.
    void FileSystem::Construct(rw::IResourceAllocator* lpAllocator,
                               rw::IResourceAllocator* lpDebugAllocator,
                               char* lpcFilePrefix, DiskLayout* lpDiskLayout)
    {
        for (u32 luFile = 0; luFile < KU_MAXNOOFFILES; ++luFile)
            maFiles[luFile].Construct(this, &mLog, static_cast<s32>(luFile));
        for (u32 luStream = 0; luStream < KU_MAX_READ_STREAMS; ++luStream)
            maReadStreams[luStream].Construct(this, &mLog);
        mpFileSystemAllocator = lpAllocator;
        mpDebugAllocator = lpDebugAllocator;
        mPatchManager.Construct(lpAllocator);
        DeviceManager::InitializeDeviceManager(mpFileSystemAllocator);
        mX360DVDDrive.Construct(0);
        mX360HardDrive.Construct(1);
        GetDeviceManager()->AddPhysicalDevice(&mX360DVDDrive, "p_dvd", DiskErrorHandler);
        GetDeviceManager()->AddPhysicalDevice(&mX360HardDrive, "p_hdd", nullptr);
        mX360RemapDVDDrive.Construct("/d");
        mX360RemapHardDrive.Construct("/e");
        GetDeviceManager()->AddVirtualDevice(&mX360RemapDVDDrive, "d", "p_dvd", nullptr);
        GetDeviceManager()->AddVirtualDevice(&mX360RemapHardDrive, "e", "p_hdd", nullptr);
        mbPrepared = false;
        mpDiskLayout = lpDiskLayout;
        CopyConstructionString(maFilePrefix, KU_MAXPREFIXFILELENGTH, lpcFilePrefix);
        mDiskCache.Construct();
    }

    void FileSystem::Construct()
    {
        for (u32 luIndex = 0; luIndex < KU_MAX_READ_STREAMS; ++luIndex)
            maReadStreams[luIndex].Construct(this, &mLog);
        muNumUsedReadStreams = 0;
        mbReleasing = false;
    }





}
