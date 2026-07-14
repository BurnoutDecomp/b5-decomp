// CgsFileRW.cpp — CgsFileSystem::File, the concrete buffered file handle, driving the raw
// rw::core::filesys::AsyncOp layer for async open/read/write/close.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the PowerPC asm is ground truth). Each public
// entry point takes the inherited BaseFile::mFutex, asserts the file state, allocates one
// AsyncOp through rw::core::filesys::Manager, submits the request with the matching static
// *Callback trampoline (which recovers `this` from the op's user word and calls the On*
// handler), then bumps the outstanding-operation count. The On* handlers run under the same
// lock, fold the completed op's status (-2 fail / -1 cancel / 1 done) into meFileState,
// decrement the count, and destroy+free the op.
//
// NOTE on the u64 request args: the Hex-Rays pseudocode split luPosition/luSize into 32-bit
// pairs (__PAIR64__ / HIDWORD) — that is a PPC64 mis-read. The asm passes each as a single
// 64-bit register (std this+0x68/+0x70; r6/r7 into AsyncOp::Read/Write), so they are modelled
// here as plain u64 values.

#include "GameShared/GameClasses/System/FileSystem/CgsFileRW.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT / Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (formatted assert text)
#include "SDKs/EATech/rwcore/filesys/asyncop.h"             // rw::core::filesys::AsyncOp / Handle
#include "SDKs/EATech/rwcore/filesys/device.h"              // rw::core::filesys::Manager (Allocate/Free)
#include "rw/core/stdc/stdc.h"                              // rw::core::stdc::StringnCopy

#include <new>       // placement new over the Manager-allocated op storage

namespace CgsFileSystem
{
    using rw::core::filesys::AsyncOp;
    using rw::core::filesys::Manager;

    namespace
    {
        // The X360 read a completed op's status with the "want-wait" flag pointing at a rodata
        // false (the op is already done inside a completion callback), so GetStatus never
        // blocks. Modelled as a named no-wait flag.
        const s32 KI_NO_WAIT = 0;

        // Build a formatted assert message via StrStream (as the X360 did with the shared assert
        // buffer) and fire it. Used for the asserts whose text embeds a runtime value.
        void FireFormattedAssert(const char* lpcMessage)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lpcMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        // Allocate one AsyncOp through the filesys Manager and construct it in place (asm:
        // Manager::Allocate(0x48); if non-null, AsyncOp::AsyncOp(mem); else null).
        AsyncOp* AllocateAsyncOp()
        {
            void* lpMem = Manager::Allocate(static_cast<u32>(sizeof(AsyncOp)));
            return lpMem ? new (lpMem) AsyncOp() : nullptr;
        }

        // Destroy + free a completed op (asm: ~AsyncOp(op); Manager::Free(op)).
        void FreeAsyncOp(AsyncOp* lpOp)
        {
            if (lpOp)
            {
                lpOp->~AsyncOp();
                Manager::Free(lpOp);
            }
        }
    }

    // Open @0x828F0090 — latch name/access/priority, mark the handle OPENING with one pending
    // op, and queue the async device open (completion -> OpenCallback -> OnOpen). Write access
    // opens with flags 7; read access with 0.
    bool File::Open(const char* lpcFileName, FileAccess leFileAccess, s32 liPriority,
                    bool lbUseHDCache)
    {
        (void)lbUseHDCache;   // accepted; the X360 open path ignores it here

        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meFileState == E_FILESTATE_CLOSED, "Invalid state to perform operation\n");
        CGS_ASSERT(muPendingOperationCount == 0,
                   "Should only never have pending operations on open request\n");

        mpHandle                = nullptr;
        mpcFileName             = lpcFileName;
        meFileAccess            = leFileAccess;
        muLastOperationSize     = 0;
        miPriority              = liPriority;
        meFileState             = E_FILESTATE_OPENING;
        muPendingOperationCount = 1;
        muCacheId               = 0xFFFFFFFFu;   // -1: no disk-cache entry

        u32 luFlags = (leFileAccess == E_FILEACCESS_WRITE) ? 7u : 0u;

        AsyncOp* lpOp = AllocateAsyncOp();
        lpOp->Open(lpcFileName, luFlags, &File::OpenCallback,
                   reinterpret_cast<uintptr_t>(this), 0);
        lpOp->SetPriority(miPriority);

        rw::core::stdc::StringnCopy(macFileName, lpcFileName, 255);
        return true;
    }

    // Read @0x828F0248 — record the pending read (position/size/dst) and queue the async device
    // read (completion -> ReadCallback -> OnRead).
    bool File::Read(void* lpBuffer, u64 luPosition, u64 luSize)
    {
        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meFileState != E_FILESTATE_CLOSED, "Invalid state to perform operation\n");
        CGS_ASSERT(mpHandle != nullptr, "No file handle to begin read\n");

        muAttemptedReadFilePos   = luPosition;
        muAttemptedReadOpSize    = luSize;
        mpAttemptedReadOpAddress = lpBuffer;

        AsyncOp* lpOp = AllocateAsyncOp();
        lpOp->Read(mpHandle, lpBuffer, luPosition, luSize, &File::ReadCallback,
                   reinterpret_cast<uintptr_t>(this), 0);
        lpOp->SetPriority(miPriority);
        ++muPendingOperationCount;
        return true;
    }

    // Write @0x828E81F8 — queue the async device write (completion -> WriteCallback -> OnWrite).
    // Only valid on a write-access handle.
    bool File::Write(const void* lpBuffer, u64 luPosition, u64 luSize)
    {
        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meFileState != E_FILESTATE_CLOSED, "Invalid state to perform operation\n");
        CGS_ASSERT(meFileAccess == E_FILEACCESS_WRITE,
                   "File has only read access. Cannot write to it\n");
        CGS_ASSERT(mpHandle != nullptr, "No file handle to begin write\n");

        AsyncOp* lpOp = AllocateAsyncOp();
        lpOp->Write(mpHandle, const_cast<void*>(lpBuffer), luPosition, luSize,
                    &File::WriteCallback, reinterpret_cast<uintptr_t>(this), 0);
        lpOp->SetPriority(miPriority);
        ++muPendingOperationCount;
        return true;
    }

    // Close @0x828F03E0 — queue the async device close (completion -> CloseCallback -> OnClose).
    bool File::Close()
    {
        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meFileState != E_FILESTATE_CLOSED, "Invalid state to perform operation\n");

        AsyncOp* lpOp = AllocateAsyncOp();
        lpOp->Close(mpHandle, &File::CloseCallback, reinterpret_cast<uintptr_t>(this), 0);
        lpOp->SetPriority(miPriority);
        ++muPendingOperationCount;
        return true;
    }

    // OnOpen @0x828E83E0 — device open completion. Fold the op status into the handle state:
    // -2 fail (ERROR + "Failed to open file" report), -1 cancel (back to CLOSED), 1 success
    // (latch the result Handle, go OPEN); anything else is an unhandled-response assert.
    void File::OnOpen(AsyncOp* lpOp)
    {
        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meFileState == E_FILESTATE_OPENING, "Incorrect file state\n");
        CGS_ASSERT(muPendingOperationCount != 0, "Received file event with 0 pending operations\n");

        s32 liStatus = lpOp->GetStatus(&KI_NO_WAIT);
        if (liStatus == -2)
        {
            mpHandle    = nullptr;
            meFileState = E_FILESTATE_ERROR;

            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Failed to open file '"
                     << (mpcFileName ? mpcFileName : "<NULLSTRING>") << "'";
            FireFormattedAssert(lacMessage);
        }
        else if (liStatus == -1)
        {
            mpHandle    = nullptr;
            meFileState = E_FILESTATE_CLOSED;
        }
        else if (liStatus == 1)
        {
            mpHandle    = static_cast<rw::core::filesys::Handle*>(lpOp->GetResultHandle());
            meFileState = E_FILESTATE_OPEN;
        }
        else
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Unhandled response: " << liStatus << "\n";
            FireFormattedAssert(lacMessage);
        }

        --muPendingOperationCount;
        FreeAsyncOp(lpOp);
    }

    // OnRead @0x828E8670 — device read completion. -2 -> ERROR, -1 -> OPEN, 1 -> record the
    // bytes read and go OPEN; unhandled response leaves the state untouched.
    void File::OnRead(AsyncOp* lpOp)
    {
        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meFileState != E_FILESTATE_CLOSED, "Incorrect file state\n");
        CGS_ASSERT(muPendingOperationCount != 0, "Received file event with 0 pending operations\n");

        s32 liStatus = lpOp->GetStatus(&KI_NO_WAIT);
        if (liStatus == -2)
        {
            meFileState = E_FILESTATE_ERROR;
        }
        else if (liStatus == -1)
        {
            meFileState = E_FILESTATE_OPEN;
        }
        else if (liStatus == 1)
        {
            muLastOperationSize = lpOp->GetResultSize();
            meFileState         = E_FILESTATE_OPEN;
        }
        else
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Unhandled response: " << liStatus << "\n";
            FireFormattedAssert(lacMessage);
        }

        --muPendingOperationCount;
        FreeAsyncOp(lpOp);
    }

    // OnWrite @0x828DCBA0 — device write completion (same status folding as OnRead).
    void File::OnWrite(AsyncOp* lpOp)
    {
        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meFileState != E_FILESTATE_CLOSED, "Incorrect file state\n");
        CGS_ASSERT(muPendingOperationCount != 0, "Received file event with 0 pending operations\n");

        s32 liStatus = lpOp->GetStatus(&KI_NO_WAIT);
        if (liStatus == -2)
        {
            meFileState = E_FILESTATE_ERROR;
        }
        else if (liStatus == -1)
        {
            meFileState = E_FILESTATE_OPEN;
        }
        else if (liStatus == 1)
        {
            muLastOperationSize = lpOp->GetResultSize();
            meFileState         = E_FILESTATE_OPEN;
        }
        else
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Unhandled response: " << liStatus << "\n";
            FireFormattedAssert(lacMessage);
        }

        --muPendingOperationCount;
        FreeAsyncOp(lpOp);
    }

    // OnClose @0x828E8868 — device close completion. -2 -> ERROR, -1 -> OPEN, 1 -> fully closed
    // (drop the access mode + handle, state CLOSED); unhandled response asserts.
    void File::OnClose(AsyncOp* lpOp)
    {
        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meFileState != E_FILESTATE_CLOSED, "Incorrect file state\n");
        CGS_ASSERT(muPendingOperationCount != 0, "Received file event with 0 pending operations\n");

        s32 liStatus = lpOp->GetStatus(&KI_NO_WAIT);
        if (liStatus == -2)
        {
            meFileState = E_FILESTATE_ERROR;
        }
        else if (liStatus == -1)
        {
            meFileState = E_FILESTATE_OPEN;
        }
        else if (liStatus == 1)
        {
            meFileAccess = E_FILEACCESS_CLOSED;
            mpHandle     = nullptr;
            meFileState  = E_FILESTATE_CLOSED;
        }
        else
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Unhandled response: " << liStatus << "\n";
            FireFormattedAssert(lacMessage);
        }

        --muPendingOperationCount;
        FreeAsyncOp(lpOp);
    }

    // OpenCallback @0x828E8A58 — recover the owning File from the op's user word and dispatch.
    void File::OpenCallback(AsyncOp* lpOp)
    {
        File* lpFile = reinterpret_cast<File*>(lpOp->mUserData);
        lpFile->OnOpen(lpOp);
    }

    // ReadCallback @0x828E8A68.
    void File::ReadCallback(AsyncOp* lpOp)
    {
        File* lpFile = reinterpret_cast<File*>(lpOp->mUserData);
        lpFile->OnRead(lpOp);
    }

    // WriteCallback @0x828DCD98.
    void File::WriteCallback(AsyncOp* lpOp)
    {
        File* lpFile = reinterpret_cast<File*>(lpOp->mUserData);
        lpFile->OnWrite(lpOp);
    }

    // CloseCallback @0x828E8A78.
    void File::CloseCallback(AsyncOp* lpOp)
    {
        File* lpFile = reinterpret_cast<File*>(lpOp->mUserData);
        lpFile->OnClose(lpOp);
    }

} // namespace CgsFileSystem
