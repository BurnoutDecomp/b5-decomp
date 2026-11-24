#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceMemFileSystem.h"
#include <new>
#include <cstddef>

// Native filesystem lifetime and stream dispatch; ARTIST assembly is authoritative.
//
// SOURCE LINES: ARTIST bakes this file's lines into its asserts (file string @0x820F9640):
// OpenInternal 455, ReadInternal 481, WriteInternal 500, CloseInternal 520,
// OpenReadStreamInternal 578/608, RegisterMemFileInternal 663, UnregisterMemFileInternal 681.
// The PS3 DecFIGS copies pass 452/478/497/517/575/601/656/674 (e.g. CloseInternal @0xD013B8): the
// file is DecFIGS + 3 up to OpenReadStreamInternal, whose ARTIST body is four lines longer (the
// patch-manager remap, PS3 has none), and DecFIGS + 7 after it. Each definition sits on its DWARF
// line plus that offset; the functions with no line in the DWARF follow the last one.
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























    // ARTIST829033C0: native per-instance DVD/HDD devices and their logical remaps.
    void FileSystem::Construct(rw::IResourceAllocator* lpAllocator, rw::IResourceAllocator* lpDebugAllocator, const char* lpcSearchPath, DiskLayout* lpDiskLayout, const char* /*lpcDiskCacheDrive*/)
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
    bool FileSystem::Prepare()
    {
        // and the original search path. Construction does not report prepared early.
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
    bool FileSystem::Release()
    {
        // release files before retiring the RW manager and device workers.
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








































































































    // ARTIST828F0718 selects the first effectively closed file.
    s32 FileSystem::OpenInternal(const char* lpcFileName, FileAccess leFileAccessType, s32 liPriority, bool lbUseHDCache)
    {
        u32 itter;

        for (itter = 0; itter < KU_MAXNOOFFILES; ++itter)
        {
            if (maFiles[itter].GetStatus() == E_FILESTATE_CLOSED)
            {
                break;
            }
        }


        if (itter == KU_MAXNOOFFILES)
        {
            CGS_ASSERT(false, "No more files available to open.\n");
            return -1;
        }

        maFiles[itter].Open(lpcFileName, leFileAccessType, liPriority, lbUseHDCache);
        return static_cast<s32>(itter);
    }
















    // ARTIST @0x828F07F8. The native bounds assertion is non-terminal; execution then indexes maFiles.
    bool FileSystem::ReadInternal(u32 luFileID, void* lpOutputBuffer, u64 luFilePosition, u64 luSizeToRead)
    {
        CGS_ASSERT_STREAM(luFileID < KU_MAXNOOFFILES, "Invalid file id\n");
        maFiles[luFileID].Read(lpOutputBuffer, luFilePosition, luSizeToRead);
        return true;
    }












    // ARTIST @0x828E8B50. As above, keep the assertion non-terminal and return the native constant 1.
    bool FileSystem::WriteInternal(u32 luFileID, const void* lpInputBuffer, u64 luFilePosition, u64 luSizeToWrite)
    {
        CGS_ASSERT_STREAM(luFileID < KU_MAXNOOFFILES, "Invalid file id\n");
        maFiles[luFileID].Write(lpInputBuffer, luFilePosition, luSizeToWrite);
        return true;
    }













    // ARTIST @0x828F08C8.
    bool FileSystem::CloseInternal(u32 luFileID)
    {
        CGS_ASSERT_STREAM(luFileID < KU_MAXNOOFFILES, "Invalid file id\n");
        maFiles[luFileID].Close();
        return true;
    }
















































    // ARTIST @0x82906950. Apply the patch overlay before choosing a stream slot.
    ReadStream FileSystem::OpenReadStreamInternal(const char* lpcFileName, void* lpBuffer, u32 luBufferSize, u32 luNumBlocks,
        s32 liNormalPriority, s32 liHighPrioirty, bool lbUseHDCache)
    {
        ReadStream lNewStream;

        CGS_ASSERT_STREAM(muNumUsedReadStreams < KU_MAX_READ_STREAMS, "Out of read streams\n");

        lNewStream.Construct(0);
        char lacFileName[256];
        const bool lbRemapped = mPatchManager.RemapFilename(lpcFileName, lacFileName, sizeof(lacFileName));
        for (u32 i = 0; i < KU_MAX_READ_STREAMS; ++i)
        {
            if (GetEffectiveStreamStatus(&maReadStreams[i]) != StreamDeviceDiskRead::E_STATUS_CLOSED)
            {
                continue;
            }

            maReadStreams[i].Open(lacFileName, lpBuffer, luBufferSize, luNumBlocks,
                                  liNormalPriority, liHighPrioirty, lbUseHDCache && !lbRemapped,
                                  static_cast<s32>(i));
            lNewStream.Construct(&maReadStreams[i]);
            ++muNumUsedReadStreams;
            break;
        }











        CGS_ASSERT_STREAM(lNewStream.IsValid(), "Failed to set device on stream\n");
        return lNewStream;
    }











    // ARTIST @0x82903EC8. Only a handle into the owned eight-stream table is
    void FileSystem::CloseReadStreamInternal(ReadStream lStream)
    {
        // closable; the used count drops as soon as the asynchronous close begins.
        for (u32 i = 0; i < KU_MAX_READ_STREAMS; ++i)
        {
            if (lStream.mpStreamDevice != &maReadStreams[i])
                continue;
            maReadStreams[i].Close();
            --muNumUsedReadStreams;
            return;
        }
    }

























    // ARTIST @0x828E8C20 (unnamed in the ledger).
    bool FileSystem::RegisterMemFileInternal(const char* lpcFileName, void* lpData, u64 luSize)
    {
        CGS_ASSERT_STREAM(mpMemFileSystem != 0, "No memory file system\n");
        return mpMemFileSystem->RegisterMemFile(lpcFileName, lpData, luSize);
    }












    // ARTIST @0x828E8CE0.
    bool FileSystem::UnregisterMemFileInternal(const char* lpcFileName)
    {
        CGS_ASSERT_STREAM(mpMemFileSystem != 0, "No memory file system\n");
        return mpMemFileSystem->UnregisterMemFile(lpcFileName);
    }






    // ===== Not on a DWARF line of this file =====
    // The public wrappers (header-inline in the original: ARTIST's resource handlers inline the
    // filesystem-wide lock) and the stream-status accessors, whose ARTIST asserts name
    // CgsFileSystem.h (GetStatus 581, IsReadStreamOpen 653, IsReadStreamClosed 710/712,
    // GetReadStreamIndex 730): their home is the header, not yet restored.
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

    void FileSystem::CloseReadStream(ReadStream lStream)
    {
        mFileSystemFutex.Lock();
        CloseReadStreamInternal(lStream);
        mFileSystemFutex.Unlock();
    }

    s32 FileSystem::Open(const char* lpName, FileAccess leAccess, s32 liPriority, bool lbCache)
    {
        mFileSystemFutex.Lock();
        const s32 liResult = OpenInternal(lpName, leAccess, liPriority, lbCache);
        mFileSystemFutex.Unlock();
        return liResult;
    }
    bool FileSystem::Close(u32 luId)
    {
        mFileSystemFutex.Lock();
        const bool lbResult = CloseInternal(luId);
        mFileSystemFutex.Unlock();
        return lbResult;
    }
    bool FileSystem::RegisterMemFile(const char* lpName, void* lpData, u64 luSize)
    {
        mFileSystemFutex.Lock();
        const bool lbResult = RegisterMemFileInternal(lpName, lpData, luSize);
        mFileSystemFutex.Unlock();
        return lbResult;
    }
    bool FileSystem::UnregisterMemFile(const char* lpName)
    {
        mFileSystemFutex.Lock();
        const bool lbResult = UnregisterMemFileInternal(lpName);
        mFileSystemFutex.Unlock();
        return lbResult;
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
        return lpStream->GetStatus();
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
