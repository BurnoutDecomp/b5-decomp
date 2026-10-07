#include <cstdio>
#include <cstring>
#include <new>
#include "GameSource/Replays/Stream/BrnReplayReadStream.h"
#include "GameSource/Replays/Stream/BrnReplayDiskReadStream.h"
#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static unsigned checks, failures, asserts, reads;
static bool ready=true;
static s32 observedStart,observedEnd,observedPosition,observedSize;
static u8 diskBytes[BrnReplays::ReadStream::KI_INTERMEDIATEBUFFERSIZE];
static void Check(bool value,const char* label)
{
    ++checks;
    if (!value) { ++failures;std::printf("FAIL %s\n",label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { ++asserts;return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint=nullptr; }
namespace Message { u64 gxMessageFilterFlags=0; } }
// Disk readiness and delivered bytes are observed dependency boundaries. All
// stream parsing, cursor updates, result construction and ring logic are real.
void BrnReplays::DiskReadStream::SetRange(s32 start,s32 end)
{ observedStart=start;observedEnd=end; }
bool BrnReplays::DiskReadStream::ReadBlock(s32 position,void* destination,s32 size)
{
    ++reads;observedPosition=position;observedSize=size;
    if (!ready) return false;
    std::memcpy(destination,diskBytes,size);
    return true;
}
#include "replay_read_stream.inc"
int BrnReplayWriteStream_embed_check();

using namespace BrnReplays;
int main()
{
    static_assert(sizeof(StreamOffset)==24 && offsetof(StreamOffset,miFrameNumber)==0 &&
                  offsetof(StreamOffset,miFileOffset)==8 && offsetof(StreamOffset,miFrunkSize)==12,
                  "original writer's independent frame and disk fields");
    alignas(FrunkReadResult) unsigned char resultStorage[sizeof(FrunkReadResult)];
    std::memset(resultStorage,0x5A,sizeof(resultStorage));
    auto* result=new(resultStorage) FrunkReadResult;
    Check(resultStorage[0]==0x5A && resultStorage[4]==0x5A && result->mHeader.macMagicNumber[0]==0x5A,
          "result construction preserves unpublished time, flags and header magic");
    Check(result->mHeader.miFrameNumber==0 && result->mHeader.mxFlags==0 &&
          result->mHeader.mfFrameTime==0 && result->mHeader.miNumSerialisers==0,
          "result construction resets the four original header fields");
    for (s32 id=0;id<E_ID_COUNT;++id)
        Check(result->maiSizes[id]==0 && result->mapBuffers[id]==nullptr,"all eleven result slots are reset at host pointer width");

    ReadStream stream;
    std::memset(&stream,0x5A,sizeof(stream));
    stream.Construct();
    Check(stream.mpStreamHeader==nullptr && stream.miCurrentFrunk==0 && stream.mpStream==nullptr,
          "real stream construction resets its three original fields");
    Check(stream.miStallCount==0x5A5A5A5A && stream.miCurrentFrunkPos==0x5A5A5A5A &&
          stream.miFilePosition==0x5A5A5A5A,"stream construction preserves remaining cursor storage");

    StreamOffset offsets[KI_MAX_FRUNKS]={};
    StreamHeader header={};header.mpFrameOffsets=offsets;header.miFirstFrunk=1799;header.miNumFrunks=2;
    offsets[1799].miFrameNumber=0x1234567800000042LL;
    offsets[1799].miFileOffset=0x30000;offsets[1799].miFrunkSize=0x10000;
    offsets[0].miFileOffset=0x50000;offsets[0].miFrunkSize=0x10000;
    offsets[1].miFileOffset=0x90000;offsets[1].miFrunkSize=0x20000;
    DiskReadStream disk;
    Check(stream.StartNewStream(&header,0x20000,&disk)==0x1234567800000042LL,
          "start returns the entire original 64-bit frame");
    Check(observedStart==0x30000 && observedEnd==0xB0000 && stream.miFilePosition==0,
          "start uses disk byte ranges and the original first-plus-count end slot");
    stream.miStallCount=17;
    Check(!stream.MoveToNextFrunk() && stream.miCurrentFrunk==0 && stream.miStallCount==17,
          "live ring advances across physical array wrap without clearing stalls");
    Check(stream.MoveToNextFrunk() && stream.miCurrentFrunk==1799 && stream.miStallCount==0,
          "live span wrap returns to its first frunk and clears stalls");

    FrunkHeader written={};std::memcpy(written.macMagicNumber,"frk",4);
    written.miFrameNumber=0x1234567800000042LL;written.mxFlags=KU_FLAG_KEYFRAME;
    written.mfFrameTime=11.25f;written.miNumSerialisers=3;
    const FrunkSerialiserEntry entries[]={{0,3},{5,5},{10,4}};
    std::memcpy(diskBytes,&written,sizeof(written));
    std::memcpy(diskBytes+sizeof(written),entries,sizeof(entries));
    const u8 payload[]={11,12,13,21,22,23,24,25,31,32,33,34};
    std::memcpy(diskBytes+sizeof(written)+sizeof(entries),payload,sizeof(payload));
    u8 buffers[E_ID_COUNT][16];std::memset(buffers,0xA5,sizeof(buffers));
    for (s32 id=0;id<E_ID_COUNT;++id) {result->maiSizes[id]=16;result->mapBuffers[id]=buffers[id];}
    offsets[1799].mxFlags=KU_FLAG_KEYFRAME;offsets[1799].mfFrameTime=19.5f;
    offsets[1799].miFrunkSize=91;
    Check(stream.ReadCurrentFrunk(result),"ready frunk is parsed");
    Check(observedPosition==0x30000 && observedSize==0x10000,
          "disk request uses byte position and rounds size to 64 KiB");
    Check(result->mxFlags==KU_FLAG_KEYFRAME && result->mfTime==19.5f &&
          result->mHeader.miFrameNumber==written.miFrameNumber && result->mHeader.mfFrameTime==11.25f,
          "index flags/time and complete native header are distinct sources");
    Check(result->maiSizes[0]==3 && result->maiSizes[5]==5 && result->maiSizes[10]==4 &&
          !std::memcmp(buffers[0],payload,3) && !std::memcmp(buffers[5],payload+3,5) &&
          !std::memcmp(buffers[10],payload+8,4),"payloads reach sparse serialiser ids with actual sizes");
    Check(buffers[0][3]==0xA5 && buffers[5][5]==0xA5 && buffers[10][4]==0xA5 &&
          buffers[1][0]==0xA5 && result->maiSizes[1]==16 && stream.miCurrentFrunkPos==68,
          "parser preserves unused slots, trailing bytes and exact byte cursor");
    const auto savedHeader=result->mHeader;
    ready=false;stream.miCurrentFrunkPos=77;offsets[1799].mfFrameTime=23.0f;
    Check(!stream.ReadCurrentFrunk(result) && result->mfTime==23.0f &&
          !std::memcmp(&savedHeader,&result->mHeader,sizeof(savedHeader)) && stream.miCurrentFrunkPos==77,
          "not-ready frunk publishes index time while preserving header and cursor");
    const unsigned before=reads;
    offsets[1799].mxFlags=KU_FLAG_VOID|KU_FLAG_KEYFRAME;
    Check(stream.ReadCurrentFrunk(result) && reads==before && result->mxFlags==(KU_FLAG_VOID|KU_FLAG_KEYFRAME),
          "VOID frunk succeeds without a disk read or stale payload clearing");

    std::memset(ReadStream::mpcIntermediateBuffer+131064,0x39,8);
    u8 tail[16];std::memset(tail,0xA5,sizeof(tail));stream.miCurrentFrunkPos=131064;
    stream.Read(tail,8);
    Check(tail[0]==0x39 && tail[7]==0x39 && tail[8]==0xA5 && stream.miCurrentFrunkPos==131072,
          "exact-capacity read copies only requested bytes");
    stream.Read(tail,1);
    Check(tail[0]==0x39 && stream.miCurrentFrunkPos==131072,
          "original over-capacity branch preserves destination and cursor");
    Check(BrnReplayWriteStream_embed_check()==0,"overwrite invalidation follows disk bytes despite unrelated 64-bit frame ids");
    Check(asserts==0,"valid original stream cases satisfy all assertions");

    WriteStream rewind;
    std::memset(&rewind,0,sizeof(rewind));rewind.mpStreamHeader=&header;
    std::memset(offsets,0,sizeof(offsets));
    header.miFirstFrunk=0;header.miNumFrunks=200;
    offsets[0].mxFlags=offsets[100].mxFlags=offsets[130].mxFlags=KU_FLAG_KEYFRAME;
    offsets[139].mxFlags=KU_FLAG_KEYFRAME|KU_FLAG_VOID;
    rewind.ResetStartFrame(1.0f);
    Check(header.miFirstFrunk==130 && header.miNumFrunks==70,
          "history reset retains the last live keyframe before its original 60-frunk boundary");
    rewind.ResetStartFrame(2.0f);
    Check(header.miFirstFrunk==130 && header.miNumFrunks==70,
          "history reset leaves an already-short span unchanged");
    std::memset(offsets,0,sizeof(offsets));
    header.miFirstFrunk=1700;header.miNumFrunks=200;
    offsets[1700].mxFlags=offsets[1740].mxFlags=offsets[20].mxFlags=KU_FLAG_KEYFRAME;
    rewind.ResetStartFrame(1.0f);
    Check(header.miFirstFrunk==20 && header.miNumFrunks==80,
          "history reset handles a live span crossing the physical ring boundary");
    std::memset(offsets,0,sizeof(offsets));header.miFirstFrunk=0;header.miNumFrunks=1801;
    const unsigned beforeInvalid=asserts;
    rewind.ResetStartFrame(1.0f);
    Check(asserts==beforeInvalid+1 && header.miFirstFrunk==0 && header.miNumFrunks==1801,
          "original invalid-range recursion asserts once and retains its non-gating publication");
    std::printf("ReplayReadStream: %u checks, %u failures\n",checks,failures);
    DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(disk.mMutex));
    return failures ? 1 : 0;
}
