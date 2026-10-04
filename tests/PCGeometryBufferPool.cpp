#include "pc/gcm/renderengine/GeometryBufferPoolPCLeaf.h"
#include <memory>
#include <cstring>
#include <cstdio>

using namespace renderengine;
static int checks,failures;
static void Check(bool pass,const char* name) {
    ++checks; if (!pass) { ++failures; std::printf("FAIL %s\n",name); }
}
struct Backend {
    struct Storage { std::vector<unsigned char> data; bool destroyed=false; unsigned writes=0; };
    struct Event { bool issued=false,ready=false,destroyed=false; };
    using Buffer=Storage*; using Fence=Event*; using Context=void*;
    std::vector<std::unique_ptr<Storage>> buffers;
    std::vector<std::unique_ptr<Event>> fences;
    unsigned creates=0,destroys=0,fenceCreates=0,fenceDestroys=0,discards=0,appends=0;
    bool supported=true,failCreate=false,failUpload=false,failFence=false,failIssue=false,failPoll=false;
    Fence lastIssue=nullptr;
    bool Supported(Context) { return supported; }
    Buffer CreateBuffer(Context,GeometryBufferKind,unsigned bytes) {
        if (failCreate) return nullptr;
        auto storage=std::make_unique<Storage>(); storage->data.resize(bytes,0xcd);
        auto result=storage.get(); buffers.push_back(std::move(storage)); ++creates; return result;
    }
    bool Upload(Buffer buffer,GeometryBufferKind,unsigned offset,const void* data,unsigned bytes,bool discard) {
        if (failUpload) return false;
        if (discard) { ++discards; Check(buffer->writes==0,"DISCARD never invalidates an existing slice"); }
        else ++appends;
        Check(offset+bytes<=buffer->data.size(),"upload remains inside its page");
        std::memcpy(buffer->data.data()+offset,data,bytes); ++buffer->writes; return true;
    }
    void DestroyBuffer(Buffer buffer) { Check(!buffer->destroyed,"native buffer released once"); buffer->destroyed=true; ++destroys; }
    Fence CreateFence(Context) {
        if(failFence)return nullptr;
        auto event=std::make_unique<Event>();auto result=event.get();fences.push_back(std::move(event));++fenceCreates;return result;
    }
    bool IssueFence(Fence event) { if(failIssue)return false;event->issued=true;event->ready=false;lastIssue=event;return true; }
    GeometryFenceStatus PollFence(Fence event) {
        Check(event->issued,"only issued fences are polled");
        return failPoll?GeometryFenceStatus::Failed:event->ready?GeometryFenceStatus::Complete:GeometryFenceStatus::Pending;
    }
    void DestroyFence(Fence event) { Check(!event->destroyed,"native fence released once");event->destroyed=true;++fenceDestroys; }
};
using Pool=PCGeometryBufferPool<Backend>;
int main() {
    unsigned char data[48];std::memset(data,0x43,sizeof(data));
    void* device=reinterpret_cast<void*>(1);
    {
        Pool pool(128);auto& backend=pool.GetBackend();Pool::Allocation a,b,c,d;
        Check(!pool.Store(device,GeometryBufferKind::Vertex,nullptr,48,a),"null source cannot allocate");
        Check(!pool.Store(device,GeometryBufferKind::Vertex,data,0,a),"zero-byte request cannot allocate");
        Check(pool.Store(device,GeometryBufferKind::Vertex,data,48,a),"first slice uploads");
        Check(pool.Store(device,GeometryBufferKind::Vertex,data,48,b)&&a.GetBuffer()==b.GetBuffer()&&a.muOffset!=b.muOffset,"multiple meshes share disjoint native buffer ranges");
        auto page=a.GetBuffer();
        Check(backend.creates==1&&backend.discards==1&&backend.appends==1,"append avoids allocating or discarding another native buffer");
        Check(!pool.Store(reinterpret_cast<void*>(2),GeometryBufferKind::Vertex,data,16,d)&&!page->destroyed,"device change cannot silently invalidate a live allocation");
        pool.Retire(a);Check(!a,"retirement consumes its handle");
        pool.BeginFrame();
        Check(pool.Store(device,GeometryBufferKind::Vertex,data,48,c)&&c.GetBuffer()!=page,"pending GPU work prevents range reuse");
        pool.BeginFrame();Check(!page->destroyed&&pool.mStatistics.muRetiredBytes==48,"another CPU frame is not proof of GPU completion");
        backend.lastIssue->ready=true;pool.BeginFrame();
        Check(pool.Store(device,GeometryBufferKind::Vertex,data,48,d)&&d.GetBuffer()==page&&d.muOffset==0,"completed fence permits reuse of the retired hole");
        Check(backend.discards==2&&backend.appends==2&&!std::memcmp(page->data.data()+b.muOffset,data,48),"reusing a hole preserves other live meshes");
        pool.Retire(b);pool.Retire(d);pool.BeginFrame();Check(!page->destroyed,"empty page survives until its final GPU use completes");
        backend.lastIssue->ready=true;pool.BeginFrame();Check(page->destroyed,"fully retired page returns native memory");
        pool.Retire(c);pool.BeginFrame();pool.ReleaseAll();
        Check(backend.creates==backend.destroys&&backend.fenceCreates==backend.fenceDestroys,"teardown releases pending pages and queries exactly once");
        Check(pool.mStatistics.muResidentBytes==0&&pool.mStatistics.muRetiredBytes==0,"teardown clears allocation accounting");
        pool.ReleaseAll();Check(backend.creates==backend.destroys,"repeated teardown is harmless");
    }
    {
        Pool pool(128);Pool::Allocation a,b,c;auto& backend=pool.GetBackend();
        pool.Store(device,GeometryBufferKind::Vertex,data,16,a);
        pool.Store(device,GeometryBufferKind::Index16,data,16,b);
        pool.Store(device,GeometryBufferKind::Index32,data,16,c);
        Check(backend.creates==3&&a.GetBuffer()!=b.GetBuffer()&&b.GetBuffer()!=c.GetBuffer(),"vertex and both index formats use separate pages");
    }
    for(int failure=0;failure<4;++failure) {
        Pool pool(128);Pool::Allocation a;auto& backend=pool.GetBackend();
        if(failure==0)backend.supported=false;
        if(failure==1)backend.failCreate=true;
        if(failure==2)backend.failUpload=true;
        if(failure==3)backend.failFence=true;
        Check(!pool.Store(device,GeometryBufferKind::Vertex,data,48,a)&&!a,"unsupported or failed storage leaves no published allocation");
        Check(pool.mStatistics.muLiveBytes==0,"failed upload does not leak a live range");
    }
    for(bool pollFailure:{false,true}) {
        Pool pool(128);Pool::Allocation a,b;auto& backend=pool.GetBackend();
        pool.Store(device,GeometryBufferKind::Vertex,data,48,a);auto page=a.GetBuffer();pool.Retire(a);
        backend.failIssue=!pollFailure;pool.BeginFrame();backend.failPoll=pollFailure;pool.BeginFrame();
        Check(!pool.Store(device,GeometryBufferKind::Vertex,data,48,b)&&!page->destroyed,"failed synchronization quarantines bytes instead of reusing them");
    }
    {
        Pool pool(256);auto& backend=pool.GetBackend();Pool::Allocation a,b,c,d;
        Check(!pool.Store(device,GeometryBufferKind::Vertex,data,16,a,0)
              &&!pool.Store(device,GeometryBufferKind::Vertex,data,16,a,~0u)
              &&backend.creates==0,"invalid or overflowing alignment is rejected before allocating");
        Check(pool.Store(device,GeometryBufferKind::Vertex,data,16,a),"unaligned prefix fixture uploads");
        Check(pool.Store(device,GeometryBufferKind::Vertex,data,40,b,24)
              &&a.GetBuffer()==b.GetBuffer()&&b.muOffset==48&&b.muBytes==48,
              "non-power-of-two stride preserves both 16-byte and whole-vertex alignment");
        Check(pool.Store(device,GeometryBufferKind::Vertex,data,32,c)
              &&c.GetBuffer()==a.GetBuffer()&&c.muOffset==16,
              "alignment prefix stays available for a smaller mesh");
        pool.Retire(b);pool.BeginFrame();
        Check(pool.Store(device,GeometryBufferKind::Vertex,data,48,d,24)
              &&d.muOffset==96,"a pending aligned span cannot be reused");
        backend.lastIssue->ready=true;pool.BeginFrame();
        Check(pool.Store(device,GeometryBufferKind::Vertex,data,48,b,24)
              &&b.muOffset==48,"completed aligned span is reusable without touching its neighbours");
        Check(!std::memcmp(a.GetBuffer()->data.data()+a.muOffset,data,16)
              &&!std::memcmp(c.GetBuffer()->data.data()+c.muOffset,data,32)
              &&!std::memcmp(d.GetBuffer()->data.data()+d.muOffset,data,48),
              "aligned reuse preserves all live payloads");
    }
    {
        Pool pool(256);Pool::Allocation a,b;auto& backend=pool.GetBackend();
        pool.Store(device,GeometryBufferKind::Vertex,data,16,a);
        backend.failUpload=true;
        Check(!pool.Store(device,GeometryBufferKind::Vertex,data,48,b,36)
              &&!b&&pool.mStatistics.muLiveBytes==16,
              "aligned upload failure returns its reservation without counting it live");
        Check(!std::memcmp(a.GetBuffer()->data.data(),data,16),"failed aligned upload preserves older live geometry");
        backend.failUpload=false;
        Check(pool.Store(device,GeometryBufferKind::Vertex,data,48,b,36)
              &&b.GetBuffer()!=a.GetBuffer(),"failed upload page remains quarantined");
    }
    {
        Pool pool(512);auto& backend=pool.GetBackend();
        struct Live { Pool::Allocation allocation; unsigned bytes; unsigned char value; };
        std::vector<Live> live;
        unsigned random=0x175ab823u;
        bool aligned=true,disjoint=true,preserved=true,stored=true;
        const unsigned strides[]={12,20,24,28,32,36,40,44,48,52,64,80};
        for(unsigned step=0;step<160;++step) {
            random=random*1664525u+1013904223u;
            if(live.size()>12 || (!live.empty()&&(random&3u)==0)) {
                const unsigned index=random%live.size();pool.Retire(live[index].allocation);
                live.erase(live.begin()+index);pool.BeginFrame();
                if((step&1u)==0) {
                    for(auto& fence:backend.fences)if(!fence->destroyed&&fence->issued)fence->ready=true;
                    pool.BeginFrame();
                }
            } else {
                const unsigned stride=strides[(random>>8)%12],bytes=stride*(1+(random>>16)%3);
                std::vector<unsigned char> payload(bytes,static_cast<unsigned char>(step+1));
                Live entry{{},bytes,static_cast<unsigned char>(step+1)};
                if(!pool.Store(device,GeometryBufferKind::Vertex,payload.data(),bytes,entry.allocation,stride)) {stored=false;break;}
                aligned &= entry.allocation.muOffset%stride==0&&entry.allocation.muOffset%16==0;
                for(const auto& other:live)if(entry.allocation.GetBuffer()==other.allocation.GetBuffer())
                    disjoint &= entry.allocation.muOffset+entry.allocation.muBytes<=other.allocation.muOffset
                        ||other.allocation.muOffset+other.allocation.muBytes<=entry.allocation.muOffset;
                live.push_back(entry);
            }
            for(const auto& entry:live)for(unsigned byte=0;byte<entry.bytes;++byte)
                preserved &= entry.allocation.GetBuffer()->data[entry.allocation.muOffset+byte]==entry.value;
        }
        Check(stored&&aligned,"mixed vertex strides remain aligned through fragmentation and fence completion");
        Check(disjoint&&preserved,"mixed-stride allocations preserve every live range and its complete payload");
        for(auto& entry:live)pool.Retire(entry.allocation);
        pool.BeginFrame();
        for(auto& fence:backend.fences)if(!fence->destroyed&&fence->issued)fence->ready=true;
        pool.BeginFrame();
        Check(pool.mStatistics.muResidentBytes==0&&pool.mStatistics.muRetiredBytes==0,
              "prefix and suffix fragments do not retain empty pages after final fence completion");
    }
    {
        Pool pool(512); auto& backend=pool.GetBackend(); Pool::Allocation a,b,c,d;
        pool.Store(device,GeometryBufferKind::Vertex,data,48,a);
        pool.Store(device,GeometryBufferKind::Vertex,data,48,b,128);
        unsigned char larger[128]{};
        pool.Store(device,GeometryBufferKind::Vertex,larger,128,c);
        pool.Retire(a); pool.BeginFrame(); backend.lastIssue->ready=true; pool.BeginFrame();
        Check(pool.Store(device,GeometryBufferKind::Vertex,larger,96,d)&&d.muOffset==0,
              "fence completion invalidates skipped-prefix bounds after coalescing enlarges an old hole");
    }
    {
        Pool hinted(8u*1024u*1024u,true), linear(8u*1024u*1024u,false);
        const unsigned strides[]={12,20,24,28,32,36,40,44,48,52,64,80};
        bool same=true, payloads=true;
        for(unsigned i=0;i<3000;++i) {
            const unsigned stride=strides[i%12],bytes=stride*13u;
            std::vector<unsigned char> payload(bytes,static_cast<unsigned char>(i));
            Pool::Allocation a,b;
            const bool x=hinted.Store(device,GeometryBufferKind::Vertex,payload.data(),bytes,a,stride);
            const bool y=linear.Store(device,GeometryBufferKind::Vertex,payload.data(),bytes,b,stride);
            same &= x&&y&&a.muOffset==b.muOffset&&a.muBytes==b.muBytes
                &&hinted.GetBackend().creates==linear.GetBackend().creates;
            if(x&&y)payloads &= !std::memcmp(a.GetBuffer()->data.data()+a.muOffset,payload.data(),bytes)
                &&!std::memcmp(b.GetBuffer()->data.data()+b.muOffset,payload.data(),bytes);
        }
        Check(same&&payloads,"cold mixed-stride population keeps exact first-fit offsets, page counts and payloads");
        Check(hinted.mStatistics.muRangeProbes*4u<linear.mStatistics.muRangeProbes,
              "large requests avoid rescanning known-small alignment fragments");
        std::printf("range probes: hinted=%llu linear=%llu\n",
                    hinted.mStatistics.muRangeProbes,linear.mStatistics.muRangeProbes);
    }
    {
        Pool hinted(4096,true),linear(4096,false);
        struct Pair { Pool::Allocation a,b; unsigned bytes; unsigned char value; };
        std::vector<Pair> live;
        unsigned random=0x7838a0edu;
        const unsigned strides[]={4,12,20,24,28,32,36,40,44,48,52,64,80,128};
        bool same=true,payloads=true;
        auto pageIndex=[](Backend& backend,Backend::Buffer buffer) {
            for(size_t i=0;i<backend.buffers.size();++i)if(backend.buffers[i].get()==buffer)return i;
            return backend.buffers.size();
        };
        for(unsigned step=0;step<5000;++step) {
            random=random*1664525u+1013904223u;
            if(!live.empty()&&(live.size()>80||(random&3u)==0)) {
                const unsigned index=random%live.size();
                hinted.Retire(live[index].a);linear.Retire(live[index].b);
                live.erase(live.begin()+index);
            } else {
                const unsigned stride=strides[(random>>8)%14],bytes=stride*(1+(random>>16)%19);
                Pair pair{{},{},bytes,static_cast<unsigned char>(step)};
                std::vector<unsigned char> payload(bytes,pair.value);
                const bool a=hinted.Store(device,GeometryBufferKind::Vertex,payload.data(),bytes,pair.a,stride);
                const bool b=linear.Store(device,GeometryBufferKind::Vertex,payload.data(),bytes,pair.b,stride);
                same &= a&&b&&pair.a.muOffset==pair.b.muOffset&&pair.a.muBytes==pair.b.muBytes
                    &&pageIndex(hinted.GetBackend(),pair.a.GetBuffer())==pageIndex(linear.GetBackend(),pair.b.GetBuffer());
                if(!a||!b)break;
                live.push_back(pair);
            }
            if(step%7u==0) {hinted.BeginFrame();linear.BeginFrame();}
            if(step%23u==0) {
                for(Pool* pool:{&hinted,&linear}) {
                    for(auto& fence:pool->GetBackend().fences)if(!fence->destroyed&&fence->issued)fence->ready=true;
                    pool->BeginFrame();
                }
            }
            if(step%31u==0)for(const auto& pair:live)for(unsigned i=0;i<pair.bytes;++i)
                payloads &= pair.a.GetBuffer()->data[pair.a.muOffset+i]==pair.value
                    &&pair.b.GetBuffer()->data[pair.b.muOffset+i]==pair.value;
        }
        Check(same&&hinted.GetBackend().creates==linear.GetBackend().creates,
              "search certificates preserve linear first-fit through mixed small requests, retirement and coalescing");
        Check(payloads,"changing search bounds never overwrites live geometry through fenced reuse");
    }
    std::printf("PCGeometryBufferPool: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
