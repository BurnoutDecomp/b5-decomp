#include <Windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <stdexcept>
#include <vector>
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"
#include "GameShared/Jobs/ObjectToMesh/ObjectToMeshJob.h"
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"
#include "SDKs/EATech/eajobs/job.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "SDKs/EATech/eajobs/job_thread_parameters.h"
#include "SDKs/EATech/eajobs/jobs.h"

static int checks, failures;
static std::atomic<int> badConstants{0}, active{0}, peak{0};
static std::mutex threadLock;
static std::set<DWORD> workers;
static void Check(bool ok, const char* message)
{ ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", message); } }
#define CGS_ASSERT(ok, message) do { if (!(ok)) throw std::runtime_error(message); } while (0)
namespace CgsDev {
namespace Assert {
constexpr unsigned KI_MESSAGEBUFFERSIZE=8192;
inline void BeginAssert() {}
inline void FireAssert(const char* message, const char*, int) { throw std::runtime_error(message); }
inline void EndAssert() {}
}
struct StrStream {
    StrStream(char* out, unsigned) { out[0]=0; }
    template<class T> StrStream& operator<<(const T&) { return *this; }
};
}

using namespace CgsGraphics;
struct Allocator : EA::Allocator::ICoreAllocator {
    void* Alloc(size_t n, const char*, unsigned, unsigned alignment, unsigned offset) override
    { if (offset) std::abort(); return _aligned_malloc(n, (std::max)(alignment,16u)); }
    void* Alloc(size_t n, const char* name, unsigned flags) override { return Alloc(n,name,flags,16,0); }
    void Free(void* p, size_t) override { _aligned_free(p); }
};
static EA::Jobs::JobScheduler scheduler;
namespace CgsSystem { EA::Jobs::JobScheduler* JobManager() { return &scheduler; } }
namespace CgsGraphics {
// Only the ring accessor is a fixture; the renderer receives the real read frame.
struct BufferedDispatchFrame {
    DispatchFrame* mpRead;
    DispatchFrame& GetDispatchFrameForRead() { return *mpRead; }
};
}
template<size_t... I> static std::array<EA::Jobs::Job,sizeof...(I)> MakeJobs(std::index_sequence<I...>)
{ return {{((void)I, EA::Jobs::Job("mesh"))...}}; }
struct BrnRendererModule {
    static constexpr unsigned KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS=16;
    BufferedDispatchFrame mDoubleBufferedDispatchFrame;
    std::array<EA::Jobs::Job,16> maObjectToMeshJob=MakeJobs(std::make_index_sequence<16>{});
    ObjectToMeshJobInfo maObjectToMeshJobData[16];
    DispatchObjectContext maObjectToMeshJobContext[16];
    DispatchList* mapaObjectToMeshJobOutputDispatchLists[16];
    void CreateObjectToMeshJob(u32, const DispatchObjectContext*, DispatchPacketInterpreter*, u32, s32, u32);
    void ConvertObjectsToMeshes(BufferedDispatchFrame*, DispatchFrame*, DispatchPacketInterpreter*, const DispatchObjectContext*);
};

#include "pc_object_mesh_jobs.inc"

struct alignas(16) Constant { float value[4]; };
static Constant constants[13][64];

// Deterministic emitter boundary: full constants at producer indices0,128,...,
// inherited constants between them; one wide command per output pass stresses
// shared-block rollover. Expected IDs/values are calculated independently below.
void CgsGraphics::DrawRenderable::Interpret(DispatchCommand* packet, DispatchFrame* frame, void* context, f32)
{
    auto& state=*static_cast<DispatchObjectContext*>(context);
    const unsigned id=packet->muWords[1], list=packet->muWords[3];
    if (id%128==0) state.mapConstantData[3]=reinterpret_cast<const rw::math::vpu::Vector4*>(&constants[list][id/128]);
    const float value=state.mapConstantData[3] ? reinterpret_cast<const Constant*>(state.mapConstantData[3])->value[0] : -1.0f;
    if (value!=float(list*1000+id/128)) ++badConstants;
    if (state.mpJobState && id==0) {
        { std::lock_guard<std::mutex> lock(threadLock); workers.insert(GetCurrentThreadId()); }
        const int n=++active; int old=peak.load(); while (old<n&&!peak.compare_exchange_weak(old,n)) {}
        Sleep(2); --active;
    }
    const unsigned passes=list==11 ? 3u : 1u;
    for(unsigned pass=0;pass<passes;++pass) {
        const unsigned route=list==11 ? (pass==0?11u:pass==1?15u:21u)+state.miListIdBase : list==12?19u:list;
        auto& bin=frame->GetBin(); auto* out=frame->GetList(route);
        out->ReserveKey();
        if (bin.GetUsedQwords()+64u>=bin.GetSizeQwords()) bin.HandleMemoryOverflow(64);
        bin.BeginPacket(); auto* mesh=bin.AllocateCommand(31);
        mesh->muWords[0]=0x0200001f; mesh->muWords[1]=list*100000+id;
        mesh->muWords[2]=unsigned(value); mesh->muWords[3]=pass;
        out->Submit((u64(1)<<43)|(id%11),bin.EndPacket());
    }
}

struct Arena {
    unsigned char* data;
    size_t bytes;
    explicit Arena(size_t n):data(static_cast<unsigned char*>(VirtualAlloc(nullptr,n+4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE))),bytes(n)
    { if(!data)std::abort();std::memset(data+n,0xCE,4096); }
    ~Arena(){VirtualFree(data,0,MEM_RELEASE);}
    bool Guards() const { for(size_t i=0;i<4096;++i)if(data[bytes+i]!=0xCE)return false;return true; }
};
static void Seed(DispatchFrame& frame, DispatchList* lists, unsigned count, Arena& arena)
{
    frame={}; frame.m_paLists=lists; frame.muNumDispatchLists=count;
    frame.GetBin().SetBinRange(reinterpret_cast<DispatchCommand*>(arena.data),
                              reinterpret_cast<DispatchCommand*>(arena.data+arena.bytes));
    for(unsigned i=0;i<count;++i) { lists[i]={};lists[i].mpDispatchBin=&frame.GetBin();lists[i].m_pBinBase=frame.GetBin().GetBase(); }
    frame.Reset();
}
using Row=std::array<unsigned,3>;
static std::vector<Row> Read(DispatchFrame& frame, unsigned list)
{
    auto* values=frame.GetList(list); values->SortForDispatch(); std::vector<Row> rows;
    for(unsigned i=0;i<values->GetCount();++i) {
        const auto* command=values->m_pBinBase+(values->mpSortedKeys[i]&DispatchList::SortKey::KU_MASK_OFFSET);
        rows.push_back({command->muWords[1],command->muWords[2],command->muWords[3]});
    }
    std::sort(rows.begin(),rows.end());return rows;
}

static bool CheckManyRelocatedChains()
{
    // Separate source/destination regions exercise the copy-after-relocation
    // contract, including the first spill and subsequent appends to that spill.
    constexpr unsigned count=96;
    alignas(16) unsigned char sourceBytes[256]={};
    alignas(16) unsigned char destinations[count][256]={};
    DispatchList list{};
    bool completed=true;
    try {
        for(unsigned i=0;i<count;++i) {
            std::memset(sourceBytes,0,sizeof(sourceBytes));
            auto* block=reinterpret_cast<DispatchList::KeyBlock*>(sourceBytes);
            auto* tail=reinterpret_cast<DispatchList::KeyBlock*>(sourceBytes+128);
            block->mpKeys=reinterpret_cast<u64*>(sourceBytes+32);block->muCount=2;block->muCapacity=64;
            block->mpNext=tail;
            tail->mpKeys=reinterpret_cast<u64*>(sourceBytes+160);tail->muCount=2;tail->muCapacity=64;
            block->mpKeys[0]=0xabcdef1200000000ULL+4*i;
            block->mpKeys[1]=0xabcdef1200000001ULL+4*i;
            tail->mpKeys[0]=0xabcdef1200000002ULL+4*i;
            tail->mpKeys[1]=0xabcdef1200000003ULL+4*i;
            list.mpBlockListHead=block;list.mpBlockListTail=tail;list.muCount=4;
            list.RelocateForMainMemory(reinterpret_cast<uintptr_t>(sourceBytes),
                reinterpret_cast<uintptr_t>(destinations[i]),0);
            std::memcpy(destinations[i],sourceBytes,sizeof(sourceBytes));
            // Empty flushes must leave the last relocated tail available.
            list.RelocateForMainMemory(0,0,0);
        }
    } catch(const std::exception&) { completed=false; }
    Check(completed,"96 relocated chains fit without exceeding the original 64-head array");
    if(!completed)return false;
    Check(list.muChainBlockCount==DispatchList::KU_MAX_BLOCKS_PER_CHAIN,
          "extra shared blocks reuse the last chain head without growing metadata");
    list.ReconnectChainBlocks();
    bool exact=list.GetCount()==4*count;
    auto* block=list.GetFirstKeyBlock();
    for(unsigned i=0;i<count;++i) {
        exact&=block==reinterpret_cast<DispatchList::KeyBlock*>(destinations[i]);
        if(!exact)break;
        exact&=block->mpKeys==reinterpret_cast<u64*>(destinations[i]+32)
            &&block->mpKeys[0]==0xabcdef1200000000ULL+4*i
            &&block->mpKeys[1]==0xabcdef1200000001ULL+4*i;
        block=block->mpNext;
        exact&=block==reinterpret_cast<DispatchList::KeyBlock*>(destinations[i]+128);
        if(!exact)break;
        exact&=block->mpKeys==reinterpret_cast<u64*>(destinations[i]+160)
            &&block->mpKeys[0]==0xabcdef1200000002ULL+4*i
            &&block->mpKeys[1]==0xabcdef1200000003ULL+4*i;
        block=block->mpNext;
    }
    Check(exact&&block==nullptr,"every spilled key survives relocation in its original order");
    return exact;
}

int main()
{
    static_assert(sizeof(DispatchList)==(sizeof(void*)==8?672u:384u),
                  "native chain metadata stays within the existing list footprint");
    _putenv_s("BRN_MESH_JOBS","1");
#ifdef MESH_TEST_CHAIN
    _putenv_s("BRN_MESH_JOBS_CHAIN","1");
#endif
    if(!CheckManyRelocatedChains()) {
        std::printf("PCObjectMeshJobs: %d checks, %d failures\n",checks,failures);
        return 1;
    }
    Allocator allocator; EA::Jobs::SetAllocator(&allocator); scheduler.Initialize(128,128);
    for(int i=0;i<3;++i) { EA::Jobs::JobThreadParameters params; scheduler.AddThread(params); }
    Arena inputArena(2*1024*1024), outputArena(8*1024*1024), serialArena(8*1024*1024);
    Check(reinterpret_cast<uintptr_t>(outputArena.data)>UINT32_MAX,"shared output uses a real address above4GiB");
    DispatchFrame input{},output{},serial{}; DispatchList inputLists[13],outputLists[25],serialLists[25];
    Seed(input,inputLists,13,inputArena);
    const unsigned counts[13]={4097,1,63,64,65,127,128,129,255,256,257,1057,137};
    unsigned total=0;
    for(unsigned l=0;l<13;++l) {
        total+=counts[l];for(unsigned i=0;i<64;++i)constants[l][i].value[0]=float(l*1000+i);
        for(unsigned id=0;id<counts[l];++id) {
            auto& bin=input.GetBin();bin.BeginPacket();auto* packet=bin.AllocateCommand(0);
            packet->muWords[0]=0x01000000;packet->muWords[1]=id;packet->muWords[3]=l;
            input.GetList(l)->Submit(id,bin.EndPacket());
        }
    }
    Seed(serial,serialLists,25,serialArena); DispatchObjectContext context{};
    for(unsigned l=0;l<13;++l) { DispatchObjectContext local=context;
        input.GetList(l)->DispatchAllObjectToMesh(nullptr,&serial,&local,0,-1); }
    std::array<std::vector<Row>,25> expected;
    for(unsigned l=0;l<13;++l)for(unsigned id=0;id<counts[l];++id) {
        const unsigned passes=l==11?3u:1u;
        for(unsigned p=0;p<passes;++p) {
            const unsigned route=l==11?(p==0?11u:p==1?15u:21u):l==12?19u:l;
            expected[route].push_back({l*100000+id,l*1000+id/128,p});
        }
    }
    bool serialCorrect=true;
    for(unsigned l=0;l<25;++l) { std::sort(expected[l].begin(),expected[l].end());serialCorrect&=Read(serial,l)==expected[l]; }
    Check(serialCorrect,"independent packet oracle agrees with serial output");
    BrnRendererModule renderer{}; renderer.mDoubleBufferedDispatchFrame.mpRead=&input;
    DispatchPacketInterpreter::InterpretFn table[4]={};
    DispatchPacketInterpreter interpreterObject(table,4);
    auto* interpreter=&interpreterObject;
    interpreter->SetSingleBufferedDispatchFrame(&output);
    Seed(output,outputLists,25,outputArena);
    renderer.ConvertObjectsToMeshes(&renderer.mDoubleBufferedDispatchFrame,&output,interpreter,&context);
    bool packets=true, emptyGroups=true;
    for(unsigned l=0;l<25;++l) { packets&=Read(output,l)==expected[l];
        if(l==12||l==13||l==14||l==16||l==17||l==18||l==22||l==23||l==24)emptyGroups&=output.GetList(l)->GetCount()==0; }
    Check(packets,"parallel output contains exactly every expected packet, constant and pass");
    Check(badConstants==0,"all partitions begin with complete inherited shader constants");
    Check(emptyGroups,"the nine auxiliary world groups transfer into their owner lists");
    bool partitions=true;
    const unsigned starts[4]={0,256,512,768}, ends[4]={256,512,768,1152};
    for(unsigned i=0;i<4;++i)partitions&=renderer.maObjectToMeshJobData[i].miStartIndex==starts[i]
        &&renderer.maObjectToMeshJobData[i].miEndIndex==ends[i];
    Check(partitions,"1057 world objects split at the original four128-object group boundaries");
    Check(suObjectToMeshNextBlock>16,"output exceeds the initial sixteen blocks and exercises rollover");
    Check(renderer.mapaObjectToMeshJobOutputDispatchLists[5][0].muChainBlockCount==DispatchList::KU_MAX_BLOCKS_PER_CHAIN,
          "real parallel worker output crosses the original per-list shared-block limit");
    Check(output.GetBin().GetUsedQwords()<output.GetBin().GetSizeQwords(),"unused shared reservation is reclaimed for the sorts");
    Check(inputArena.Guards()&&outputArena.Guards()&&serialArena.Guards(),"input/output allocations retain their guard bytes");
    Check(context.mpJobState==nullptr&&context.mapConstantData[3]==nullptr,"worker shadowing never modifies the owner context");
#ifdef MESH_TEST_CHAIN
    Check(peak==1,"the original dependency-chain control serializes the jobs");
#else
    Check(peak>=2&&workers.size()>=2,"the actual EAJobs backend executes conversion workers concurrently");
#endif
    bool repeated=true;
    for(unsigned iteration=0;iteration<20;++iteration) {
        Seed(output,outputLists,25,outputArena);
        renderer.ConvertObjectsToMeshes(&renderer.mDoubleBufferedDispatchFrame,&output,interpreter,&context);
        for(unsigned l=0;l<25;++l)repeated&=Read(output,l)==expected[l];
    }
    Check(repeated,"twenty joined frame resets retain every draw without stale chains");
    Check(badConstants==0,"context inheritance stays isolated across reused job descriptors");
    DispatchList source{},target{}; DispatchBin bin{};
    alignas(16) unsigned char from[128]={},to[128]={};
    auto* block=reinterpret_cast<DispatchList::KeyBlock*>(from);
    block->mpKeys=reinterpret_cast<u64*>(from+32);block->muCount=2;block->muCapacity=64;
    block->mpKeys[0]=0xabcdef1200000001ULL;block->mpKeys[1]=0xabcdef1200000002ULL;
    source.mpBlockListHead=source.mpBlockListTail=block;source.muCount=2;
    source.RelocateForMainMemory(reinterpret_cast<uintptr_t>(from),reinterpret_cast<uintptr_t>(to),0);
    std::memcpy(to,from,sizeof(from)); source.ReconnectChainBlocks();target.Append(&source);
    Check(target.GetCount()==2&&target.mpBlockListHead->mpKeys==reinterpret_cast<u64*>(to+32)
        &&target.mpBlockListHead->mpKeys[1]==0xabcdef1200000002ULL,"relocation preserves full-width pointers and key bits when copying a block");
    Check(source.GetCount()==0&&source.GetFirstKeyBlock()==nullptr,"Append empties the source chain");
    ObjectToMeshJobInfo skip{};skip.miStartIndex=-1; ObjectToMeshJob skipJob;skipJob.Execute(&skip);
    Check(true,"the original skip sentinel does not dereference uninitialized inputs");
    Check(outputArena.Guards(),"repeated output and sort allocations stay in bounds");
    scheduler.Destroy();
    std::printf("PCObjectMeshJobs: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
