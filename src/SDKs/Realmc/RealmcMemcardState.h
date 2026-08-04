#pragma once

// ===========================================================================
// SDKs/Realmc/RealmcMemcardState.h
//
// RealmcCore::MemcardState -- the memory-card task state machine. It serialises
// (behind one EA::Thread::Mutex) the small pile of state the async card worker
// tracks: the current autosave/monitor flags, a LIFO stack of running task-type
// ids (`main` = bottom, `current` = top), a FIFO queue of task ids waiting to
// start, two pending-message bitmask words, and the RealmcCore::MessageFilter the
// card interface installs. RealmcCore::IRunnableTask (the XenonRunnableTask family)
// dispatches into these entry points to report Start/Stop and to drive the
// waiting queue.
//
// Grounded purely from the X360 pseudocode + asm (BURNOUT_X360_ARTIST.XEX): no
// Feb-2007 leak source and no DWARF for this TU. `Realmc` is a vendor library
// boundary, so its identifiers (RealmcCore, MemcardState, StartTask, ...) are
// preserved verbatim per the naming convention.
//
// LAYOUT (X360 byte offsets from the per-method store/load displacements; the
// three embedded objects use their own reconstructed homes and are reached BY
// NAME -- the X360-absolute offsets below are documentation, NOT host asserts,
// because host pointers are 8 bytes wide):
//   +0x00  mpMutex            EA::Thread::Mutex*   -- *(this+0) is the Mutex the
//                             ctor stores and every method Lock/Unlocks.
//   +0x04  mbAutosaveState    bool  (byte)         -- Get/SetAutosaveState.
//   +0x08  miMonitorState     int                  -- Get/SetMonitorState.
//   +0x0C  maTaskStack        IntVector (vector<int>) -- running task-type stack.
//   +0x1C  maStartWaitingQueue Allocator64 (deque<int>) -- tasks waiting to start.
//   +0x48  muMessageSet       uint32  -- pending "set" message bitmask.
//   +0x4C  muMessageClear     uint32  -- pending "clear" message bitmask.
//   +0x50  mpMessageFilter    MessageFilter*       -- the installed message filter
//                             (ctor builds one whose handler is this MemcardState;
//                             SetMessageFilter swaps it; the dtor releases it).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/eathread/eathread_mutex.h"  // EA::Thread::Mutex
#include "SDKs/Realmc/RealmcContainers.h"          // RealmcCore::IntVector (embedded task stack)
#include "SDKs/Realmc/RealmcAllocator64.h"         // RealmcCore::Allocator64 (embedded start-waiting queue)
#include "SDKs/Realmc/RealmcCore.h"                // RealmcCore::MessageFilter (the +0x50 filter)

namespace RealmcCore
{

class MemcardState
{
public:
    // @ 0x82C47328 -- BLOCKED (honest gap, NOT fabricated). The ctor stores the
    //   mutex, zeroes the flags/masks, default-constructs the deque at +0x1C
    //   (X360 `Rea` -- the Allocator64 deque constructor, un-homed / truncated
    //   symbol), builds a RealmcCore::MessageFilter into +0x50 (AddRefing the
    //   default message held in the global off_832BE1F4, which is not declared),
    //   and reserves 10 slots on the task stack. The deque constructor and the
    //   off_832BE1F4 default-message global are un-homed collaborators, so the
    //   ctor cannot be reproduced faithfully yet -- declared only. STILL TRUE as of
    //   wave L: the dtor beside it was unblocked, but this ctor was NOT, and no
    //   definition exists anywhere in the tree. Verified by the wave-L close-out.
    explicit MemcardState(EA::Thread::Mutex* pMutex);

    // @ 0x82C46470 -- DEFINED, in RealmcMemcardState_wL_02.cpp. Do not add a second
    //   definition, and do not restore the old "BLOCKED / declared only" text below it:
    //   the deque destructor it named as an un-homed blocker was homed in wave L (its
    //   symbol was merely TRUNCATED in the export, not absent). Releases the message
    //   filter, drains and destroys the start-waiting deque, and frees the task-stack
    //   buffer through g_pRealmcAllocator's vtable slot.
    ~MemcardState();

    // @ 0x82C44D88 -- read the autosave flag (+0x04) under the lock.
    bool GetAutosaveState();

    // @ 0x82C44DE0 -- write the autosave flag (+0x04) under the lock.
    int SetAutosaveState(bool bState);

    // @ 0x82C44E20 -- read the monitor state (+0x08) under the lock.
    int GetMonitorState();

    // @ 0x82C44E78 -- write the monitor state (+0x08) under the lock.
    int SetMonitorState(int iState);

    // @ 0x82C44EB8 -- update the pending-message bitmasks under the lock. iMode 1
    //   sets iMask in muMessageSet (and clears it from muMessageClear); iMode 2
    //   sets it in muMessageClear; any other mode clears it from muMessageClear.
    //   In every case iMask is cleared from the opposite mask so a message is
    //   pending in at most one direction.
    int SetMessage(int iMode, int iMask);

    // @ 0x82C44FA8 -- install pNewFilter as the message filter (+0x50), deleting
    //   the previously installed one (its vtable slot +0 deleting destructor).
    //   Returns the previous filter pointer (the X360 returns the deleting-dtor
    //   result, i.e. the old block).
    MessageFilter* SetMessageFilter(MessageFilter* pNewFilter);

    // @ 0x82C45288 -- the `main' (bottom-of-stack) running task, or 0 when idle.
    int GetMainTask();

    // @ 0x82C45218 -- the `current' (top-of-stack) running task, or 0 when idle.
    int GetCurrentTask();

    // @ 0x82C47420 -- push iTaskType as the now-running task. Returns the Unlock
    //   result (propagated by IRunnableTask::Starting).
    int StartTask(int iTaskType);

    // @ 0x82C452F8 -- pop the top running task. iTaskType is accepted for the
    //   IRunnableTask call site but ignored by the body (the X360 pops
    //   unconditionally).
    int StopTask(int iTaskType);

    // @ 0x82C47490 -- replace the top running task: pop, then push iStartTaskType.
    //   iStopTaskType is accepted for the call site but ignored by the body.
    int StopAndStartTask(int iStopTaskType, int iStartTaskType);

    // @ 0x82C46528 -- dequeue and return the next task waiting to start (front of
    //   the FIFO), or 0 when the queue is empty.
    int GetWaitingToStartTask();

    // @ 0x82C468B8 -- enqueue iTask at the back of the start-waiting FIFO when it
    //   is non-zero. Returns the Unlock result.
    int PutInStartWaitingQueue(int iTask);

private:
    EA::Thread::Mutex* mpMutex;             // +0x00
    bool               mbAutosaveState;     // +0x04
    int                miMonitorState;      // +0x08
    IntVector          maTaskStack;         // +0x0C  running task-type stack
    Allocator64        maStartWaitingQueue; // +0x1C  tasks waiting to start
    std::uint32_t      muMessageSet;        // +0x48
    std::uint32_t      muMessageClear;      // +0x4C
    MessageFilter*     mpMessageFilter;     // +0x50
};

} // namespace RealmcCore
