#include "GameSource/Replays/BrnReplayModule.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"   // ReplayIO::RequestInterface
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"   // BaseSerialiser
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"   // AllocatorList
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // CgsMemory::LinearMalloc
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // WriteToLog
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"   // CgsResource::Events::OpenFileResponse
#include "GameSource/GameState/BrnGameEvents.h"                            // LeaveReplayEvent / E_EVENT_LEAVE_REPLAY
#include <cstdio>   // snprintf

// ============================================================================
// GameSource/Replays/BrnReplayModule.cpp
//
// BrnReplays::ReplayModule -- the constructor + the two off-path module methods.
//
//   * ReplayModule::ReplayModule         (runs on every boot)
//   * ReplayModule::Update_Dispatch
//   * ReplayModule::WaitForSerialiseJobs
//   * ReplayModule::Destruct / WaitForOpenReplayFiles / UpdateRestoring_PostSim
//
// CONSTRUCTOR, store for store as the console performs it:
//   the ModuleSingleBuffered base vtable, then the derived ReplayModule vtable
//   RWMutex construction on the base input buffer's mutex   (+0x10)
//   RWMutex construction on the base output buffer's mutex  (+0x118)
//   mGameEventCache marked unconstructed (byte 0)           (+0x23C)
//   Job construction on the embedded jobs at +0x2B10, at +0x4940 (four of them,
//     stride +0x350) and at +0x5680
//   critical-section init on mLockA (+0x3B00) and on mPrimaryDiskReadStream (+0x3F80)
//   mDebugComponent's vtable (+0x6264) -- its inlined construction
//
// In human C++ the two vtable writes + the two RWMutex constructions are the
// ModuleSingleBuffered base sub-object's own construction, and the two
// critical-section inits are the EA::Thread::Futex members' own ctors (the committed
// Futex is exactly that CRITICAL_SECTION-backed lock, reused by name, so its ctor
// performs the init -- same pattern as CgsGraphics::MoviePlayer; the read stream's own ctor does
// the second). This body therefore reproduces only the trailing stores past the base.
//
// FLAG -- DEFERRED sub-construction. The console ctor chains EA::Jobs::Job::Job(.,0) on
// each embedded job (+0x2B10, the four at +0x4940, and the +0x5680 serialise job).
// Those jobs have no complete reconstructed layout (size-only opaque placeholders in
// the header), so their sub-constructors are NOT chained here -- chaining them would
// require fabricating the embedded layout/ctor-overload, which the rules forbid. The
// GPUDiskWriteStream / DataStreamCommandPoster / Futex embeds construct trivially via
// their real types. Every scalar store the ctor makes IS reproduced by name.
// ============================================================================

namespace BrnReplays
{
    ReplayModule::ReplayModule()
    {
        mbPrepared     = false;   // the module has not prepared (base +0x228)
        mpLinearMalloc = 0;   // ReplayModule::Prepare acquires it (+0x878)

        // Base (ModuleSingleBuffered: both vtables + the two RWMutexes) and the
        // embedded mGpuWriteStream / mLockA / mPrimaryDiskReadStream / mCommandPoster /
        // mDebugComponent construct automatically before this body. mLockA's Futex ctor and
        // the read stream's ctor perform the two critical-section inits the console ctor makes
        // at +0x3B00 / +0x3F80.

        mGameEventCache.MarkUnconstructed();   // the console stores 0 at +0x23C

        // Embedded jobs (+0x2B10, the four at +0x4940, +0x5680) sub-construction DEFERRED.
    }

    // Vtable slot 3. The debug component's teardown, the game-event cache's, then the module
    // base's.
    void ReplayModule::Destruct()
    {
        mDebugComponent.Destruct();
        mGameEventCache.Destruct();
        CgsModule::ModuleSingleBuffered::Destruct();
    }

    // Callers: UpdateRecording_PreSim / UpdatePlaying_PreSim. Every queued OpenFileResponse
    // (receiver event 20) with request id 0 is the header file's open; adopt its handle. The files
    // count as open when a primary stream is open (the read stream with no operation pending,
    // or the GPU write stream) and the header file is open; the queue is then cleared.
    bool ReplayModule::WaitForOpenReplayFiles()
    {
        static const s32 KI_OPEN_FILE_RESPONSE   = 20;
        static const s32 KI_HEADER_FILE_EVENT_ID = 0;

        if (mGameDataReceiverQueue.GetLength() < 1)
            return false;

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventId = mGameDataReceiverQueue.GetFirstEvent(&lpEvent, &liEventSize);
        while (liEventId != -1)
        {
            if (liEventId == KI_OPEN_FILE_RESPONSE)
            {
                const CgsResource::Events::OpenFileResponse* lpResponse =
                    reinterpret_cast<const CgsResource::Events::OpenFileResponse*>(lpEvent);
                if (lpResponse->GetEventId() == KI_HEADER_FILE_EVENT_ID)
                    mHeaderFile = lpResponse->GetFileHandle();
            }
            liEventId = mGameDataReceiverQueue.GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        const bool lbPrimaryStreamOpen =
            mPrimaryDiskReadStream.GetStatus() == DiskReadStream::E_STATUS_OPEN ||
            mGpuWriteStream.GetStatus() == GPUDiskWriteStream::KI_STATE_OPEN;
        if (!lbPrimaryStreamOpen)
            return false;

        if (mHeaderFile.GetStatus() != CgsFileSystem::E_FILESTATE_OPEN)
            return false;

        mGameDataReceiverQueue.Clear();
        return true;
    }

    // Caller: Update_PostSim. The buffers and update set are not read on this path.
    void ReplayModule::UpdateRestoring_PostSim(const ReplayIO::InputBuffer_PostSim* /*lpInputBuffer*/,
                                               ReplayIO::OutputBuffer_PostSim* /*lpOutputBuffer*/,
                                               BrnUpdateSet /*lUpdateSet*/)
    {
        bool lbAllRestored = true;
        for (s32 liIndex = 0; liIndex < KI_NUM_SERIALISERS; ++liIndex)
        {
            if (mapSerialisers[liIndex] != 0 && !mapSerialisers[liIndex]->mbDataRestored)
            {
                lbAllRestored = false;
                break;
            }
        }

        if (lbAllRestored)
        {
            meState = E_STREAM_STATE_IDLE;

            const BrnGameState::GameStateModuleIO::LeaveReplayEvent lEvent =
                BrnGameState::GameStateModuleIO::LeaveReplayEvent();
            mGameEventCache.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                                     BrnGameState::GameStateModuleIO::E_EVENT_LEAVE_REPLAY,
                                     static_cast<s32>(sizeof(lEvent)));

            for (s32 liIndex = 0; liIndex < KI_NUM_SERIALISERS; ++liIndex)
            {
                if (mapSerialisers[liIndex] != 0)
                {
                    mapSerialisers[liIndex]->Lock();
                    mapSerialisers[liIndex]->SetMode(BaseSerialiser::E_MODE_IDLE);
                    mapSerialisers[liIndex]->Unlock();
                }
            }
        }
    }

    // Tail-call the GPU disk write stream's Dispatch: the console branches straight into
    // GPUDiskWriteStream::Dispatch with this advanced to the embedded stream at +0x980.
    void ReplayModule::Update_Dispatch()
    {
        mGpuWriteStream.Dispatch();
    }

    // If a serialise job is in flight, wait for it to finish, end the
    // command poster, and clear the poster-active flag; then always clear the
    // serialise-active flag.
    void ReplayModule::WaitForSerialiseJobs()
    {
        if (mbSerialiseActive)
        {
            // The console waits on mSerialiseJob here. The serialise job is a
            // size-only opaque placeholder (no reconstructed layout), so the WaitOn
            // is DEFERRED here -- it folds in with the job-layout pass. FLAG.
            mCommandPoster.End();
            mbPosterActive = false;
        }
        mbSerialiseActive = false;
    }

    // ARTIST 0x8264B8B8: every occupied slot is locked in increasing id order.
    void ReplayModule::LockSerialisers()
    {
        for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
        {
            if (mapSerialisers[liSerialiser])
                mapSerialisers[liSerialiser]->Lock();
        }
    }

    // ARTIST 0x8264B910: release in the same increasing id order.
    void ReplayModule::UnlockSerialisers()
    {
        for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
        {
            if (mapSerialisers[liSerialiser])
                mapSerialisers[liSerialiser]->Unlock();
        }
    }

}
