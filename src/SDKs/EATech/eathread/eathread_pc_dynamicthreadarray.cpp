// XDK size optimization matches the compact loop and tests at ARTIST 0x82B42328.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include <xtl.h>
#include "SDKs/EATech/eathread/eathread_pc_dynamicthreadarray.h"
#include "eathread/eathread.h"

// Walks the fixed 24-entry handle table. For each live handle, closes and clears
// it when bCloseAll is set, or when the thread can no longer be queried / is no
// longer STILL_ACTIVE (exit code != 259). Guarded by mCriticalSection.
//
// X360 NOTE: unlike the modern upstream copy, this build does NOT call
// Initialize() first -- it assumes the critical section is already constructed.
void EA::Thread::DynamicThreadArray::CheckDynamicThreadArray(bool bCloseAll)
{
    EnterCriticalSection(&mCriticalSection);

    HANDLE* lpHandle = mhDynamicThreadArray;
    for (size_t luRemaining = kMaxThreadDynamicArrayCount; luRemaining != 0; --luRemaining, ++lpHandle)
    {
        HANDLE lhThread = *lpHandle;
        if (lhThread)
        {
            DWORD ldwExitCode = 0;

            // If the thread id is invalid or it has exited, drop our reference.
            if (bCloseAll || !GetExitCodeThread(lhThread, &ldwExitCode) || (ldwExitCode != STILL_ACTIVE))
            {
                CloseHandle(*lpHandle);
                *lpHandle = 0;
            }
        }
    }

    LeaveCriticalSection(&mCriticalSection);
}

// bAdd==true: insert hThread into the first free (null) slot of the 24-entry
// table; if the table is full, the handle could not be stored and is closed.
// bAdd==false: find hThread in the table, close it, and clear the slot.
// Guarded by mCriticalSection. A null hThread is a no-op.
//
// X360 NOTE: this build does NOT call Initialize() first (see class comment).
void EA::Thread::DynamicThreadArray::AddDynamicThreadHandle(HANDLE hThread, bool bAdd)
{
    if (!hThread)
        return;

    EnterCriticalSection(&mCriticalSection);
    if (bAdd)
    {
        for (size_t luIndex = 0; luIndex < kMaxThreadDynamicArrayCount; ++luIndex)
        {
            if (!mhDynamicThreadArray[luIndex])
            {
                mhDynamicThreadArray[luIndex] = hThread;
                hThread = 0;
                break;
            }
        }
        if (hThread)
            CloseHandle(hThread);
    }
    else
    {
        for (size_t luIndex = 0; luIndex < kMaxThreadDynamicArrayCount; ++luIndex)
        {
            if (mhDynamicThreadArray[luIndex] == hThread)
            {
                CloseHandle(hThread);
                mhDynamicThreadArray[luIndex] = 0;
                break;
            }
        }
    }
    LeaveCriticalSection(&mCriticalSection);
}
