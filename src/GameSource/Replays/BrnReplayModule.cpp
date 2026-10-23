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
#include "GameSource/Replays/BrnReplayModuleIO.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/System/CgsHardwareInit.h" // StopPlaying conditional call
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <cstring>
#include <cstdint>
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
// The native Job embeds use the existing EA::Jobs implementation. The GPU stream
// already constructs its nested Relocator job and lock. Other retained bodies are
// WaitForOpenReplayFiles, Update_Dispatch, WaitForSerialiseJobs, Destruct, and
// UpdateRestoring_PostSim.
// ============================================================================

namespace BrnReplays
{
    namespace
    {
        // Typed views of the payload returned by the GUI queue, with its queue
        // header already removed. Names for target-only bytes are descriptive
        // addresses only; no 12-byte GuiEvent prefix or new event policy is added.
        struct ReplayEvent349Payload { u32 mValue; };
        struct ReplayModePayload { s32 miMode; }; // DecFIGS GuiReplaySetModeEvent
        struct ReplayReelPayload { s32 miReel; }; // DecFIGS GuiReplayDeleteReelEvent
        struct ReplayPlayReelPayload             // ARTIST event 526 reads +0,+4,+5,+6
        {
            s32 miReel;
            u8 mByte04;
            u8 mByte05;
            u8 mByte06;
        };
        struct ReplayReelNamePayload            // ARTIST event594: reel then 256-byte name
        {
            s32 miReel;
            char macName[256];
        };
        static_assert(sizeof(ReplayModePayload) ==4 && sizeof(ReplayReelPayload) ==4,
                      "original scalar replay GUI payloads");
        static_assert(sizeof(ReplayPlayReelPayload) ==8 && sizeof(ReplayReelNamePayload) ==260,
                      "original replay play/name payload extents");

        // The current inline CgsModuleUtils helpers keep the lock order but omit
        // the null checks emitted by ARTIST sub_823B6FE0/sub_823B7060. Keep the
        // original odd parameter labels and explicit source provenance here.
        void AssertReplayIOBufferPair(const void* lpDestBuffer, const void* lpSourceBuffer,
                                      s32 liDestLine, s32 liSourceLine)
        {
            static const char KACgsModuleUtilsPath[] =
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h";
            if (lpDestBuffer == nullptr)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpInputBuffer", KACgsModuleUtilsPath, liDestLine);
                CgsDev::Assert::EndAssert();
            }
            if (lpSourceBuffer == nullptr)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpOutputBuffer0", KACgsModuleUtilsPath, liSourceLine);
                CgsDev::Assert::EndAssert();
            }
        }
    }

    ReplayModule::ReplayModule()
        : maJobs4940{EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr),
                     EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr)}
        , mSerialiseJob(nullptr)
    {
        // Base and embedded stream constructors own their locks and the nested Relocator job.
        // ARTIST 827E0448..046C constructs these four module jobs in slot order, then the final job.
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
            // ARTIST 8264B4CC..DC: null callback/context and sleep=-1, before poster End.
            mSerialiseJob.WaitOn(nullptr, nullptr, -1);
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

    // ARTIST 8265F618. Original complete post-sim entry: IO locks, request/event
    // publication, GUI requests, then exactly one state-specific post-sim body.
    void ReplayModule::Update_PostSim(const ReplayIO::InputBuffer_PostSim* lpInputBuffer,
                                      ReplayIO::OutputBuffer_PostSim* lpOutputBuffer,
                                      BrnUpdateSet lUpdateSet)
    {
        // sub_823B6FE0 receives output in r3 and input in r4.
        AssertReplayIOBufferPair(lpOutputBuffer, lpInputBuffer, 259, 260);
        CgsModule::LockBuffersForIO(lpOutputBuffer, lpInputBuffer);

        StoreSerialisers(*lpInputBuffer->GetRequestInterface());
        lpOutputBuffer->AppendGameEventQueue(&mGameEventCache);
        mGameEventCache.Clear();
        mbStopPlayingRequestReceived = false;
        mUnidentified6259 = false;
        mUnidentified625A = false;

        const ReplayIO::InputBuffer_PostSim::GuiEventQueue* lpQueue =
            lpInputBuffer->GetGuiEvent();
        const CgsModule::Event* lpEvent = nullptr;
        s32 liEventSize = 0;
        s32 liEventType = lpQueue->GetFirstEvent(&lpEvent, &liEventSize);
        while (lpEvent != nullptr)
        {
            switch (liEventType)
            {
            case 349:
                mUnidentified6260 = reinterpret_cast<const ReplayEvent349Payload*>(lpEvent)->mValue;
                break;
            case 525:
            {
                const s32 liMode = reinterpret_cast<const ReplayModePayload*>(lpEvent)->miMode;
                CGS_ASSERT(static_cast<u32>(liMode) < static_cast<u32>(E_MODE_COUNT), "Invalid mode\n");
                meMode = static_cast<EMode>(liMode);
                break;
            }
            case 526:
            {
                const ReplayPlayReelPayload* lpPlay =
                    reinterpret_cast<const ReplayPlayReelPayload*>(lpEvent);
                const s32 liReel = lpPlay->miReel;
                CGS_ASSERT(meMode == E_MODE_PAUSEMENU, "Invalid mode to trigger playback\n");
                CGS_ASSERT(meState == E_STREAM_STATE_IDLE, "Invalid state to trigger playback\n");
                if (static_cast<u32>(liReel) >= 6u)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::MessageBufferPC lBuffer;
                    CgsDev::StrStream lMessage(lBuffer.GetBuffer(), lBuffer.GetSize());
                    lMessage << "Reel " << liReel << " out of range\n";
                    CgsDev::Assert::FireAssert(lBuffer.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }
                if (liReel == miCurrentRecordReel)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::MessageBufferPC lBuffer;
                    CgsDev::StrStream lMessage(lBuffer.GetBuffer(), lBuffer.GetSize());
                    lMessage << "Currently recording to reel " << liReel << " so can't play it\n";
                    CgsDev::Assert::FireAssert(lBuffer.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }
                if (!maReels[liReel].mbUsed)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::MessageBufferPC lBuffer;
                    CgsDev::StrStream lMessage(lBuffer.GetBuffer(), lBuffer.GetSize());
                    lMessage << "Reel " << liReel << " is empty so can't play it\n";
                    CgsDev::Assert::FireAssert(lBuffer.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }
                if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
                    *CgsDev::Log::gpDebugPrint << "Replay module playing back reel " << liReel << "\n";
                miCurrentPlayReel = liReel;
                mUnidentified5C38 = lpPlay->mByte04;
                mUnidentified5C39 = lpPlay->mByte06;
                const u8 luByte05 = lpPlay->mByte05;
                mUnidentified5C3B = false;
                mUnidentified5C3C = luByte05;
                break;
            }
            case 527:
            {
                const s32 liReel = reinterpret_cast<const ReplayReelPayload*>(lpEvent)->miReel;
                CGS_ASSERT(meMode == E_MODE_PAUSEMENU, "Invalid mode to trigger delete\n");
                CGS_ASSERT(meState == E_STREAM_STATE_IDLE, "Invalid state to trigger delete\n");
                if (static_cast<u32>(liReel) >= 6u)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::MessageBufferPC lBuffer;
                    CgsDev::StrStream lMessage(lBuffer.GetBuffer(), lBuffer.GetSize());
                    lMessage << "Reel " << liReel << " out of range\n";
                    CgsDev::Assert::FireAssert(lBuffer.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }
                if (liReel == miCurrentRecordReel)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::MessageBufferPC lBuffer;
                    CgsDev::StrStream lMessage(lBuffer.GetBuffer(), lBuffer.GetSize());
                    lMessage << "Currently recording to reel " << liReel << " so can't delete it\n";
                    CgsDev::Assert::FireAssert(lBuffer.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }
                if (!maReels[liReel].mbUsed)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::MessageBufferPC lBuffer;
                    CgsDev::StrStream lMessage(lBuffer.GetBuffer(), lBuffer.GetSize());
                    lMessage << "Reel " << liReel << " is empty so can't delete it\n";
                    CgsDev::Assert::FireAssert(lBuffer.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }
                if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
                    *CgsDev::Log::gpDebugPrint << "Replay module deleting reel " << liReel << "\n";
                maReels[liReel].mbUsed = false;
                break;
            }
            case 528:
                CGS_ASSERT(meMode == E_MODE_PAUSEMENU, "Invalid mode to stop playback\n");
                CGS_ASSERT(meState == E_STREAM_STATE_PLAYING, "Invalid state to stop playback\n");
                mbStopPlayingRequestReceived = true;
                break;
            case 529:
                CGS_ASSERT(meMode == E_MODE_PAUSEMENU, "Invalid mode to stop playback\n");
                CGS_ASSERT(meState == E_STREAM_STATE_PLAYING, "Invalid state to stop playback\n");
                if (mUnidentified5C3C == 0)
                    mUnidentified5C3B = true;
                break;
            case 530:
                CGS_ASSERT(meMode == E_MODE_PAUSEMENU, "Invalid mode to stop playback\n");
                CGS_ASSERT(meState == E_STREAM_STATE_PLAYING, "Invalid state to stop playback\n");
                mUnidentified5C3C = 0;
                mUnidentified5C3B = false;
                break;
            case 531:
                mUnidentified6259 = true;
                break;
            case 532:
                mUnidentified625A = true;
                break;
            case 533:
                if (meState == E_STREAM_STATE_RECORDING &&
                    mGpuWriteStream.GetStatus() == GPUDiskWriteStream::KI_STATE_OPEN &&
                    !(mfEndTime > 0.0f))
                    mWriteStream.ResetStream();
                break;
            case 594:
            {
                const ReplayReelNamePayload* lpName =
                    reinterpret_cast<const ReplayReelNamePayload*>(lpEvent);
                if (std::strlen(lpName->macName) >= 256u)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::MessageBufferPC lBuffer;
                    CgsDev::StrStream lMessage(lBuffer.GetBuffer(), lBuffer.GetSize());
                    lMessage << "String too long: " << lpName->macName;
                    CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                        "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h",55);
                    CgsDev::Assert::EndAssert();
                }
                // Original has no additional reel-index guard or forced terminator.
                std::strncpy(maReels[lpName->miReel].macName, lpName->macName,256);
                break;
            }
            default:
                break;
            }
            liEventType = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        switch (meState)
        {
        case E_STREAM_STATE_IDLE:
            break;
        case E_STREAM_STATE_RECORDING:
            UpdateRecording_PostSim(lpInputBuffer, lpOutputBuffer, lUpdateSet);
            break;
        case E_STREAM_STATE_PLAYING:
            UpdatePlaying_PostSim(lpInputBuffer, lpOutputBuffer, lUpdateSet);
            break;
        case E_STREAM_STATE_RESTORING:
            UpdateRestoring_PostSim(lpInputBuffer, lpOutputBuffer, lUpdateSet);
            break;
        default:
            CGS_ASSERT(false, "Invalid update state for replay module\n");
            break;
        }

        AssertReplayIOBufferPair(lpOutputBuffer, lpInputBuffer, 272, 273);
        CgsModule::UnlockBuffersForIO(lpOutputBuffer, lpInputBuffer);
    }

    // ARTIST 8265F0F8: wait before reading any serialiser's populated bytes, then
    // append the compact entry/pointer lists in original slot order.
    void ReplayModule::UpdateRecording_PostSim(const ReplayIO::InputBuffer_PostSim* /*lpInputBuffer*/,
                                               ReplayIO::OutputBuffer_PostSim* /*lpOutputBuffer*/,
                                               BrnUpdateSet /*lUpdateSet*/)
    {
        if (meStreamStage != E_STREAM_STAGE_OPENING && meStreamStage != E_STREAM_STAGE_OPEN)
            return;

        WaitForSerialiseJobs();
        const bool lbIsKeyFrame =
            miCurrentFrame % static_cast<s64>(miKeyFrameInterval) == 0;
        FrunkSerialiserEntry laEntries[KI_NUM_SERIALISERS];
        void* lapBuffers[KI_NUM_SERIALISERS];
        s32 liNumSerialisers = 0;
        for (s32 liSlot = 0; liSlot < KI_NUM_SERIALISERS; ++liSlot)
        {
            const BaseSerialiser* lpSerialiser = mapSerialisers[liSlot];
            if (lpSerialiser != nullptr && lpSerialiser->miBufferUsed > 0)
            {
                laEntries[liNumSerialisers].miId = lpSerialiser->meId;
                laEntries[liNumSerialisers].miSize = lpSerialiser->miBufferUsed;
                lapBuffers[liNumSerialisers] = lpSerialiser->mpBuffer;
                ++liNumSerialisers;
            }
        }
        if (liNumSerialisers > 0)
        {
            mWriteStream.AddFrunk(miCurrentFrame, mfCurrentTime, laEntries,
                                  lapBuffers, liNumSerialisers, lbIsKeyFrame);
            ++miCurrentFrame;
            mfCurrentTime += 0.016666668f; // original f32 word 3C888889 at 8209DB3C
        }
    }

    // ARTIST 8264B570: the original readiness conjunction, not a new playback pump.
    void ReplayModule::UpdatePlaying_PostSim(const ReplayIO::InputBuffer_PostSim* /*lpInputBuffer*/,
                                             ReplayIO::OutputBuffer_PostSim* /*lpOutputBuffer*/,
                                             BrnUpdateSet /*lUpdateSet*/)
    {
        if (meStreamStage != E_STREAM_STAGE_OPEN || !mbStreamDataReady ||
            !mbReadyToPlay || mbPlaybackDataReady || miHeaderStreamPos > 0)
            return;

        bool lbAllReady = true;
        for (s32 liSlot = 0; liSlot < KI_NUM_SERIALISERS; ++liSlot)
        {
            const BaseSerialiser* lpSerialiser = mapSerialisers[liSlot];
            if (lpSerialiser != nullptr)
                lbAllReady = lbAllReady && lpSerialiser->mbDataReady;
        }
        mbPlaybackDataReady = lbAllReady;
    }


    // ARTIST 0x8264E630. Reset every occupied serialiser for the requested mode.
    // The frame remainder, readiness store, mode, keyframe/time/streaming fields, and
    // used/read cursors follow the original eleven-slot walk; no scheduler or queue
    // submission is inferred here.
    void ReplayModule::ClearSerialisers(BaseSerialiser::EMode leMode, bool lbAllowStreaming)
    {
        const bool lbIsKeyFrame =
            (miCurrentFrame % static_cast<s64>(miKeyFrameInterval)) == 0;

        for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
        {
            BaseSerialiser* lpSerialiser = mapSerialisers[liId];
            if (lpSerialiser != nullptr)
            {
                lpSerialiser->SetDataReady(true);
                lpSerialiser->SetMode(leMode);
                lpSerialiser->mbIsKeyFrame = lbIsKeyFrame;
                lpSerialiser->mfTime = mfCurrentTime;
                lpSerialiser->mbAllowStreaming = lbAllowStreaming;
                lpSerialiser->miBufferRead = 0;
                lpSerialiser->miBufferUsed = 0;
            }
        }
    }

    // ARTIST 0x82660E78. The X360 lock helper is output-write/input-read and the
    // unlock helper reverses that order. State-specific work is dispatched while all
    // registered serialisers are locked.
    void ReplayModule::Update_PreSim(const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
                                    ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                                    BrnUpdateSet lUpdateSet)
    {
        AssertReplayIOBufferPair(lpOutputBuffer, lpInputBuffer, 259, 260);
        CgsModule::LockBuffersForIO(lpOutputBuffer, lpInputBuffer);

        LockSerialisers();
        RenderDebugHUD(); // Callee body is a retained-source gap; keep the attested call.
        mDebugComponent.PreUpdateRecord();

        if (meState == E_STREAM_STATE_IDLE)
        {
            for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
            {
                BaseSerialiser* lpSerialiser = mapSerialisers[liId];
                if (lpSerialiser != nullptr)
                {
                    lpSerialiser->miBufferUsed = 0;
                    lpSerialiser->miBufferRead = 0;
                }
            }
        }

        switch (meState)
        {
        case E_STREAM_STATE_IDLE:
            break;
        case E_STREAM_STATE_RECORDING:
            UpdateRecording_PreSim(lpInputBuffer, lpOutputBuffer, lUpdateSet);
            break;
        case E_STREAM_STATE_PLAYING:
            UpdatePlaying_PreSim(lpInputBuffer, lpOutputBuffer, lUpdateSet);
            break;
        case E_STREAM_STATE_RESTORING:
            for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
            {
                BaseSerialiser* lpSerialiser = mapSerialisers[liId];
                if (lpSerialiser != nullptr)
                {
                    lpSerialiser->SetMode(BaseSerialiser::E_MODE_RESTORING);
                    lpSerialiser->SetDataReady(false);
                }
            }
            break;
        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Invalid update state for replay module\n",
                "..\\..\\..\\GameSource\\Replays/BrnReplayModule.cpp", 355);
            CgsDev::Assert::EndAssert();
            break;
        }

        mbStartPlaying = false;
        mbStartRecording = false;
        mbStopPlaying = false;
        mbStopRecording = false;
        mbMarkActionReplay = false;
        mbStartActionReplay = false;

        SetStatusInterface(lpOutputBuffer->GetStatusInterface());
        mDebugComponent.PostUpdateRecord();
        UnlockSerialisers();
        AssertReplayIOBufferPair(lpOutputBuffer, lpInputBuffer, 272, 273);
        CgsModule::UnlockBuffersForIO(lpOutputBuffer, lpInputBuffer);
    }

    // ARTIST 0x82660430. Advance the recording-side stream and its header/close
    // stages. No OpenReplayFiles producer exists in the retained ARTIST export set.
    void ReplayModule::UpdateRecording_PreSim(
            const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
            ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
            BrnUpdateSet lUpdateSet)
    {
        if (mUnidentified6259)
        {
            s32 liUsedReels = 0;
            for (s32 liReel = 0; liReel < 6; ++liReel)
            {
                if (!maReels[liReel].mbUsed)
                    break;
                ++liUsedReels;
            }
            if (liUsedReels == 6)
            {
                mfDebugHudAlpha = 1.0f;
                mUnidentified6259 = false;
            }
        }

        mfCurrentTime = lpInputBuffer->GetTimerStatusInterface()->GetSimTimerStatus()
                            ->GetTime().GetFloatVal();
        if (mfEndTime > 0.0f)
            mfDebugHudAlpha = 1.0f;

        switch (meStreamStage)
        {
        case E_STREAM_STAGE_CLOSED:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Should never have closed streams while recording (only ever opening, open or closing\n",
                "..\\..\\..\\GameSource\\Replays/BrnReplayModule.cpp", 1035);
            CgsDev::Assert::EndAssert();
            break;

        case E_STREAM_STAGE_OPENING:
            ClearSerialisers(BaseSerialiser::E_MODE_RECORDING, true);
            if (WaitForOpenReplayFiles())
                meStreamStage = E_STREAM_STAGE_OPEN;
            break;

        case E_STREAM_STAGE_OPEN:
            ClearSerialisers(BaseSerialiser::E_MODE_RECORDING, true);
            if (mbStopRecording || mUnidentified6259)
            {
                mWriteStream.ResetStartFrame(15.0f);
                mfStartTime = mfCurrentTime - 15.0f;
                mUnidentified5C3E = true;
                mfEndTime = mfStartTime + 20.0f; // ARTIST flt_820054CC
            }
            else if (mfEndTime > 0.0f &&
                     (mfCurrentTime > mfEndTime || mUnidentified625A))
            {
                StopRecording(lpInputBuffer, lpOutputBuffer, lUpdateSet);
                StreamHeader* lpHeader = reinterpret_cast<StreamHeader*>(mpcHeaderBuffer);
                if (lpHeader->mpFrameOffsets != nullptr)
                {
                    lpHeader->ChopOffTailFrames();
                    const std::uintptr_t luHeaderBase =
                        reinterpret_cast<std::uintptr_t>(lpHeader);
                    const std::uintptr_t luOffsets =
                        reinterpret_cast<std::uintptr_t>(lpHeader->mpFrameOffsets);
                    lpHeader->mpFrameOffsets =
                        reinterpret_cast<StreamOffset*>(luOffsets - luHeaderBase);
                }
                mHeaderFile.Write(mpcHeaderBuffer, 0, 0x20000);
            }
            else if (meMode != E_MODE_INGAME)
            {
                maReels[miCurrentRecordReel].mbUsed = false;
                StopRecording(lpInputBuffer, lpOutputBuffer, lUpdateSet);
            }
            break;

        case E_STREAM_STAGE_HEADER:
            ClearSerialisers(BaseSerialiser::E_MODE_IDLE, true);
            if (!mWriteStream.mbEnded)
            {
                mWriteStream.mHeaderMalloc.FreeAll();
                mWriteStream.mHeaderMalloc.Destruct();
                mWriteStream.miNumFrunksAllocated = 0;
                mWriteStream.mbEnded = true;
            }
            if (mHeaderFile.GetStatus() == CgsFileSystem::E_FILESTATE_OPEN)
            {
                CloseReplayFiles(lpOutputBuffer);
                meStreamStage = E_STREAM_STAGE_CLOSING;
            }
            break;

        case E_STREAM_STAGE_CLOSING:
            ClearSerialisers(BaseSerialiser::E_MODE_IDLE, true);
            if (mGpuWriteStream.GetStatus() != GPUDiskWriteStream::KI_STATE_OPEN &&
                mGameDataReceiverQueue.GetLength() >= 1)
            {
                mGameDataReceiverQueue.Clear();
                meState = E_STREAM_STATE_IDLE;
                meStreamStage = E_STREAM_STAGE_CLOSED;
            }
            break;

        default:
            break;
        }
    }

    // ARTIST 0x826608A8. Preserve the original playback read, stall, keyframe and
    // close transitions; the separate start/open producer remains outside this packet.
    void ReplayModule::UpdatePlaying_PreSim(
            const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
            ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
            BrnUpdateSet lUpdateSet)
    {
        mfDebugHudAlpha = 0.0f;

        switch (meStreamStage)
        {
        case E_STREAM_STAGE_CLOSED:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Should never have closed streams while playing (only ever opening, open or closing\n",
                "..\\..\\..\\GameSource\\Replays/BrnReplayModule.cpp", 1155);
            CgsDev::Assert::EndAssert();
            return;

        case E_STREAM_STAGE_OPENING:
            if (!WaitForOpenReplayFiles())
                return;
            mHeaderFile.Read(mpcHeaderBuffer, 0, 0x20000);
            meStreamStage = E_STREAM_STAGE_HEADER;
            [[fallthrough]];

        case E_STREAM_STAGE_HEADER:
        {
            if (mHeaderFile.GetStatus() != CgsFileSystem::E_FILESTATE_OPEN)
                return;

            StreamHeader* lpHeader = reinterpret_cast<StreamHeader*>(mpcHeaderBuffer);
            if (lpHeader->mpFrameOffsets != nullptr)
            {
                const std::uintptr_t luHeaderBase =
                    reinterpret_cast<std::uintptr_t>(lpHeader);
                const std::uintptr_t luOffsets =
                    reinterpret_cast<std::uintptr_t>(lpHeader->mpFrameOffsets);
                lpHeader->mpFrameOffsets =
                    reinterpret_cast<StreamOffset*>(luHeaderBase + luOffsets);
            }
            miCurrentFrame = mReadStream.StartNewStream(
                mpcHeaderBuffer, 0x20000, &mPrimaryDiskReadStream);
            mbPlaybackDataReady = false;
            mbStreamDataReady = false;
            meStreamStage = E_STREAM_STAGE_OPEN;
            [[fallthrough]];
        }

        case E_STREAM_STAGE_OPEN:
        {
            if (!mbStreamDataReady)
            {
                const StreamOffset& lrOffset =
                    mReadStream.mpStreamHeader->mpFrameOffsets[mReadStream.miCurrentFrunk];
                const s32 liReadSize = (lrOffset.miFrunkSize + 0xFFFF) & 0xFFFF0000;
                if (!mPrimaryDiskReadStream.ReadBlock(lrOffset.miFileOffset, nullptr, liReadSize))
                    return;
            }

            mbStreamDataReady = true;
            mbReadyToPlay = true;
            FrunkReadResult lReadResult;
            lReadResult.mfTime = 0.0f;
            lReadResult.mxFlags = 0;
            for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
            {
                BaseSerialiser* lpSerialiser = mapSerialisers[liId];
                if (lpSerialiser != nullptr)
                {
                    lReadResult.mapBuffers[liId] = lpSerialiser->mpBuffer;
                    lReadResult.maiSizes[liId] = lpSerialiser->miBufferSize;
                }
            }

            if (mbPlaybackDataReady)
            {
                if (mUnidentified5C3C != 0)
                {
                    for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
                    {
                        BaseSerialiser* lpSerialiser = mapSerialisers[liId];
                        if (lpSerialiser != nullptr)
                            lpSerialiser->miBufferRead = 0;
                    }
                }
                else
                {
                    const bool lbReadCurrentFrunk = mReadStream.ReadCurrentFrunk(&lReadResult);
                    if (!lbReadCurrentFrunk || (lReadResult.mxFlags & KU_FLAG_VOID) != 0)
                    {
                        ClearSerialisers(BaseSerialiser::E_MODE_PLAYING_STALLED, true);
                        for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
                        {
                            BaseSerialiser* lpSerialiser = mapSerialisers[liId];
                            if (lpSerialiser != nullptr)
                                lpSerialiser->miBufferRead = 0;
                        }
                        ++miCurrentFrame;
                        if (mReadStream.MoveToNextFrunk())
                        {
                            mUnidentified5C3D = true;
                            mUnidentified5C3C = 1;
                        }
                    }
                    else
                    {
                        ClearSerialisers(BaseSerialiser::E_MODE_PLAYING, true);
                        for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
                        {
                            BaseSerialiser* lpSerialiser = mapSerialisers[liId];
                            if (lpSerialiser != nullptr)
                            {
                                lpSerialiser->miBufferRead = 0;
                                lpSerialiser->miBufferUsed = lReadResult.maiSizes[liId];
                                lpSerialiser->SetDataReady(false);
                            }
                        }
                        ++miCurrentFrame;
                        if (mReadStream.MoveToNextFrunk())
                        {
                            mUnidentified5C3C = 1;
                            mUnidentified5C3D = true;
                        }
                        if (mUnidentified5C3B &&
                            (lReadResult.mxFlags & KU_FLAG_KEYFRAME) != 0)
                        {
                            mUnidentified5C3B = false;
                            mUnidentified5C3C = 1;
                        }
                    }
                }
            }
            else if (miHeaderStreamPos == 10)
            {
                const bool lbReadCurrentFrunk = mReadStream.ReadCurrentFrunk(&lReadResult);
                if (!lbReadCurrentFrunk)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "mReadStream.ReadCurrentFrunk( &lReadResult )",
                        "..\\..\\..\\GameSource\\Replays/BrnReplayModule.cpp", 1226);
                    CgsDev::Assert::EndAssert();
                }
                if ((lReadResult.mxFlags & KU_FLAG_VOID) != 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "First frame should never be VOID\n",
                        "..\\..\\..\\GameSource\\Replays/BrnReplayModule.cpp", 1229);
                    CgsDev::Assert::EndAssert();
                }
                if ((lReadResult.mxFlags & KU_FLAG_KEYFRAME) == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "First frame should always be key frame\n",
                        "..\\..\\..\\GameSource\\Replays/BrnReplayModule.cpp", 1230);
                    CgsDev::Assert::EndAssert();
                }

                ClearSerialisers(BaseSerialiser::E_MODE_PLAYING, true);
                for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
                {
                    BaseSerialiser* lpSerialiser = mapSerialisers[liId];
                    if (lpSerialiser != nullptr)
                    {
                        lpSerialiser->miBufferRead = 0;
                        lpSerialiser->miBufferUsed = 0;
                        lpSerialiser->miBufferRead = 0;
                        lpSerialiser->miBufferUsed = lReadResult.maiSizes[liId];
                        lpSerialiser->SetDataReady(false);
                    }
                }
                ++miCurrentFrame;
                --miHeaderStreamPos;
                if (mReadStream.MoveToNextFrunk())
                {
                    mUnidentified5C3D = true;
                    mUnidentified5C3C = 1;
                }
            }
            else
            {
                for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
                {
                    BaseSerialiser* lpSerialiser = mapSerialisers[liId];
                    if (lpSerialiser != nullptr)
                    {
                        lpSerialiser->miBufferRead = 0;
                        lpSerialiser->SetDataReady(false);
                    }
                }
                --miHeaderStreamPos;
            }

            if (mbStopPlayingRequestReceived)
            {
                mUnidentified5C3A = true;
                StopPlaying(lpInputBuffer, lpOutputBuffer, lUpdateSet);
            }
            break;
        }

        case E_STREAM_STAGE_CLOSING:
            if (!WaitForCloseReplayFiles())
                return;
            meStreamStage = E_STREAM_STAGE_CLOSED;
            BeginRestoring(lpInputBuffer, lpOutputBuffer, lUpdateSet);
            return;

        default:
            return;
        }

        UnlockSerialisers();
        if (mbSerialiseActive)
        {
            mSerialiseJob.WaitOn(nullptr, nullptr, -1);
            mCommandPoster.End();
            mbPosterActive = false;
        }
        mbSerialiseActive = false;
        LockSerialisers();
    }

    // ARTIST 0x8264B518. Change to restoring and reset each occupied serializer.
    void ReplayModule::BeginRestoring(const ReplayIO::InputBuffer_PreSim* /*lpInputBuffer*/,
                                      ReplayIO::OutputBuffer_PreSim* /*lpOutputBuffer*/,
                                      BrnUpdateSet /*lUpdateSet*/)
    {
        meState = E_STREAM_STATE_RESTORING;
        for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
        {
            BaseSerialiser* lpSerialiser = mapSerialisers[liId];
            if (lpSerialiser != nullptr)
            {
                lpSerialiser->SetMode(BaseSerialiser::E_MODE_RESTORING);
                lpSerialiser->SetDataReady(false);
            }
        }
    }

    // ARTIST 0x826528B8. The assertion continues, then the original stop stores run.
    void ReplayModule::StopRecording(const ReplayIO::InputBuffer_PreSim* /*lpInputBuffer*/,
                                     ReplayIO::OutputBuffer_PreSim* /*lpOutputBuffer*/,
                                     BrnUpdateSet /*lUpdateSet*/)
    {
        if (meState != E_STREAM_STATE_RECORDING)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Can only stop recording from recording state\n",
                "..\\..\\..\\GameSource\\Replays/BrnReplayModule.cpp", 1556);
            CgsDev::Assert::EndAssert();
        }

        ClearSerialisers(BaseSerialiser::E_MODE_IDLE, true);
        mUnidentified5C3E = false;
        meStreamStage = E_STREAM_STAGE_HEADER;
        miCurrentRecordReel = -1;
    }

    // ARTIST 0x826607C8. Preserve the conditional existing thread-enable call.
    void ReplayModule::StopPlaying(const ReplayIO::InputBuffer_PreSim* /*lpInputBuffer*/,
                                   ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                                   BrnUpdateSet /*lUpdateSet*/)
    {
        if (meState != E_STREAM_STATE_PLAYING)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Can only stop playing from playing state\n",
                "..\\..\\..\\GameSource\\Replays/BrnReplayModule.cpp", 1581);
            CgsDev::Assert::EndAssert();
        }

        miCurrentPlayReel = -1;
        ClearSerialisers(BaseSerialiser::E_MODE_RESTORING, true);
        CloseReplayFiles(lpOutputBuffer);
        mReadStream.mpStreamHeader = nullptr;
        meStreamStage = E_STREAM_STAGE_CLOSING;
        if (mUnidentified5C38 != 0)
            CgsSystem::HardwareInit::EnableJobThread();
    }

    // ARTIST 0x8264E470. Publish status bits, six reels and playback time.
    void ReplayModule::SetStatusInterface(ReplayIO::StatusInterface* lpStatusInterface)
    {
        if (meState == E_STREAM_STATE_RECORDING)
        {
            lpStatusInterface->mxStatusFlags =
                (lpStatusInterface->mxStatusFlags & 0xFFFFFF8Du) | 2u;
            if (mUnidentified5C3E)
                lpStatusInterface->mxStatusFlags |= 4u;
            else
                lpStatusInterface->mxStatusFlags &= ~4u;
        }
        else if (meState == E_STREAM_STATE_PLAYING)
        {
            lpStatusInterface->mxStatusFlags |= 1u;
            if (mbPlaybackDataReady)
                lpStatusInterface->mxStatusFlags &= ~0x10u;
            else
                lpStatusInterface->mxStatusFlags |= 0x10u;
            if (mUnidentified5C3C != 0)
                lpStatusInterface->mxStatusFlags |= 0x20u;
            else
                lpStatusInterface->mxStatusFlags &= ~0x20u;
            if (mUnidentified5C3D)
                lpStatusInterface->mxStatusFlags |= 0x40u;
            else
                lpStatusInterface->mxStatusFlags &= ~0x40u;
            lpStatusInterface->mxStatusFlags &= ~4u;
        }
        else if (meState == E_STREAM_STATE_RESTORING)
        {
            lpStatusInterface->mxStatusFlags |= 0x11u;
            if (mUnidentified5C3C != 0)
                lpStatusInterface->mxStatusFlags |= 0x20u;
            else
                lpStatusInterface->mxStatusFlags &= ~0x20u;
            if (mUnidentified5C3D)
                lpStatusInterface->mxStatusFlags |= 0x40u;
            else
                lpStatusInterface->mxStatusFlags &= ~0x40u;
            lpStatusInterface->mxStatusFlags &= ~4u;
        }
        else
        {
            // ARTIST state 0 / default leaves status flags untouched here.
        }

        s32 liUsedReels = 0;
        for (s32 liReel = 0; liReel < 6; ++liReel)
        {
            lpStatusInterface->SetReel(liReel, &maReels[liReel]);
            if (maReels[liReel].mbUsed)
                ++liUsedReels;
        }
        if (liUsedReels == 6)
            lpStatusInterface->mxStatusFlags |= 8u;

        lpStatusInterface->miCurrentRecordReel = miCurrentRecordReel;
        lpStatusInterface->miCurrentPlaybackReel = miCurrentPlayReel;
        if (meState == E_STREAM_STATE_PLAYING && mReadStream.mpStreamHeader != nullptr)
        {
            s32 liFrunksFromFirst =
                mReadStream.miCurrentFrunk - mReadStream.mpStreamHeader->miFirstFrunk;
            if (liFrunksFromFirst < 0)
                liFrunksFromFirst += KI_MAX_FRUNKS;
            const s32 liFrunksRemaining =
                mReadStream.mpStreamHeader->miNumFrunks - liFrunksFromFirst;
            lpStatusInterface->mfPlaybackTimeRemaining =
                static_cast<f32>(liFrunksRemaining) * 0.016666668f;
        }
        else
        {
            lpStatusInterface->mfPlaybackTimeRemaining = 0.0f;
        }
    }

    // ARTIST 0x8265F228. Post the file close request, close eligible primary streams,
    // and clear the receiver queue exactly as the source does.
    void ReplayModule::CloseReplayFiles(ReplayIO::OutputBuffer_PreSim* lpOutputBuffer)
    {
        CgsResource::Events::FileHandleEvent lCloseRequest;
        lCloseRequest.Construct(&mGameDataReceiverQueue, 0);
        lCloseRequest.SetFileHandle(mHeaderFile);
        lpOutputBuffer->GetGameDataRequestInterface()->mRequestQueue.AddEvent(
            &lCloseRequest, 21);

        if (mPrimaryDiskReadStream.GetStatus() == DiskReadStream::E_STATUS_OPEN)
            mPrimaryDiskReadStream.Close();
        if (mGpuWriteStream.GetStatus() == GPUDiskWriteStream::KI_STATE_OPEN)
            mGpuWriteStream.Close();
        mGameDataReceiverQueue.Clear();
    }

    // ARTIST 0x8264B968. A live GPU write stream or empty receiver queue is not done.
    bool ReplayModule::WaitForCloseReplayFiles()
    {
        if (mGpuWriteStream.GetStatus() == GPUDiskWriteStream::KI_STATE_OPEN ||
            mGameDataReceiverQueue.GetLength() < 1)
            return false;

        mGameDataReceiverQueue.Clear();
        return true;
    }


    // ARTIST 0x8264B9C0. Render the recording status only while its alpha is positive.
    // DebugInterface's automatic lifetime mirrors the original acquire/release pair.
    void ReplayModule::RenderDebugHUD()
    {
        if (mfDebugHudAlpha <= 0.0f)
            return;

        CgsDev::DebugInterface lInterface;
        CgsDev::DebugRender& lrRender = lInterface.Get2dRender();
        const u8 luAlpha = static_cast<u8>(mfDebugHudAlpha * 255.0f);

        if (mfEndTime > 0.0f)
        {
            const f32 lfElapsed = mfCurrentTime - mfStartTime;
            const f32 lfDuration = mfEndTime - mfStartTime;
            const f32 lfPercentComplete = (lfElapsed * 100.0f) / lfDuration;
            char lacText[256];
            CgsCore::SPrintf(lacText, 0x100, "Recording: %.1f complete",
                             static_cast<double>(lfPercentComplete));
            const u32 luColour = (static_cast<u32>(luAlpha) << 24) | 0x0044FF44u;
            lrRender.Draw2DText(lacText, 300.0f, 50.0f, 16.0f, luColour);
            mfDebugHudAlpha = 0.0f;
        }
        else
        {
            const u32 luColour = (static_cast<u32>(luAlpha) << 24) | 0x004444FFu;
            lrRender.Draw2DText("Can't record - no space", 300.0f, 50.0f, 16.0f, luColour);
            mfDebugHudAlpha -= 0.02f;
        }
    }

}
