// Original disk callback/service flow. Device submission and OS locks are observed boundaries.
#include <cstdio>
#include <cstring>
#include <string>
#include <initializer_list>
#include "GameSource/Replays/Stream/BrnReplayDiskReadStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static unsigned checks, failures, asserts, enters, leaves, closes, reads;
static int lockDepth;
static std::string messages;
static CgsFileSystem::AsyncCompletionCallback closeCallback;
static void* closeContext;
static CgsFileSystem::Handle closeHandle;
static s32 closePriority;
static void Check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { ++asserts; return 0; }
void* EndAssert() { return nullptr; }
} namespace Log {
StrStreamBase& DebugPrint::operator<<(const char* text) { messages += text; return *this; }
static DebugPrint sink;
DebugPrint* gpDebugPrint = &sink;
} namespace Message { u64 gxMessageFilterFlags = 0; } }
extern "C" void RtlEnterCriticalSection(void*) { ++enters; ++lockDepth; }
extern "C" void RtlLeaveCriticalSection(void*) { ++leaves; --lockDepth; }
extern "C" long RtlInitializeCriticalSection(void*) { return 0; } // observed OS boundary
extern "C" void* XMemCpy(void* to,const void* from,size_t size) { return std::memcpy(to,from,size); }

// The fixture records the native device API call; it does not run filesystem jobs.
namespace CgsFileSystem {
alignas(DeviceManager) static unsigned char managerTag[sizeof(DeviceManager)];
DeviceManager* GetDeviceManager() { return reinterpret_cast<DeviceManager*>(managerTag); }
void DeviceManager::Close(Handle handle, AsyncCompletionCallback callback, void* context, s32 priority)
{
    ++closes; closeHandle=handle; closeCallback=callback; closeContext=context; closePriority=priority;
}
void DeviceManager::Read(Handle,u64,void*,u32,AsyncCompletionCallback,void*,s32) { ++reads; }
}
#include "replay_disk_read_lifecycle.inc"

using BrnReplays::DiskReadStream;
static char bytes[2*DiskReadStream::KI_BLOCK_SIZE];
static void Reset(DiskReadStream& stream)
{
    std::memset(&stream,0,sizeof(stream));
    stream.meStatus=DiskReadStream::E_STATUS_OPEN;
    stream.miNumBlocks=2;
    stream.miBlockSize=DiskReadStream::KI_BLOCK_SIZE;
    stream.miBufferSize=sizeof(bytes);
    stream.mpBuffer=bytes;
    stream.miNormalPriority=7;
    stream.miUrgentPriority=11;
    stream.miCurrentPriority=19;
    stream.mbServiced=true;
    stream.miLoopEnd=sizeof(bytes);
    std::strcpy(stream.macFileName,"replay-data.bin");
    for (s32 index=0;index<DiskReadStream::KI_MAX_STREAM_BLOCKS;++index)
    {
        stream.maBlocks[index].mpOwner=&stream;
        stream.maBlocks[index].miStreamPos=index*stream.miBlockSize;
    }
    stream.mHandle.mpDevice=reinterpret_cast<CgsFileSystem::Device*>(0x1234567812345678ULL);
    stream.mHandle.mDeviceHandle=reinterpret_cast<void*>(0x2345678923456789ULL);
}
static void Reading(DiskReadStream& stream)
{
    Reset(stream);
    stream.miPendingOperationCount=stream.miInputRequestCount=1;
    stream.miBlocksUsed=1;
    stream.maBlocks[0].muFlags=DiskReadStream::KU_RSBFLAG_READING;
    stream.miNextInputPosition=0x123450000LL;
}
int main()
{
    DiskReadStream stream;
    Reading(stream);
    const auto handle=stream.mHandle;
    DiskReadStream::ReadCallback(0,handle,DiskReadStream::KI_BLOCK_SIZE,&stream.maBlocks[0]);
    Check(stream.muLastReadSize==DiskReadStream::KI_BLOCK_SIZE
          && stream.maBlocks[0].miDataEnd==DiskReadStream::KI_BLOCK_SIZE,
          "read callback publishes full completion size and block cursor");
    Check(stream.maBlocks[0].muFlags==DiskReadStream::KU_RSBFLAG_FULL
          && stream.miNextInputPosition==0x123450000LL,
          "complete block preserves next input position without EOF");
    Check(stream.miPendingOperationCount==0 && stream.miInputRequestCount==0,
          "read completion consumes both original counters");
    Check(asserts==0 && lockDepth==0,"valid callback balances real nested service locks");

    Reading(stream);
    DiskReadStream::ReadCallback(0,handle,0,&stream.maBlocks[0]);
    Check(stream.maBlocks[0].muFlags==(DiskReadStream::KU_RSBFLAG_FULL|DiskReadStream::KU_RSBFLAG_EOF)
          && stream.maBlocks[0].miDataEnd==0 && stream.miNextInputPosition==0,
          "zero read is original EOF and restarts input at zero");
    Check(asserts==0,"zero EOF is legal without a short-file assertion");

    Reading(stream);
    unsigned beforeAsserts=asserts;
    DiskReadStream::ReadCallback(0,handle,0x100000010ULL,&stream.maBlocks[0]);
    Check(stream.muLastReadSize==0x100000010ULL && stream.maBlocks[0].miDataEnd==16,
          "64-bit completion size narrows only at the block cursor");
    Check(asserts==beforeAsserts+1 && stream.maBlocks[0].muFlags==6 && stream.miNextInputPosition==0,
          "nonzero partial read asserts but retains original EOF publication");

    for (s32 result : {-2,-1,77})
    {
        Reading(stream); stream.meStatus=DiskReadStream::E_STATUS_OPENING;
        stream.muLastReadSize=0xABCDEFFF12345678ULL;
        beforeAsserts=asserts;
        DiskReadStream::ReadCallback(result,handle,37,&stream.maBlocks[0]);
        Check(stream.meStatus==(result==-2?DiskReadStream::E_STATUS_ERROR:
                              result==-1?DiskReadStream::E_STATUS_OPEN:DiskReadStream::E_STATUS_OPENING),
              "read error, cancellation and unhandled response retain original distinct states");
        Check(stream.maBlocks[0].muFlags==DiskReadStream::KU_RSBFLAG_READING
              && stream.muLastReadSize==0xABCDEFFF12345678ULL,
              "unsuccessful read preserves payload publication");
        Check(stream.miPendingOperationCount==0 && stream.miInputRequestCount==0
              && asserts==beforeAsserts+(result==77),"all response paths consume counters after diagnostics");
    }

    for (s32 result : {-2,-1,0,77})
    {
        Reset(stream); stream.meStatus=DiskReadStream::E_STATUS_ERROR;
        stream.miPendingOperationCount=2;
        const unsigned oldReads=reads;
        beforeAsserts=asserts;
        DiskReadStream::CloseCallback(result,handle,0,&stream);
        Check(stream.meStatus==(result==-1?DiskReadStream::E_STATUS_OPEN:
                              result==77?DiskReadStream::E_STATUS_ERROR:DiskReadStream::E_STATUS_CLOSED),
              "close failure/success close, cancellation reopens, unhandled response retains state");
        Check(stream.miPendingOperationCount==1 && reads==oldReads
              && asserts==beforeAsserts+(result==77),"close callback consumes only its operation without service");
    }

    Reset(stream);
    unsigned oldCloses=closes;
    stream.Close();
    Check(closes==oldCloses+1 && closeCallback==&DiskReadStream::CloseCallback && closeContext==&stream,
          "Close submits actual callback and owning stream context");
    Check(closePriority==11 && closeHandle.mpDevice==handle.mpDevice
          && closeHandle.mDeviceHandle==handle.mDeviceHandle,
          "close submission uses urgent priority and full native compound handle");
    Check(stream.miPendingOperationCount==1 && !stream.mbWaitingToClose
          && stream.meStatus==DiskReadStream::E_STATUS_OPEN,
          "submission consumes request flag but leaves state open until completion");
    closeCallback(0,closeHandle,0,closeContext);
    Check(stream.meStatus==DiskReadStream::E_STATUS_CLOSED && stream.miPendingOperationCount==0,
          "submitted close completes through the original callback");
    CgsDev::Message::gxMessageFilterFlags=1;
    messages.clear(); oldCloses=closes;
    stream.Close();
    Check(closes==oldCloses && messages=="WARNING: Attempt to close a stream that is already closed or closing - ignoring request\n",
          "closed request emits the original repeat-close warning");
    CgsDev::Message::gxMessageFilterFlags=0;

    Reading(stream); oldCloses=closes;
    stream.Close();
    Check(stream.mbWaitingToClose && closes==oldCloses && stream.miPendingOperationCount==1,
          "outstanding read defers close submission");
    DiskReadStream::ReadCallback(0,handle,DiskReadStream::KI_BLOCK_SIZE,&stream.maBlocks[0]);
    Check(closes==oldCloses+1 && stream.miPendingOperationCount==1
          && stream.miInputRequestCount==0 && !stream.mbWaitingToClose,
          "real read completion service submits the deferred close");
    closeCallback(-1,closeHandle,0,closeContext);
    Check(stream.meStatus==DiskReadStream::E_STATUS_OPEN && stream.miPendingOperationCount==0,
          "deferred cancellation retains original reopened state");
    Check(enters==leaves && lockDepth==0,"all original lock and service paths balance");
    std::printf("ReplayDiskReadLifecycle: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
