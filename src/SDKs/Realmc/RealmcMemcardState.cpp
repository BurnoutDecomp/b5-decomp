#include "SDKs/Realmc/RealmcMemcardState.h"

#include <new>   // placement new (the ctor's backend-allocated MessageFilter)

// ===========================================================================
// RealmcCore::MemcardState -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No reference source / no DWARF: SHAPE and BODIES come from the X360 pseudocode +
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
    , mpMessageFilter(0)
{
    // X360: backend->Allocate(0x10 /* console sizeof(MessageFilter) */, 0,0,0,0)
    // through vtable slot +4, which is exactly what AllocateMem(tag, size) does;
    // the tag register is zero at the call site, hence nullptr. The block is then
    // null-guarded and placement-constructed as a bare MessageFilter whose
    // handler is this MemcardState (the compiler inlined
    // MessageFilter::MessageFilter @ 0x82C45F08 here). Same repo idiom as
    // RealmcMemcardInterface.cpp and RealmcIfaceXenonMessageFilter.cpp.
    void* lpMem = AllocateMem(0, sizeof(MessageFilter));
    mpMessageFilter = lpMem ? new (lpMem) MessageFilter(this) : 0;

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
        muMessageSet   |= static_cast<uint32_t>(iMask);
        muMessageClear &= ~static_cast<uint32_t>(iMask);
    }
    else
    {
        if (iMode == 2)
            muMessageClear |= static_cast<uint32_t>(iMask);
        else
            muMessageClear &= ~static_cast<uint32_t>(iMask);
        muMessageSet &= ~static_cast<uint32_t>(iMask);
    }
    return mpMutex->Unlock();
}

// Read the hidden-message mask under the lock: lock, load +0x48, unlock,
//   return the loaded word.
uint32_t MemcardState::GetHiddenMessages()
{
    mpMutex->Lock();
    const uint32_t luMask = muMessageSet;
    mpMutex->Unlock();
    return luMask;
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

// Dequeue the next task waiting to start (front of the FIFO), under the lock.
IRunnableTask* MemcardState::GetWaitingToStartTask()
{
    mpMutex->Lock();
    IRunnableTask* const pTask = maStartWaitingQueue.PopFront();
    mpMutex->Unlock();
    return pTask;
}

// Enqueue pTask at the back of the start-waiting FIFO when set, under the lock.
int MemcardState::PutInStartWaitingQueue(IRunnableTask* pTask)
{
    mpMutex->Lock();
    if (pTask)
        maStartWaitingQueue.PushBack(pTask);
    return mpMutex->Unlock();
}

} // namespace RealmcCore

// FOLDED FROM RealmcMemcardState_wL_02.cpp
#include "SDKs/Realmc/RealmcMemcardState.h"

// ===========================================================================
// SDKs/Realmc/RealmcMemcardState_wL_02.cpp
//
// Wave-L part-file for RealmcCore::MemcardState -- carries the special members
// that RealmcMemcardState.cpp left declared-only. Reconstructed from the raw
// X360 asm (BURNOUT_X360_ARTIST.XEX); no LegacySrc reference source and no DWARF
// exists for this TU, so every claim below is asm-measured or explicitly
// labelled inference.
//
// SCOPE OF THIS FILE
//   landed here : RealmcCore::MemcardState::~MemcardState  @ 0x82C46470
//   NOT here    : RealmcCore::MemcardState::MemcardState   @ 0x82C47328 --
//                 DEFINED in RealmcMemcardState.cpp (landed wave N, from the
//                 formerly parked body). Do not add a second definition here.
//
// STATE OF THE FORMER BLOCKERS (fix pass): the `Rea` / `Re` truncated symbols
// are RealmcCore::Allocator64's ctor @ 0x82C45B48 and dtor @ 0x82C45A10 (see
// scratchpad/waveL/MemcardState.spec.md §1). Both are now declared in
// RealmcAllocator64.h with bodies in RealmcAllocator64.cpp, and the stale
// blocker text in RealmcMemcardState.cpp has been rewritten. RealmcMemcardState.h
// still carries its original per-member blocker comments -- including a claim
// that ~MemcardState is "declared only", which THIS FILE contradicts; that
// header was out of this pass's edit scope and is reported for the conductor.
// ===========================================================================

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// @ 0x82C46470 -- RealmcCore::MemcardState::~MemcardState
// (sole X360 caller: RealmcIface::MemcardInterfaceImpl::~MemcardInterfaceImpl
//  @ 0x82B52CF0, which embeds a MemcardState at its +0x14C.)
//
// MEASURED, store for store, from the raw asm:
//   0x82C46484  lwz  r3, 0x50(r31)          ; mpMessageFilter
//   0x82C46488  cmplwi r3, 0 / beq ...      ; the null guard reproduced below
//   0x82C46490  lwz r11,0(r3); lwz r11,0(r11); li r4,1; bctrl
//                                           ; vtable slot +0 with the delete
//                                           ; flag == the `vector deleting
//                                           ; destructor' == C++ `delete'
//   0x82C464A4..D8                          ; the inlined deque element-destroy
//                                           ; walk over maStartWaitingQueue: it
//                                           ; only LOADS (front cursor +0x08 and
//                                           ; +0x10, back cursor +0x18, slot
//                                           ; array +0x14, page stride 0x100) and
//                                           ; stores nothing -- the trivial
//                                           ; ~int destructor loop the compiler
//                                           ; emitted and then could not elide.
//                                           ; It has no observable effect, so no
//                                           ; C++ source line corresponds to it.
//   0x82C464DC  bl Re                       ; ~Allocator64(maStartWaitingQueue)
//   0x82C464E0..510                         ; the inlined ~IntVector: when
//                                           ; mpBegin != 0, backend vtable +0xC
//                                           ; Free(mpBegin, (mpCapEnd - mpBegin)
//                                           ; * 4) -- CAPACITY bytes, console
//                                           ; element width 4.
//
// Everything after the filter delete is compiler-emitted member teardown, run in
// reverse declaration order (maStartWaitingQueue, then maTaskStack) -- exactly
// the order the X360 runs it in. So the C++ body is just the filter release.
//
// FORMER CAVEAT, NOW CLOSED: when this body was written neither
// RealmcCore::Allocator64 nor RealmcCore::IntVector declared a destructor, so the
// compiler-emitted member teardown was trivial and the two heap blocks the X360
// frees (the deque pages, page-pointer array, and task-stack buffer) were left allocated.
// Both destructors now exist -- `~Allocator64` in RealmcAllocator64.h/.cpp (the
// 0x82C45A10 body) and `~IntVector` inline in RealmcContainers.h (the teardown
// inlined at 0x82C464E0..510) -- so the implicit member teardown emitted here
// releases both blocks, matching the X360. No change to the body below was
// required; that is the point of expressing the teardown as member destructors.
//
// SECOND CAVEAT: `delete` on a bare RealmcCore::MessageFilter currently routes to
// the host global operator delete, while its storage came from the Realmc backend
// (the X360 `vector deleting destructor' @ 0x82C46240 frees 0x10 bytes back
// through the backend). Shared-header request R3 -- a class-scope sized
// `operator delete` on MessageFilter -- fixes that with no call-site change. The
// same latency already exists in the committed SetMessageFilter @ 0x82C44FA8.
// ---------------------------------------------------------------------------
MemcardState::~MemcardState()
{
    if (mpMessageFilter)
        delete mpMessageFilter;   // X360: vtbl[+0](filter, 1) -- deleting dtor
}

} // namespace RealmcCore

// FORMERLY PARKED, NOW LANDED -- RealmcCore::MemcardState::MemcardState
// @ 0x82C47328 was moved from scratchpad/waveL/parked/ into
// RealmcMemcardState.cpp in wave N, once wave L's fix pass had landed its
// prerequisites (`explicit Allocator64(unsigned int)` + `~Allocator64` in
// RealmcAllocator64.{h,cpp}, `IntVector()` / `~IntVector()` in
// RealmcContainers.h, and MessageFilter's class-scope sized `operator delete`
// in RealmcCore.h). All 15 ledger functions of this TU now have bodies.
// ===========================================================================
