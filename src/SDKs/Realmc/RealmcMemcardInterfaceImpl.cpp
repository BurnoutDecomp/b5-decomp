// ===========================================================================
// RealmcIface::MemcardInterfaceImpl -- the concrete memory-card interface and
// the MemcardInterface::CreateInstance factory.
// ===========================================================================

#include "SDKs/Realmc/RealmcMemcardInterfaceImpl.h" // RealmcIface::MemcardInterfaceImpl (the concrete impl)
#include "SDKs/Realmc/RealmcCore.h"                 // RealmcCore::GetMemAllocator / AllocateMem / EA::Allocator::ICoreAllocator
#include "SDKs/Realmc/RealmcObjectManager.h"        // RealmcCore::ObjectManager::Initialize
#include "SDKs/Realmc/RealmcLocale.h"               // RealmcCore::Locale::SetLocaleGetStrCallback
#include "SDKs/Realmc/RealmcCoreMessageClear.h"
#include "SDKs/Realmc/RealmcIfaceXenonMessageFilter.h"
#include "SDKs/Realmc/RealmcIfaceCheckCardTask.h"
#include "SDKs/Realmc/RealmcIfaceSetActiveCardTask.h"
#include "SDKs/Realmc/RealmcIfaceSaveCheckTask.h"
#include "SDKs/Realmc/RealmcIfaceSetAutosaveTask.h"
#include "SDKs/Realmc/RealmcIfaceBootupCheckTask.h"
#include "SDKs/Realmc/RealmcIfaceSaveTask.h"
#include "SDKs/Realmc/RealmcIfaceLoadTask.h"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"   // EA::Thread::ThreadParameters

#include <new>   // placement new -- the impl is constructed in the block CreateInstance allocates.

namespace RealmcIface
{

namespace
{
    // off_832500BC -- the process-wide singleton shared by CreateInstance and
    // Shutdown (ARTIST's global; not a function-local factory cache).
    MemcardInterface* s_pInstance = 0;
}

// @0x82B522D0. Construct the concrete implementation in ARTIST order: base,
// copied GameInfo, uninitialized mutex wrapper, worker pointer, queue, state
// machine, four CardData subobjects, then thread parameters. The body performs
// the native two-phase mutex init, replaces MemcardState's default filter with
// XenonMessageFilter, initializes the queue, creates the game callback processor,
// and finally initializes each E0-stride Xenon state.
MemcardInterfaceImpl::MemcardInterfaceImpl(EA::Thread::ThreadParameters* pThreadParams,
                                           IGameInterface* pGameInterface,
                                           const GameInfo& rGameInfo)
    : MemcardInterface()
    , mGameInfo(rGameInfo)
    , mMutex(0, false)
    , mpWorkerThread(0)
    , mMessageQueue()
    , mpCallbackProcessor(0)
    , mMemcardState(&mMutex)
    , mpThreadParams(pThreadParams)
{
    EA::Thread::MutexParameters lMutexParameters(true, 0);
    mMutex.Init(&lMutexParameters);

    void* const lpFilterMemory = RealmcCore::AllocateMem(
        0, sizeof(XenonMessageFilter));
    XenonMessageFilter* const lpFilter = lpFilterMemory
        ? new (lpFilterMemory) XenonMessageFilter(&mMemcardState)
        : 0;
    mMemcardState.SetMessageFilter(lpFilter);

    mMessageQueue.Initialize(-1);

    void* const lpProcessorMemory = RealmcCore::AllocateMem(
        0, sizeof(GameCallbackProcessor));
    mpCallbackProcessor = lpProcessorMemory
        ? new (lpProcessorMemory) GameCallbackProcessor(pGameInterface, &mMessageQueue)
        : 0;

    for (int i = 0; i < 4; ++i)
        XenonUtil::InitState(&maStates[i], mGameInfo.muField4C);
}

// @0x82B52CF0. Explicit shutdown work precedes the normal reverse-order C++
// destruction of the embedded MemcardState, MessageQueue, Mutex and base.
MemcardInterfaceImpl::~MemcardInterfaceImpl()
{
    mMessageQueue.ShutDown();
    CleanupThread();

    if (mpCallbackProcessor)
        delete mpCallbackProcessor;

    for (int i = 0; i < 4; ++i)
        XenonUtil::CloseState(&maStates[i]);
}

// @0x82B52420. Wait for the previous worker, destroy its Thread wrapper, free
// the four-byte EA::Thread::Thread object and clear the member pointer.
void MemcardInterfaceImpl::CleanupThread()
{
    if (!mpWorkerThread)
        return;

    mpWorkerThread->WaitForEnd(EA::Thread::kTimeoutNone, 0);
    mpWorkerThread->~Thread();
    RealmcCore::FreeMemSize(mpWorkerThread, 4);
    mpWorkerThread = 0;
}

// @0x82B52070. Serialize singleton removal with the native mutex, invoke the
// deleting destructor after unlocking, then finalize Realmc's shared objects.
void MemcardInterfaceImpl::Shutdown()
{
    mMutex.Lock();
    MemcardInterface* pOldInstance = s_pInstance;
    s_pInstance = 0;
    mMutex.Unlock();

    if (pOldInstance)
        delete pOldInstance;

    RealmcCore::ObjectManager::Finalize();
}

// @0x82B52480. Drain one initialized message queue entry, return the main task
// id, and when idle consume the current user's device-change event while
// autosave or monitoring is active.
int MemcardInterfaceImpl::Update(int /*liArg*/)
{
    if (mMessageQueue.IsInitialized())
    {
        RealmcCore::MessagePtr lMessage = mMessageQueue.GetMessage();
        lMessage.Apply(mpCallbackProcessor);
    }

    const int liMainTask = mMemcardState.GetMainTask();
    if (liMainTask == 0)
    {
        CleanupThread();

        if (gSignedUserIndex >= 0 &&
            (mMemcardState.GetAutosaveState() || mMemcardState.GetMonitorState() == 1))
        {
            XenonUtil::State& rState = maStates[gSignedUserIndex];
            if (!rState.mbFieldDA && XenonUtil::CheckCardChanged(&rState) == 1)
            {
                rState.mbFieldDA = 1;
                if (mMemcardState.GetAutosaveState() || mMemcardState.GetMonitorState() == 1)
                {
                    static RealmcCore::MessageClear sMessageClear;
                    mMemcardState.SetAutosaveState(false);
                    rState.mbFieldD8 = 0;
                    mMemcardState.SetMessage(0, 0x100);
                    static_cast<RealmcCore::IMessageProcessor*>(mpCallbackProcessor)
                        ->ProcessMessage(&sMessageClear);
                }
            }
        }
    }

    return liMainTask;
}

// @0x82B520E8.
bool MemcardInterfaceImpl::IsResettable()
{
    return mMemcardState.GetMainTask() == 0;
}

// @0x82B52118. This global is shared with XenonUtil::CheckState and the
// content-system helpers; the selected user's per-state +0xD8 byte seeds the
// MemcardState autosave flag.
int MemcardInterfaceImpl::SetSignedUserIndex(int liUserIndex)
{
    gSignedUserIndex = liUserIndex;
    return mMemcardState.SetAutosaveState(maStates[liUserIndex].mbFieldD8 != 0);
}

// @0x82B52600. Map the UI choice to a Response, then apply it to the held
// message through the game-thread callback processor.
void MemcardInterfaceImpl::MessageDone(int liChoice)
{
    int liResponseValue = (liChoice >= 1 && liChoice <= 4) ? liChoice : 0;
    void* lpMemory = RealmcCore::AllocateMem(0, sizeof(RealmcCore::Response));
    RealmcCore::Response* lpResponse = lpMemory
        ? new (lpMemory) RealmcCore::Response(liResponseValue)
        : 0;
    RealmcCore::ResponsePtr lResponse(lpResponse);
    lResponse.Apply(mpCallbackProcessor);
}

// @0x82B52138. The two incoming words pass directly to MemcardState::SetMessage.
int MemcardInterfaceImpl::SetMessageState(int liMode, int liMask)
{
    return mMemcardState.SetMessage(liMode, liMask);
}

// @0x82B52140.
int MemcardInterfaceImpl::SetMonitor(int liState)
{
    return mMemcardState.SetMonitorState(liState);
}

// @0x82B52E08. Create the native task, using the currently signed-in user's
// Xenon state, then route it through the shared queue/worker path.
int MemcardInterfaceImpl::CheckCard(int liArg)
{
    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(CheckCardTask));
    CheckCardTask* const lpTask = lpMemory
        ? new (lpMemory) CheckCardTask(&mMessageQueue, &mMemcardState,
                                      &maStates[gSignedUserIndex], &mGameInfo,
                                      static_cast<u32>(liArg), 0)
        : 0;
    return RunAsync(lpTask);
}

// @0x82B52D88. The native byte argument is preserved separately from the
// two integer fields carried by SetActiveCardTask.
int MemcardInterfaceImpl::SetActiveCard(int liField18, int liField24, int liField20)
{
    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(SetActiveCardTask));
    SetActiveCardTask* const lpTask = lpMemory
        ? new (lpMemory) SetActiveCardTask(&mMessageQueue, &mMemcardState,
                                          &maStates[gSignedUserIndex], &mGameInfo,
                                          liField18, 0, liField20,
                                          static_cast<char>(liField24))
        : 0;
    return RunAsync(lpTask);
}

// @0x82B52F18. SaveCheckTask takes its own deep copy of the caller's params.
int MemcardInterfaceImpl::SaveCheck(SaveCheckParams* lpParams)
{
    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(SaveCheckTask));
    SaveCheckTask* const lpTask = lpMemory
        ? new (lpMemory) SaveCheckTask(&mMessageQueue, &mMemcardState,
                                      &maStates[gSignedUserIndex], &mGameInfo,
                                      *lpParams)
        : 0;
    return RunAsync(lpTask);
}

// @0x82B53030. SetAutosaveTask owns a deep copy and performs its check through
// IRunnableTask::InvokeSynchronously on the same card-thread state.
int MemcardInterfaceImpl::SetAutosave(int liEnabled, SaveCheckParams* lpParams)
{
    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(SetAutosaveTask));
    SetAutosaveTask* const lpTask = lpMemory
        ? new (lpMemory) SetAutosaveTask(&mMessageQueue, &mMemcardState,
                                        &maStates[gSignedUserIndex], &mGameInfo,
                                        liEnabled, *lpParams)
        : 0;
    return RunAsync(lpTask);
}

// @0x82B52E90. The native wrapper allocates 0x15C bytes, passes the queue,
// MemcardState, signed user's Xenon state, GameInfo, SaveInfo, count, entries
// and TitleInfo to BootupCheckTask, then submits the task through RunAsync.
int MemcardInterfaceImpl::BootupCheck(SaveInfo* lpSaveInfo, int liNumEntries,
                                      LoadEntryInfo* lpEntries, TitleInfo* lpTitleInfo)
{
    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(BootupCheckTask));
    BootupCheckTask* const lpTask = lpMemory
        ? new (lpMemory) BootupCheckTask(&mMessageQueue, &mMemcardState,
                                        &maStates[gSignedUserIndex], &mGameInfo,
                                        *lpSaveInfo, liNumEntries, lpEntries,
                                        *lpTitleInfo)
        : 0;
    return RunAsync(lpTask);
}

// @0x82B52FA0. Pass the caller's complete flags word into SaveTask; the task
// owns the SaveInfo, TitleInfo and LoadEntryInfo copies as in ARTIST.
int MemcardInterfaceImpl::Save(SaveInfo* lpSaveInfo, int liCount,
                               LoadEntryInfo* lpEntries, int liFlags,
                               TitleInfo* lpTitleInfo)
{
    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(SaveTask));
    SaveTask* const lpTask = lpMemory
        ? new (lpMemory) SaveTask(&mMessageQueue, &mMemcardState,
                                  &maStates[gSignedUserIndex], &mGameInfo,
                                  *lpSaveInfo, liCount, lpEntries, liFlags,
                                  *lpTitleInfo)
        : 0;
    return RunAsync(lpTask);
}

// @0x82B530A8. Pass the caller's entry name, count, entry array, complete flags
// word and title into LoadTask, then hand the task to the shared RunAsync path.
int MemcardInterfaceImpl::Load(EntryContentName* lpEntryContentName,
                               int liNumEntries, LoadEntryInfo* lpEntries,
                               int liFlags, TitleInfo* lpTitleInfo)
{
    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(LoadTask));
    LoadTask* const lpTask = lpMemory
        ? new (lpMemory) LoadTask(&mMessageQueue, &mMemcardState,
                                  &maStates[gSignedUserIndex], &mGameInfo,
                                  *lpEntryContentName, liNumEntries, lpEntries,
                                  liFlags, *lpTitleInfo)
        : 0;
    return RunAsync(lpTask);
}

// ---------------------------------------------------------------------------
// MemcardInterface::CreateInstance -- the memory-card interface factory /
// singleton accessor:
//   1. install the caller's allocator backend         (GetMemAllocator)
//   2. install the caller's locale template callback   (SetLocaleGetStrCallback)
//   3. pick the worker's EA::Thread::ThreadParameters: the caller's override
//      when non-null, else a module-static default that is default-constructed
//      once and field-configured once with the "realmemcard_xenon" settings.
//   4. bring up the Realmc ObjectManager                (ObjectManager::Initialize())
//   5. lazily construct the single MemcardInterfaceImpl into a 0x528-byte block and
//      cache it; return the cached instance.
// ObjectManager::Initialize() takes no arguments (see RealmcObjectManager.h), so
// the earlier calls' return values are discarded.
// ---------------------------------------------------------------------------
MemcardInterface* MemcardInterface::CreateInstance(const CreateParams* pParams,
                                                   IGameInterface* pGameInterface, const GameInfo& rGameInfo)
{
    // Install the caller's allocator backend and locale template getter.
    RealmcCore::GetMemAllocator(pParams->mpAllocator);
    RealmcCore::Locale::SetLocaleGetStrCallback(pParams->mpfnGetStr);

    // Select the ThreadParameters the memory-card worker thread runs under.
    EA::Thread::ThreadParameters* pThreadParams;
    if (pParams->mpThreadParams)
    {
        pThreadParams = pParams->mpThreadParams;
    }
    else
    {
        // Module-static default, constructed once and configured once with the
        // memory-card worker's stack size / affinity / name.
        static EA::Thread::ThreadParameters s_defaultParams;
        static bool s_bDefaultConfigured = false;
        if (!s_bDefaultConfigured)
        {
            s_defaultParams.mpStack     = 0;              // +0x00
            s_defaultParams.mnStackSize = 0x10000;             // +0x04  64KB
            s_defaultParams.mnPriority  = 0;                   // +0x08
            s_defaultParams.mnProcessor = -1;                  // +0x0C
            s_defaultParams.mbSuspended = false;               // +0x10
            s_defaultParams.mpName      = "realmemcard_xenon"; // +0x14
            s_bDefaultConfigured = true;
        }
        pThreadParams = &s_defaultParams;
    }

    // Bring up the three persistent Realmc backend objects.
    RealmcCore::ObjectManager::Initialize();

    // Lazily construct the single MemcardInterfaceImpl (the shared native singleton).
    if (!s_pInstance)
    {
        // The raw object storage (0x528 == 1320 bytes, the impl's fixed size) is
        // allocated with a null tag, then the impl is constructed in place.
        void* pMem = RealmcCore::AllocateMem(0, 1320);
        s_pInstance = pMem
                          ? new (pMem) MemcardInterfaceImpl(pThreadParams, pGameInterface, rGameInfo)
                          : 0;
    }
    return s_pInstance;
}

} // namespace RealmcIface
