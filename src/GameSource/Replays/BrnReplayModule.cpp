#include "GameSource/Replays/BrnReplayModule.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"   // ReplayIO::RequestInterface
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"   // BaseSerialiser
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"   // AllocatorList
#include "GameSource/Resource/BrnResourceAllocator.h"                // GetDebugAllocator
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // CgsMemory::LinearMalloc
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // WriteToLog
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"   // CgsResource::Events::OpenFileResponse
#include "GameSource/GameState/BrnGameEvents.h"                            // LeaveReplayEvent / E_EVENT_LEAVE_REPLAY
#include <cstdio>   // snprintf

// ============================================================================
// GameSource/Replays/BrnReplayModule.cpp
//
// ReplayModule has two distinct construction phases. The native C++ constructor
// @0x827E03D0 initializes the base locks, the event-queue unconstructed marker,
// native stream locks, job subobjects, and embedded DebugComponent vtable.
// ReplayModule::Construct @0x82656E20 performs the logical state initialization,
// including the debug component and typed stream initializers recovered here.
//
// The job fields remain opaque placeholders in this B5 source slice; this delta
// does not claim their native EA::Jobs constructor chain. Other retained bodies are
// WaitForOpenReplayFiles, Update_Dispatch, WaitForSerialiseJobs, Destruct, and
// UpdateRestoring_PostSim.
// ============================================================================

namespace BrnReplays
{
    ReplayModule::ReplayModule()
    {
        // ARTIST's native constructor initializes the base and stream locks and
        // chains the EA::Jobs subconstructors. This B5 source retains the event-queue
        // marker; the job placeholders remain explicitly unconstructed.
        mGameEventCache.MarkUnconstructed();
    }

    // ARTIST 0x82656E20. Initialize the named ReplayModule members and invoke
    // the existing typed constructors for the embedded debug component and streams.
    void ReplayModule::Construct()
    {
        CgsModule::ModuleSingleBuffered::Construct();

        meReleaseStage = E_RELEASE_STAGE_DONE;
        mePrepareStage = E_PREPARE_STAGE_START;
        meState = E_STREAM_STATE_IDLE;

        // GetDebugAllocator returns the same allocator object passed by ARTIST as
        // off_82F2C7DC; its assertion is the original null-allocator assertion.
        mDebugComponent.Construct(this, BrnResource::GetDebugAllocator());

        mbStartPlaying = false;
        mbStopPlaying = false;
        mbStartRecording = false;
        mbStopRecording = false;
        mfCurrentTime = 0.0f;
        mbMarkActionReplay = false;
        mfDebugHudAlpha = 0.0f;
        mbStartActionReplay = false;
        mbAutoStart = true;

        mpLinearMalloc = nullptr;
        mpcPrimaryStreamBuffer = nullptr;
        mpcHeaderStreamBuffer = nullptr;
        mpSecondaryStreamBuffer = nullptr;
        miCurrentFrame = 0;
        miKeyFrameInterval = 10;
        meMode = E_MODE_INGAME;
        miCurrentRecordReel = -1;
        miCurrentPlayReel = -1;
        mbStopPlayingRequestReceived = false;
        mUnidentified5C3E = false;

        mGameEventCache.Construct();
        mGameDataReceiverQueue.Construct();
        mWriteStream.Construct();
        mReadStream.Construct();
        mGpuWriteStream.Construct();
        mPrimaryDiskReadStream.Construct();
        meStreamStage = E_STREAM_STAGE_CLOSED;

        for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
            mapSerialisers[liSerialiser] = nullptr;

        for (s32 liReel = 0; liReel < 6; ++liReel)
        {
            maReels[liReel].macName[0] = 0;
            maReels[liReel].mbUsed = false;
        }

        mbIsNewModule = true;
        mUnidentified5C3C = false;
        mUnidentified5C3B = false;
        mUnidentified6260 = 0;
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
