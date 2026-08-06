#include "SDKs/Realmc/RealmcMemcardState.h"

#include <new>   // placement new (the ctor's backend-allocated MessageFilter)

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
// WHERE THE SPECIAL MEMBERS LIVE (one definition each -- do NOT add a second
// definition anywhere; `cl /c` cannot see an ODR clash across TUs):
//   MemcardState  @ 0x82C47328 -- DEFINED in THIS file (landed wave N; formerly
//     parked at scratchpad/waveL/parked/RealmcMemcardState_02_MemcardState.cpp).
//   ~MemcardState @ 0x82C46470 -- DEFINED in RealmcMemcardState_wL_02.cpp.
//
// The ctor's former blockers are all closed: the truncated export symbols
// `Rea` / `Re` were identified by xref in wave L as RealmcCore::Allocator64's
// constructor @ 0x82C45B48 and destructor @ 0x82C45A10 (see
// scratchpad/waveL/MemcardState.spec.md section 1), both now declared in
// RealmcAllocator64.h with bodies in RealmcAllocator64.cpp, and the
// off_832BE1F4 default-message global needs no declaration here -- the ctor
// reaches it by calling the already-homed MessageFilter(this) constructor
// (RealmcCore.cpp), which OWNS the known modelling divergence on that global
// (wave-L spec section 4, defect 3: it binds MessagePtr::EMPTY_MESSAGE(), i.e.
// off_832BE1F0, where the asm reads *(off_832BE1F4)+4). Calling it instead of
// re-inlining keeps that divergence in the one place that owns it.
// ===========================================================================

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// @ 0x82C47328 -- construct the memory-card task state machine around the mutex
// the RealmcIface::MemcardInterfaceImpl ctor @ 0x82B522D0 hands it (r4 there is
// a freshly constructed EA::Thread::Mutex* -- verified in that asm). Member
// initialiser order == member declaration order == the X360 store order.
//
// MEASURED, raw asm (X360 console offsets in comments; host reaches every
// member BY NAME -- gotcha 1):
//   0x82C47348  stw r4, 0(r31)          -> mpMutex = pMutex
//   0x82C47350  stb r30(=0), 4(r31)     -> mbAutosaveState = false   (BYTE store)
//   0x82C47354  stw r30, 8(r31)         -> miMonitorState = 0
//   0x82C47358/5C/60  stw r30, 0/4/8(r29=this+0xC)
//                                       -> maTaskStack{begin,end,capEnd} = 0
//                                          (the inlined IntVector ctor)
//   0x82C47364  bl Rea(r3=this+0x1C, r4=0)
//                                       -> maStartWaitingQueue(0u); the r5 =
//                                          &<1-byte stack temp> third argument is
//                                          the stateless allocator temporary --
//                                          never read by Rea/DoInit, dropped on
//                                          the host (measured on the Allocator64
//                                          ctor declaration in RealmcAllocator64.h)
//   0x82C4736C/70/78  stw r30, 0x48/0x4C/0x50(r31)
//                                       -> muMessageSet = muMessageClear = 0,
//                                          mpMessageFilter = nullptr
//   0x82C47388..9C  r3 = *off_832BE204; vtbl slot +4 Allocate(0x10, 0,0,0,0)
//                                       -> AllocateMem(nullptr, sizeof(MessageFilter))
//                                          (0x10 == the CONSOLE sizeof; the host
//                                           size comes from sizeof)
//   0x82C473A0/A4   cmplwi r3,0 / beq   -> the null guard on the allocation
//   0x82C473AC..0x82C47400             -> MessageFilter::MessageFilter @ 0x82C45F08
//                                          inlined by the compiler (final vtable,
//                                          mpHandler = this, MessagePtr bind +
//                                          interrupt-masked AddRef of the default
//                                          message *(off_832BE1F4)+4) -- reproduced
//                                          by CALLING the homed ctor, see the file
//                                          header note
//   0x82C47408      stw r30, 0x50(r31)  -> mpMessageFilter = filter-or-null (r30
//                                          is 0 on the alloc-failure path)
//   0x82C47404/0C/10  li r4,0xA; r3=this+0xC; bl reserve
//                                       -> maTaskStack.reserve(10)
//   0x82C47414      mr r3, r31          -> returns this (implicit)
// ---------------------------------------------------------------------------
MemcardState::MemcardState(EA::Thread::Mutex* pMutex)
    : mpMutex(pMutex)
    , mbAutosaveState(false)
    , miMonitorState(0)
    , maTaskStack()
    , maStartWaitingQueue(0u)   // bl Rea(this + 0x1C, 0) -- capacity-0 deque
    , muMessageSet(0)
    , muMessageClear(0)
    , mpMessageFilter(nullptr)
{
    // X360: backend->Allocate(0x10 /* console sizeof(MessageFilter) */, 0,0,0,0)
    // through vtable slot +4, which is exactly what AllocateMem(tag, size) does;
    // the tag register is zero at the call site, hence nullptr. The block is then
    // null-guarded and placement-constructed as a bare MessageFilter whose
    // handler is this MemcardState (the compiler inlined
    // MessageFilter::MessageFilter @ 0x82C45F08 here). Same repo idiom as
    // RealmcMemcardInterface.cpp and RealmcIfaceXenonMessageFilter.cpp.
    void* lpMem = AllocateMem(nullptr, sizeof(MessageFilter));
    mpMessageFilter = lpMem ? new (lpMem) MessageFilter(this) : nullptr;

    maTaskStack.reserve(10);   // X360 0x82C47404..0x82C47410 (li r4, 0xA)
}

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
