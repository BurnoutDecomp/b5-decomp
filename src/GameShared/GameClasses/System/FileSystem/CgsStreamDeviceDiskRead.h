#pragma once

// CgsStreamDeviceDiskRead.h — canonical home for CgsFileSystem::StreamDeviceDiskRead, the
// async read-stream engine that backs each of the file system's KU_MAX_READ_STREAMS slots,
// plus the EReadStreamStatus vocabulary and the ReadStreamBlock ring entry.
//
// SOURCES: DecFIGS DWARF (CgsStreamDeviceDiskRead.h) for the member NAMES/TYPES/order and the
// enum values; the X360 ARTIST asm of CgsFileSystem::FileSystem::Is{Open,Closed} reconciles the
// effective-status read (status field + the outstanding-operation count collapse to PENDING).
//
// MINIMAL SLICE: the only StreamDeviceDiskRead state the committed code reads today is the
// per-stream status pair (meStatus + miPendingOperationCount) that FileSystem's Is*/index
// accessors test. The full streaming machinery (the 32-block ring, the buffer cursors, the
// device Handle, all the Open/Read/Close/Service plumbing) lives in CgsStreamDeviceDiskRead.cpp
// and is a follow-on TU; its members are DECLARED here in faithful DWARF order so the class is
// shape-correct, but the type is never embedded by exact-byte offset anywhere (FileSystem holds
// the array by value and reaches members BY NAME), so the follow-on layout is free to land.

#include "types.hpp"
// Committed vendor Futex: CRITICAL_SECTION-backed fast mutex.
#include "eathread/eathread_futex.h"

namespace CgsFileSystem
{
    // ---- DWARF CgsStreamDeviceDiskRead.h:85-88 ----
    // Per-block load-state flags (ReadStreamBlock::muFlags).
    const u32 KU_RSBFLAG_EMPTY   = 0;
    const u32 KU_RSBFLAG_LOADING = 1;
    const u32 KU_RSBFLAG_LOADED  = 2;
    const u32 KU_RSBFLAG_EOF      = 4;

    // ---- DWARF CgsStreamDeviceDiskRead.h:130 ----
    const s32 KI_MAX_STREAM_BLOCKS = 32;

    class FileSystem;
    class FileLog;

    // The async device file handle (X360 CgsDeviceManager Handle). Identical to the typedef in
    // CgsDeviceManager.h; declared here too so the stream can store/pass it without pulling in the
    // whole DeviceManager header (a repeated identical typedef is legal in the same namespace).
    typedef s32 Handle;

    // ---- DWARF CgsStreamDeviceDiskRead.h:99 ----
    // One entry of the read-stream's block ring: where in the file it maps, how much has been
    // streamed into it, and its load state.
    struct ReadStreamBlock
    {
        volatile u64 muFilePos;   // :101
        volatile s32 miStreamPos; // :102
        volatile s32 miWritten;   // :103
        volatile s32 miRead;      // :104
        volatile u32 muFlags;     // :105
        struct StreamDeviceDiskRead* mpOwner; // :106
    };

    // ---- DWARF CgsStreamDeviceDiskRead.h:118 ----
    // The async read-stream engine for a single open file. FileSystem owns an array of these and
    // reads the status pair below to answer Is{Open,Closed} queries.
    struct StreamDeviceDiskRead
    {
        // ---- DWARF CgsStreamDeviceDiskRead.h:121 ----
        enum EReadStreamStatus
        {
            E_STATUS_CLOSED  = 0,
            E_STATUS_OPENING = 1,
            E_STATUS_OPEN    = 2,
            E_STATUS_ERROR   = 3,
            E_STATUS_PENDING = 4
        };

        // The file system inlines the status read (see FileSystem::IsReadStreamOpen/Closed in
        // the X360 asm), so it touches these members directly.
        friend class FileSystem;

    private:
        // ---- instance layout (DWARF CgsStreamDeviceDiskRead.h:217-270) ----
        // DECLARE-ONLY shape: faithful DWARF order + types. Bodies for the streaming API live in
        // CgsStreamDeviceDiskRead.cpp (a follow-on TU).
        EA::Thread::Futex     mFutex;                  // :217
        FileSystem*           mpFileSystem;            // :219
        FileLog*              mpLog;                   // :220
        char                  macFileName[256];        // :221
        bool                  mbPrintToLog;            // :222
        char*                 mpBuffer;                // :225
        s32                   miBufferSize;            // :226
        s32                   miNumBlocks;             // :228
        s32                   miBlockSize;             // :229
        s32                   miNormalPriority;        // :231
        s32                   miHighPriority;          // :232
        s32                   miLogIndex;              // :233
        u32                   muCacheId;               // :234
        const void*           mpDEBUGCacheEntry;       // :235 (DiskCacheEntry*, opaque here)
        Handle                mHandle;                 // :238 (the async device file handle)
        ReadStreamBlock       maBlocks[KI_MAX_STREAM_BLOCKS]; // :241
        volatile u64          muAmountRequested;       // :250
        volatile u64          muLastOperationSize;     // :251
        volatile s32          miOutputBlock;           // :254
        volatile bool         mbIsOutputing;           // :255
        volatile u64          muOutputPosition;        // :256
        volatile s32          miInputBlock;            // :258
        volatile s32          miInputRequestsPending;  // :259
        volatile u64          muInputPosition;         // :260
        volatile bool         mbIsEndOfFile;           // :262
        volatile bool         mbWaitingToClose;        // :263
        volatile s32          miBlocksUsed;            // :264
        volatile s32          miNextPriority;          // :265
        volatile bool         mbWaitingToSeek;         // :267
        volatile EReadStreamStatus meStatus;           // :269  (X360 asm: the raw stream status)
        volatile s32          miPendingOperationCount; // :270  (X360 asm: >0 collapses to PENDING)

    public:
        // ---- public API (DWARF CgsStreamDeviceDiskRead.h:134-203) ----
        // DECLARE-ONLY: bodies are the follow-on CgsStreamDeviceDiskRead.cpp.
        void                Construct(FileSystem* lpFileSystem, FileLog* lpLog); // :134
        void                Prepare();                                           // :138
        void                Open(const char* lpcName, void* lpBuffer, u32 luBufferSize,
                                  u32 luNumBlocks, s32 liNormalPriority, s32 liHighPriority,
                                  bool lbPrintToLog, s32 liLogIndex);            // :151
        void                Close();                                            // :155
        void                Seek(u64 luPosition);                               // :159
        u64                 Tell();                                             // :163
        u32                 Read(u32 luAmount, void* lpDest);                   // :170
        bool                StartAsyncRead(void** lppData, u32* lpuAmount);     // :177
        void                StopAsyncRead(u32 luAmount);                        // :183
        u32                 GetAmountOfDataInBuffer() const;                    // :187
        s32                 GetCurrentFilePriority();                           // :191
        bool                IsEndOfFile() const;                                // :194
        u32                 GetBufferSize() const;                              // :197
        EReadStreamStatus   GetStatus() const;                                  // :200
        void                LogAmountOfDataInBuffer() const;                    // :203

    private:
        // ---- internal streaming machinery (DWARF CgsStreamDeviceDiskRead.h:274-297) ----
        // Bodies live in CgsStreamDeviceDiskRead.cpp; all run under mFutex (held by the public
        // entry point that calls them). ResetStreamBlocks/Service/Submit* are the ring-buffer
        // engine; the On*/callback pairs are the device-completion handlers.
        void ResetStreamBlocks();                                               // :274
        void Service();                                                         // :277
        bool SubmitReadRequest();                                              // :280
        bool SubmitCloseRequest();                                             // :283
        void OnOpen(s32 liResult, Handle lHandle, u64 luSize, void* lpContext); // :286
        void OnRead(s32 liResult, Handle lHandle, u64 luSize, void* lpContext); // :289
        void OnClose(s32 liResult, Handle lHandle, u64 luSize, void* lpContext);// :292
        bool StartAsyncReadInternal(void** lppData, u32* lpuAmount);            // :297
        void StopAsyncReadInternal(u32 luAmount);

        // Device-completion trampolines handed to DeviceManager::{Open,Read,Close} as the async
        // callback; each recovers the owning stream and forwards to the matching On* handler.
        static void OpenCallback (s32 liResult, Handle lHandle, u64 luSize, void* lpContext);
        static void ReadCallback (s32 liResult, Handle lHandle, u64 luSize, void* lpContext);
        static void CloseCallback(s32 liResult, Handle lHandle, u64 luSize, void* lpContext);
    };

} // namespace CgsFileSystem
