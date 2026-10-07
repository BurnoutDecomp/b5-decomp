#include "GameSource/Replays/BrnReplayModule.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"
#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>

namespace BrnReplays
{
    // ARTIST 827E03D0: contained GPU relocator job/lock and disk-reader lock
    // construct in their own real objects before the four module jobs.
    ReplayModule::ReplayModule()
        : maSerialiseJobs{EA::Jobs::Job(nullptr),EA::Jobs::Job(nullptr),
                          EA::Jobs::Job(nullptr),EA::Jobs::Job(nullptr)},
          mSerialiseJob(nullptr)
    {
        mGameEventCache.MarkUnconstructed();
    }

    // ARTIST 82656E20; selective two-phase initialization, not blanket zeroing.
    void ReplayModule::Construct()
    {
        CgsModule::ModuleSingleBuffered::Construct();
        meReleaseStage=E_RELEASESTAGE_DONE;
        mePrepareStage=E_PREPARESTAGE_START;
        meState=E_STREAM_STATE_IDLE;
        CGS_ASSERT(BrnResource::Allocators::mpInternalDebugAllocator,"Allocators::mpInternalDebugAllocator");
        mDebugComponent.Construct(this,BrnResource::GetDebugAllocator());
        mbStartPlaying=false;mbStopPlaying=false;
        mbStartRecording=false;mbStopRecording=false;
        mfCurrentTime=0.0f;mbMarkActionReplay=false;
        mfDebugHudAlpha=0.0f;mbStartActionReplay=false;mbAutoStart=true;
        mpLinearMalloc=nullptr;mpcPrimaryStreamBuffer=nullptr;
        mpcHeaderStreamBuffer=nullptr;mpSecondaryStreamBuffer=nullptr;
        miCurrentFrame=0;miKeyFrameInterval=10;meMode=E_MODE_INGAME;
        miCurrentRecordReel=-1;miCurrentPlayReel=-1;
        mbStopPlayingRequestReceived=false;mbWaitingForRecordPause=false;
        mGameEventCache.Construct();
        mGameDataReceiverQueue.Construct();
        mWriteStream.mHeaderMalloc.Construct();
        mWriteStream.mpStreamHeader=nullptr;mWriteStream.miStallCount=0;
        mWriteStream.miFilePosition=0;mWriteStream.mbEnded=true;
        mWriteStream.mbPaused=false;mWriteStream.miNumFrunksAllocated=0;
        mWriteStream.miBufferPosition=0;
        mReadStream.Construct();
        mGpuWriteStream.Construct();mDiskReadStream.Construct();
        meStreamStage=E_STREAM_STAGE_CLOSED;
        for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId) mapSerialisers[liId]=nullptr;
        for (s32 liReel=0;liReel<6;++liReel)
        {
            maReels[liReel].macName[0]='\0';maReels[liReel].mbUsed=false;
        }
        mbIsNewModule=true;
        mbPaused=false;mbPauseOnKeyFrame=false;miLastRecordFrame=0;
    }

    void ReplayModule::Update_Dispatch() { mGpuWriteStream.Dispatch(); }

    // ARTIST 8264B4A8, also inlined after the playback update releases serialisers.
    void ReplayModule::WaitForSerialiseJobs()
    {
        if (mbSerialiseActive)
        {
            mSerialiseJob.WaitOn(nullptr,nullptr,-1);
            mCommandPoster.End();mbPosterActive=false;
        }
        mbSerialiseActive=false;
    }

    void ReplayModule::LockSerialisers()
    {
        for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
            if (mapSerialisers[liId]) mapSerialisers[liId]->Lock();
    }
    void ReplayModule::UnlockSerialisers()
    {
        for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
            if (mapSerialisers[liId]) mapSerialisers[liId]->Unlock();
    }

    // ARTIST 8264E630 uses signed divd on the full frame and signed interval.
    void ReplayModule::ClearSerialisers(BaseSerialiser::EMode leMode,bool lbAllowStreaming)
    {
        const bool lbKeyFrame=(miCurrentFrame % miKeyFrameInterval)==0;
        for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
            if (BaseSerialiser* lpSerialiser=mapSerialisers[liId])
            {
                lpSerialiser->mbDataReady=true;
                lpSerialiser->meMode=leMode;
                lpSerialiser->mbIsKeyFrame=lbKeyFrame;
                lpSerialiser->mfTime=mfCurrentTime;
                lpSerialiser->mbAllowStreaming=lbAllowStreaming;
                lpSerialiser->miBufferRead=0;lpSerialiser->miBufferUsed=0;
            }
    }

    // Raw ARTIST 8264B9C0..8264BB68. Buffered debug output and RAII release
    // follow the original path; no renderer-null substitute is introduced.
    void ReplayModule::RenderDebugHUD()
    {
        if (!(mfDebugHudAlpha>0.0f)) return;
        CgsDev::DebugInterface lDebug;
        CgsDev::DebugRender& lrRender=lDebug.Get2dRender();
        const u32 luAlpha=static_cast<u8>(static_cast<s64>(mfDebugHudAlpha*255.0f));
        if (mfEndTime>0.0f)
        {
            const f32 lfPercent=((mfCurrentTime-mfStartTime)*100.0f)/(mfEndTime-mfStartTime);
            char lacText[256];std::snprintf(lacText,sizeof(lacText),"Recording: %.1f complete",lfPercent);
            lrRender.Draw2DText(lacText,300.0f,50.0f,16.0f,(luAlpha<<24)|0x0044FF44u);
            mfDebugHudAlpha=0.0f;
        }
        else
        {
            lrRender.Draw2DText("Can't record - no space",300.0f,50.0f,16.0f,(luAlpha<<24)|0x004444FFu);
            mfDebugHudAlpha-=0.02f;
        }
    }

    // ARTIST 8264E470. In IDLE the existing status bits are preserved.
    void ReplayModule::SetStatusInterface(ReplayIO::StatusInterface* lpStatus)
    {
        if (meState==E_STREAM_STATE_RECORDING)
        {
            lpStatus->mxStatusFlags=(lpStatus->mxStatusFlags&0xFFFFFF8Du)|2u;
            if (mbWaitingForRecordPause) lpStatus->mxStatusFlags|=4u;
            else lpStatus->mxStatusFlags&=~4u;
        }
        else if (meState==E_STREAM_STATE_PLAYING || meState==E_STREAM_STATE_RESTORING)
        {
            lpStatus->mxStatusFlags|=meState==E_STREAM_STATE_RESTORING?0x11u:1u;
            if (meState==E_STREAM_STATE_PLAYING)
            {
                if (mbPlaybackDataReady) lpStatus->mxStatusFlags&=~0x10u;
                else lpStatus->mxStatusFlags|=0x10u;
            }
            if (mbPaused) lpStatus->mxStatusFlags|=0x20u;
            else lpStatus->mxStatusFlags&=~0x20u;
            if (mbPlaybackEnded) lpStatus->mxStatusFlags|=0x40u;
            else lpStatus->mxStatusFlags&=~0x40u;
            lpStatus->mxStatusFlags&=~4u;
        }
        s32 liUsed=0;
        for (s32 liReel=0;liReel<6;++liReel)
        {
            lpStatus->SetReel(liReel,&maReels[liReel]);
            if (maReels[liReel].mbUsed) ++liUsed;
        }
        if (liUsed==6) lpStatus->mxStatusFlags|=8u;
        lpStatus->miCurrentRecordReel=miCurrentRecordReel;
        lpStatus->miCurrentPlaybackReel=miCurrentPlayReel;
        if (meState==E_STREAM_STATE_PLAYING && mReadStream.mpStreamHeader)
        {
            s32 liOffset=mReadStream.miCurrentFrunk-mReadStream.mpStreamHeader->miFirstFrunk;
            if (liOffset<0) liOffset+=KI_MAX_FRUNKS;
            // The existing canonical tail name is inferred; this original store
            // publishes remaining playback seconds, not the module's HUD opacity.
            lpStatus->mfDebugHudAlpha=(mReadStream.mpStreamHeader->miNumFrunks-liOffset)*0.016666668f;
        }
        else lpStatus->mfDebugHudAlpha=0.0f;
    }

    // ARTIST 8264E8F8. Payload+4 is the inherited event id, not a result field.
    bool ReplayModule::WaitForOpenReplayFiles()
    {
        if (mGameDataReceiverQueue.GetCount()<1) return false;
        const CgsModule::Event* lpEvent=nullptr;s32 liSize=0;
        s32 liType=mGameDataReceiverQueue.GetFirstEvent(&lpEvent,&liSize);
        while (liType!=-1)
        {
            if (liType==20)
            {
                const auto* lpResponse=reinterpret_cast<const CgsResource::Events::OpenFileResponse*>(lpEvent);
                if (lpResponse->GetEventId()==0) mHeaderFile=lpResponse->GetFileHandle();
            }
            const CgsModule::Event* lpNext=nullptr;
            liType=mGameDataReceiverQueue.GetNextEvent(lpEvent,&lpNext,&liSize);
            lpEvent=lpNext;
        }
        if (((mDiskReadStream.miPendingOperationCount>0 || mDiskReadStream.meStatus!=DiskReadStream::E_STATUS_OPEN)
             && mGpuWriteStream.GetStatus()!=GPUDiskWriteStream::KI_STATE_OPEN)
            || mHeaderFile.GetStatus()!=CgsFileSystem::E_FILESTATE_OPEN)
            return false;
        mGameDataReceiverQueue.Clear();return true;
    }

    // ARTIST 8265F228. A real typed request is posted even when neither stream
    // is open; external resource tags20/21 processing is a separate dependency.
    void ReplayModule::CloseReplayFiles(ReplayIO::OutputBuffer_PreSim* lpOutput)
    {
        CgsResource::Events::CloseFileRequest lRequest;
        lRequest.Construct(&mGameDataReceiverQueue,0,mHeaderFile);
        lpOutput->GetGameDataRequestInterface()->mRequestQueue.AddEvent(&lRequest,21);
        if (mDiskReadStream.miPendingOperationCount<=0 && mDiskReadStream.meStatus==DiskReadStream::E_STATUS_OPEN)
            mDiskReadStream.Close();
        if (mGpuWriteStream.GetStatus()==GPUDiskWriteStream::KI_STATE_OPEN) mGpuWriteStream.Close();
        mGameDataReceiverQueue.Clear();
    }

    bool ReplayModule::WaitForCloseReplayFiles()
    {
        if (mGpuWriteStream.GetStatus()==GPUDiskWriteStream::KI_STATE_OPEN || mGameDataReceiverQueue.GetCount()<1)
            return false;
        mGameDataReceiverQueue.Clear();return true;
    }

    void ReplayModule::BeginRestoring(const ReplayIO::InputBuffer_PreSim*,ReplayIO::OutputBuffer_PreSim*,BrnUpdateSet)
    {
        meState=E_STREAM_STATE_RESTORING;
        for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
            if (BaseSerialiser* lpSerialiser=mapSerialisers[liId])
            {
                lpSerialiser->SetMode(BaseSerialiser::E_MODE_RESTORING);
                lpSerialiser->mbDataRestored=false;
            }
    }

    void ReplayModule::StopRecording(const ReplayIO::InputBuffer_PreSim*,ReplayIO::OutputBuffer_PreSim*,BrnUpdateSet)
    {
        CGS_ASSERT(meState==E_STREAM_STATE_RECORDING,"Can only stop recording from recording state\n");
        ClearSerialisers(BaseSerialiser::E_MODE_IDLE,true);
        mbWaitingForRecordPause=false;meStreamStage=E_STREAM_STAGE_HEADER;miCurrentRecordReel=-1;
    }
    void ReplayModule::StopPlaying(const ReplayIO::InputBuffer_PreSim*,ReplayIO::OutputBuffer_PreSim* lpOutput,BrnUpdateSet)
    {
        CGS_ASSERT(meState==E_STREAM_STATE_PLAYING,"Can only stop playing from playing state\n");
        miCurrentPlayReel=-1;
        ClearSerialisers(BaseSerialiser::E_MODE_RESTORING,true);
        CloseReplayFiles(lpOutput);mReadStream.mpStreamHeader=nullptr;
        meStreamStage=E_STREAM_STAGE_CLOSING;
        // Original HardwareInit::EnableJobThread 828D6138 is an unconditional
        // unsupported-platform assert. De-inline that exact body on this path.
        if (mbJobThreadDisabled) CGS_ASSERT(false,"Not supported on xbox\n");
    }

    void ReplayModule::UpdateRecording_PreSim(const ReplayIO::InputBuffer_PreSim* lpInput,
                                              ReplayIO::OutputBuffer_PreSim* lpOutput,BrnUpdateSet leUpdateSet)
    {
        if (mbStopRecordingRequestReceived)
        {
            s32 liUsed=0;while (liUsed<6 && maReels[liUsed].mbUsed) ++liUsed;
            if (liUsed==6) {mfDebugHudAlpha=1.0f;mbStopRecordingRequestReceived=false;}
        }
        const CgsSystem::Time& lrTime=lpInput->GetTimerStatusInterface()->GetSimTimerStatus()->GetTime();
        mfCurrentTime=static_cast<f32>(lrTime.GetSeconds())+lrTime.GetFraction();
        if (mfEndTime>0.0f) mfDebugHudAlpha=1.0f;
        switch (meStreamStage)
        {
        case E_STREAM_STAGE_CLOSED:
            CGS_ASSERT(false,"Should never have closed streams while recording (only ever opening, open or closing\n");
            break;
        case E_STREAM_STAGE_OPENING:
            ClearSerialisers(BaseSerialiser::E_MODE_RECORDING,true);
            if (WaitForOpenReplayFiles()) meStreamStage=E_STREAM_STAGE_OPEN;
            break;
        case E_STREAM_STAGE_OPEN:
            ClearSerialisers(BaseSerialiser::E_MODE_RECORDING,true);
            if (mbStopRecording || mbStopRecordingRequestReceived)
            {
                mWriteStream.ResetStartFrame(15.0f);
                mfStartTime=mfCurrentTime-15.0f;mbWaitingForRecordPause=true;
                mfEndTime=mfStartTime+20.0f;
            }
            else if (mfEndTime>0.0f && (mfCurrentTime>mfEndTime || mbFinishRecordingRequested))
            {
                StopRecording(lpInput,lpOutput,leUpdateSet);
                reinterpret_cast<StreamHeader*>(mpcHeaderBuffer)->FixDown();
                mHeaderFile.Write(mpcHeaderBuffer,0,0x20000);
            }
            else if (meMode!=E_MODE_INGAME)
            {
                maReels[miCurrentRecordReel].mbUsed=false;
                StopRecording(lpInput,lpOutput,leUpdateSet);
            }
            break;
        case E_STREAM_STAGE_HEADER:
            ClearSerialisers(BaseSerialiser::E_MODE_IDLE,true);
            if (!mWriteStream.mbEnded)
            {
                mWriteStream.mHeaderMalloc.FreeAll();mWriteStream.mHeaderMalloc.Destruct();
                mWriteStream.mpStreamHeader=nullptr;mWriteStream.mbEnded=true;
            }
            if (mHeaderFile.GetStatus()==CgsFileSystem::E_FILESTATE_OPEN)
            {CloseReplayFiles(lpOutput);meStreamStage=E_STREAM_STAGE_CLOSING;}
            break;
        case E_STREAM_STAGE_CLOSING:
            ClearSerialisers(BaseSerialiser::E_MODE_IDLE,true);
            if (WaitForCloseReplayFiles()) {meState=E_STREAM_STATE_IDLE;meStreamStage=E_STREAM_STAGE_CLOSED;}
            break;
        default: break;
        }
    }

    void ReplayModule::UpdatePlaying_PreSim(const ReplayIO::InputBuffer_PreSim* lpInput,
                                            ReplayIO::OutputBuffer_PreSim* lpOutput,BrnUpdateSet leUpdateSet)
    {
        mfDebugHudAlpha=0.0f;
        switch (meStreamStage)
        {
        case E_STREAM_STAGE_CLOSED:
            CGS_ASSERT(false,"Should never have closed streams while playing (only ever opening, open or closing\n");
            return;
        case E_STREAM_STAGE_OPENING:
            if (!WaitForOpenReplayFiles()) return;
            mHeaderFile.Read(mpcHeaderBuffer,0,0x20000);meStreamStage=E_STREAM_STAGE_HEADER;
            [[fallthrough]];
        case E_STREAM_STAGE_HEADER:
            if (mHeaderFile.GetStatus()!=CgsFileSystem::E_FILESTATE_OPEN) return;
            reinterpret_cast<StreamHeader*>(mpcHeaderBuffer)->FixUp();
            miCurrentFrame=mReadStream.StartNewStream(mpcHeaderBuffer,0x20000,&mDiskReadStream);
            mbPlaybackDataReady=false;mbStreamDataReady=false;meStreamStage=E_STREAM_STAGE_OPEN;
            [[fallthrough]];
        case E_STREAM_STAGE_OPEN: break;
        case E_STREAM_STAGE_CLOSING:
            if (WaitForCloseReplayFiles()) {meStreamStage=E_STREAM_STAGE_CLOSED;BeginRestoring(lpInput,lpOutput,leUpdateSet);}
            return;
        default: return;
        }
        if (!mbStreamDataReady)
        {
            const StreamOffset& lrOffset=mReadStream.mpStreamHeader->mpFrameOffsets[mReadStream.miCurrentFrunk];
            if (!mReadStream.mpStream->ReadBlock(lrOffset.miFileOffset,nullptr,(lrOffset.miFrunkSize+0xFFFF)&~0xFFFF)) return;
        }
        mbStreamDataReady=true;mbReadyToPlay=true;
        FrunkReadResult lReadResult;
        for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
            if (BaseSerialiser* lpSerialiser=mapSerialisers[liId])
            {
                lReadResult.mapBuffers[liId]=lpSerialiser->mpBuffer;
                lReadResult.maiSizes[liId]=lpSerialiser->miBufferSize;
            }
        if (mbPlaybackDataReady)
        {
            if (mbPaused)
            {
                for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
                    if (mapSerialisers[liId]) mapSerialisers[liId]->miBufferRead=0;
            }
            else if (!mReadStream.ReadCurrentFrunk(&lReadResult) || (lReadResult.mxFlags&KU_FLAG_VOID)!=0)
            {
                ClearSerialisers(BaseSerialiser::E_MODE_PLAYING_STALLED,true);
                for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
                    if (mapSerialisers[liId]) mapSerialisers[liId]->miBufferRead=0;
                ++miCurrentFrame;
                if (mReadStream.MoveToNextFrunk()) {mbPlaybackEnded=true;mbPaused=true;}
            }
            else
            {
                ClearSerialisers(BaseSerialiser::E_MODE_PLAYING,true);
                for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
                    if (BaseSerialiser* lpSerialiser=mapSerialisers[liId])
                    {lpSerialiser->miBufferRead=0;lpSerialiser->miBufferUsed=lReadResult.maiSizes[liId];}
                ++miCurrentFrame;
                if (mReadStream.MoveToNextFrunk()) {mbPlaybackEnded=true;mbPaused=true;}
                if (mbPauseOnKeyFrame && (lReadResult.mxFlags&KU_FLAG_KEYFRAME)!=0)
                {mbPauseOnKeyFrame=false;mbPaused=true;}
            }
        }
        else if (miPlaybackPreRoll==10)
        {
            CGS_ASSERT(mReadStream.ReadCurrentFrunk(&lReadResult),"mReadStream.ReadCurrentFrunk( &lReadResult )");
            CGS_ASSERT((lReadResult.mxFlags&KU_FLAG_VOID)==0,"First frame should never be VOID\n");
            CGS_ASSERT((lReadResult.mxFlags&KU_FLAG_KEYFRAME)!=0,"First frame should always be key frame\n");
            ClearSerialisers(BaseSerialiser::E_MODE_PLAYING,true);
            for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
                if (BaseSerialiser* lpSerialiser=mapSerialisers[liId])
                {
                    lpSerialiser->miBufferRead=0;lpSerialiser->miBufferUsed=0;
                    lpSerialiser->miBufferRead=0;lpSerialiser->miBufferUsed=lReadResult.maiSizes[liId];
                    lpSerialiser->mbDataReady=false;
                }
            ++miCurrentFrame;--miPlaybackPreRoll;
            if (mReadStream.MoveToNextFrunk()) {mbPlaybackEnded=true;mbPaused=true;}
        }
        else
        {
            for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
                if (BaseSerialiser* lpSerialiser=mapSerialisers[liId])
                {lpSerialiser->miBufferRead=0;lpSerialiser->mbDataReady=false;}
            --miPlaybackPreRoll;
        }
        if (mbStopPlayingRequestReceived)
        {mbPlayNext=true;StopPlaying(lpInput,lpOutput,leUpdateSet);}
        UnlockSerialisers();WaitForSerialiseJobs();LockSerialisers();
    }

    // ARTIST 82660E78, exact producer bracket and original state dispatch.
    void ReplayModule::Update_PreSim(const ReplayIO::InputBuffer_PreSim* lpInput,
                                     ReplayIO::OutputBuffer_PreSim* lpOutput,BrnUpdateSet leUpdateSet)
    {
        // Expand the original 823B6FE0 lock helper, including its assertions.
        CGS_ASSERT(lpOutput,"lpInputBuffer");CGS_ASSERT(lpInput,"lpOutputBuffer0");
        lpOutput->LockForWrite();lpInput->LockForRead();
        LockSerialisers();RenderDebugHUD();mDebugComponent.PreUpdateRecord();
        if (meState==E_STREAM_STATE_IDLE)
            for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
                if (BaseSerialiser* lpSerialiser=mapSerialisers[liId])
                {lpSerialiser->miBufferRead=0;lpSerialiser->miBufferUsed=0;}
        switch (meState)
        {
        case E_STREAM_STATE_IDLE: break;
        case E_STREAM_STATE_RECORDING:UpdateRecording_PreSim(lpInput,lpOutput,leUpdateSet);break;
        case E_STREAM_STATE_PLAYING:UpdatePlaying_PreSim(lpInput,lpOutput,leUpdateSet);break;
        case E_STREAM_STATE_RESTORING:
            for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
                if (BaseSerialiser* lpSerialiser=mapSerialisers[liId])
                {lpSerialiser->SetMode(BaseSerialiser::E_MODE_RESTORING);lpSerialiser->mbDataRestored=false;}
            break;
        default:CGS_ASSERT(false,"Invalid update state for replay module\n");break;
        }
        mbStartPlaying=false;mbStartRecording=false;mbStopPlaying=false;
        mbStopRecording=false;mbMarkActionReplay=false;mbStartActionReplay=false;
        SetStatusInterface(lpOutput->GetStatusInterface());mDebugComponent.PostUpdateRecord();
        UnlockSerialisers();
        // Original reverse helper 823B7060 retains the same pointer assertions.
        CGS_ASSERT(lpOutput,"lpInputBuffer");CGS_ASSERT(lpInput,"lpOutputBuffer0");
        lpInput->UnlockForRead();lpOutput->UnlockForWrite();
    }
}
