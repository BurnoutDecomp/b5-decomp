// Production state bodies with injected frame state and observed service boundaries.
// This is not a ModuleSingleBuffered/base-lifecycle or end-to-end replay fixture.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <type_traits>
#include <windows.h>
#include "GameSource/Replays/BrnReplayModule.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static unsigned checks,failures,asserts,preCalls,postCalls,diskReads,diskCloses,gpuCloses;
static unsigned fileReads,fileWrites,waits,ends,draws,acquires,releases;
static bool transportReady=true,unlockedAtWait=true;
static CgsFileSystem::FileState fileStatus=CgsFileSystem::E_FILESTATE_OPEN;
static u32 observedFileId;
static void* fileBuffer;
static u64 filePosition,fileSize;
static s32 rangeStart,rangeEnd;
static std::string drawnText,jobOrder;
static f32 drawnX,drawnY,drawnScale;
static u32 drawnColour;
static unsigned char frunkBytes[65536],headerBytes[131072];
static BrnReplays::ReplayIO::InputBuffer_PreSim* observedInput;
static BrnReplays::ReplayIO::OutputBuffer_PreSim* observedOutput;
static void* observedModule;
static void Check(bool value,const char* label)
{
    ++checks;
    if (!value) {++failures;std::printf("FAIL %s\n",label);}
}
namespace CgsDev { namespace Assert {
int BeginAssert() {return 0;}
int FireAssert(const char*,const char*,int) {++asserts;return 0;}
void* EndAssert() {return nullptr;}
} namespace Log { DebugPrint* gpDebugPrint=nullptr; }
namespace Message {u64 gxMessageFilterFlags=0;}
DebugInterface::DebugInterface() {++acquires;mbIsAutomaticClass=true;mpDebugManager=nullptr;}
DebugInterface::~DebugInterface() {if(mbIsAutomaticClass)++releases;}
DebugRender& DebugInterface::Get2dRender() {static DebugRender render;return render;}
void DebugRender::Draw2DText(const char* text,f32 x,f32 y,f32 scale,RGBA colour)
{++draws;drawnText=text;drawnX=x;drawnY=y;drawnScale=scale;drawnColour=colour;}
}
extern "C" long RtlInitializeCriticalSection(void*);
namespace CgsFileSystem {
FileState FileSystem::GetStatus(u32 id) {observedFileId=id;return fileStatus;}
bool FileHandle::Read(void* buffer,u64 position,u64 size)
{
    ++fileReads;fileBuffer=buffer;filePosition=position;fileSize=size;
    std::memcpy(buffer,headerBytes,static_cast<size_t>(size));return true;
}
bool FileHandle::Write(const void* buffer,u64 position,u64 size)
{++fileWrites;fileBuffer=const_cast<void*>(buffer);filePosition=position;fileSize=size;return true;}
}
namespace BrnReplays {
struct ObservedDebug {void PreUpdateRecord();void PostUpdateRecord();};
bool DiskReadStream::ReadBlock(s32,void* output,s32 size)
{++diskReads;if(!transportReady)return false;if(output)std::memcpy(output,frunkBytes,size);return true;}
void DiskReadStream::SetRange(s32 start,s32 end) {rangeStart=start;rangeEnd=end;}
void DiskReadStream::Close() {++diskCloses;}
void GPUDiskWriteStream::Close() {++gpuCloses;}
}
#include "replay_presim_states.inc"

using namespace BrnReplays;
static_assert(std::is_same<decltype(ReplayModule::mSerialiseJob),EA::Jobs::Job>::value,
              "actual module embeds canonical Job");
static_assert(std::is_same<decltype(ReplayModule::mWriteStream),WriteStream>::value &&
              std::is_same<decltype(ReplayModule::mReadStream),ReadStream>::value,
              "actual module embeds canonical streams rather than header-pointer aliases");
static bool SerialisersLocked(bool locked)
{
    auto& module=*static_cast<ReplayModuleFixture*>(observedModule);
    for (auto* serialiser:module.mapSerialisers)
        if(serialiser && serialiser->mbLocked!=locked)return false;
    return true;
}
void BrnReplays::ObservedDebug::PreUpdateRecord()
{
    ++preCalls;
    Check(SerialisersLocked(true),"original debug pre-call follows all serialiser locks");
    Check(observedInput->IsBufferLockedForReading() && observedOutput->IsBufferLockedForWriting(),
          "producer IO bracket covers original pre-call");
}
void BrnReplays::ObservedDebug::PostUpdateRecord()
{
    ++postCalls;
    Check(SerialisersLocked(true),"original post-call precedes serialiser unlocks");
    Check(observedInput->IsBufferLockedForReading() && observedOutput->IsBufferLockedForWriting(),
          "producer IO bracket covers original post-call");
}
void EA::Jobs::Job::WaitOn(WaitOnCallback* callback,void* context,s32 sleep) const
{++waits;jobOrder+='W';unlockedAtWait&=SerialisersLocked(false)&&!callback&&!context&&sleep==-1;}
void CgsMemory::DataStreamCommandPoster::End()
{++ends;jobOrder+='E';unlockedAtWait&=SerialisersLocked(false);}

struct HeaderStore {StreamHeader header;StreamOffset offsets[KI_MAX_FRUNKS];};
static HeaderStore streamHeader;
static BaseSerialiser serialisers[3];
static unsigned char serialiserData[3][16],headerAllocation[256];
alignas(CgsFileSystem::FileSystem) static unsigned char fileSystemTag[sizeof(CgsFileSystem::FileSystem)];
static void Reset(ReplayModuleFixture& m,ReplayIO::InputBuffer_PreSim& input,ReplayIO::OutputBuffer_PreSim& output)
{
    observedModule=&m;observedInput=&input;observedOutput=&output;
    input.Construct();output.Construct();
    input.mTimerStatusInterface.mSimTimerStatus.mTime.SetSeconds(100);
    input.mTimerStatusInterface.mSimTimerStatus.mTime.SetFraction(0.25f);
    m.meState=E_STREAM_STATE_IDLE;m.meStreamStage=E_STREAM_STAGE_OPEN;m.meMode=ReplayModuleFixture::E_MODE_INGAME;
    m.miCurrentFrame=0x100000004LL;m.miKeyFrameInterval=10;m.mfCurrentTime=3.5f;
    m.mfStartTime=0;m.mfEndTime=0;m.mfDebugHudAlpha=0;m.miPlaybackPreRoll=10;
    m.mbReadyToPlay=false;m.mbPlaybackDataReady=false;m.mbStreamDataReady=true;
    m.mbJobThreadDisabled=false;m.mbPlayNext=false;m.mbPauseOnKeyFrame=false;
    m.mbPaused=false;m.mbPlaybackEnded=false;m.mbWaitingForRecordPause=false;
    m.mbStartPlaying=m.mbStopPlaying=m.mbStartRecording=m.mbStopRecording=false;
    m.mbMarkActionReplay=m.mbStartActionReplay=false;m.mbAutoStart=true;
    m.mbStopPlayingRequestReceived=m.mbStopRecordingRequestReceived=m.mbFinishRecordingRequested=false;
    m.miCurrentRecordReel=1;m.miCurrentPlayReel=2;
    m.mbSerialiseActive=false;m.mbPosterActive=false;
    for (s32 id=0;id<E_ID_COUNT;++id)m.mapSerialisers[id]=nullptr;
    for (s32 index=0;index<3;++index)
    {
        const s32 id=index==0?0:index==1?5:10;
        serialisers[index].Construct(id,0,16,0,"state-test",0);
        serialisers[index].SetBuffer(serialiserData[index]);
        serialisers[index].mbDataReady=false;serialisers[index].mbDataRestored=true;
        serialisers[index].miBufferUsed=8;serialisers[index].miBufferRead=7;
        m.mapSerialisers[id]=&serialisers[index];
    }
    for (s32 reel=0;reel<6;++reel)
    {std::snprintf(m.maReels[reel].macName,256,"reel%d",reel);m.maReels[reel].mbUsed=false;}
    m.mGameDataReceiverQueue.Construct();
    m.mHeaderFile.mpFileSystem=reinterpret_cast<CgsFileSystem::FileSystem*>(fileSystemTag);m.mHeaderFile.muFileId=7;
    m.mGpuWriteStream.miState=0;m.mDiskReadStream.meStatus=DiskReadStream::E_STATUS_OPEN;
    m.mDiskReadStream.miPendingOperationCount=0;
    std::memset(&streamHeader,0,sizeof(streamHeader));
    streamHeader.header.mpFrameOffsets=streamHeader.offsets;streamHeader.header.miNumFrunks=2;
    for (s32 slot=0;slot<3;++slot)
    {
        auto& offset=streamHeader.offsets[slot];offset.miFrameNumber=m.miCurrentFrame+slot;
        offset.miFileOffset=slot*65536;offset.miFrunkSize=65536;offset.mxFlags=KU_FLAG_KEYFRAME;
    }
    m.mReadStream.Construct();m.mReadStream.mpStreamHeader=&streamHeader.header;
    m.mReadStream.mpStream=&m.mDiskReadStream;m.mReadStream.miCurrentFrunk=0;
    m.mReadStream.miStallCount=0;m.mReadStream.miCurrentFrunkPos=0;
    m.mWriteStream.mpStreamHeader=&streamHeader.header;m.mWriteStream.mbEnded=true;
    m.mWriteStream.mHeaderMalloc.Construct();m.mWriteStream.mHeaderMalloc.Create(headerAllocation,sizeof(headerAllocation));
    m.mpcHeaderBuffer=reinterpret_cast<char*>(headerBytes);
    std::memset(frunkBytes,0,sizeof(frunkBytes));
    auto* header=reinterpret_cast<FrunkHeader*>(frunkBytes);header->miFrameNumber=m.miCurrentFrame;
    header->mxFlags=KU_FLAG_KEYFRAME;header->miNumSerialisers=3;
    auto* entries=reinterpret_cast<FrunkSerialiserEntry*>(frunkBytes+sizeof(FrunkHeader));
    entries[0]={0,0};entries[1]={5,4};entries[2]={10,0};
    const u32 value=0x12345678;std::memcpy(entries+3,&value,sizeof(value));
    transportReady=true;fileStatus=CgsFileSystem::E_FILESTATE_OPEN;
}
static void Frame(ReplayModuleFixture& m,ReplayIO::InputBuffer_PreSim& input,ReplayIO::OutputBuffer_PreSim& output)
{
    const unsigned oldPre=preCalls,oldPost=postCalls;
    m.Update_PreSim(&input,&output,0xFFFF);
    Check(preCalls==oldPre+1 && postCalls==oldPost+1,"each original producer call brackets both debug records");
    Check(!input.IsBufferLocked()&&!output.IsBufferLocked()&&SerialisersLocked(false),
          "producer returns with original IO and serialiser locks released");
}
int main()
{
    ReplayModuleFixture m;ReplayIO::InputBuffer_PreSim input;ReplayIO::OutputBuffer_PreSim output;
    Reset(m,input,output);
    serialisers[1].meMode=BaseSerialiser::E_MODE_PLAYING;
    output.mStatusInterface.mxStatusFlags=0x80;
    m.mbStartPlaying=m.mbStopPlaying=m.mbStartRecording=m.mbStopRecording=m.mbMarkActionReplay=m.mbStartActionReplay=true;
    Frame(m,input,output);
    Check(serialisers[1].miBufferUsed==0&&serialisers[1].miBufferRead==0
          &&serialisers[1].meMode==BaseSerialiser::E_MODE_PLAYING&&!serialisers[1].mbDataReady,
          "IDLE clears only original counters, retaining mode/readiness");
    Check(!m.mbStartPlaying&&!m.mbStopPlaying&&!m.mbStartRecording&&!m.mbStopRecording
          &&!m.mbMarkActionReplay&&!m.mbStartActionReplay&&m.mbAutoStart,
          "producer clears six original one-shot requests and retains auto-start");
    Check(output.mStatusInterface.mxStatusFlags==0x80&&output.mStatusInterface.miCurrentRecordReel==1
          &&output.mStatusInterface.miCurrentPlaybackReel==2
          &&std::strcmp(output.mStatusInterface.maReels[5].macName,"reel5")==0,
          "actual output publishes all six reels/indices without an IDLE flag facade");

    Reset(m,input,output);m.meState=E_STREAM_STATE_RESTORING;
    Frame(m,input,output);
    Check(serialisers[1].meMode==BaseSerialiser::E_MODE_RESTORING&&!serialisers[1].mbDataRestored
          &&serialisers[1].miBufferUsed==8&&serialisers[1].miBufferRead==7,
          "RESTORING selects real mode/restored flag while preserving counters");
    Check((output.mStatusInterface.mxStatusFlags&0x11)==0x11,"restore status retains original playing/waiting bits");

    Reset(m,input,output);m.meState=E_STREAM_STATE_RECORDING;m.mbStopRecording=true;
    Frame(m,input,output);
    Check(m.mfCurrentTime==100.25f&&m.mfStartTime==85.25f&&m.mfEndTime==105.25f&&m.mbWaitingForRecordPause,
          "record stop request retains original signed timer and fifteen-plus-twenty window");
    Check(serialisers[1].meMode==BaseSerialiser::E_MODE_RECORDING&&serialisers[1].mbDataReady
          &&serialisers[1].mbIsKeyFrame&&serialisers[1].mfTime==100.25f,
          "record clear publishes real mode, signed64 keyframe, time and readiness");
    Check((output.mStatusInterface.mxStatusFlags&6)==6,"record output reports original saving bit");
    Reset(m,input,output);m.meState=E_STREAM_STATE_RECORDING;m.mbStopRecordingRequestReceived=true;
    for(auto& reel:m.maReels)reel.mbUsed=true;
    Frame(m,input,output);
    Check(m.mfDebugHudAlpha==1&&!m.mbStopRecordingRequestReceived&&!m.mbWaitingForRecordPause
          &&(output.mStatusInterface.mxStatusFlags&8)!=0,"full reels report authored no-space state without starting a window");

    Reset(m,input,output);m.meState=E_STREAM_STATE_RECORDING;m.meMode=ReplayModuleFixture::E_MODE_PAUSEMENU;
    m.maReels[1].mbUsed=true;m.mbWaitingForRecordPause=true;
    Frame(m,input,output);
    Check(m.meStreamStage==E_STREAM_STAGE_HEADER&&m.miCurrentRecordReel==-1&&!m.maReels[1].mbUsed
          &&!m.mbWaitingForRecordPause,"pause-menu record stop clears original reel and waiting flag");
    Reset(m,input,output);m.meState=E_STREAM_STATE_RECORDING;m.mfEndTime=99;
    const unsigned oldWrites=fileWrites;Frame(m,input,output);
    Check(fileWrites==oldWrites+1&&fileSize==131072&&filePosition==0&&fileBuffer==headerBytes
          &&m.meStreamStage==E_STREAM_STAGE_HEADER,"expired recording writes original full header through real state path");

    Reset(m,input,output);m.meState=E_STREAM_STATE_RECORDING;m.meStreamStage=E_STREAM_STAGE_HEADER;
    m.mWriteStream.mbEnded=false;m.mGpuWriteStream.miState=2;
    const unsigned oldGpuCloses=gpuCloses,oldDiskCloses=diskCloses;
    Frame(m,input,output);
    Check(m.meStreamStage==E_STREAM_STAGE_CLOSING&&m.mWriteStream.mbEnded
          &&m.mWriteStream.mpStreamHeader==nullptr&&!m.mWriteStream.mHeaderMalloc.mbCreated,
          "header completion frees/destructs real allocator and starts close stage");
    Check(gpuCloses==oldGpuCloses+1&&diskCloses==oldDiskCloses+1,"close path targets both actual open stream backends");
    const CgsModule::Event* event=nullptr;s32 size=0;
    const auto& requests=output.mGameDataRequestInterface.mRequestQueue;
    const bool hasClose=requests.GetFirstEvent(&event,&size)==21&&event
                        &&size==sizeof(CgsResource::Events::CloseFileRequest);
    Check(hasClose,
          "original close request carries its canonical native payload size");
    bool closePayload=false;
    if(hasClose) // fixture isolation: report omitted producer output without dereferencing absent bytes
    {
        const auto* close=reinterpret_cast<const CgsResource::Events::CloseFileRequest*>(event);
        const auto file=close->GetFileHandle();
        closePayload=close->GetUser()==&m.mGameDataReceiverQueue&&close->GetEventId()==0
                     &&file.mpFileSystem==m.mHeaderFile.mpFileSystem&&file.muFileId==7;
    }
    Check(closePayload,"close request preserves receiver, event id and full native file handle");
    CgsResource::Events::OpenFileResponse response;response.Construct(&m.mGameDataReceiverQueue,0,m.mHeaderFile);
    m.mGameDataReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&response),21,sizeof(response));
    m.mGpuWriteStream.miState=0;Frame(m,input,output);
    Check(m.meState==E_STREAM_STATE_IDLE&&m.meStreamStage==E_STREAM_STAGE_CLOSED,
          "real close readiness returns recording to IDLE only after injected external response");

    Reset(m,input,output);m.meState=E_STREAM_STATE_PLAYING;m.mbPlaybackDataReady=false;m.miPlaybackPreRoll=10;
    const s64 firstFrame=m.miCurrentFrame;Frame(m,input,output);
    u32 payload=0;std::memcpy(&payload,serialiserData[1],4);
    Check(m.miCurrentFrame==firstFrame+1&&m.miPlaybackPreRoll==9&&m.mReadStream.miCurrentFrunk==1
          &&serialisers[1].miBufferUsed==4&&!serialisers[1].mbDataReady&&payload==0x12345678,
          "warmup reads real keyframe bytes, retains64-bit frame and withholds readiness");
    const unsigned readsBeforeHold=diskReads;Frame(m,input,output);
    Check(m.miPlaybackPreRoll==8&&m.miCurrentFrame==firstFrame+1&&diskReads==readsBeforeHold
          &&serialisers[1].miBufferUsed==4&&!serialisers[1].mbDataReady,
          "remaining warmup ticks retain payload and wait for actual post-sim readiness");

    Reset(m,input,output);m.meState=E_STREAM_STATE_PLAYING;m.mbPlaybackDataReady=true;
    m.mbPauseOnKeyFrame=true;m.mbSerialiseActive=true;m.mbPosterActive=true;
    const unsigned oldWaits=waits,oldEnds=ends;jobOrder.clear();Frame(m,input,output);
    Check(serialisers[1].meMode==BaseSerialiser::E_MODE_PLAYING&&serialisers[1].miBufferUsed==4
          &&serialisers[1].mbDataReady&&m.mbPaused&&!m.mbPauseOnKeyFrame,
          "ready playback publishes real payload and authored keyframe pause");
    Check(waits==oldWaits+1&&ends==oldEnds+1&&jobOrder=="WE"&&unlockedAtWait
          &&!m.mbSerialiseActive&&!m.mbPosterActive,"playback unlocks around original job wait/poster end then restores bracket");
    const s64 pausedFrame=m.miCurrentFrame;const unsigned beforePausedReads=diskReads;Frame(m,input,output);
    Check(m.miCurrentFrame==pausedFrame&&diskReads==beforePausedReads&&serialisers[1].miBufferRead==0
          &&serialisers[1].miBufferUsed==4,"paused playback rewinds read cursors without consuming another frunk");

    Reset(m,input,output);m.meState=E_STREAM_STATE_PLAYING;m.mbPlaybackDataReady=true;transportReady=false;
    m.mReadStream.miCurrentFrunk=1;Frame(m,input,output);
    Check(serialisers[1].meMode==BaseSerialiser::E_MODE_PLAYING_STALLED&&serialisers[1].miBufferUsed==0
          &&m.mbPaused&&m.mbPlaybackEnded&&m.mReadStream.miCurrentFrunk==0,
          "not-ready current frunk stalls and retains original wrap/pause behavior");
    Reset(m,input,output);m.meState=E_STREAM_STATE_PLAYING;m.mbPlaybackDataReady=true;
    streamHeader.offsets[0].mxFlags=KU_FLAG_VOID;const unsigned beforeVoidReads=diskReads;Frame(m,input,output);
    Check(diskReads==beforeVoidReads&&serialisers[1].meMode==BaseSerialiser::E_MODE_PLAYING_STALLED
          &&m.mReadStream.miCurrentFrunk==1,"VOID frunk stalls without transport I/O");
    Reset(m,input,output);m.meState=E_STREAM_STATE_PLAYING;m.mbStreamDataReady=false;transportReady=false;
    const s64 prefetchFrame=m.miCurrentFrame;Frame(m,input,output);
    Check(m.miCurrentFrame==prefetchFrame&&!m.mbReadyToPlay&&!m.mbStreamDataReady
          &&serialisers[1].miBufferUsed==8,"initial prefetch wait preserves unready original state");

    Reset(m,input,output);m.meState=E_STREAM_STATE_PLAYING;m.mbPlaybackDataReady=true;
    m.mbStopPlayingRequestReceived=true;Frame(m,input,output);
    Check(m.mbPlayNext&&m.meStreamStage==E_STREAM_STAGE_CLOSING&&m.mReadStream.mpStreamHeader==nullptr
          &&m.miCurrentPlayReel==-1,"real stop-playing request closes and retains its original next-play latch");
    response.Construct(&m.mGameDataReceiverQueue,0,m.mHeaderFile);
    m.mGameDataReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&response),21,sizeof(response));
    Frame(m,input,output);
    Check(m.meState==E_STREAM_STATE_RESTORING&&m.meStreamStage==E_STREAM_STAGE_CLOSED
          &&serialisers[1].meMode==BaseSerialiser::E_MODE_RESTORING&&!serialisers[1].mbDataRestored,
          "closing playback begins real restoring after external close response");

    Reset(m,input,output);m.meState=E_STREAM_STATE_PLAYING;m.meStreamStage=E_STREAM_STAGE_OPENING;
    m.mDiskReadStream.meStatus=DiskReadStream::E_STATUS_CLOSED;
    response.Construct(&m.mGameDataReceiverQueue,1,m.mHeaderFile);
    m.mGameDataReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&response),20,sizeof(response));
    Check(!m.WaitForOpenReplayFiles()&&m.mGameDataReceiverQueue.GetCount()==1,
          "open wait retains queue until actual stream/file readiness");
    m.mGpuWriteStream.miState=2;
    Check(m.WaitForOpenReplayFiles()&&m.mGameDataReceiverQueue.GetCount()==0&&observedFileId==7,
          "original OR readiness permits open GPU writer with closed reader and real file status");

    Reset(m,input,output);m.meState=E_STREAM_STATE_RECORDING;m.meStreamStage=E_STREAM_STAGE_OPENING;
    Frame(m,input,output);
    Check(m.meStreamStage==E_STREAM_STAGE_OPENING&&serialisers[1].meMode==BaseSerialiser::E_MODE_RECORDING
          &&serialisers[1].mbDataReady,"record opening clears real serialisers while retaining external wait");
    response.Construct(&m.mGameDataReceiverQueue,0,m.mHeaderFile);
    m.mGameDataReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&response),20,sizeof(response));
    Frame(m,input,output);
    Check(m.meStreamStage==E_STREAM_STAGE_OPEN&&m.mGameDataReceiverQueue.GetCount()==0,
          "record opening advances only after typed response and real stream/file readiness");

    Reset(m,input,output);m.meState=E_STREAM_STATE_PLAYING;m.meStreamStage=E_STREAM_STAGE_OPENING;
    auto* diskHeader=reinterpret_cast<StreamHeader*>(headerBytes);
    *diskHeader=streamHeader.header;
    diskHeader->mpFrameOffsets=reinterpret_cast<StreamOffset*>(64);
    std::memcpy(headerBytes+64,streamHeader.offsets,3*sizeof(StreamOffset));
    response.Construct(&m.mGameDataReceiverQueue,0,m.mHeaderFile);
    m.mGameDataReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&response),20,sizeof(response));
    const unsigned beforeHeaderReads=fileReads;const s64 headerFrame=m.miCurrentFrame;
    Frame(m,input,output);
    Check(fileReads==beforeHeaderReads+1&&fileSize==131072&&filePosition==0
          &&m.meStreamStage==E_STREAM_STAGE_OPEN&&m.miCurrentFrame==headerFrame+1,
          "playback opening reads authored header and enters actual warmup in the same update");
    Check(diskHeader->mpFrameOffsets==reinterpret_cast<StreamOffset*>(headerBytes+64)
          &&rangeStart==0&&rangeEnd==196608&&m.miPlaybackPreRoll==9&&!m.mbPlaybackDataReady,
          "real header fixup, first-plus-count disk range and warmup preserve original behavior");
    Reset(m,input,output);m.meState=E_STREAM_STATE_PLAYING;m.meStreamStage=E_STREAM_STAGE_HEADER;
    m.mReadStream.mpStreamHeader=nullptr;fileStatus=CgsFileSystem::E_FILESTATE_READING;
    Frame(m,input,output);
    Check(m.meStreamStage==E_STREAM_STAGE_HEADER&&!m.mbReadyToPlay,
          "header I/O wait remains explicit and manufactures no ready playback state");

    Reset(m,input,output);m.mfDebugHudAlpha=0.5f;m.mfStartTime=10;m.mfEndTime=20;m.mfCurrentTime=15;
    const unsigned beforeDraws=draws;m.RenderDebugHUD();
    Check(draws==beforeDraws+1&&drawnText=="Recording: 50.0 complete"&&drawnX==300&&drawnY==50
          &&drawnScale==16&&drawnColour==0x7F44FF44&&m.mfDebugHudAlpha==0,
          "actual HUD body retains authored percent, position, colour and reset");
    m.mfDebugHudAlpha=0.5f;m.mfEndTime=0;m.RenderDebugHUD();
    Check(drawnText=="Can't record - no space"&&drawnColour==0x7F4444FF
          &&std::fabs(m.mfDebugHudAlpha-0.48f)<0.000001f&&acquires==releases,
          "actual no-space HUD body retains authored fade and automatic release");

    Reset(m,input,output);m.meState=static_cast<EStreamState>(99);
    const unsigned beforeInvalid=asserts;Frame(m,input,output);
    Check(asserts==beforeInvalid+1,"invalid original state asserts while preserving cleanup bracket");
    streamHeader.header.mpFrameOffsets=streamHeader.offsets;streamHeader.header.miFirstFrunk=0;
    streamHeader.header.miNumFrunks=3;streamHeader.offsets[0].mxFlags=KU_FLAG_KEYFRAME;
    streamHeader.offsets[1].mxFlags=KU_FLAG_VOID;streamHeader.offsets[2].mxFlags=0;
    streamHeader.header.FixDown();
    Check(streamHeader.header.miNumFrunks==1
          &&reinterpret_cast<uintptr_t>(streamHeader.header.mpFrameOffsets)==offsetof(HeaderStore,offsets),
          "original fixdown trims unreplayable tail and stores native relative pointer");
    streamHeader.header.FixUp();
    Check(streamHeader.header.mpFrameOffsets==streamHeader.offsets
          &&streamHeader.offsets[0].miFrameNumber==0x100000004LL,
          "header roundtrip restores actual native index pointer and64-bit frame data");
    DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m.mDiskReadStream.mMutex));
    DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m.mGpuWriteStream.maLock));
    std::printf("ReplayPreSimStates: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
