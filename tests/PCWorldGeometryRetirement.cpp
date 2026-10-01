#include "pc/gcm/renderengine/WorldGeometryPCLeaf.cpp"
#include <algorithm>
#include <random>

namespace renderengine {
    IDirect3DDevice9* gDevice=nullptr;
    void WorldVd32_OnResourceMemoryFreed(const void*,size_t) {}
    void WorldVd32_ReleaseAll() {}
}
namespace BrnDiag { void LogBoundSurfaces(const char*,u32,bool) {} }
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }
static int checks,failures;
static void Check(bool pass,const char* label) {
    ++checks;if(!pass){++failures;std::printf("FAIL %s\n",label);}
}
using namespace renderengine;
struct Record {
    VertexKey vertex;IndexKey index;
    size_t header,begin,end;bool live=true;
};
static Record Add(u8* arena,size_t header,size_t begin,size_t bytes) {
    WorldGeometryVertexPlan vp{};vp.mpHeader=arena+header;vp.mpData=arena+begin;
    vp.muNumVertices=1;vp.muSourceStride=vp.muExpandedStride=16;
    WorldGeometryIndexPlan ip{};ip.mpHeader=arena+header;ip.mpRun=arena+begin;
    ip.muIndexCount=3;ip.miMappedPrimitiveType=D3DPT_TRIANGLELIST;ip.muMappedPrimitiveCount=1;
    Record record{MakeVertexKey(vp),MakeIndexKey(ip),header,begin,begin+bytes};
    RetainedVertexBuffer vertex{};vertex.mpHeader=vp.mpHeader;
    vertex.mpSourceBegin=arena+begin;vertex.mpSourceEnd=arena+begin+bytes;
    RetainedIndexBuffer index{};index.mpHeader=ip.mpHeader;
    index.mpSourceBegin=vertex.mpSourceBegin;index.mpSourceEnd=vertex.mpSourceEnd;
    // Null mirrors intentionally cover cached creation failures too: their CPU
    // lookup entries still have to retire before source addresses can be reused.
    sVertexBuffers.emplace(record.vertex,vertex);sIndexBuffers.emplace(record.index,index);
    RegisterPages(sVertexPages,record.vertex,vertex.mpHeader,vertex.mpSourceBegin,vertex.mpSourceEnd);
    RegisterPages(sIndexPages,record.index,index.mpHeader,index.mpSourceBegin,index.mpSourceEnd);
    return record;
}
static bool Present(const Record& record) {
    return sVertexBuffers.find(record.vertex)!=sVertexBuffers.end()
        &&sIndexBuffers.find(record.index)!=sIndexBuffers.end();
}
static bool Absent(const Record& record) {
    return sVertexBuffers.find(record.vertex)==sVertexBuffers.end()
        &&sIndexBuffers.find(record.index)==sIndexBuffers.end();
}
int main() {
    constexpr size_t bytes=8u*1024u*1024u;
    // Address-only fixtures: production retirement never reads source contents.
    auto* arena=static_cast<u8*>(VirtualAlloc(nullptr,bytes,MEM_RESERVE,PAGE_NOACCESS));
    if(!arena)return 2;
    {
        const auto before=Add(arena,4095,0x30000,32);
        const auto first=Add(arena,4096,0x31000,32);
        const auto last=Add(arena,4111,0x32000,32);
        const auto end=Add(arena,4112,0x33000,32);
        const auto generation=suGeometryGeneration;
        WorldGeometry_OnResourceMemoryFreed(nullptr,32);
        WorldGeometry_OnResourceMemoryFreed(arena+4096,0);
        WorldGeometry_OnResourceMemoryFreed(arena+0x40000,128);
        Check(Present(before)&&Present(first)&&Present(last)&&Present(end)
              &&suGeometryGeneration==generation,"empty and unrelated frees preserve entries and cached draws");
        WorldGeometry_OnResourceMemoryFreed(arena+4096,16);
        Check(Present(before)&&Absent(first)&&Absent(last)&&Present(end),
              "partial header frees preserve both neighbours and the exclusive end");
        Check(suGeometryGeneration==generation+4,"each retired vertex/index invalidates borrowed draw metadata");
        WorldGeometry_OnResourceMemoryFreed(arena+4096,16);
        Check(suGeometryGeneration==generation+4,"duplicate notifications cannot retire entries twice");
        const auto reused=Add(arena,4096,0x50000,32);
        WorldGeometry_OnResourceMemoryFreed(arena+0x31000,32);
        Check(Present(reused),"stale source-page keys cannot evict a replacement at the same header address");
        WorldGeometry_OnResourceMemoryFreed(arena+0x5001f,1);
        Check(Absent(reused),"the replacement retires when its new source's last byte is freed");
        WorldGeometry_OnResourceMemoryFreed(arena,bytes);
        Check(sVertexBuffers.empty()&&sIndexBuffers.empty()&&sVertexPages.empty()&&sIndexPages.empty(),
              "separate header and source notifications eventually clear every lookup entry");
    }
    for(size_t boundary:{size_t(4096),size_t(16384),size_t(65536)}) {
        WorldGeometry_ReleaseAll();
        const size_t edge=0x100000+boundary;
        const auto crossed=Add(arena,128,edge-8,16);
        WorldGeometry_OnResourceMemoryFreed(arena+edge+7,1);
        Check(Absent(crossed),"a source crossing an address-bucket boundary retires from its final byte");
        const auto starts=Add(arena,256,edge,16);
        WorldGeometry_OnResourceMemoryFreed(arena+edge-1,1);
        WorldGeometry_OnResourceMemoryFreed(arena+edge+16,1);
        Check(Present(starts),"adjacent free ranges cannot retire source bytes outside the interval");
        WorldGeometry_OnResourceMemoryFreed(arena+edge,1);
        Check(Absent(starts),"a source's first byte is included");
        const auto spans=Add(arena,384,edge-32,boundary*2+64);
        WorldGeometry_OnResourceMemoryFreed(arena+edge+boundary,1);
        Check(Absent(spans),"multi-bucket sources are found through an interior bucket");
    }
    WorldGeometry_ReleaseAll();
    {
        std::vector<Record> records;
        for(unsigned i=0;i<384;++i)
            records.push_back(Add(arena,0x20000+i*32,0x200000+i*257,1+i%257));
        std::mt19937 random(0x19768);std::shuffle(records.begin(),records.end(),random);
        bool exact=true;
        for(unsigned i=0;i<192;++i) {
            const bool header=i%2==0;
            const size_t begin=header?records[i].header:records[i].begin;
            const size_t count=header?size_t(1):records[i].end-records[i].begin;
            WorldGeometry_OnResourceMemoryFreed(arena+begin,count);
            for(auto& record:records) {
                const bool hit=(record.header>=begin&&record.header<begin+count)
                    ||(record.begin<begin+count&&record.end>begin);
                record.live &= !hit;
                exact &= record.live?Present(record):Absent(record);
            }
        }
        Check(exact,"dense mixed frees match the independent byte-interval oracle for both buffer kinds");
        WorldGeometry_OnResourceMemoryFreed(arena+0x20000,384*32);
        Check(sVertexBuffers.empty()&&sIndexBuffers.empty(),"header-only retirement removes every remaining owner");
        Check(!sVertexPages.empty()&&!sIndexPages.empty(),"separate source references survive until their own notification");
        WorldGeometry_OnResourceMemoryFreed(arena+0x200000,384*257);
        Check(sVertexPages.empty()&&sIndexPages.empty(),"source lookup cleanup still runs after all owners are gone");
    }
    WorldGeometry_ReleaseAll();VirtualFree(arena,0,MEM_RELEASE);
    std::printf("PCWorldGeometryRetirement: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
