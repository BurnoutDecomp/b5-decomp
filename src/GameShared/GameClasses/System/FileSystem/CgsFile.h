#pragma once

// CgsFile.h — canonical home for CgsFileSystem::BaseFile (the lock-guarded base of
// every file handle in the resource file-I/O subsystem) plus the FileAccess/FileState
// vocabulary and the FileFutexHelper RAII lock guard.
//
// SOURCES: the DecFIGS DWARF CgsFile.h is GROUND TRUTH for the inline accessor bodies;
// the X360 ARTIST DWARF (CgsFile.h) + pseudocode are authority where they disagree.
//   - BaseFile::GetStatus @ 0x8264AD10 (the one boot-trace ledger function for this TU):
//     locks the per-file critical section, returns E_FILESTATE_CLOSED unchanged, else
//     E_FILESTATE_PENDING when operations are outstanding, else the raw state. The asm
//     reads meFileState at +8, muPendingOperationCount at +32, and Rtl{Enter,Leave}-
//     CriticalSection on the lock at +56 — i.e. the lock is a Win32 CRITICAL_SECTION.
//
// X360-vs-PS3 RECONCILIATION: the PS3 DWARF guards with EA::Thread::Mutex via a
// FileMutexHelper(EA::Thread::Mutex&). The X360 ARTIST DWARF renames these to `Futex mFutex`
// and FileFutexHelper(Futex&, const char*) (a debug-name'd guard). Per the project rule
// (X360 asm/DWARF overrides PS3 DWARF), this home uses the Futex form. The committed
// vendor EA::Thread::Futex IS that type — a CRITICAL_SECTION wrapper whose Lock()/Unlock()
// map onto Enter/LeaveCriticalSection, matching the asm — so it is reused BY NAME rather
// than forked.

#include "types.hpp"
// Committed vendor Futex: CRITICAL_SECTION-backed fast mutex (Lock/Unlock).
#include "eathread/eathread_futex.h"

namespace CgsFileSystem
{
    // ---- DWARF CgsFile.h:45 / the DWARF ----
    enum FileAccess
    {
        E_FILEACCESS_CLOSED      = 0,
        E_FILEACCESS_READ        = 1,
        E_FILEACCESS_WRITE       = 2,
        E_FILEACCESS_COUNT       = 3,
        E_FILEACCESS_FORCE_DWORD = 0xFFFFFFFF
    };

    // ---- DWARF CgsFile.h:57 / the DWARF ----
    enum FileState
    {
        E_FILESTATE_CLOSED  = 0,
        E_FILESTATE_OPENING = 1,
        E_FILESTATE_OPEN    = 2,
        E_FILESTATE_READING = 3,
        E_FILESTATE_WRITING = 4,
        E_FILESTATE_CLOSING = 5,
        E_FILESTATE_SEEKING = 6,
        E_FILESTATE_PENDING = 7,
        E_FILESTATE_ERROR   = 8,
        E_FILESTATE_COUNT   = 9
    };

    // Forward-declared: BaseFile references these by pointer only.
    class FileLog;
    class FileSystem;

    // ---- DWARF CgsFile.h:83 (X360 FileFutexHelper) ----
    // Scoped lock guard. Locks the supplied Futex for the lifetime of the helper. The
    // X360 build threads a debug-name string through for the file-system lock profiler;
    // it is accepted and ignored here (the committed Futex carries no naming hook).
    // (the PS3 DWARF spelling: FileMutexHelper(EA::Thread::Mutex&), no name arg.)
    class FileFutexHelper
    {
    public:
        inline FileFutexHelper(EA::Thread::Futex& lFutex, const char* lpcName = nullptr)
            : mpFutex(&lFutex)
        {
            (void)lpcName;
            mpFutex->Lock();
        }

        inline ~FileFutexHelper()
        {
            mpFutex->Unlock();
        }

    private:
        EA::Thread::Futex* mpFutex;  // DWARF CgsFile.h:138
    };

    // ---- DWARF CgsFile.h:158 ----
    // Base of every file handle. Owns the file's access mode, name, state, seek cursor,
    // last-operation size, the outstanding-operation count and a per-file lock. All the
    // status/priority accessors take that lock for the duration of the read/write.
    class BaseFile
    {
    public:
        // @0x8264AD10. Locked read of the file state, collapsing any outstanding-op
        // situation to E_FILESTATE_PENDING (but never masking the CLOSED state).
        inline FileState GetStatus()
        {
            FileFutexHelper lGuard(mFutex);

            FileState leReturnVal;
            if (meFileState != E_FILESTATE_CLOSED)
            {
                leReturnVal = (muPendingOperationCount == 0) ? meFileState
                                                             : E_FILESTATE_PENDING;
            }
            else
            {
                leReturnVal = meFileState;
            }
            return leReturnVal;
        }

        // ---- remaining inline accessors (the DWARF layout; not separately
        //      attested in the boot trace, homed here so the class is complete) ----
        inline u64 GetLastOperationSize()
        {
            FileFutexHelper lGuard(mFutex);
            return muLastOperationSize;
        }

        inline s32 GetPriority()
        {
            FileFutexHelper lGuard(mFutex);
            return miPriority;
        }

        inline void SetPriority(s32 liPriority)
        {
            FileFutexHelper lGuard(mFutex);
            miPriority = liPriority;
        }

        inline void IncreasePendingOperations()
        {
            FileFutexHelper lGuard(mFutex);
            ++muPendingOperationCount;
        }

    protected:
        // ---- instance layout (DWARF CgsFile.h:187-199) ----
        FileAccess        meFileAccess;            // :187
        const char*       mpcFileName;             // :188
        FileState         meFileState;             // :189  (asm +8)
        u64               muSeekPosition;          // :190
        u64               muLastOperationSize;     // :191
        u32               muPendingOperationCount; // :192  (asm +32)
        s32               miPriority;              // :194
        char              macSeekBuffer[4];        // :195
        FileLog*          mpLog;                   // :196
        FileSystem*       mpFileSystem;            // :197
        EA::Thread::Futex mFutex;                  // :198  (asm +56; the Rtl-CS lock)
        s32               miIndex;                 // :199

        // ---- lifecycle (DWARF CgsFile.h:204-216) ----
        // DECLARE-ONLY: bodies are separate TUs (CgsFile.cpp).
        void Construct(FileSystem* lpFileSystem, FileLog* lpLog, s32 liIndex); // :204
        void Prepare();                                                        // :208
        void Release();                                                        // :212
        void Destruct();                                                       // :216

        // The X360 file-system debug component pokes BaseFile internals directly.
        friend class DebugComponentFileSystem;
    };

} // namespace CgsFileSystem
