#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <thread>
#include <malloc.h>
#include "GameShared/Jobs/DecompressionJob/CgsDecompressor.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"

static int checks,failures,assertions;
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char* s,const char*,int){++assertions;std::printf("ASSERT %s\n",s);return 0;}
void* EndAssert(){return nullptr;}
} }
// Allocator boundary only. All zlib state, snapshots, copies and job-parameter
// selection are the actual production implementations and native zlib build.
void* CgsMemory::HeapMalloc::Malloc(s32 size,s32){void* p=_aligned_malloc(size,16);if(p)++outstanding;return p;}
void CgsMemory::HeapMalloc::Free(void* p){if(p){--outstanding;_aligned_free(p);}}
#include "pc_compressed_data.inc"
static void Check(bool ok,const char* text){++checks;if(!ok){++failures;std::printf("FAIL %s\n",text);}}
using namespace CgsResource;
struct Decode {
    static constexpr u64 guard=0xCAFED00D12345678ull;
    u64 before=guard;
    Decompressor worker{};
    u64 middle=guard;
    DecompressionJobStatus status{};
    u64 after=guard;
    CgsMemory::HeapMalloc heap;
    std::vector<u8> output;
    CompressedData entry{};
    DecompressionJobData job{};
    Decode(size_t size):output(size+32,0xCD){
        status.miLastInflateResult=Z_STREAM_END;
        entry.mpDestinationBuffer=output.data()+16;entry.muDestinationSize=static_cast<u32>(size);
        job.mpStatus=&status;job.mpEntries=&entry;job.muNumEntries=1;
        job.mpHeapMalloc=&heap;job.mpNativeWorker=&worker;
    }
    void Run(const u8* data,u32 size,bool otherThread=false){
        entry.mpSourceBuffer=const_cast<u8*>(data);entry.muSourceSize=size;
        auto dispatch=[this]{
            DecompressionJobEntry(EA::Jobs::Param(reinterpret_cast<void*>(0x1234)),
                EA::Jobs::Param(static_cast<void*>(&job)),EA::Jobs::Param(),EA::Jobs::Param());
        };
        if(otherThread){std::thread t(dispatch);t.join();}else dispatch();
    }
    bool Bytes(int salt) const {
        for(size_t i=0;i<entry.muDestinationSize;++i)
            if(output[16+i]!=static_cast<u8>((i*29+(i>>8)*salt)^(i>>4)))return false;
        return true;
    }
    bool Guards() const {
        if(before!=guard || middle!=guard || after!=guard)return false;
        for(int i=0;i<16;++i)if(output[i]!=0xCD || output[output.size()-16+i]!=0xCD)return false;
        return true;
    }
};
int main(){
    Check(sizeof(z_stream)>56,"native zlib structure exceeds the console's 56-byte ABI");
    {
        Decode a(65536);a.Run(compressedA,sizeof(compressedA));
        Check(a.status.miLastInflateResult==Z_STREAM_END && a.Bytes(17),"actual job entry decodes the second parameter into the exact payload");
        Check(a.status.mDecompressionStream.total_in==sizeof(compressedA)
            && a.status.mDecompressionStream.total_out==65536,"native snapshot preserves complete zlib totals");
        Check(a.Guards() && a.heap.outstanding==0,"completed job preserves guards and frees its zlib allocations");
    }
    {
        Decode a(65536),b(32768);
        const u32 splitA=sizeof(compressedA)/2,splitB=sizeof(compressedB)/3;
        a.Run(compressedA,splitA);b.Run(compressedB,splitB);
        Check(a.status.miLastInflateResult==Z_OK && b.status.miLastInflateResult==Z_OK,
            "incomplete streams retain a resumable state");
        const auto* opaqueA=a.status.mDecompressionStream.opaque;
        a.Run(compressedA+splitA,sizeof(compressedA)-splitA,true);
        b.Run(compressedB+splitB,sizeof(compressedB)-splitB,true);
        Check(a.Bytes(17) && b.Bytes(91),"interleaved streams resume on another thread without losing history");
        Check(a.status.miLastInflateResult==Z_STREAM_END && b.status.miLastInflateResult==Z_STREAM_END,
            "both resumed streams reach a genuine zlib end");
        Check(opaqueA==&a.worker && b.status.mDecompressionStream.opaque==&b.worker,
            "each stream retains its own stable native worker across thread changes");
        Check(a.Guards() && b.Guards() && a.heap.outstanding==0 && b.heap.outstanding==0,
            "partial-job snapshots preserve guards and release both streams' allocations");
    }
    {
        Decode a(65536),b(32768);
        CompressedData entries[2]={a.entry,b.entry};
        entries[0].mpSourceBuffer=const_cast<u8*>(compressedA);entries[0].muSourceSize=sizeof(compressedA);
        entries[1].mpSourceBuffer=const_cast<u8*>(compressedB);entries[1].muSourceSize=sizeof(compressedB);
        a.job.mpEntries=entries;a.job.muNumEntries=2;
        a.worker.Execute(&a.job);
        Check(a.Bytes(17) && b.Bytes(91) && a.status.miLastInflateResult==Z_STREAM_END,
            "one production job decodes multiple independent resources");
        Check(a.Guards() && b.Guards() && a.heap.outstanding==0,
            "next-entry initialization preserves boundaries and frees previous inflate state");
    }
    {
        // RunFlushJobs excludes an unfinished, source-empty last entry from
        // its count. ARTIST Execute still primes that supplied entry's state.
        Decode a(65536);a.job.muNumEntries=0;a.Run(nullptr,0);
        Check(a.status.miLastInflateResult==Z_BUF_ERROR && a.status.mDecompressionStream.state,
            "source-empty flush preserves the original resumable buffer-error state");
        a.job.muNumEntries=1;a.Run(compressedA,sizeof(compressedA),true);
        Check(a.Bytes(17) && a.status.miLastInflateResult==Z_STREAM_END && a.Guards() && a.heap.outstanding==0,
            "a subsequent populated job resumes a previously source-empty entry");
    }
    Check(assertions==0,"valid job parameters satisfy production assertions");
    std::printf("PCDecompressionWorker: %d checks, %d failures (z_stream=%zu)\n",checks,failures,sizeof(z_stream));
    return failures?1:0;
}
