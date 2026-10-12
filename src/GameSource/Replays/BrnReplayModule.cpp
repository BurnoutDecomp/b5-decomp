#include "GameSource/Replays/BrnReplayModule.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"   // ReplayIO::RequestInterface
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"   // BaseSerialiser
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"   // AllocatorList
#include "GameSource/Resource/BrnResourceAllocator.h"       // Allocators / GetDebugAllocator
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // CgsMemory::LinearMalloc
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"     // Lock/UnlockBuffersForIO
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h" // DebugInterface
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h" // DebugRender::Draw2DText
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // WriteToLog
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameShared/GameClasses/System/CgsHardwareInit.h" // original unsupported EnableJobThread path
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"   // CgsResource::Events::OpenFileResponse
#include "GameSource/Replays/BrnReplayModuleIO.h"            // pre-simulation input/output buffers
#include "GameSource/Replays/BrnReplayStatusInterface.h"     // ReplayIO::StatusInterface
#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h" // StreamHeader::ChopOffTailFrames
#include "GameSource/GameState/BrnGameEvents.h"                            // LeaveReplayEvent / E_EVENT_LEAVE_REPLAY
#include <cstddef>  // ptrdiff_t for the serialized stream-header relative pointer
#include <cstdint>  // uintptr_t for ReadStream's X360-relative header fix-up
#include <cstdio>   // snprintf

// ============================================================================
// GameSource/Replays/BrnReplayModule.cpp
//
// BrnReplays::ReplayModule -- the constructor + the two off-path module methods.
//
//   * ReplayModule::ReplayModule         (runs on every boot)
//   * ReplayModule::Construct             (the module's original runtime initialization)
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
// The original +0x5680 serialise job is now a typed EA::Jobs::Job so its WaitOn dependency
// can be restored. The unrelated +0x2B10 and +0x4940 job objects remain size-only placeholders;
// their sub-construction stays deferred. Every other scalar store is reproduced by name.
// ============================================================================

namespace BrnReplays
{
    ReplayModule::ReplayModule()
        : mSerialiseJob(nullptr)
    {
        mbPrepared     = false;   // the module has not prepared (base +0x228)
        mpLinearMalloc = 0;   // ReplayModule::Prepare acquires it (+0x878)

        // Base (ModuleSingleBuffered: both vtables + the two RWMutexes) and the
        // embedded mGpuWriteStream / mLockA / mPrimaryDiskReadStream / mCommandPoster /
        // mDebugComponent construct automatically before this body. mLockA's Futex ctor and
        // the read stream's ctor perform the two critical-section inits the console ctor makes
        // at +0x3B00 / +0x3F80.

        mGameEventCache.MarkUnconstructed();   // the console stores 0 at +0x23C

        // The typed serialise job is constructed with its original null name above;
        // the unrelated size-only job placeholders remain unconstructed.
    }

    // X360 0x82656E20. The embedded streams and debug component are initialized here,
    // after the C++ object constructor and before Prepare registers the component.
    void ReplayModule::Construct()
    {
        CgsModule::ModuleSingleBuffered::Construct();

        // The X360 stores 2 at +0x22C, clears the base prepare-stage word at +0x228,
        // then sets the replay state at +0x230 to IDLE. +0x22C has no recovered name.
        muConsoleValue22C = 2;
        mbPrepared = false;
        meState = E_STREAM_STATE_IDLE;

        CGS_ASSERT(BrnResource::Allocators::mpInternalDebugAllocator,
                   "Allocators::mpInternalDebugAllocator");
        mDebugComponent.Construct(this, BrnResource::GetDebugAllocator());

        mbStartPlaying = false;
        mbStopPlaying = false;
        mbStartRecording = false;
        mbStopRecording = false;
        mfCurrentRecordingTime = 0.0f;
        mbMarkActionReplay = false;
        mfConsoleValue625C = 0.0f;
        mbStartActionReplay = false;
        mbAutoStart = true;

        mpLinearMalloc = 0;
        mpRecordingBuffer = 0;
        mpSmallRecordingBuffer = 0;
        mpDebugMemoryResource = 0;
        miConsoleValue918 = 0;
        miConsoleValue924 = 10;
        mbConsoleFlag238 = false;
        maPad239[0] = 0;
        maPad239[1] = 0;
        maPad239[2] = 0;
        miCurrentRecordReel = -1;
        miCurrentPlaybackReel = -1;
        mbConsoleFlag6258 = false;
        mbConsoleFlag5C3E = false;

        mGameEventCache.Construct();
        mGameDataReceiverQueue.Construct();

        // The module embeds a WriteStream whose first member is its LinearMalloc base.
        // Preserve each original store by the existing typed member name.
        mWriteStream.mHeaderMalloc.Construct();
        mWriteStream.mpStreamHeader = 0;
        mWriteStream.miStallCount = 0;
        mWriteStream.mbEnded = true;
        mWriteStream.mbPaused = false;
        mWriteStream.mpStream = 0;
        mWriteStream.miFilePosition = 0;
        mWriteStream.miBufferPosition = 0;

        // ARTIST initializes the embedded ReadStream's header, current-frunk and disk-stream
        // members directly at +0x8E0/+0x8E4/+0x8F0; its other cursors are not written here.
        mReadStream.mpStreamHeader = 0;
        mReadStream.miCurrentFrunk = 0;
        mReadStream.mpStream = 0;
        mGpuWriteStream.Construct();

        // ReplayModule::Construct inlines the owner assignment for all 32 disk-read
        // slots, clears the stream status/range, and stores the original -1 sentinel.
        mPrimaryDiskReadStream.miConsoleValue13C = -1;
        mPrimaryDiskReadStream.meStatus = DiskReadStream::E_STATUS_CLOSED;
        mPrimaryDiskReadStream.miLoopStart = 0;
        mPrimaryDiskReadStream.miLoopEnd = 0;
        for (s32 liBlock = 0; liBlock < DiskReadStream::KI_MAX_STREAM_BLOCKS; ++liBlock)
            mPrimaryDiskReadStream.maBlocks[liBlock].mpOwner = &mPrimaryDiskReadStream;

        meStreamStage = E_STREAM_STAGE_CLOSED;
        for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
            mapSerialisers[liSerialiser] = 0;

        for (s32 liReel = 0; liReel < 6; ++liReel)
        {
            maReels[liReel].macName[0] = '\0';
            maReels[liReel].mbUsed = false;
        }

        mbConsoleFlag5C3C = false;
        mbConsoleFlag5C3B = false;
        muConsoleValue6260 = 0;
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
            mSerialiseJob.WaitOn(0, 0, -1);
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

    // ARTIST 0x8264E630. Reset each live serialiser's mode, readiness/keyframe state,
    // replay time, streaming permission, and buffer cursors. The original computes a
    // keyframe marker from the signed qword/divisor pair at +0x918/+0x924.
    void ReplayModule::ClearSerialisers(BaseSerialiser::EMode leMode, bool lbAllowStreaming)
    {
        const bool lbKeyFrame = (miConsoleValue918 % miConsoleValue924) == 0;
        for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
        {
            BaseSerialiser* lpSerialiser = mapSerialisers[liSerialiser];
            if (lpSerialiser == 0)
                continue;

            lpSerialiser->mbDataReady = true;
            lpSerialiser->meMode = leMode;
            lpSerialiser->mbIsKeyFrame = lbKeyFrame;
            lpSerialiser->mfTime = mfCurrentRecordingTime;
            lpSerialiser->mbAllowStreaming = lbAllowStreaming;
            lpSerialiser->miBufferRead = 0;
            lpSerialiser->miBufferUsed = 0;
        }
    }

    // ARTIST 0x826528B8 / DecFIGS declaration: stop only from the recording state,
    // clear serialisers, close the recording stream, and invalidate the current reel.
    void ReplayModule::StopRecording(const ReplayIO::InputBuffer_PreSim* /*lpInputBuffer*/,
                                     ReplayIO::OutputBuffer_PreSim* /*lpOutputBuffer*/,
                                     BrnUpdateSet /*lUpdateSet*/)
    {
        if (meState != E_STREAM_STATE_RECORDING)
            CGS_ASSERT(false, "Can only stop recording from recording state\n");

        ClearSerialisers(BaseSerialiser::E_MODE_IDLE, true);
        mbConsoleFlag5C3E = false;
        meStreamStage = E_STREAM_STAGE_CLOSING;
        miCurrentRecordReel = -1;
    }

    // ARTIST 0x8265F228. Queue a close request for the header file, close any open
    // replay streams, then clear the receiver queue as the console does.
    void ReplayModule::CloseReplayFiles(ReplayIO::OutputBuffer_PreSim* lpOutputBuffer)
    {
        static const s32 KI_CLOSE_FILE_REQUEST = 21;
        static const s32 KI_HEADER_FILE_EVENT_ID = 0;

        CgsResource::Events::FileHandleEvent lCloseRequest;
        lCloseRequest.FileEvent::Construct(&mGameDataReceiverQueue, KI_HEADER_FILE_EVENT_ID);
        lCloseRequest.SetFileHandle(mHeaderFile);
        // ARTIST posts 16 bytes for the X360 {receiver, event id, file handle}; this
        // host's native pointer and FileHandle widths expand the typed record, so copy
        // its complete PC representation for the same receiver to read by member name.
        lpOutputBuffer->GetGameDataRequestInterface()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lCloseRequest),
            KI_CLOSE_FILE_REQUEST, static_cast<s32>(sizeof(lCloseRequest)));

        if (mPrimaryDiskReadStream.GetStatus() == DiskReadStream::E_STATUS_OPEN)
            mPrimaryDiskReadStream.Close();
        if (mGpuWriteStream.GetStatus() == GPUDiskWriteStream::KI_STATE_OPEN)
            mGpuWriteStream.Close();

        mGameDataReceiverQueue.Clear();
    }

    // ARTIST 0x8264E470. Set the status bits for the replay state, copy all six
    // reels, then publish current reel/frame progress in the output interface.
    void ReplayModule::SetStatusInterface(ReplayIO::StatusInterface* lpInterface)
    {
        u32 luFlags = lpInterface->mxStatusFlags;
        switch (meState)
        {
        case E_STREAM_STATE_RECORDING:
            luFlags = (luFlags & ~0x72u) | 0x02u;
            if (mbConsoleFlag5C3E)
                luFlags |= 0x04u;
            else
                luFlags &= ~0x04u;
            break;

        case E_STREAM_STATE_PLAYING:
            luFlags |= 0x01u;
            if (!mbConsoleFlag5C31)
                luFlags |= 0x10u;
            else
                luFlags &= ~0x10u;
            if (mbConsoleFlag5C3C)
                luFlags |= 0x20u;
            else
                luFlags &= ~0x20u;
            break;

        case E_STREAM_STATE_RESTORING:
            luFlags |= 0x11u;
            if (mbConsoleFlag5C3C)
                luFlags |= 0x20u;
            else
                luFlags &= ~0x20u;
            if (mbConsoleFlag5C3D)
                luFlags |= 0x40u;
            else
                luFlags &= ~0x40u;
            luFlags &= ~0x04u;
            break;

        default:
            break;
        }
        lpInterface->mxStatusFlags = luFlags;

        s32 liUsedReels = 0;
        for (s32 liReel = 0; liReel < 6; ++liReel)
        {
            lpInterface->SetReel(liReel, &maReels[liReel]);
            if (maReels[liReel].mbUsed)
                ++liUsedReels;
        }
        if (liUsedReels == 6)
            lpInterface->mxStatusFlags |= 0x08u;

        lpInterface->miCurrentRecordReel = miCurrentRecordReel;
        lpInterface->miCurrentPlaybackReel = miCurrentPlaybackReel;
        if (meState == E_STREAM_STATE_PLAYING && mReadStream.mpStreamHeader != 0)
        {
            s32 liFrunkPosition = mReadStream.miCurrentFrunk -
                                  mReadStream.mpStreamHeader->miFirstFrunk;
            if (liFrunkPosition < 0)
                liFrunkPosition += KI_MAX_FRUNKS;
            const s32 liFramePosition = mReadStream.mpStreamHeader->miNumFrunks - liFrunkPosition;
            lpInterface->mfDebugHudAlpha = static_cast<f32>(liFramePosition) * 0.016666668f;
        }
        else
        {
            lpInterface->mfDebugHudAlpha = 0.0f;
        }
    }

    // ARTIST 0x8264B9C0 (the raw-image body; the named JSON export is absent). Fade the
    // recording overlay through the existing DebugInterface/DebugRender path. The X360
    // constants decode to {100.0, 255.0, (300,50), scale 16.0, fade step 0.02}.
    void ReplayModule::RenderDebugHUD()
    {
        if (mfConsoleValue625C <= 0.0f)
            return;

        CgsDev::DebugInterface lDebugInterface;
        CgsDev::DebugRender& lrRenderer = lDebugInterface.Get2dRender();
        const Vector2 lPosition(300.0f, 50.0f);

        // The PPC `ble` branch is not taken for an unordered comparison; spelling this
        // as !(value <= 0) preserves that NaN path from the X360 fcmpu.
        if (!(mfStopRecordingTime <= 0.0f))
        {
            char lacMessage[0x100];
            const f32 lfPercentComplete =
                ((mfCurrentRecordingTime - mfHistoryStartTime) * 100.0f) /
                (mfStopRecordingTime - mfHistoryStartTime);
            CgsCore::SPrintf(lacMessage, sizeof(lacMessage),
                             "Recording: %.1f complete", lfPercentComplete);

            const u8 luAlpha = static_cast<u8>(static_cast<s64>(mfConsoleValue625C * 255.0f));
            const CgsDev::RGBA luColour =
                (static_cast<CgsDev::RGBA>(luAlpha) << 24) | 0x0044FF44u;
            lrRenderer.Draw2DText(lacMessage, lPosition, 16.0f, luColour);
            mfConsoleValue625C = 0.0f;
        }
        else
        {
            const u8 luAlpha = static_cast<u8>(static_cast<s64>(mfConsoleValue625C * 255.0f));
            const CgsDev::RGBA luColour =
                (static_cast<CgsDev::RGBA>(luAlpha) << 24) | 0x004444FFu;
            lrRenderer.Draw2DText("Can't record - no space", lPosition, 16.0f, luColour);
            mfConsoleValue625C -= 0.02f;
        }
    }

    // ARTIST 0x82660430. The recording stage machine carries OpenFileResponse,
    // serialiser-clear, rolling-history, header-write, and close-completion legs.
    void ReplayModule::UpdateRecording_PreSim(const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
                                              ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                                              BrnUpdateSet lUpdateSet)
    {
        if (mbConsoleFlag6259)
        {
            s32 liUsedReels = 0;
            while (liUsedReels < 6 && maReels[liUsedReels].mbUsed)
                ++liUsedReels;
            if (liUsedReels == 6)
            {
                mfConsoleValue625C = 1.0f;
                mbConsoleFlag6259 = false;
            }
        }

        const CgsSystem::Time lCurrentTime =
            lpInputBuffer->GetTimerStatusInterface()->GetSimTimerStatus()->GetTime();
        mfCurrentRecordingTime = lCurrentTime.GetFloatVal();
        if (mfStopRecordingTime > 0.0f)
            mfConsoleValue625C = 1.0f;

        switch (meStreamStage)
        {
        case E_STREAM_STAGE_CLOSED:
            CGS_ASSERT(false,
                       "Should never have closed streams while recording (only ever opening, open or closing\n");
            break;

        case E_STREAM_STAGE_OPENING:
            ClearSerialisers(BaseSerialiser::E_MODE_RECORDING, true);
            if (WaitForOpenReplayFiles())
                meStreamStage = E_STREAM_STAGE_OPEN;
            break;

        case E_STREAM_STAGE_OPEN:
            ClearSerialisers(BaseSerialiser::E_MODE_RECORDING, true);
            if (mbStopRecording || mbConsoleFlag6259)
            {
                mWriteStream.ResetStartFrame(15.0f);
                mfHistoryStartTime = mfCurrentRecordingTime - 15.0f;
                mbConsoleFlag5C3E = true;
                mfStopRecordingTime = mfHistoryStartTime + 20.0f;
            }
            else if (mfStopRecordingTime > 0.0f &&
                     (mfCurrentRecordingTime > mfStopRecordingTime || mbConsoleFlag625A))
            {
                StopRecording(lpInputBuffer, lpOutputBuffer, lUpdateSet);
                if (mpFileStreamHeader->mpFrameOffsets != 0)
                {
                    mpFileStreamHeader->ChopOffTailFrames();
                    const std::ptrdiff_t liFrameOffsets =
                        reinterpret_cast<const char*>(mpFileStreamHeader->mpFrameOffsets) -
                        reinterpret_cast<const char*>(mpFileStreamHeader);
                    mpFileStreamHeader->mpFrameOffsets = reinterpret_cast<StreamOffset*>(liFrameOffsets);
                }
                mHeaderFile.Write(mpFileStreamHeader, 0, 0x20000);
            }
            else if (mbConsoleFlag238)
            {
                maReels[miCurrentRecordReel].mbUsed = false;
                StopRecording(lpInputBuffer, lpOutputBuffer, lUpdateSet);
            }
            break;

        case E_STREAM_STAGE_CLOSING:
            ClearSerialisers(BaseSerialiser::E_MODE_IDLE, true);
            if (!mWriteStream.mbEnded)
            {
                mWriteStream.mHeaderMalloc.FreeAll();
                mWriteStream.mHeaderMalloc.Destruct();
                mWriteStream.mpStreamHeader = 0;
                mWriteStream.mbEnded = true;
            }
            if (mHeaderFile.GetStatus() == CgsFileSystem::E_FILESTATE_OPEN)
            {
                CloseReplayFiles(lpOutputBuffer);
                meStreamStage = E_STREAM_STAGE_ERROR;
            }
            break;

        case E_STREAM_STAGE_ERROR:
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

    // DecFIGS-attested signature; ARTIST 0x826607C8. Stop the playback stream and
    // move the module to the close-wait stage. The guarded EnableJobThread call is
    // the original unsupported X360 assertion, preserved in the PC HardwareInit path.
    void ReplayModule::StopPlaying(const ReplayIO::InputBuffer_PreSim* /*lpInputBuffer*/,
                                   ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                                   BrnUpdateSet /*lUpdateSet*/)
    {
        if (meState != E_STREAM_STATE_PLAYING)
            CGS_ASSERT(false, "Can only stop playing from playing state\n");

        miCurrentPlaybackReel = -1;
        ClearSerialisers(BaseSerialiser::E_MODE_RESTORING, true);
        CloseReplayFiles(lpOutputBuffer);

        const bool lbEnableJobThread = mbConsoleFlag5C38;
        mReadStream.mpStreamHeader = 0;
        meStreamStage = E_STREAM_STAGE_ERROR;
        if (lbEnableJobThread)
            CgsSystem::HardwareInit::EnableJobThread();
    }

    // ARTIST 0x8264B968. The close leg completes only after the GPU writer is no longer
    // open and at least one file-close response is queued; then the receiver queue clears.
    bool ReplayModule::WaitForCloseReplayFiles()
    {
        if (mGpuWriteStream.GetStatus() == GPUDiskWriteStream::KI_STATE_OPEN ||
            mGameDataReceiverQueue.GetLength() < 1)
            return false;

        mGameDataReceiverQueue.Clear();
        return true;
    }

    // DecFIGS-attested signature; ARTIST 0x8264B518. Put every live serialiser into
    // restoring mode and clear its restored marker after the close leg completes.
    void ReplayModule::BeginRestoring(const ReplayIO::InputBuffer_PreSim* /*lpInputBuffer*/,
                                     ReplayIO::OutputBuffer_PreSim* /*lpOutputBuffer*/,
                                     BrnUpdateSet /*lUpdateSet*/)
    {
        meState = E_STREAM_STATE_RESTORING;
        for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
        {
            BaseSerialiser* lpSerialiser = mapSerialisers[liSerialiser];
            if (lpSerialiser != 0)
            {
                lpSerialiser->SetMode(BaseSerialiser::E_MODE_RESTORING);
                lpSerialiser->mbDataRestored = false;
            }
        }
    }

    // DecFIGS-attested signature; ARTIST 0x8264B570. Once the first frunk has been
    // staged and the initial-frame countdown has drained, publish the all-serialiser
    // readiness bit consumed by UpdatePlaying_PreSim.
    void ReplayModule::UpdatePlaying_PostSim(const ReplayIO::InputBuffer_PostSim* /*lpInputBuffer*/,
                                             ReplayIO::OutputBuffer_PostSim* /*lpOutputBuffer*/,
                                             BrnUpdateSet /*lUpdateSet*/)
    {
        if (meStreamStage != E_STREAM_STAGE_OPEN || !mbConsoleFlag5C32 || !mbConsoleFlag5C30 ||
            mbConsoleFlag5C31 || miConsoleValue914 > 0)
            return;

        bool lbAllReady = true;
        for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
        {
            BaseSerialiser* lpSerialiser = mapSerialisers[liSerialiser];
            if (lpSerialiser != 0 && (!lbAllReady || !lpSerialiser->mbDataReady))
                lbAllReady = false;
        }
        mbConsoleFlag5C31 = lbAllReady;
    }

    // ARTIST 0x826608A8. Read the replay header, stream its frunks, and drive the
    // original playback/restore state machine through the typed embedded ReadStream.
    void ReplayModule::UpdatePlaying_PreSim(const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
                                            ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                                            BrnUpdateSet lUpdateSet)
    {
        mfConsoleValue625C = 0.0f;

        if (meStreamStage == E_STREAM_STAGE_CLOSED)
        {
            CGS_ASSERT(false,
                       "Should never have closed streams while playing (only ever opening, open or closing\n");
            return;
        }

        if (meStreamStage == E_STREAM_STAGE_OPENING)
        {
            if (!WaitForOpenReplayFiles())
                return;

            mHeaderFile.Read(mpFileStreamHeader, 0, 0x20000);
            meStreamStage = E_STREAM_STAGE_CLOSING;
        }

        if (meStreamStage == E_STREAM_STAGE_CLOSING)
        {
            if (mHeaderFile.GetStatus() != CgsFileSystem::E_FILESTATE_OPEN)
                return;

            if (mpFileStreamHeader->mpFrameOffsets != 0)
            {
                const std::uintptr_t luHeaderBase =
                    reinterpret_cast<std::uintptr_t>(mpFileStreamHeader);
                const std::uintptr_t luFrameOffset =
                    reinterpret_cast<std::uintptr_t>(mpFileStreamHeader->mpFrameOffsets);
                mpFileStreamHeader->mpFrameOffsets =
                    reinterpret_cast<StreamOffset*>(luHeaderBase + luFrameOffset);
            }

            miConsoleValue918 =
                mReadStream.StartNewStream(mpFileStreamHeader, 0x20000, &mPrimaryDiskReadStream);
            mbConsoleFlag5C31 = false;
            mbConsoleFlag5C32 = false;
            meStreamStage = E_STREAM_STAGE_OPEN;
        }

        if (meStreamStage == E_STREAM_STAGE_ERROR)
        {
            if (WaitForCloseReplayFiles())
            {
                meStreamStage = E_STREAM_STAGE_CLOSED;
                BeginRestoring(lpInputBuffer, lpOutputBuffer, lUpdateSet);
            }
            return;
        }

        if (meStreamStage != E_STREAM_STAGE_OPEN)
            return;

        if (!mbConsoleFlag5C32)
        {
            const StreamOffset& lrFrunk =
                mReadStream.mpStreamHeader->mpFrameOffsets[mReadStream.miCurrentFrunk];
            const s32 liReadSize = (lrFrunk.miFrunkSize + 0xFFFF) & 0xFFFF0000;
            if (!mReadStream.mpStream->ReadBlock(lrFrunk.miFileOffset, 0, liReadSize))
                return;
        }

        mbConsoleFlag5C32 = true;
        mbConsoleFlag5C30 = true;

        FrunkReadResult lReadResult;
        lReadResult.mfTime = 0.0f;
        lReadResult.mxFlags = 0;
        for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
        {
            BaseSerialiser* lpSerialiser = mapSerialisers[liSerialiser];
            if (lpSerialiser != 0)
            {
                lReadResult.maiSizes[liSerialiser] = lpSerialiser->miBufferSize;
                lReadResult.mapBuffers[liSerialiser] = lpSerialiser->mpBuffer;
            }
        }

        if (mbConsoleFlag5C31)
        {
            if (mbConsoleFlag5C3C)
            {
                for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
                {
                    if (mapSerialisers[liSerialiser] != 0)
                        mapSerialisers[liSerialiser]->miBufferRead = 0;
                }
            }
            else
            {
                const bool lbReadFrunk = mReadStream.ReadCurrentFrunk(&lReadResult);
                if (!lbReadFrunk || (lReadResult.mxFlags & KU_FLAG_VOID) != 0)
                {
                    ClearSerialisers(BaseSerialiser::E_MODE_PLAYING_STALLED, true);
                    for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
                    {
                        if (mapSerialisers[liSerialiser] != 0)
                            mapSerialisers[liSerialiser]->miBufferRead = 0;
                    }

                    ++miConsoleValue918;
                    if (mReadStream.MoveToNextFrunk())
                    {
                        mbConsoleFlag5C3D = true;
                        mbConsoleFlag5C3C = true;
                    }
                }
                else
                {
                    ClearSerialisers(BaseSerialiser::E_MODE_PLAYING, true);
                    for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
                    {
                        BaseSerialiser* lpSerialiser = mapSerialisers[liSerialiser];
                        if (lpSerialiser != 0)
                        {
                            lpSerialiser->miBufferRead = 0;
                            lpSerialiser->miBufferUsed = lReadResult.maiSizes[liSerialiser];
                        }
                    }

                    ++miConsoleValue918;
                    if (mReadStream.MoveToNextFrunk())
                    {
                        mbConsoleFlag5C3C = true;
                        mbConsoleFlag5C3D = true;
                    }
                    if (mbConsoleFlag5C3B && (lReadResult.mxFlags & KU_FLAG_KEYFRAME) != 0)
                    {
                        mbConsoleFlag5C3B = false;
                        mbConsoleFlag5C3C = true;
                    }
                }
            }
        }
        else if (miConsoleValue914 == 10)
        {
            const bool lbReadFrunk = mReadStream.ReadCurrentFrunk(&lReadResult);
            CGS_ASSERT(lbReadFrunk, "mReadStream.ReadCurrentFrunk( &lReadResult )");
            CGS_ASSERT((lReadResult.mxFlags & KU_FLAG_VOID) == 0,
                       "First frame should never be VOID\n");
            CGS_ASSERT((lReadResult.mxFlags & KU_FLAG_KEYFRAME) != 0,
                       "First frame should always be key frame\n");

            ClearSerialisers(BaseSerialiser::E_MODE_PLAYING, true);
            for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
            {
                BaseSerialiser* lpSerialiser = mapSerialisers[liSerialiser];
                if (lpSerialiser != 0)
                {
                    lpSerialiser->miBufferRead = 0;
                    lpSerialiser->miBufferUsed = 0;
                    lpSerialiser->miBufferRead = 0;
                    lpSerialiser->miBufferUsed = lReadResult.maiSizes[liSerialiser];
                    lpSerialiser->mbDataReady = false;
                }
            }

            ++miConsoleValue918;
            --miConsoleValue914;
            if (mReadStream.MoveToNextFrunk())
            {
                mbConsoleFlag5C3D = true;
                mbConsoleFlag5C3C = true;
            }
        }
        else
        {
            for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
            {
                BaseSerialiser* lpSerialiser = mapSerialisers[liSerialiser];
                if (lpSerialiser != 0)
                {
                    lpSerialiser->miBufferRead = 0;
                    lpSerialiser->mbDataReady = false;
                }
            }
            --miConsoleValue914;
        }

        if (mbConsoleFlag6258)
        {
            mbConsoleFlag5C3A = true;
            StopPlaying(lpInputBuffer, lpOutputBuffer, lUpdateSet);
        }

        UnlockSerialisers();
        if (mbSerialiseActive)
        {
            mSerialiseJob.WaitOn(0, 0, -1);
            mCommandPoster.End();
            mbPosterActive = false;
        }
        mbSerialiseActive = false;
        LockSerialisers();
    }

    // ARTIST 0x82660E78. RenderDebugHUD was recovered from its raw-image target because
    // the named export JSON is missing; both direct callees now retain their original bodies.
    void ReplayModule::Update_PreSim(const ReplayIO::InputBuffer_PreSim* lpInputBuffer,
                                    ReplayIO::OutputBuffer_PreSim* lpOutputBuffer,
                                    BrnUpdateSet lUpdateSet)
    {
        CgsModule::LockBuffersForIO(lpOutputBuffer, const_cast<ReplayIO::InputBuffer_PreSim*>(lpInputBuffer));
        LockSerialisers();
        RenderDebugHUD();
        mDebugComponent.PreUpdateRecord();

        if (!mbSerialiseActive)
        {
            for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
            {
                if (mapSerialisers[liSerialiser] != 0)
                {
                    mapSerialisers[liSerialiser]->miBufferRead = 0;
                    mapSerialisers[liSerialiser]->miBufferUsed = 0;
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
            for (s32 liSerialiser = 0; liSerialiser < KI_NUM_SERIALISERS; ++liSerialiser)
            {
                BaseSerialiser* lpSerialiser = mapSerialisers[liSerialiser];
                if (lpSerialiser != 0)
                {
                    lpSerialiser->SetMode(BaseSerialiser::E_MODE_RESTORING);
                    lpSerialiser->mbDataRestored = false;
                }
            }
            break;
        default:
            CGS_ASSERT(false, "Invalid update state for replay module\n");
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
        CgsModule::UnlockBuffersForIO(lpOutputBuffer,
                                     const_cast<ReplayIO::InputBuffer_PreSim*>(lpInputBuffer));
    }

}
