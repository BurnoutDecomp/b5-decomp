#pragma once

#include "types.hpp"
// Real instance layout: the 16 file handles, the 8 read-stream engines, the embedded log + lock.
#include "GameShared/GameClasses/System/FileSystem/CgsFile.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileLog.h"
#include "GameShared/GameClasses/System/FileSystem/CgsReadStream.h"
#include "GameShared/GameClasses/System/FileSystem/CgsStreamDeviceDiskRead.h"
// Committed vendor Futex: CRITICAL_SECTION-backed fast mutex (the file-system-wide lock).
#include "eathread/eathread_futex.h"

// CgsFileSystem::FileSystem - the resource file-I/O subsystem embedded by value inside
// CgsResource::ResourceModule. It owns the device manager (DeviceManager singleton) and, on the
// real X360, the rw::core::filesys layer, per-stream critical sections, a FileLog, and the
// async open/read/close stream machinery the BundleLoaderModule reads .BUNDLE files through.
//
// SOURCES (X360 ARTIST): ctor 0x827DF320, Construct 0x829033C0, Prepare 0x828F04E8,
// Release 0x82903880, Destruct 0x828E8A88.
//
// STATUS: the async device engine (DeviceManager + OperationPool + per-device worker thread +
// the Win32 DevicePhysicalPC leaf) is reconstructed and LIVE; the bundle loader reads through it.
// EnsureDeviceManagerUp() does the idempotent bring-up — FileSystem::Prepare calls it, and the
// bundle loader calls it lazily so EVERY load (incl. the early movie/debug-font bootstrapping,
// which runs before Prepare) goes through the engine (no CRT fallback). The rw::core::filesys
// Manager + the 16-slot BaseFile/OpenReadStream wrapper above DeviceManager remain follow-on.
namespace CgsFileSystem
{
    // Idempotent: initialise the DeviceManager + register the PC physical device (spawning its
    // worker thread) if not already up. Callable from FileSystem::Prepare or lazily from the
    // first file read. (X360: FileSystem::Prepare did this once, early in its boot; on PC the
    // resource module prepares late, so the loader also calls it to cover early bootstrapping.)
    void EnsureDeviceManagerUp();

    // Pointer-only members of the layout below; reconstructed in their own follow-on TUs.
    class DiskLayout;
    class DeviceMemFileSystem;

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
    // LAYOUT POLICY: faithful DWARF member ORDER with natural PC widths (the project convention —
    // not exact X360 byte offsets). The X360 ARTIST asm of the methods below is the authority for
    // BEHAVIOUR (bounds checks, the read-stream effective-status collapse); members are reached BY
    // NAME, never by raw offset. The PS3-only device members (mPS3*Drive / the RemapDevices) are
    // gated OUT — they are not part of the X360 build that the asm/ledger attest.
    class FileSystem
    {
    public:
        // ---- PC-native bring-up shim (separate X360 funcs, reconstructed as no-ops) ----
        void Construct();   // 0x829033C0
        bool Prepare();     // 0x828F04E8 — brings up the async device engine
        bool Release();     // 0x82903880
        void Destruct();    // 0x828E8A88

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

    private:
        // Shared effective-status read used by Is{Open,Closed}: the raw stream status, except that
        // any outstanding operation collapses it to E_STATUS_PENDING (the X360 asm: v=PENDING; if
        // pendingCount<=0 v=meStatus).
        static StreamDeviceDiskRead::EReadStreamStatus
            GetEffectiveStreamStatus(const StreamDeviceDiskRead* lpStream);

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
        // (CgsFileSystem.h:261 mDiskCache, :262 mDiskErrorOccured, :263 mReleasing and the
        //  :365 mDebugComponent are follow-on; the PS3 device block :275-281 is gated out.)
    };
}
