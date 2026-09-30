#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <thread>
#include <malloc.h>
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"

static int checks,failures,assertions;
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char* s,const char*,int){if(++assertions<6)std::printf("ASSERT %s\n",s);return 0;}
void* EndAssert(){return nullptr;}
}
namespace Log { static DebugPrint sink; DebugPrint* gpDebugPrint=&sink;
StrStreamBase& DebugPrint::operator<<(const char*){return *this;} }
namespace Message {u64 gxMessageFilterFlags=0;}
}
void* CgsMemory::HeapMalloc::Malloc(s32 size,s32){void* p=_aligned_malloc(size,16);if(p)++outstanding;return p;}
void CgsMemory::HeapMalloc::Free(void* p){if(p){--outstanding;_aligned_free(p);}}
static void Check(bool ok,const char* message){++checks;if(!ok){++failures;std::printf("FAIL %s\n",message);}}
struct Reader {std::vector<u8> bytes;size_t position=0,available=0;bool exposed=false;};
namespace CgsFileSystem {
void ReadStream::Construct(StreamDeviceDiskRead* p){mpStreamDevice=p;}
ReadStream& ReadStream::operator=(StreamDeviceDiskRead* p){mpStreamDevice=p;return *this;}
bool ReadStream::IsValid() const{return mpStreamDevice!=nullptr;}
u64 ReadStream::Tell() const{return reinterpret_cast<Reader*>(mpStreamDevice)->position;}
u32 ReadStream::GetAmountOfDataInBuffer() const {const auto& r=*reinterpret_cast<Reader*>(mpStreamDevice);return static_cast<u32>(r.available-r.position);}
bool ReadStream::IsBufferComplete() const {const auto& r=*reinterpret_cast<Reader*>(mpStreamDevice);return r.available==r.bytes.size();}
bool ReadStream::StartAsyncRead(void** p,u32* n){auto& r=*reinterpret_cast<Reader*>(mpStreamDevice);
    if(r.exposed)std::abort();r.exposed=true;*p=r.bytes.data()+r.position;*n=static_cast<u32>(r.available-r.position);return *n!=0;}
void ReadStream::StopAsyncRead(u32 n){auto& r=*reinterpret_cast<Reader*>(mpStreamDevice);
    if(!r.exposed||r.position+n>r.available)std::abort();r.exposed=false;r.position+=n;}
u32 ReadStream::Read(u32 n,void* out){auto& r=*reinterpret_cast<Reader*>(mpStreamDevice);
    const u32 take=static_cast<u32>(std::min<size_t>(n,r.available-r.position));
    if(out)std::memcpy(out,r.bytes.data()+r.position,take);r.position+=take;return take;}
}
#include "pc_bundle_compressed.inc"
#include "pc_compressed_payload.inc"
using namespace CgsResource;

struct Fixture {
    BundleLoaderModule loader{};
    BundleLoaderIO::OutputBuffer output;
    EA::Jobs::JobScheduler scheduler;
    CompressedData jobs[4]{};
    BundleV2::ResourceEntry entries[3]{};
    SmallResource resources[3]{};
    bool needs[3]={true,false,true};
    Reader reader;
    std::vector<u8> secondary,large,small;
    Fixture():secondary(BundleLoaderModule::KU_SECONDARY_STREAM_BUFFER_SIZE+32,0xCE),large(sizeof(Payload)+32,0xCA),small(96,0xAB){
        loader.Construct();output.Construct();
        loader.mpcSecondaryStreamBuffer=reinterpret_cast<char*>(secondary.data()+16);
        loader.mAllocationResponse={};loader.mAllocationResponse.miPoolId=3;
        loader.mAllocationResponse.miNumEntries=3;loader.mAllocationResponse.mpEntries=entries;
        loader.mAllocationResponse.mpResources=resources;loader.mAllocationResponse.mpNeeds=needs;
        loader.miCurrentMemoryType=0;loader.miCurrentResource=0;loader.miCurrentResourcePosition=0;
        loader.miNextFixUpRequestIndex=0;loader.miMaxPartialFixups=60;
        loader.mbStreamJobStarted=false;loader.mbOnLastStreamJob=false;
        loader.mabStreamBuffersUsed[0]=loader.mabStreamBuffersUsed[1]=true;
        loader.mCurrentBundle.mauResourceDataOffset[0]=128;
        entries[0].mauSizeAndAlignmentOnDisk[0]=sizeof(Packed);
        entries[0].mauUncompressedSizeAndAlignment[0]=sizeof(Payload);
        entries[0].mauDiskOffset[0]=32;
        entries[1].mauSizeAndAlignmentOnDisk[0]=32;entries[1].mauUncompressedSizeAndAlignment[0]=32;
        entries[1].mauDiskOffset[0]=32+sizeof(Packed);
        entries[2].mauSizeAndAlignmentOnDisk[0]=sizeof(SmallPacked);
        entries[2].mauUncompressedSizeAndAlignment[0]=64;
        entries[2].mauDiskOffset[0]=32+sizeof(Packed)+96;
        resources[0].m_baseResources[0]=large.data()+16;resources[2].m_baseResources[0]=small.data()+16;
        reader.bytes.resize(128+entries[2].mauDiskOffset[0]+sizeof(SmallPacked),0xD7);
        std::memcpy(reader.bytes.data()+160,Packed,sizeof(Packed));
        std::memcpy(reader.bytes.data()+128+entries[2].mauDiskOffset[0],SmallPacked,sizeof(SmallPacked));
        loader.maStreams[0].Construct(reinterpret_cast<CgsFileSystem::StreamDeviceDiskRead*>(&reader));
        loader.mDecompressionJobInterface.Construct(&scheduler,jobs,4);
        loader.mDecompressionJobInterface.BeginStream();
        loader.mDecompressionJobInterface.CreateEntry(large.data()+16,sizeof(Payload));
    }
    bool Tick(){output.LockForWrite();bool done=loader.StreamCompressedDataAsJobFunc(&output);output.UnlockForWrite();return done;}
    bool Guards() const {for(int i=0;i<16;++i)if(large[i]!=0xCA||large[large.size()-16+i]!=0xCA
        ||small[i]!=0xAB||small[small.size()-16+i]!=0xAB||secondary[i]!=0xCE||secondary[secondary.size()-16+i]!=0xCE)return false;return true;}
};
int main(){
    {
        Fixture f;f.reader.available=f.reader.bytes.size();
        Check(!f.Tick()&&f.scheduler.submitted==1&&f.loader.mbStreamJobStarted,
            "first compressed update submits exactly one bounded batch");
        Check(f.reader.position==160+BundleLoaderModule::KU_SECONDARY_STREAM_BUFFER_SIZE
            && f.jobs[0].muSourceSize==BundleLoaderModule::KU_SECONDARY_STREAM_BUFFER_SIZE,
            "disk gaps are skipped and an incompressible resource is split at512KiB");
        const auto before=f.secondary;const auto pos=f.reader.position;
        Check(!f.Tick()&&f.secondary==before&&f.reader.position==pos&&f.scheduler.submitted==1,
            "polling an unfinished job cannot reuse staging bytes or advance the ring");
        std::thread worker([&]{f.scheduler.Complete();});worker.join();
        Check(f.loader.mDecompressionJobInterface.mJobStatus.miLastInflateResult==Z_OK,
            "first production inflate batch remains resumable");
        Check(!f.Tick()&&f.scheduler.submitted==2&&f.loader.mbOnLastStreamJob,
            "completion carries the partial entry then batches the remaining resource");
        f.scheduler.Complete();
        Check(f.Tick()&&f.loader.mDecompressionJobInterface.meStage==E_DJS_IDLE,
            "final completed job ends the stream on a later update");
        Check(std::memcmp(f.large.data()+16,Payload,sizeof(Payload))==0
            &&std::memcmp(f.small.data()+16,Payload,64)==0,
            "real zlib reproduces both payloads across native entry carry and resident gaps");
        Check(f.Guards()&&f.loader.mDecompressionJobInterface.mHeap.outstanding==0,
            "staging and destination guards survive and inflate allocations are released");
    }
    {
        Fixture f;f.reader.available=160+sizeof(Packed);
        bool done=false;
        for(int i=0;i<5&&!f.loader.mDecompressionJobInterface.mpEntries[0].mpSourceBuffer;++i){f.Tick();f.scheduler.Complete();}
        // Drain batches until the large resource is done but the following
        // nonresident resource has no source bytes available yet.
        for(int i=0;i<5&&f.loader.miCurrentResource!=2;++i){f.Tick();f.scheduler.Complete();}
        if(f.loader.mbStreamJobStarted)f.scheduler.Complete();
        Check(!f.Tick()&&f.loader.mDecompressionJobInterface.muNumEntries==1
            &&f.loader.mDecompressionJobInterface.mpEntries[0].mpDestinationBuffer==f.small.data()+16
            &&f.loader.mDecompressionJobInterface.mpEntries[0].mpSourceBuffer==nullptr,
            "created but source-empty next entry survives the completed-resource flush");
        const int submits=f.scheduler.submitted;
        Check(!f.Tick()&&f.scheduler.submitted==submits,
            "source-empty carried entry submits no empty job while disk data is pending");
        f.reader.available=f.reader.bytes.size();
        for(int i=0;i<5&&!done;++i){done=f.Tick();f.scheduler.Complete();}
        Check(done&&std::memcmp(f.small.data()+16,Payload,64)==0&&f.Guards(),
            "later disk data resumes the carried entry and completes intact");
    }
    {
        EA::Jobs::JobScheduler scheduler;DecompressionJobInterface decoder;CompressedData entries[2]{};u8 bytes[64]{};
        decoder.Construct(&scheduler,entries,2);decoder.BeginStream();decoder.CreateEntry(bytes,sizeof(bytes));
        Check(!decoder.RunFlushJobs()&&scheduler.submitted==0,"an initial entry without source never submits a job");
        decoder.AppendToEntry(const_cast<u8*>(SmallPacked),sizeof(SmallPacked));decoder.FinishEntry();decoder.RunFlushJobs();
        Check(decoder.WaitForFlushJobs(true)&&std::memcmp(bytes,Payload,64)==0,
            "explicit blocking wait completes the actual worker before accepting its output");
        scheduler.Complete();decoder.EndStream();
        Check(decoder.mHeap.outstanding==0,"blocking completion releases all inflate allocations");
    }
    Check(assertions==0,"valid compressed stream lifecycle satisfies engine assertions");
    std::printf("PCBundleCompressed: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
