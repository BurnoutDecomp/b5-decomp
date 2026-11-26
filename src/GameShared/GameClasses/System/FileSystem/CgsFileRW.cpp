// CgsFileRW.cpp -- CgsFileSystem::File, the concrete buffered file handle, driving the raw
// rw::core::filesys::AsyncOp layer for async open/read/write/close. ARTIST asm is ground truth.
// Each entry point takes the inherited BaseFile::mFutex, asserts the file state, allocates one
// AsyncOp, submits it with the matching static *Callback trampoline, then bumps the
// outstanding-operation count; the On* handlers fold the op status into meFileState.
// SOURCE LINES: every definition sits on its DecFIGS DWARF line (ARTIST's asserts, file string
// @0x820F95E8, pass the DecFIGS lines unchanged: Open 131/132 .. OnClose 466/467/498).
// STILL DIFFERENT from the original: the DWARF shows `new AsyncOp` / `delete lpOperator` through
// AsyncOp's class operator new/delete (rw async.h:101-103: Manager::Allocate / Manager::Free,
// inlined by ARTIST); our rw AsyncOp lacks them, so the two file-local helpers below spell the
// same calls.

#include "GameShared/GameClasses/System/FileSystem/CgsFileRW.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "SDKs/EATech/rwcore/filesys/asyncop.h"
#include "SDKs/EATech/rwcore/filesys/device.h"
#include "rw/core/stdc/stdc.h"

#include <new>

namespace CgsFileSystem
{
    using rw::core::filesys::AsyncOp;
    using rw::core::filesys::Manager;

    namespace
    {
        AsyncOp* AllocateAsyncOp()
        {
            void* lpMem = Manager::Allocate(static_cast<u32>(sizeof(AsyncOp)));
            return lpMem ? new (lpMem) AsyncOp() : 0;
        }

        void FreeAsyncOp(AsyncOp* lpOp)
        {
            if (lpOp)
            {
                lpOp->~AsyncOp();
                Manager::Free(lpOp);
            }
        }
    }
















































































    // Open @0x828F0090. Write access opens with flags 7, read access with 0.
    bool File::Open(const char* lpcFileName, FileAccess leFileAccessType, s32 liPriority, bool lbUseHDCache)
    {
        FileFutexHelper lFutexHelper(mFutex, __FUNCTION__);
        (void)lbUseHDCache;   // accepted; the X360 open path ignores it here

        CGS_ASSERT_STREAM(meFileState == E_FILESTATE_CLOSED, "Invalid state to perform operation\n");
        CGS_ASSERT_STREAM(muPendingOperationCount == 0, "Should only never have pending operations on open request\n");

        mpHandle                = 0;
        mpcFileName             = lpcFileName;
        meFileAccess            = leFileAccessType;
        muLastOperationSize     = 0;
        miPriority              = liPriority;
        meFileState             = E_FILESTATE_OPENING;
        muPendingOperationCount = 1;
        muCacheId               = 0xFFFFFFFFu;   // -1: no disk-cache entry

        u32 luFileAccess = 0;
        if (leFileAccessType == E_FILEACCESS_WRITE)
        {
            luFileAccess = 7;
        }

        AsyncOp* lpOperator = AllocateAsyncOp();
        lpOperator->Open(lpcFileName, luFileAccess, &File::OpenCallback, this, 0);
        lpOperator->SetPriority(miPriority);

        rw::core::stdc::StringnCopy(macFileName, lpcFileName, 255);
        return true;
    }


















































    bool File::Read(void* lpBuffer, u64 luPosition, u64 luSize)
    {
        FileFutexHelper lFutexHelper(mFutex, __FUNCTION__);

        // Read @0x828F0248 -- record the pending read and queue the async device read.

        CGS_ASSERT_STREAM(meFileState != E_FILESTATE_CLOSED, "Invalid state to perform operation\n");
        CGS_ASSERT_STREAM(mpHandle != 0, "No file handle to begin read\n");

        muAttemptedReadOpSize    = luSize;
        muAttemptedReadFilePos   = luPosition;
        mpAttemptedReadOpAddress = lpBuffer;

        AsyncOp* lpOperator = AllocateAsyncOp();
        lpOperator->Read(mpHandle, lpBuffer, luPosition, luSize, &File::ReadCallback, this, 0);
        lpOperator->SetPriority(miPriority);
        ++muPendingOperationCount;
        return true;
    }

















    bool File::Write(const void* lpBuffer, u64 luPosition, u64 luSize)
    {
        FileFutexHelper lFutexHelper(mFutex, __FUNCTION__);

        CGS_ASSERT_STREAM(meFileState != E_FILESTATE_CLOSED, "Invalid state to perform operation\n");
        CGS_ASSERT_STREAM(meFileAccess == E_FILEACCESS_WRITE, "File has only read access. Cannot write to it\n");
        CGS_ASSERT_STREAM(mpHandle != 0, "No file handle to begin write\n");

        // Write @0x828E81F8 -- queue the async device write.
        AsyncOp* lpOperator = AllocateAsyncOp();
        lpOperator->Write(mpHandle, lpBuffer, luPosition, luSize, &File::WriteCallback, this, 0);
        lpOperator->SetPriority(miPriority);
        ++muPendingOperationCount;
        return true;
    }















    bool File::Close()
    {
        FileFutexHelper lFutexHelper(mFutex, __FUNCTION__);

        CGS_ASSERT_STREAM(meFileState != E_FILESTATE_CLOSED, "Invalid state to perform operation\n");

        // Close @0x828F03E0 -- queue the async device close.
        AsyncOp* lpOperator = AllocateAsyncOp();
        lpOperator->Close(mpHandle, &File::CloseCallback, this, 0);
        lpOperator->SetPriority(miPriority);
        ++muPendingOperationCount;
        return true;
    }













    void File::OnOpen(AsyncOp* lpOperator)
    {
        // OnOpen @0x828E83E0 -- device open completion: -2 fail, -1 cancel, 1 success.

        FileFutexHelper lFutexHelper(mFutex, __FUNCTION__);


        CGS_ASSERT_STREAM(meFileState == E_FILESTATE_OPENING, "Incorrect file state\n");
        CGS_ASSERT_STREAM(muPendingOperationCount > 0, "Received file event with 0 pending operations\n");

        switch (lpOperator->GetStatus(EA::Thread::kTimeoutImmediate))
        {
            case -2:
                mpHandle    = 0;
                meFileState = E_FILESTATE_ERROR;















                CGS_FAIL_STREAM("Failed to open file '" << mpcFileName << "'");
                break;

            default:


                CGS_FAIL_STREAM("Unhandled response: " << lpOperator->GetStatus(EA::Thread::kTimeoutImmediate) << "\n");
                break;
            case -1:
                mpHandle    = 0;
                meFileState = E_FILESTATE_CLOSED;
                break;
            case 1:
                mpHandle    = lpOperator->GetResultHandle();
                meFileState = E_FILESTATE_OPEN;
                break;
        }

        --muPendingOperationCount;
        FreeAsyncOp(lpOperator);
    }


    void File::OnRead(AsyncOp* lpOperator)
    {
        // OnRead @0x828E8670 -- device read completion: -2 -> ERROR, -1 -> OPEN, 1 -> OPEN with
        // the bytes read recorded.
        FileFutexHelper lFutexHelper(mFutex, __FUNCTION__);


        CGS_ASSERT_STREAM(meFileState != E_FILESTATE_CLOSED, "Incorrect file state\n");
        CGS_ASSERT_STREAM(muPendingOperationCount > 0, "Received file event with 0 pending operations\n");

        switch (lpOperator->GetStatus(EA::Thread::kTimeoutImmediate))
        {
            case -2:
                meFileState = E_FILESTATE_ERROR;
                break;
            case -1:
                meFileState = E_FILESTATE_OPEN;
                break;
            case 1:
                muLastOperationSize = lpOperator->GetResultSize();
                meFileState         = E_FILESTATE_OPEN;
                break;
            default:




















                CGS_FAIL_STREAM("Unhandled response: " << lpOperator->GetStatus(EA::Thread::kTimeoutImmediate) << "\n");
                break;
        }

        --muPendingOperationCount;
        FreeAsyncOp(lpOperator);
    }










    void File::OnWrite(AsyncOp* lpOperator)
    {
        // OnWrite @0x828DCBA0 -- device write completion (same folding as OnRead).
        FileFutexHelper lFutexHelper(mFutex, __FUNCTION__);


        CGS_ASSERT_STREAM(meFileState != E_FILESTATE_CLOSED, "Incorrect file state\n");
        CGS_ASSERT_STREAM(muPendingOperationCount > 0, "Received file event with 0 pending operations\n");

        switch (lpOperator->GetStatus(EA::Thread::kTimeoutImmediate))
        {
            case -2:
                meFileState = E_FILESTATE_ERROR;
                break;
            case -1:
                meFileState = E_FILESTATE_OPEN;
                break;
            case 1:
                muLastOperationSize = lpOperator->GetResultSize();
                meFileState         = E_FILESTATE_OPEN;
                break;
            default:










                CGS_FAIL_STREAM("Unhandled response: " << lpOperator->GetStatus(EA::Thread::kTimeoutImmediate) << "\n");
                break;
        }

        --muPendingOperationCount;
        FreeAsyncOp(lpOperator);
    }










    void File::OnClose(AsyncOp* lpOperator)
    {
        // OnClose @0x828E8868 -- -2 -> ERROR, -1 -> OPEN, 1 -> closed (no access mode, no handle).
        FileFutexHelper lFutexHelper(mFutex, __FUNCTION__);


        CGS_ASSERT_STREAM(meFileState != E_FILESTATE_CLOSED, "Incorrect file state\n");
        CGS_ASSERT_STREAM(muPendingOperationCount > 0, "Received file event with 0 pending operations\n");

        switch (lpOperator->GetStatus(EA::Thread::kTimeoutImmediate))
        {
            case -2:
                meFileState = E_FILESTATE_ERROR;
                break;
            case -1:
                meFileState = E_FILESTATE_OPEN;
                break;
            case 1:
                meFileAccess = E_FILEACCESS_CLOSED;
                mpHandle     = 0;
                meFileState  = E_FILESTATE_CLOSED;
                break;
            default:















                CGS_FAIL_STREAM("Unhandled response: " << lpOperator->GetStatus(EA::Thread::kTimeoutImmediate) << "\n");
                break;
        }

        --muPendingOperationCount;
        FreeAsyncOp(lpOperator);
    }










    void File::OpenCallback(AsyncOp* lpOperator)
    {
        File* lpFile = static_cast<File*>(lpOperator->GetContext());
        lpFile->OnOpen(lpOperator);
    }










    void File::ReadCallback(AsyncOp* lpOperator)
    {
        File* lpFile = static_cast<File*>(lpOperator->GetContext());
        lpFile->OnRead(lpOperator);
    }










    void File::WriteCallback(AsyncOp* lpOperator)
    {
        File* lpFile = static_cast<File*>(lpOperator->GetContext());
        lpFile->OnWrite(lpOperator);
    }










    void File::CloseCallback(AsyncOp* lpOperator)
    {
        File* lpFile = static_cast<File*>(lpOperator->GetContext());
        lpFile->OnClose(lpOperator);
    }

} // namespace CgsFileSystem
