#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <windows.h>
#include "GameShared/Jobs/DecompressionJob/DecompressionJobInterface.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "SDKs/EATech/eajobs/local_backend.h"
#include "SDKs/EATech/eajobs/jobs.h"
#include "pc_native_compressed_payload.inc"

static int checks,failures,assertions;
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char* message,const char*,int){++assertions;std::printf("ASSERT %s\n",message);return 0;}
void* EndAssert(){return nullptr;}
} }
static void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}}
static bool Wait(CgsResource::DecompressionJobInterface& decoder){
    const auto deadline=GetTickCount64()+5000;
    while(!decoder.WaitForFlushJobs(false)){
        if(GetTickCount64()>=deadline)return false;
        Sleep(1);
    }
    return true;
}
static bool Guards(const std::vector<u8>& data){
    for(int i=0;i<16;++i)if(data[i]!=0xCE||data[data.size()-1-i]!=0xCE)return false;
    return true;
}

int main(){
    // Actual original-sized engine heap, production EAJobs/native EAThread,
    // DecompressionJobInterface, Decompressor and zlib. Only input bytes and
    // assertion reporting are fixtures; no scheduler or allocator substitute.
    alignas(128) static u8 heapStorage[400*1024];
    CgsMemory::HeapMallocCoreAllocator heap;heap.Construct(heapStorage,sizeof(heapStorage));
    EA::Jobs::SetAllocator(&heap);
    auto& general=heap.mAllocator;
    const u32 originalFree=general.GetLargestFreeBlock(true);
    {
        EA::Jobs::JobScheduler scheduler;scheduler.Initialize(128,128);
        auto* backend=static_cast<EA::Jobs::LocalBackend::LocalBackend*>(scheduler.mSchedulers[0]);
        Check(backend&&backend->mpJobInstances&&backend->mpPriorityQueue,
            "128 native job records and queue fit the original 400KiB engine arena");
        if(!backend||!backend->mpJobInstances||!backend->mpPriorityQueue)std::abort();
        EA::Jobs::JobThreadParameters worker;
        for(int i=0;i<3;++i)scheduler.AddThread(worker);
        Check(backend->GetNumThreads()==3,"three actual workers run against the engine allocator");
        {
            CgsResource::DecompressionJobInterface decoder;
            CgsResource::CompressedData entries[4]{};
            decoder.Construct(&scheduler,entries,4);
            const u32 originalInflateFree=decoder.mHeap.GetLargestFreeBlock(true);
            std::vector<u8> output(sizeof(Payload)+32,0xCE);
            decoder.BeginStream();decoder.CreateEntry(output.data()+16,sizeof(Payload));
            const unsigned split=0x80000;
            decoder.AppendToEntry(const_cast<u8*>(Packed),split);
            Check(decoder.RunFlushJobs()&&Wait(decoder),"the first 512KiB batch completes through native scheduling and polling");
            Check(decoder.mJobStatus.miLastInflateResult==Z_OK&&decoder.muNumEntries==1
                &&entries[0].mpSourceBuffer==nullptr&&decoder.mbEntryInProgress,
                "a partial entry retains its real inflate state across worker submissions");
            decoder.AppendToEntry(const_cast<u8*>(Packed)+split,sizeof(Packed)-split);
            decoder.FinishEntry();
            Check(decoder.RunFlushJobs()&&Wait(decoder),"the second batch resumes and finishes the same resource");
            decoder.EndStream();
            Check(std::memcmp(output.data()+16,Payload,sizeof(Payload))==0&&Guards(output),
                "native decompression reproduces the entire payload without touching guards");
            Check(decoder.mHeap.GetLargestFreeBlock(true)==originalInflateFree,
                "the fixed 128KiB inflate heap is fully recovered at stream end");
            // Exercise actual blocking Job::WaitOn, not a fixture which runs work
            // on the waiting thread. Repeated streams also reuse the native arena.
            bool repeated=true;
            for(int i=0;i<20;++i){
                u8 outputSmall[64]{};
                decoder.BeginStream();decoder.CreateEntry(outputSmall,sizeof(outputSmall));
                decoder.AppendToEntry(const_cast<u8*>(SmallPacked),sizeof(SmallPacked));decoder.FinishEntry();
                repeated=decoder.RunFlushJobs()&&repeated;
                repeated=decoder.WaitForFlushJobs(true)&&repeated;
                decoder.EndStream();
                repeated=repeated&&std::memcmp(outputSmall,Payload,sizeof(outputSmall))==0;
            }
            Check(repeated&&decoder.mHeap.GetLargestFreeBlock(true)==originalInflateFree,
                "blocking completion and repeated streams preserve outputs and reclaim inflate storage");
            decoder.BeginStream();decoder.CreateEntry(output.data()+16,sizeof(Payload));
            decoder.AppendToEntry(const_cast<u8*>(Packed),split);decoder.RunFlushJobs();
            decoder.CancelStreamPC();
            Check(decoder.meStage==CgsResource::E_DJS_IDLE&&!decoder.mbEntryInProgress
                &&decoder.mHeap.GetLargestFreeBlock(true)==originalInflateFree,
                "cancelling an unfinished stream joins the worker and frees its native inflate workspace");
            u8 resumed[64]{};
            decoder.BeginStream();decoder.CreateEntry(resumed,sizeof(resumed));
            decoder.AppendToEntry(const_cast<u8*>(SmallPacked),sizeof(SmallPacked));decoder.FinishEntry();
            decoder.RunFlushJobs();decoder.WaitForFlushJobs(true);decoder.EndStream();
            Check(std::memcmp(resumed,Payload,sizeof(resumed))==0
                &&decoder.mHeap.GetLargestFreeBlock(true)==originalInflateFree,
                "a cancelled interface can start another stream without stale inflate state");
        }
        scheduler.Destroy();
    }
    Check(general.GetLargestFreeBlock(true)==originalFree,
        "worker join and scheduler destruction return the complete fixed arena");
    EA::Jobs::SetAllocator(nullptr);heap.Destruct();
    Check(assertions==0,"the integrated path satisfies engine assertions");
    std::printf("PCNativeDecompression: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
