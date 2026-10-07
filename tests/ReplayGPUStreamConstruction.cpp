// CPU-only native lock and actual contained Job construction; no GPU work.
#include <cstdio>
#include <cstring>
#include <new>
#include <windows.h>
#include "GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static unsigned checks, failures, asserts;
static void Check(bool value,const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n",label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { ++asserts; return 0; }
void* EndAssert() { return nullptr; }
} }
#include "replay_gpu_stream_construction.inc"

int main()
{
    using BrnReplays::GPUDiskWriteStream;
    struct Guarded {
        u64 before[2];
        alignas(GPUDiskWriteStream) unsigned char bytes[sizeof(GPUDiskWriteStream)];
        u64 after[2];
    } storage;
    std::memset(&storage,0xA5,sizeof(storage));
    auto* stream=new(storage.bytes) GPUDiskWriteStream;
    Check(storage.before[0]==0xA5A5A5A5A5A5A5A5ULL && storage.before[1]==storage.before[0]
          && storage.after[0]==storage.before[0] && storage.after[1]==storage.before[0],
          "real job and lock constructor stay inside their allocation");
    Check(sizeof(stream->maLock)>=sizeof(CRITICAL_SECTION),"stream lock fits actual Win64 CRITICAL_SECTION");
    auto* lock=reinterpret_cast<CRITICAL_SECTION*>(stream->maLock);
    Check(reinterpret_cast<uintptr_t>(lock)%alignof(CRITICAL_SECTION)==0,
          "stream lock has actual native pointer alignment");
    Check(lock->LockCount==-1 && lock->RecursionCount==0,"native ntdll constructor creates an unlocked section");
    RtlEnterCriticalSection(stream->maLock);
    Check(lock->RecursionCount==1 && lock->OwningThread!=nullptr,"native ntdll acquires initialized stream lock");
    RtlEnterCriticalSection(stream->maLock);
    Check(lock->RecursionCount==2,"original critical section remains recursive");
    RtlLeaveCriticalSection(stream->maLock);
    RtlLeaveCriticalSection(stream->maLock);
    Check(lock->LockCount==-1 && lock->RecursionCount==0,"native ntdll releases both recursive acquisitions");
    Check(stream->mRelocator.mJob.GetEntryPoint().mName[0]==0,
          "contained actual Job starts unnamed as both original owners require");
    Check(stream->mRelocator.mJob.mDependencies.mSize==0
          && stream->mRelocator.mJob.mDependents.mSize==0,
          "real SDK Job initializes its actual dependency lists");
    Check(asserts==0,"native construction satisfies actual SDK job assertions");
    DeleteCriticalSection(lock); // fixture lifetime cleanup; no production destructor claim
    stream->~GPUDiskWriteStream();
    std::printf("ReplayGPUStreamConstruction: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
