#pragma once

#include "types.hpp"
#include "SDKs/EATech/eathread/eathread_atomic.h"
// Real instance layout: the 16 file handles, the 8 read-stream engines, the embedded log + lock.
#include "GameShared/GameClasses/System/FileSystem/CgsFile.h"
#include "GameShared/GameClasses/System/FileSystem/CgsPatchManager.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDiskCache.h"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevicePhysicalX360.h"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsRemapDevice.h"
// Complete CgsFileSystem::File definition — maFiles[16] is embedded by value below.
#include "GameShared/GameClasses/System/FileSystem/CgsFileRW.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileLog.h"
#include "GameShared/GameClasses/System/FileSystem/CgsReadStream.h"
#include "GameShared/GameClasses/System/FileSystem/CgsStreamDeviceDiskRead.h"
// Committed vendor Futex: CRITICAL_SECTION-backed fast mutex (the file-system-wide lock).
#include "eathread/eathread_futex.h"

// Native resource filesystem:16 buffered files,8 read-stream engines, the
// allocator-owned RW manager and memory driver, patch map and DVD/HDD devices.
// ARTIST governs lifecycle/ABI; AssertNativeLayout checks the original offsets.
namespace CgsFileSystem
{
    // Pointer-only members of the layout below; reconstructed in their own follow-on TUs.
    class DiskLayout;
    class DeviceMemFileSystem;

    // The value handle (CgsFileHandle.h) locks this class's mFileSystemFutex and calls
    // Read/WriteInternal directly — it is intimate with FileSystem.
    struct FileHandle;

    // ---- DWARF CgsFileSystem.h:56-62 ----
    const u32 KU_MAXNOOFFILES                    = 16;
    const u32 KU_MAXPREFIXFILELENGTH             = 1024;
    const u32 KU_FILE_SYSTEM_BUFFER_SIZE         = 16384;
    const u32 KU_FILE_SYSTEM_EXTENDED_BUFFER_SIZE = 20480;
    const u32 KU_MAX_READ_STREAMS                = 8;
    const u32 KU_INVALID_FILE_ID                 = 0xFFFFFFFFu;

    // ---- DWARF CgsFileSystem.h:90 ----
    // The resource file-I/O subsystem. Owns 16 buffered file handles, 8 async read-stream engines,
    // the file log, a system scratch buffer and a subsystem-wide lock. Embedded by value inside
    // CgsResource::ResourceModule.
    //
    class FileSystem
    {
    public:
        // Native allocator/device lifecycle.
        void Construct(rw::IResourceAllocator* lpAllocator,
                       rw::IResourceAllocator* lpDebugAllocator,
                       const char* lpcSearchPath, DiskLayout* lpDiskLayout); // ARTIST829033C0
        bool Prepare();     // ARTIST828F04E8
        bool Release();     // 0x82903880
        void Destruct();    // 0x828E8A88

        bool HasDiskErrorOccured() const;

        // ---- X360-attested accessors (this TU) ----
        // @0x827DF320 (boot trace). Default-constructs the embedded handles/streams/log; the
        // member subobjects install their own locks (the X360 ctor inlined RtlInitializeCriticalSection
        // per embedded Futex).
        FileSystem();

        // @0x8264AE28. Locked file-state read for file id `luFileId` (0..KU_MAXNOOFFILES-1).
        FileState GetStatus(u32 luFileId);

        // @0x828D6860. Index of `lStream` within maReadStreams (0..KU_MAX_READ_STREAMS-1).
        u32 GetReadStreamIndex(ReadStream lStream) const;

        // @0x828D6650. True when the stream is OPEN (collapsing outstanding ops to PENDING first).
        bool IsReadStreamOpen(ReadStream lStream);

        // @0x828D6708. True when the read-stream slot `liIndex` is CLOSED.
        bool IsReadStreamClosed(s32 liIndex);

        // DWARF CgsFileSystem.h:189/195. The public locked read-stream pair.
        ReadStream OpenReadStream(const char* lpcName, void* lpBuffer,
                                  u32 luBufferSize, u32 luNumBlocks,
                                  s32 liNormalPriority, s32 liHighPriority,
                                  bool lbUseHDCache);
        void CloseReadStream(ReadStream lStream);

    private:
        static EA::Thread::AtomicInt32 mDiskErrorOccured;
        static EA::Thread::AtomicInt32 mReleasing;
        static void AssertNativeLayout();
        static bool RWDiskErrorHandler(s32 liIndex);

        // Shared effective-status read used by Is{Open,Closed}: the raw stream status, except that
        // any outstanding operation collapses it to E_STATUS_PENDING (the X360 asm: v=PENDING; if
        // pendingCount<=0 v=meStatus).
        static StreamDeviceDiskRead::EReadStreamStatus
            GetEffectiveStreamStatus(const StreamDeviceDiskRead* lpStream);

        // ---- DWARF CgsFileSystem.h:308/317 (private) ----
        // The actual file I/O primitives, called under mFileSystemFutex by FileHandle::Read/Write.
        // DECLARE-ONLY here: bodies are their own TUs (ReadInternal @0x828E8B50-region /
        // WriteInternal @0x828E8B50, homed in CgsFileSystem.cpp).
        bool ReadInternal(u32 luFileID, void* lpOutputBuffer, u64 luFilePosition, u64 luSizeToRead);
        bool WriteInternal(u32 luFileID, const void* lpInputBuffer, u64 luFilePosition,
                           u64 luSizeToWrite);
        ReadStream OpenReadStreamInternal(const char* lpcName, void* lpBuffer,
                                          u32 luBufferSize, u32 luNumBlocks,
                                          s32 liNormalPriority, s32 liHighPriority,
                                          bool lbUseHDCache);
        void CloseReadStreamInternal(ReadStream lStream);

        // FileHandle::Read/Write lock this->mFileSystemFutex and delegate to Read/WriteInternal.
        friend struct FileHandle;

        // ---- instance layout (faithful DWARF order, CgsFileSystem.h:249-263; X360 members only) ----
        char                  maFilePrefix[KU_MAXPREFIXFILELENGTH];               // :249
        File                  maFiles[KU_MAXNOOFFILES];                           // :250
        char                  macSystemBuffer[KU_FILE_SYSTEM_EXTENDED_BUFFER_SIZE]; // :251
        bool                  mbPrepared;                                         // :252
        StreamDeviceDiskRead  maReadStreams[KU_MAX_READ_STREAMS];                 // :253
        u32                   muNumUsedReadStreams;                               // :254
        rw::IResourceAllocator* mpFileSystemAllocator;                            // :255
        rw::IResourceAllocator* mpDebugAllocator;                                 // :256
        FileLog               mLog;                                               // :257
        DiskLayout*           mpDiskLayout;                                       // :258
        EA::Thread::Futex     mFileSystemFutex;                                   // :259
        DeviceMemFileSystem*  mpMemFileSystem;                                    // :260
        DiskCache mDiskCache; // ARTIST82903864..74 and DecFIGS debug option names.
        PatchManager mPatchManager; // ARTIST829034BC: Construct atfilesystem+39240.
        DevicePhysicalX360 mX360DVDDrive;
        DevicePhysicalX360 mX360HardDrive;
        RemapDevice mX360RemapDVDDrive;
        RemapDevice mX360RemapHardDrive;
    };
}
