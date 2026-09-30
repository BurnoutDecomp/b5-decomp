#include <cstdio>
#include <cstring>
#include <vector>
#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h"
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"
#include "GameShared/GameClasses/Containers/CgsIndexedPool.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"

static int checks,failures,assertions;
static std::vector<int> order;
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint=nullptr; }
namespace Message { u64 gxMessageFilterFlags=0; } }
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char* text,const char*,int){++assertions;std::printf("ASSERT %s\n",text);return 0;}
void* EndAssert(){return nullptr;}
} }
static void Check(bool ok,const char* text){++checks;if(!ok){++failures;std::printf("FAIL %s\n",text);}}
namespace CgsFileSystem {
void ReadStream::Construct(StreamDeviceDiskRead* p){mpStreamDevice=p;}
bool ReadStream::IsValid() const{return mpStreamDevice!=nullptr;}
}
namespace CgsResource {
struct FakeFileSystem {
    struct State{bool open=false,failed=false,closed=false;} states[8];
    int opens=0,closes=0;void* buffer=nullptr;u32 size=0,blocks=0;
    CgsFileSystem::ReadStream OpenReadStream(const char*,void* p,u32 n,u32 count,s32,s32,bool){
        buffer=p;size=n;blocks=count;CgsFileSystem::ReadStream result;
        result.Construct(reinterpret_cast<CgsFileSystem::StreamDeviceDiskRead*>(&states[opens++]));return result;
    }
    State& StateOf(CgsFileSystem::ReadStream s){return *reinterpret_cast<State*>(s.mpStreamDevice);}
    u32 GetReadStreamIndex(CgsFileSystem::ReadStream s){return static_cast<u32>(&StateOf(s)-states);}
    bool HasReadStreamFailedPC(CgsFileSystem::ReadStream s){return !s.IsValid()||StateOf(s).failed;}
    bool IsReadStreamOpen(CgsFileSystem::ReadStream s){return StateOf(s).open;}
    void CloseReadStream(CgsFileSystem::ReadStream){++closes;}
    bool IsReadStreamClosed(s32 i){return states[i].closed;}
};
struct FakeLoader {
    int cached=0;std::vector<int> seen;
    bool Update(void*,void*){order.push_back(1);seen.push_back(cached);cached=0;return false;}
    void RecordPostUpdateEvents(const BundleLoaderIO::InputBuffer_Record* input){
        order.push_back(3);auto* lock=const_cast<BundleLoaderIO::InputBuffer_Record*>(input);lock->LockForRead();
        const CgsModule::Event* p=nullptr;s32 n=0;
        cached=input->GetPoolReceiveQueue()->GetFirstEvent(&p,&n)==17?1:0;lock->UnlockForRead();
    }
};
struct FakePool {
    bool busy=true;
    bool Update(void*,void* out){order.push_back(2);auto* output=static_cast<PoolIO::OutputBuffer*>(out);
        Events::AllocateResourceListResponse reply{};output->LockForWrite();
        output->GetPoolOutputQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&reply),17,sizeof(reply));
        output->UnlockForWrite();return busy;}
};
struct FakeMemory {void Update(void*,void*,void*,void*){order.push_back(4);}};
struct Shuttle {
    static const int KI_MAX_PENDING_FILE_SYSTEM_RESPONSES=16;
    struct PendingFileResponse{void* mpResponse;s32 meEvent;u32 muFileId;};
    PendingFileResponse records[16];s8 indices[16];
    CgsContainers::IndexedPool<PendingFileResponse,16,s8> mPendingFileResponses;
    FakeFileSystem mFileSystem;FakeLoader mBundleLoaderModule;FakePool mPoolModule;FakeMemory mMemoryModule;
    Shuttle(){mPendingFileResponses.Construct(records,indices,16);}
    bool AddOpenReadStreamRequest(const Events::OpenReadStreamRequest*);
    bool AddCloseReadStreamRequest(const Events::CloseReadStreamRequest*);
    bool AddOpenWriteStreamRequest(const Events::OpenWriteStreamRequest*){++assertions;return false;}
    bool AddCloseWriteStreamRequest(const Events::CloseWriteStreamRequest*){++assertions;return false;}
    void ProcessPendingFileSystemResponses();
    void ProcessResourceRequests(ResourceIO::InputBuffer*,CgsMemory::MemoryIO::InputBuffer*,BundleLoaderIO::InputBuffer_Update*,PoolIO::InputBuffer*);
    void ProcessBundleLoaderStreamRequests(const BundleLoaderIO::OutputBuffer*);
    void ProcessPoolOutputResponses(PoolIO::OutputBuffer*);
    void ProcessResourceResponses(const BundleLoaderIO::OutputBuffer*,PoolIO::OutputBuffer*);
    void ProcessPoolResourceRequests(CgsMemory::MemoryIO::InputBuffer*,PoolIO::OutputBuffer*);
    void ProcessMemoryResponses(CgsMemory::MemoryIO::OutputBuffer*);
    bool Update(void*,void*);
};
}
#include "pc_resource_shuttle.inc"

int main(){
    using namespace CgsResource;
    Shuttle shuttle;ResourceIO::InputBuffer input;PoolIO::InputBuffer pool;
    BundleLoaderIO::InputBuffer_Update loader;CgsMemory::MemoryIO::InputBuffer memory;
    input.Construct();pool.Construct();loader.Construct();memory.Construct();
    Events::LoadBundleRequest load{};load.mpUser=reinterpret_cast<CgsModule::BaseEventReceiverQueue*>(0x123456780000ull);
    load.miPoolId=17;load.miEventId=123;std::strcpy(load.macFileName,"native.bndl");
    input.LockForWrite();input.GetResourceQueue()->AddEvent(reinterpret_cast<CgsModule::Event*>(&load),2,sizeof(load));
    Events::PoolEvent marker{};
    for(int tag=4;tag<=8;++tag)input.GetResourceQueue()->AddEvent(reinterpret_cast<CgsModule::Event*>(&marker),tag,sizeof(marker));
    for(int tag=9;tag<=15;++tag)input.GetResourceQueue()->AddEvent(reinterpret_cast<CgsModule::Event*>(&marker),tag,sizeof(marker));
    input.UnlockForWrite();input.LockForRead();pool.LockForWrite();loader.LockForWrite();memory.LockForWrite();
    shuttle.ProcessResourceRequests(&input,&memory,&loader,&pool);
    pool.UnlockForWrite();loader.UnlockForWrite();memory.UnlockForWrite();input.UnlockForRead();
    loader.LockForRead();const auto& copied=static_cast<const BundleLoaderIO::InputBuffer_Update&>(loader).GetLoadBundleRequestQueue()->GetEvent(0);
    Check(copied.mpUser==load.mpUser&&copied.miEventId==123&&copied.miPoolId==17,"bundle intake preserves native pointers and request identity");loader.UnlockForRead();
    pool.LockForRead();const auto* q=static_cast<const PoolIO::InputBuffer&>(pool).GetPoolInputQueue();
    const CgsModule::Event* p=nullptr;s32 bytes=0;int tag=q->GetFirstEvent(&p,&bytes);std::vector<int> tags;
    while(p){tags.push_back(tag);const auto* prev=p;tag=q->GetNextEvent(prev,&p,&bytes);}
    Check(tags==std::vector<int>({4,5,8,11,12}),"resource operations use the original pool request tags");pool.UnlockForRead();
    memory.LockForRead();const auto* mq=static_cast<const CgsMemory::MemoryIO::InputBuffer&>(memory).GetMemoryRequestQueue();
    tag=mq->GetFirstEvent(&p,&bytes);int count=0;bool valid=true;
    while(p){++count;valid=valid&&tag==1;const auto* prev=p;tag=mq->GetNextEvent(prev,&p,&bytes);}
    Check(count==7&&valid,"all memory request variants reach the memory module");memory.UnlockForRead();

    CgsModule::EventReceiverQueue<1024,16> replies;replies.Construct();
    BundleLoaderIO::OutputBuffer out;out.Construct();Events::OpenReadStreamRequest open;
    open.Construct(&replies,3);open.SetFileName("native.bndl");open.SetBuffer(reinterpret_cast<void*>(0x12345678AB00ull));
    open.SetBufferSize(0x400000);open.SetNumBlocks(4);open.SetNormalPriority(25);open.SetHighPriority(25);open.SetUseHDCache(false);
    out.LockForWrite();out.GetStreamRequestQueue()->AddEvent(reinterpret_cast<CgsModule::Event*>(&open),16,sizeof(open));out.UnlockForWrite();
    out.LockForRead();shuttle.ProcessBundleLoaderStreamRequests(&out);out.UnlockForRead();
    Check(shuttle.mFileSystem.opens==1&&shuttle.mFileSystem.buffer==open.GetBuffer()&&shuttle.mFileSystem.size==0x400000&&shuttle.mFileSystem.blocks==4,
        "stream shuttle forwards the native buffer and nonzero ring dimensions");
    shuttle.ProcessPendingFileSystemResponses();Check(replies.GetLength()==0,"pending opens do not report completion early");
    shuttle.mFileSystem.states[0].open=true;shuttle.ProcessPendingFileSystemResponses();
    tag=replies.GetFirstEvent(&p,&bytes);const auto* opened=reinterpret_cast<const Events::OpenReadStreamResponse*>(p);
    auto handle=opened->GetStream();
    Check(tag==16&&bytes==sizeof(*opened)&&opened->GetEventId()==3&&handle.IsValid(),"completed opens return their native handle and event ID");
    replies.Clear();Events::CloseReadStreamRequest close;close.Construct(&replies,3,handle);shuttle.AddCloseReadStreamRequest(&close);
    shuttle.ProcessPendingFileSystemResponses();Check(replies.GetLength()==0&&shuttle.mFileSystem.closes==1,"close replies wait for actual closure");
    shuttle.mFileSystem.states[0].closed=true;shuttle.ProcessPendingFileSystemResponses();
    Check(replies.GetFirstEvent(&p,&bytes)==18,"closed streams return a close response");replies.Clear();
    shuttle.AddOpenReadStreamRequest(&open);shuttle.mFileSystem.states[1].failed=true;shuttle.ProcessPendingFileSystemResponses();
    Check(replies.GetLength()==0&&shuttle.mFileSystem.closes==2,"failed opens close and release their stream slot before replying");
    shuttle.mFileSystem.states[1].closed=true;shuttle.ProcessPendingFileSystemResponses();
    tag=replies.GetFirstEvent(&p,&bytes);opened=reinterpret_cast<const Events::OpenReadStreamResponse*>(p);
    Check(tag==16&&!opened->GetStream().IsValid()&&opened->GetEventId()==3,"failed opens return an invalid handle without losing request identity");
    Shuttle::PendingFileResponse* pending[16]{};
    Check(shuttle.mPendingFileResponses.Get(pending,16)==0,"success and failure paths release every pending response record");
    input.Construct();order.clear();const bool busy=shuttle.Update(&input,nullptr);
    Check(busy&&order==std::vector<int>({1,2,3,4})&&shuttle.mBundleLoaderModule.seen.back()==0,
        "update runs loader before pool and returns the pool busy result");
    shuttle.Update(&input,nullptr);
    Check(shuttle.mBundleLoaderModule.seen.back()==1&&assertions==0,"pool replies become visible on the next loader update with valid IO locks");
    std::printf("PCResourceShuttle: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
