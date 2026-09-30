#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static int checks,failures,assertions;
namespace CgsDev {
namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char* s,const char*,int){if(++assertions<4)std::printf("ASSERT %s\n",s);return 0;}
void* EndAssert(){return nullptr;}
}
namespace Log { static DebugPrint sink; DebugPrint* gpDebugPrint=&sink;
StrStreamBase& DebugPrint::operator<<(const char*){return *this;} }
namespace Message {u64 gxMessageFilterFlags=0;}
}
static void Check(bool ok,const char* text){++checks;if(!ok){++failures;std::printf("FAIL %s\n",text);}}

// File-system boundary: bytes become available in bounded chunks. These
// methods cannot create pool resources, report load completion or apply fixups.
struct Reader { std::vector<u8> data; size_t position=0,available=0; };
namespace CgsFileSystem {
void ReadStream::Construct(StreamDeviceDiskRead* p){mpStreamDevice=p;}
ReadStream& ReadStream::operator=(StreamDeviceDiskRead* p){mpStreamDevice=p;return *this;}
bool ReadStream::IsValid() const{return mpStreamDevice!=nullptr;}
bool ReadStream::StartAsyncRead(void**,u32*){std::abort();}
void ReadStream::StopAsyncRead(u32){std::abort();}
bool ReadStream::IsBufferComplete() const{std::abort();}
u64 ReadStream::Tell() const{return reinterpret_cast<Reader*>(mpStreamDevice)->position;}
u32 ReadStream::GetAmountOfDataInBuffer() const {
    const auto& r=*reinterpret_cast<Reader*>(mpStreamDevice);return static_cast<u32>(r.available-r.position);
}
u32 ReadStream::Read(u32 size,void* dst){
    auto& r=*reinterpret_cast<Reader*>(mpStreamDevice);
    const u32 amount=static_cast<u32>(std::min<size_t>(size,r.available-r.position));
    if(dst && amount)std::memcpy(dst,r.data.data()+r.position,amount);
    r.position+=amount;return amount;
}
}
#include "pc_bundle_stages.inc"
using namespace CgsResource;

static void ClearOutput(BundleLoaderIO::OutputBuffer& out){
    out.LockForWrite();out.GetPoolSendQueue()->Clear();out.GetStreamRequestQueue()->Clear();
    out.GetLoadBundleResponseQueue()->Clear();out.GetUnloadBundleResponseQueue()->Clear();out.UnlockForWrite();
}
static void ClearInput(BundleLoaderIO::InputBuffer_Update& in){
    in.LockForWrite();in.GetLoadBundleRequestQueue()->Clear();in.GetUnloadBundleRequestQueue()->Clear();in.UnlockForWrite();
}
template<class Q,class T>static bool EventAt(const Q* queue,int wanted,T& result){
    const CgsModule::Event* event=nullptr; s32 size=0;
    s32 tag=queue->GetFirstEvent(&event,&size);
    while(event){
        if(tag==wanted){if(size!=sizeof(T))return false;std::memcpy(&result,event,sizeof(T));return true;}
        const CgsModule::Event* next=nullptr;tag=queue->GetNextEvent(event,&next,&size);event=next;
    }return false;
}
struct Fixture {
    BundleLoaderModule loader{};
    BundleLoaderIO::InputBuffer_Update input;
    BundleLoaderIO::OutputBuffer output;
    BundleLoaderIO::InputBuffer_Record record;
    LoadedBundleData loaded[4]{};
    alignas(16) char header[512]{};
    char debug[32]{};
    bool needs[3]{};
    SmallResource resources[3]{};
    Fixture(){
        loader.Construct();input.Construct();output.Construct();record.Construct();
        for(auto& item:loaded)item.miPoolId=-1;
        loader.mpLoadedBundles=loaded;loader.miMaxLoadedBundles=4;loader.miNumLoadedBundles=0;
        loader.meStreamStage=BundleLoaderModule::STREAMSTAGE_IDLE;
        loader.maStreams[0]=nullptr;loader.maStreams[1]=nullptr;
        loader.mabStreamBuffersUsed[0]=loader.mabStreamBuffersUsed[1]=false;
        loader.mpcHeaderBuffer=header;loader.miBundleHeaderBufferSize=sizeof(header);
        loader.mpcDebugDataBuffer=debug;loader.miDebugBufferSize=sizeof(debug);
        loader.miMaxResourcesPerBundle=3;loader.mpNeeds=needs;loader.mpResources=resources;
        loader.miHeaderPos=0;loader.mbForceUpperCaseFileNames=false;loader.miStreamBufferSize=4*1024*1024;
    }
    void Tick(){loader.Update(&input,&output);ClearInput(input);}
    template<class T>void Reply(int tag,const T& event){
        record.LockForWrite();auto* q=record.GetPoolReceiveQueue();q->Clear();
        q->AddEvent(reinterpret_cast<const CgsModule::Event*>(&event),tag,sizeof(T));record.UnlockForWrite();
        loader.RecordPostUpdateEvents(&record);
    }
};

int main(){
    {
        Fixture f;Reader reader;reader.data.resize(464,0xEF);
        BundleV2 bundle{};std::memcpy(bundle.macMagicNumber,"bnd2",4);
        bundle.muVersion=2;bundle.muPlatform=4;bundle.muDebugDataOffset=48;
        bundle.muResourceEntriesCount=3;bundle.muResourceEntriesOffset=80;
        bundle.mauResourceDataOffset[0]=288;bundle.mauResourceDataOffset[1]=400;
        bundle.mauResourceDataOffset[2]=464;bundle.muFlags=14;
        std::memcpy(reader.data.data(),&bundle,sizeof(bundle));
        std::memset(reader.data.data()+48,0xD1,32);
        BundleV2::ResourceEntry entries[3]{};
        u8 main[3][16],graphics[3][16];std::memset(main,0xAA,sizeof(main));std::memset(graphics,0xBB,sizeof(graphics));
        for(int i=0;i<3;++i){
            entries[i].mResourceId.SetHash(0x1234567800000000ull+i);
            entries[i].mauSizeAndAlignmentOnDisk[0]=entries[i].mauSizeAndAlignmentOnDisk[1]=0x40000010;
            entries[i].mauUncompressedSizeAndAlignment[0]=entries[i].mauUncompressedSizeAndAlignment[1]=0x40000010;
            entries[i].mauDiskOffset[0]=i*32;entries[i].mauDiskOffset[1]=i*24;
            std::memset(reader.data.data()+288+i*32,0x40+i,16);
            std::memset(reader.data.data()+400+i*24,0x60+i,16);
            f.resources[i].m_baseResources[0]=main[i];f.resources[i].m_baseResources[1]=graphics[i];
        }
        std::memcpy(reader.data.data()+80,entries,sizeof(entries));
        f.loader.maStreams[0].Construct(reinterpret_cast<CgsFileSystem::StreamDeviceDiskRead*>(&reader));
        f.loader.mabStreamBuffersUsed[0]=true;f.loader.meStreamStage=BundleLoaderModule::STREAMSTAGE_STREAMHEADER;
        f.loader.mLoadRequest={};f.loader.mLoadRequest.SetFileName("world/TRK_TEST.bundle");
        f.loader.mLoadRequest.mpUser=&f.loader.mReceiverQueue;f.loader.mLoadRequest.miEventId=713;
        f.loader.mLoadRequest.miPoolId=7;f.loader.mLoadRequest.mbAllowFailiure=true;
        bool allocated=false,fixed=false,closed=false,complete=false,earlyCompletion=false;
        Events::AllocateResourceListRequest allocation{};
        Events::FixUpAndResolveResourceListRequest fixup{};
        Events::LoadBundleResponse done{};
        SmallResource listMemory{}; ResourceHandle listHandle{};
        listHandle.mpResourceMemory=&listMemory;listHandle.mpSourceEntry=reinterpret_cast<Entry*>(entries);
        for(int frame=0;frame<160 && !complete;++frame){
            reader.available=std::min(reader.data.size(),reader.available+17);f.Tick();
            f.output.LockForRead();const auto& out=static_cast<const BundleLoaderIO::OutputBuffer&>(f.output);
            if(EventAt(out.GetPoolSendQueue(),16,allocation)){
                allocated=true;f.needs[0]=f.needs[2]=true;f.needs[1]=false;
                Events::AllocateResourceListResponse response{};
                response.miPoolId=allocation.miPoolId;response.mListId=allocation.mListId;
                response.mpEntries=allocation.mpEntries;response.miNumEntries=allocation.miNumEntries;
                response.mpNeeds=f.needs;response.mpResources=f.resources;f.Reply(17,response);
            }
            Events::CloseReadStreamRequest close{};
            if(EventAt(out.GetStreamRequestQueue(),18,close)){
                closed=true;Events::CloseReadStreamResponse response{};
                response.Construct(&f.loader.mReceiverQueue,close.GetEventId(),close.GetStream());
                f.loader.mReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&response),18,sizeof(response));
            }
            if(EventAt(out.GetPoolSendQueue(),18,fixup)){
                fixed=true;Events::FixUpAndResolveResourceListResponse response{};
                response.miPoolId=7;response.mListHandle=listHandle;f.Reply(19,response);
            }
            if(out.GetLoadBundleResponseQueue()->GetLength()){
                earlyCompletion=!fixed;done=out.GetLoadBundleResponseQueue()->GetEvent(0);complete=true;
            }
            f.output.UnlockForRead();ClearOutput(f.output);
        }
        Check(complete && allocated && fixed && closed && !earlyCompletion,"split reads complete only after allocation, data, close and final fixup response");
        Check(allocation.mpEntries==reinterpret_cast<BundleV2::ResourceEntry*>(f.header)
            && allocation.mpResources==f.resources && allocation.mpNeeds==f.needs && allocation.miNumEntries==3,
            "native allocation records retain complete table and destination pointers");
        Check(allocation.mListId.GetHash()==(static_cast<u32>(ID::HashString(reinterpret_cast<const u8*>("WORLD/TRK_TEST.BUNDLE")))|0x8000000000000000ull),
            "bundle list identity retains the original high-bit tag");
        bool bytes=true,debug=true;
        for(int i=0;i<3;++i)for(int b=0;b<16;++b)
            bytes &= main[i][b]==(i==1 ? 0xAA : 0x40+i) && graphics[i][b]==(i==1 ? 0xBB : 0x60+i);
        for(char c:f.debug)debug &= static_cast<u8>(c)==0xD1;
        Check(bytes,"gapped main/graphics ranges copy correctly and already-resident resources remain untouched");
        Check(debug,"fragmented debug-data reads preserve the exact debug block");
        Check(fixup.mbFinalFixup && fixup.miFirstIndex==0 && fixup.miCount==3,"whole-list publication follows complete uncompressed data");
        Check(done.mpUser==&f.loader.mReceiverQueue && done.miEventId==713 && done.miPoolId==7
            && done.meResult==Events::LoadBundleResponse::E_RESULT_SUCCESS,"completion preserves requester, correlation and pool");
        Check(f.loaded[0].miPoolId==7 && f.loaded[0].miRefCount==1 && f.loaded[0].mHandle.mpResourceMemory==&listMemory,
            "final pool response stores the loaded-bundle record and native retained handle");
        Check(!f.loader.mabStreamBuffersUsed[0] && !f.loader.maStreams[0].IsValid(),"close acknowledgement releases the correct stream buffer");
        Check(reader.position==464,"stream cursor skips holes and reaches the last requested resource");
    }
    {
        Fixture f; f.loader.meStreamStage=BundleLoaderModule::STREAMSTAGE_WAITFORALLOCATE;
        const char* names[]={"ordinary.bundle","TRK_Prop.bundle","TRK_Unit.bundle","GuiApt\\SaveLoadComponent.bundle"};
        f.input.LockForWrite();
        for(int i=0;i<4;++i){Events::LoadBundleRequest r{};r.SetFileName(names[i]);r.miEventId=i;f.input.GetLoadBundleRequestQueue()->AddEvent(r);}
        f.input.UnlockForWrite();f.Tick();
        Events::LoadBundleRequest r{};const int order[]={3,2,1,0};bool priority=true;
        for(int id:order){priority &= f.loader.mLoadRequestQueue.Pop(&r) && r.miEventId==id;}
        Check(priority && f.loader.mLoadRequestQueue.GetLength()==0,"original save UI, track, prop and ordinary priorities are preserved");
    }
    {
        Fixture f; f.loader.mAllocationResponse={};f.loader.mAllocationResponse.miPoolId=9;
        f.loader.mAllocationResponse.miNumEntries=200;f.loader.mAllocationResponse.mListId.SetHash(0xFEDCBA9876543210ull);
        f.loader.miCurrentMemoryType=0;f.loader.miCurrentResource=140;f.loader.miNextFixUpRequestIndex=0;
        f.loader.mAllocationRequest.mbLiveUpdateReplace=true;
        for(int i=0;i<3;++i){
            f.output.LockForWrite();f.loader.SendPartialFixupRequest(&f.output);f.output.UnlockForWrite();
            f.output.LockForRead();Events::FixUpAndResolveResourceListRequest r{};
            const auto& out=static_cast<const BundleLoaderIO::OutputBuffer&>(f.output);
            Check(EventAt(out.GetPoolSendQueue(),18,r) && r.miFirstIndex==i*60 && r.miCount==(i==2?20:60)
                && !r.mbFinalFixup && r.mbFixUpDependencies && r.mListId==f.loader.mAllocationResponse.mListId,
                "partial fixups cover only new ready entries, bounded to sixty, with native identity");
            f.output.UnlockForRead();ClearOutput(f.output);
        }
        f.output.LockForWrite();f.loader.SendPartialFixupRequest(&f.output);f.output.UnlockForWrite();
        Check(f.loader.miNextFixUpRequestIndex==140,"repeated partial fixup does not advance without new data");
        f.loader.miCurrentMemoryType=1;f.loader.miCurrentResource=0;
        f.output.LockForWrite();f.loader.SendPartialFixupRequest(&f.output);f.output.UnlockForWrite();
        Check(f.loader.miNextFixUpRequestIndex==200,"graphics phase makes all main entries eligible for bounded fixup");
        ClearOutput(f.output);f.output.LockForWrite();f.loader.StreamDoneFunc(&f.output);f.output.UnlockForWrite();
        f.output.LockForRead();Events::FixUpAndResolveResourceListRequest r{};
        const auto& out=static_cast<const BundleLoaderIO::OutputBuffer&>(f.output);
        Check(EventAt(out.GetPoolSendQueue(),18,r) && r.mbFinalFixup && r.miFirstIndex==200 && r.miCount==0,
            "zero remaining entries still produce the final publication request");f.output.UnlockForRead();
        ClearOutput(f.output);f.loader.mAllocationResponse.mbFailed=true;
        f.output.LockForWrite();bool result=f.loader.StreamDoneFunc(&f.output);f.output.UnlockForWrite();
        Check(!result && f.loader.meStreamStage==BundleLoaderModule::STREAMSTAGE_LOADDONE,"failed allocations bypass fixup and progress to failure completion");
        Check(!f.loader.MoveToFirstResource(),"failed allocation never dereferences its entry pointers");
        f.loader.mAllocationResponse.mbFailed=false;f.loader.mAllocationResponse.miNumEntries=0;
        Check(!f.loader.MoveToFirstResource(),"empty allocation list does not read a nonexistent first entry");
    }
    {
        Fixture f;Reader next;
        f.loader.meStreamStage=BundleLoaderModule::STREAMSTAGE_FIXUP;
        f.loader.mLoadRequest.SetFileName("active.bundle");
        f.loader.mabStreamBuffersUsed[0]=true;
        f.loader.mapcStreamBuffers[1]=f.header;
        Events::LoadBundleRequest request{};request.SetFileName("next.bundle");request.miPoolId=3;
        request.miEventId=57;request.mbUseHDCache=true;f.loader.mLoadRequestQueue.Push(&request,0);
        f.output.LockForWrite();bool opened=f.loader.CheckForLoads(&f.output);
        bool closed=f.loader.StreamClose(&f.output);f.output.UnlockForWrite();
        f.output.LockForRead();const auto& out=static_cast<const BundleLoaderIO::OutputBuffer&>(f.output);
        Events::OpenReadStreamRequest open{};Events::CloseReadStreamRequest close{};
        bool records=EventAt(out.GetStreamRequestQueue(),16,open)&&EventAt(out.GetStreamRequestQueue(),18,close);
        Check(opened && closed && records,"one native stream request queue holds prefetch-open and current-close together");
        Check(records && open.GetEventId()==1 && open.GetBuffer()==f.header
            && open.GetBufferSize()==4*1024*1024 && open.GetNumBlocks()==4
            && open.GetNormalPriority()==25 && open.GetHighPriority()==25 && open.GetUseHDCache(),
            "prefetch retains the inactive native buffer, block count, priorities and cache policy");
        Check(records && close.GetEventId()==0 && f.loader.mQueuedLoads.GetLength()==1
            && std::strcmp(f.loader.mLoadRequest.macFileName,"active.bundle")==0,
            "prefetch queues the next load while preserving the active load record");
        f.output.UnlockForRead();ClearOutput(f.output);
        request.SetFileName("third.bundle");f.loader.mLoadRequestQueue.Push(&request,0);
        f.output.LockForWrite();opened=f.loader.CheckForLoads(&f.output);f.output.UnlockForWrite();
        Check(!opened && f.loader.mabStreamBuffersUsed[0] && f.loader.mabStreamBuffersUsed[1],
            "neither closing nor prefetched buffers are reused before their acknowledgements");
        CgsFileSystem::ReadStream stream;stream.Construct(reinterpret_cast<CgsFileSystem::StreamDeviceDiskRead*>(&next));
        Events::OpenReadStreamResponse ready{};ready.Construct(&f.loader.mReceiverQueue,1,stream);
        Events::CloseReadStreamResponse retired{};retired.Construct(&f.loader.mReceiverQueue,0,stream);
        f.loader.mReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&ready),16,sizeof(ready));
        f.loader.mReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&retired),18,sizeof(retired));
        f.loader.ProcessReceiverQueue();
        Check(f.loader.maStreams[1].mpStreamDevice==stream.mpStreamDevice && !f.loader.mabStreamBuffersUsed[0]
            && f.loader.mabStreamBuffersUsed[1] && f.loader.mReceiverQueue.GetCount()==0,
            "native open/close replies update their own stream and buffer only");
        f.loader.meStreamStage=BundleLoaderModule::STREAMSTAGE_IDLE;
        f.output.LockForWrite();bool queued=f.loader.StreamIdleFunc(&f.output);f.output.UnlockForWrite();
        Check(queued && std::strcmp(f.loader.mLoadRequest.macFileName,"next.bundle")==0
            && f.loader.mQueuedLoads.GetLength()==0,"idle consumes the prefetched request before opening another load");
    }
    {
        Fixture f;
        Events::LoadBundleRequest request{};request.SetFileName("Shared.Bundle");request.miPoolId=3;request.miEventId=91;
        request.mpUser=&f.loader.mReceiverQueue;
        f.loaded[0].miPoolId=3;f.loaded[0].miRefCount=1;
        f.loaded[0].mResourceId.SetHash(static_cast<u32>(ID::HashString(reinterpret_cast<const u8*>(request.macFileName)))|0x8000000000000000ull);
        f.loader.miNumLoadedBundles=1;f.loader.mLoadRequestQueue.Push(&request,0);
        f.output.LockForWrite();bool opened=f.loader.CheckForLoads(&f.output);f.output.UnlockForWrite();
        f.output.LockForRead();const auto& out=static_cast<const BundleLoaderIO::OutputBuffer&>(f.output);
        Check(!opened && f.loaded[0].miRefCount==2 && out.GetLoadBundleResponseQueue()->GetLength()==1
            && out.GetLoadBundleResponseQueue()->GetEvent(0).miEventId==91,
            "resident bundle load takes one logical reference and completes without reopening the file");
        f.output.UnlockForRead();ClearOutput(f.output);
        Events::UnloadBundleRequest unload{};static_cast<Events::BundleLoaderEvent&>(unload)=request;
        f.loader.mUnloadRequestQueue.Push(&unload,0);
        f.output.LockForWrite();bool unloaded=f.loader.CheckForUnloads(&f.output);f.output.UnlockForWrite();
        f.output.LockForRead();Events::UnloadResourceListRequest retire{};
        Check(unloaded && f.loaded[0].miRefCount==1 && f.loaded[0].miPoolId==3
            && !EventAt(out.GetPoolSendQueue(),20,retire) && out.GetUnloadBundleResponseQueue()->GetLength()==1,
            "shared unload releases only its logical reference while another owner remains");
        f.output.UnlockForRead();ClearOutput(f.output);
        f.loader.mUnloadRequestQueue.Push(&unload,0);
        f.output.LockForWrite();f.loader.CheckForUnloads(&f.output);f.output.UnlockForWrite();
        f.output.LockForRead();
        Check(EventAt(out.GetPoolSendQueue(),20,retire) && retire.miPoolId==3
            && retire.mListId==f.loaded[0].mResourceId && f.loaded[0].miRefCount==0
            && f.loaded[0].miPoolId==-1 && f.loader.miNumLoadedBundles==0,
            "last logical owner emits the native pool unload and frees the bundle-table slot");
        f.output.UnlockForRead();
    }
    {
        Fixture f;
        Events::LoadBundleRequest request{};request.SetFileName("Shared.Bundle");request.miPoolId=3;
        request.mbLiveUpdateReplace=true;
        f.loaded[0].miPoolId=3;f.loaded[0].miRefCount=2;
        f.loaded[0].mResourceId.SetHash(static_cast<u32>(ID::HashString(reinterpret_cast<const u8*>(request.macFileName)))|0x8000000000000000ull);
        f.loader.miNumLoadedBundles=1;f.loader.mLoadRequestQueue.Push(&request,0);
        f.output.LockForWrite();bool opened=f.loader.CheckForLoads(&f.output);f.output.UnlockForWrite();
        f.output.LockForRead();const auto& out=static_cast<const BundleLoaderIO::OutputBuffer&>(f.output);
        Events::OpenReadStreamRequest open{};
        Check(opened && EventAt(out.GetStreamRequestQueue(),16,open)
            && out.GetLoadBundleResponseQueue()->GetLength()==0 && f.loaded[0].miRefCount==2
            && f.loader.miNumLoadedBundles==1 && f.loader.mLoadRequest.mbLiveUpdateReplace,
            "live replacement of a resident bundle opens its data without an extra logical reference or premature success");
        f.output.UnlockForRead();
    }
    {
        Fixture f;
        f.loader.meStreamStage=BundleLoaderModule::STREAMSTAGE_FIXUP;
        f.loader.mLoadRequest.SetFileName("active.bundle");
        f.loader.mLoadRequest.miEventId=90;f.loader.mLoadRequest.miPoolId=3;
        Events::LoadBundleRequest cached{};cached.SetFileName("resident.bundle");
        cached.miEventId=91;cached.miPoolId=3;
        f.loaded[0].miPoolId=3;f.loaded[0].miRefCount=1;
        f.loaded[0].mResourceId.SetHash(static_cast<u32>(ID::HashString(
            reinterpret_cast<const u8*>(cached.macFileName)))|0x8000000000000000ull);
        f.loader.mLoadRequestQueue.Push(&cached,0);
        f.Tick();
        f.output.LockForRead();const auto& out=static_cast<const BundleLoaderIO::OutputBuffer&>(f.output);
        Check(out.GetLoadBundleResponseQueue()->GetLength()==0 && f.loaded[0].miRefCount==1
            && f.loader.mLoadRequestQueue.GetLength()==1,
            "a resident prefetch waits for the earlier bundle's final fixup and completion");
        f.output.UnlockForRead();
        f.loader.meStreamStage=BundleLoaderModule::STREAMSTAGE_LOADDONE;
        f.loader.mAllocationResponse.mbFailed=false;
        f.Tick();f.Tick();
        f.output.LockForRead();const auto* replies=out.GetLoadBundleResponseQueue();
        Check(replies->GetLength()==2 && replies->GetEvent(0).miEventId==90
            && replies->GetEvent(1).miEventId==91 && f.loaded[0].miRefCount==2,
            "the active load completes before the cached load with exactly one new reference");
        f.output.UnlockForRead();
    }
    {
        Fixture f;
        Events::LoadBundleRequest request{};request.SetFileName("missing.bundle");
        request.miPoolId=3;request.miEventId=97;request.mpUser=&f.loader.mReceiverQueue;
        f.loader.mLoadRequestQueue.Push(&request,0);f.Tick();ClearOutput(f.output);
        CgsFileSystem::ReadStream invalid;invalid.Construct(nullptr);
        Events::OpenReadStreamResponse failed{};
        failed.Construct(&f.loader.mReceiverQueue,f.loader.miCurrentStream,invalid);
        f.loader.mReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&failed),16,sizeof(failed));
        f.Tick();
        f.output.LockForRead();const auto& out=static_cast<const BundleLoaderIO::OutputBuffer&>(f.output);
        const auto* replies=out.GetLoadBundleResponseQueue();
        Check(replies->GetLength()==1 && replies->GetEvent(0).miEventId==97
            && replies->GetEvent(0).meResult==Events::LoadBundleResponse::E_RESULT_OUT_OF_MEMORY
            && f.loader.meStreamStage==BundleLoaderModule::STREAMSTAGE_IDLE
            && !f.loader.mabStreamBuffersUsed[f.loader.miCurrentStream],
            "failed opens return the original request identity and release the stream reservation");
        f.output.UnlockForRead();ClearOutput(f.output);
        request.SetFileName("next.bundle");request.miEventId=98;
        f.loader.mLoadRequestQueue.Push(&request,0);f.Tick();
        f.output.LockForRead();Events::OpenReadStreamRequest open{};
        Check(EventAt(out.GetStreamRequestQueue(),16,open)
            && f.loader.meStreamStage==BundleLoaderModule::STREAMSTAGE_STREAMHEADER
            && f.loader.mLoadRequest.miEventId==98,
            "a failed open does not block the next queued bundle");
        f.output.UnlockForRead();
    }
    Check(assertions==0,"valid staged protocol satisfies engine assertions");
    std::printf("PCBundleStages: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
