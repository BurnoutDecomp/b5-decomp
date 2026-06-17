#include <Windows.h>
#include "SDKs/EATech/eathread/eathread_pc_dynamicthreadarray.h"
#include "eathread/eathread.h"

// Walks the fixed 24-entry handle table. For each live handle, closes and clears
// it when bCloseAll is set, or when the thread can no longer be queried / is no
// longer STILL_ACTIVE (exit code != 259). Guarded by mCriticalSection.
//
// X360 NOTE: unlike the modern upstream copy, this build does NOT call
// Initialize() first -- it assumes the critical section is already constructed.
void DynamicThreadArray::CheckDynamicThreadArray(bool bCloseAll)
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
void DynamicThreadArray::AddDynamicThreadHandle(HANDLE hThread, bool bAdd)
{
    if (hThread)
    {
        EnterCriticalSection(&mCriticalSection);

        if (bAdd)
        {
            size_t  luIndex = 0;
            HANDLE* lpSlot  = mhDynamicThreadArray;
            while (*lpSlot)
            {
                ++luIndex;
                ++lpSlot;
                if (luIndex >= kMaxThreadDynamicArrayCount)
                    goto Done; // table full: leave hThread non-null so it is closed below
            }

            mhDynamicThreadArray[luIndex] = hThread;
            hThread = 0; // stored successfully -- suppresses the CloseHandle below

        Done:
            if (hThread)
                CloseHandle(hThread); // could not store it; close to match the DuplicateHandle that created it
        }
        else
        {
            size_t  luIndex = 0;
            HANDLE* lpSlot  = mhDynamicThreadArray;
            while (*lpSlot != hThread)
            {
                ++luIndex;
                ++lpSlot;
                if (luIndex >= kMaxThreadDynamicArrayCount)
                    goto Leave; // not found: by design not an error
            }

            CloseHandle(hThread);
            mhDynamicThreadArray[luIndex] = 0;
        }

    Leave:
        LeaveCriticalSection(&mCriticalSection);
    }
}
