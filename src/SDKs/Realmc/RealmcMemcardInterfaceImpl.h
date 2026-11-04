#pragma once

// ===========================================================================
// RealmcIface::MemcardInterfaceImpl -- the X360 concrete memory-card interface.
// Its constructor is the factory target at 0x82B522D0 and receives the shared
// ThreadParameters, the game callback object and a GameInfo record.
//
// The member order below follows the ARTIST constructor stores and destructor
// calls. In particular, the local X360 EA::Thread::Mutex occupies 0x30 bytes,
// the worker Thread* is at +0x88, the MessageQueue begins at +0x90, the
// MemcardState begins at +0x14C, the four XenonUtil::State records begin at
// +0x1A0 with 0xE0 stride, and the shared ThreadParameters* is at +0x520.
// The factory allocates 0x528 bytes.
// ===========================================================================

#include "types.hpp"

#include "SDKs/Realmc/RealmcMemcardInterface.h"
#include "SDKs/Realmc/RealmcGameInfo.h"
#include "SDKs/Realmc/RealmcMessageQueue.h"
#include "SDKs/Realmc/RealmcMemcardState.h"
#include "SDKs/Realmc/RealmcXenonUtil.h"
#include "SDKs/Realmc/RealmcIfaceGameCallbackProcessor.h"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"
#include <eathread/eathread_mutex.h>
#include <new>

namespace RealmcIface
{

class MemcardInterfaceImpl : public MemcardInterface
{
public:
    // @0x82B522D0 -- constructs the embedded state, callback processor, queue,
    // mutex and four per-user Xenon states in the native initialization order.
    MemcardInterfaceImpl(EA::Thread::ThreadParameters* pThreadParams,
                         IGameInterface* pGameInterface, const GameInfo& rGameInfo);

    // @0x82B52CF0 -- shuts down the queue, joins/frees the worker, deletes the
    // callback processor and closes the four Xenon states. C++ then tears down
    // the embedded MemcardState, MessageQueue, Mutex and base in reverse order.
    ~MemcardInterfaceImpl();

    // ARTIST vtable off_821485DC, slots +0x04 through +0x38.
    void Shutdown();
    int Update(int liArg);
    bool IsResettable();
    int SetSignedUserIndex(int liUserIndex);
    void MessageDone(int liChoice);
    int SetMessageState(int liMode, int liMask);
    int CheckCard(int liArg);
    int SetActiveCard(int liArg1, int liArg2, int liArg3);
    int BootupCheck(SaveInfo* lpSaveInfo, int liNumEntries,
                    LoadEntryInfo* lpEntries, TitleInfo* lpTitleInfo);
    int SaveCheck(SaveCheckParams* lpParams);
    int Save(SaveInfo* lpSaveInfo, int liCount, LoadEntryInfo* lpEntries,
             int liFlags, TitleInfo* lpTitleInfo);
    int SetAutosave(int liEnabled, SaveCheckParams* lpParams);
    int SetMonitor(int liState);
    int Load(EntryContentName* lpEntryContentName, int liNumEntries,
             LoadEntryInfo* lpEntries, int liFlags, TitleInfo* lpTitleInfo);

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    void CleanupThread();

    template <class TTask>
    int RunAsync(TTask* pTask)
    {
        mMutex.Lock();

        if (mMemcardState.GetCurrentTask())
        {
            mMemcardState.PutInStartWaitingQueue(pTask);
            return mMutex.Unlock();
        }

        CleanupThread();
        pTask->Starting();

        void* const lpThreadMemory = RealmcCore::AllocateMem(
            "RealmcCore::Thread", sizeof(EA::Thread::Thread));
        mpWorkerThread = lpThreadMemory
            ? new (lpThreadMemory) EA::Thread::Thread()
            : 0;

        mMutex.Unlock();
        const EA::Thread::RunnableFunctionUserWrapper lUserWrapper =
            EA::Thread::Thread::GetGlobalRunnableFunctionUserWrapper();
        return reinterpret_cast<int>(mpWorkerThread->Begin(
            &RealmcCore::ThrFunction<TTask>, pTask, mpThreadParams, lUserWrapper));
    }

    GameInfo mGameInfo;                          // +0x04 .. +0x54
    EA::Thread::Mutex mMutex;                    // +0x58 .. +0x88
    EA::Thread::Thread* mpWorkerThread;           // +0x88
    RealmcCore::MessageQueue mMessageQueue;       // +0x90 .. +0x148
    GameCallbackProcessor* mpCallbackProcessor;   // +0x148
    RealmcCore::MemcardState mMemcardState;       // +0x14C .. +0x1A0
    XenonUtil::State maStates[4];                 // +0x1A0 .. +0x520
    EA::Thread::ThreadParameters* mpThreadParams; // +0x520
};

CGS_STATIC_ASSERT((sizeof(MemcardInterfaceImpl) == 0x528), "ARTIST memcard implementation size");

} // namespace RealmcIface
