#include "SDKs/Realmc/RealmcMemcardState.h"

// ===========================================================================
// RealmcCore::MemcardState -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES come from the X360 pseudocode +
// asm. See RealmcMemcardState.h for the per-offset layout map. Every method
// serialises on the embedded EA::Thread::Mutex (stored as a pointer at +0x00);
// the &unk_821BA1E4 the X360 passes to Mutex::Lock is the kTimeoutNone ThreadTime
// default argument, reproduced by calling Lock() with no explicit timeout.
//
// The task stack (IntVector @ +0x0C) and the start-waiting queue (Allocator64
// deque @ +0x1C) expose the element-level front/back/push/pop helpers this file
// drives; the inlined container arithmetic itself is homed in those classes so
// MemcardState reaches them by NAME, not by raw offset.
//
// WHERE THE SPECIAL MEMBERS LIVE (this file carries none of them -- do NOT add
// a second definition here; `cl /c` cannot see an ODR clash across TUs):
//   ~MemcardState @ 0x82C46470 -- DEFINED in RealmcMemcardState_wL_02.cpp.
//   MemcardState  @ 0x82C47328 -- still declared-only in the header. The body is
//     complete and asm-faithful but parked out of tree at
//     scratchpad/waveL/parked/RealmcMemcardState_02_MemcardState.cpp.
//
// The original blocker text here named the deque ctor/dtor `Rea` / `Re` as
// "un-homed truncated symbols". That is now OUT OF DATE and was deleted rather
// than left standing: both were identified by xref in wave L as
// RealmcCore::Allocator64's constructor @ 0x82C45B48 and destructor @ 0x82C45A10
// (see scratchpad/waveL/MemcardState.spec.md section 1), and both are now
// declared in RealmcAllocator64.h with bodies in RealmcAllocator64.cpp -- so the
// declaration blocker the parked ctor was waiting on no longer exists.
//
// The one remaining honest gap on the ctor is a MODELLING gap, not a missing
// symbol: it AddRefs the default message held in the global off_832BE1F4, whose
// modelling divergence is owned by RealmcCore.cpp (spec section 4, defect 3).
// ===========================================================================

namespace RealmcCore
{

// @ 0x82C44D88 -- read the autosave flag under the lock.
bool MemcardState::GetAutosaveState()
{
    mpMutex->Lock();
    const bool bState = mbAutosaveState;
    mpMutex->Unlock();
    return bState;
}

// @ 0x82C44DE0 -- write the autosave flag under the lock.
int MemcardState::SetAutosaveState(bool bState)
{
    mpMutex->Lock();
    mbAutosaveState = bState;
    return mpMutex->Unlock();
}

// @ 0x82C44E20 -- read the monitor state under the lock.
int MemcardState::GetMonitorState()
{
    mpMutex->Lock();
    const int iState = miMonitorState;
    mpMutex->Unlock();
    return iState;
}

// @ 0x82C44E78 -- write the monitor state under the lock.
int MemcardState::SetMonitorState(int iState)
{
    mpMutex->Lock();
    miMonitorState = iState;
    return mpMutex->Unlock();
}

// @ 0x82C44EB8 -- update the pending-message bitmasks under the lock.
int MemcardState::SetMessage(int iMode, int iMask)
{
    mpMutex->Lock();
    if (iMode == 1)
    {
        muMessageSet   |= static_cast<std::uint32_t>(iMask);
        muMessageClear &= ~static_cast<std::uint32_t>(iMask);
    }
    else
    {
        if (iMode == 2)
            muMessageClear |= static_cast<std::uint32_t>(iMask);
        else
            muMessageClear &= ~static_cast<std::uint32_t>(iMask);
        muMessageSet &= ~static_cast<std::uint32_t>(iMask);
    }
    return mpMutex->Unlock();
}

// @ 0x82C44FA8 -- install a new message filter, deleting the previous one.
//   Not lock-guarded in the X360 (it runs at construction time). The deleting
//   destructor (vtable slot +0 with the delete flag) is reproduced by `delete`;
//   the previous pointer is returned exactly as the X360 leaves it in r3.
MessageFilter* MemcardState::SetMessageFilter(MessageFilter* pNewFilter)
{
    MessageFilter* pOld = mpMessageFilter;
    if (pOld)
        delete pOld;
    mpMessageFilter = pNewFilter;
    return pOld;
}

// @ 0x82C45288 -- the `main' (bottom-of-stack) running task.
int MemcardState::GetMainTask()
{
    mpMutex->Lock();
    const int iTask = maTaskStack.GetFront();
    mpMutex->Unlock();
    return iTask;
}

// @ 0x82C45218 -- the `current' (top-of-stack) running task.
int MemcardState::GetCurrentTask()
{
    mpMutex->Lock();
    const int iTask = maTaskStack.GetBack();
    mpMutex->Unlock();
    return iTask;
}

// @ 0x82C47420 -- push iTaskType as the now-running task.
int MemcardState::StartTask(int iTaskType)
{
    mpMutex->Lock();
    maTaskStack.push_back(iTaskType);
    return mpMutex->Unlock();
}

// @ 0x82C452F8 -- pop the top running task (iTaskType ignored, as in the X360).
int MemcardState::StopTask(int /*iTaskType*/)
{
    mpMutex->Lock();
    maTaskStack.pop_back();
    return mpMutex->Unlock();
}

// @ 0x82C47490 -- replace the top running task (iStopTaskType ignored, as in the
//   X360): pop the current top, then push iStartTaskType.
int MemcardState::StopAndStartTask(int /*iStopTaskType*/, int iStartTaskType)
{
    mpMutex->Lock();
    maTaskStack.pop_back();
    maTaskStack.push_back(iStartTaskType);
    return mpMutex->Unlock();
}

// @ 0x82C46528 -- dequeue the next task waiting to start (front of the FIFO).
int MemcardState::GetWaitingToStartTask()
{
    mpMutex->Lock();
    const int iTask = maStartWaitingQueue.PopFront();
    mpMutex->Unlock();
    return iTask;
}

// @ 0x82C468B8 -- enqueue iTask at the back of the start-waiting FIFO when set.
int MemcardState::PutInStartWaitingQueue(int iTask)
{
    mpMutex->Lock();
    if (iTask)
        maStartWaitingQueue.PushBack(iTask);
    return mpMutex->Unlock();
}

} // namespace RealmcCore
