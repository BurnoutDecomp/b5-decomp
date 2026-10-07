// Canonical debug allocation and fixed-ring construction, CPU dependency boundaries.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "GameSource/Replays/BrnReplayDebugComponent.h"
#include "rw/rwcore_structs.h"

static unsigned checks,failures,asserts,unexpectedCalls;
static void Check(bool value,const char* label)
{
    ++checks;
    if (!value) { ++failures;std::printf("FAIL %s\n",label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { ++asserts;return 0; }
void* EndAssert() { return nullptr; }
} }
// The allocator is an observed dependency, not the RenderWare backend under test.
namespace EA { namespace Allocator { ICoreAllocator::~ICoreAllocator() {} } }
namespace rw {
void* IResourceAllocator::Alloc(size_t,const char*,uint32_t,uint32_t,uint32_t) { ++unexpectedCalls;return nullptr; }
void* IResourceAllocator::Alloc(size_t,const char*,uint32_t) { ++unexpectedCalls;return nullptr; }
void IResourceAllocator::Free(void*,size_t) { ++unexpectedCalls; }
Resource IResourceAllocator::DoAllocate(const ResourceDescriptor&,const char*) { ++unexpectedCalls;return Resource(); }
void IResourceAllocator::DoFreeDisposable(Resource&) { ++unexpectedCalls; }
}
struct Allocator : rw::IResourceAllocator
{
    struct Block { unsigned char* allocation;void* data;u32 size; };
    std::vector<Block> blocks;
    std::vector<rw::ResourceDescriptor> descriptors;
    std::vector<const char*> names;
    rw::Resource DoAllocate(const rw::ResourceDescriptor& descriptor,const char* name) override
    {
        const u32 size=descriptor.m_baseResourceDescriptors[0].m_size;
        auto* allocation=static_cast<unsigned char*>(_aligned_malloc(size+32,16));
        std::memset(allocation,0xC3,size+32);
        std::memset(allocation+16,0x5A,size);
        blocks.push_back({allocation,allocation+16,size});
        descriptors.push_back(descriptor); names.push_back(name);
        rw::Resource result;result.m_baseResources[0]=allocation+16;return result;
    }
    ~Allocator() { for (const auto& block:blocks) _aligned_free(block.allocation); }
};
#include "replay_debug_construction.inc"

// These unopened display/menu entry points close the class vtable only. Every
// invocation fails the fixture; their engine behavior is not under test here.
namespace BrnReplays {
void DebugComponent::Update() { ++unexpectedCalls; }
void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender*) { ++unexpectedCalls; }
void DebugComponent::OnActivate() { ++unexpectedCalls; }
}
int main()
{
    Allocator allocator;
    BrnReplays::DebugComponent debug;
    debug.Construct(nullptr,&allocator); // owner is stored only; no fabricated module producer
    Check(debug.mpReplayModule==nullptr && debug.mpAllocator==&allocator,
          "constructor publishes the actual owner and allocator arguments");
    Check(debug.miMaxSerialisers==11 && debug.miCurrSerialisers==0 && !debug.mbShowHud
          && debug.miWriteSlotsUsed==0 && debug.miWriteBufferUsed==0,
          "constructor resets the original snapshot and HUD counters");
    Check(allocator.blocks.size()==4,"constructor performs all four actual allocations");
    for (unsigned index=0;index<4;++index)
    {
        const auto& descriptor=allocator.descriptors[index];
        Check(descriptor.m_baseResourceDescriptors[0].m_size==
              (index==0?sizeof(BrnReplays::DebugSerialiserInfo)*11:sizeof(BrnReplays::DebugGraph)),
              "descriptor requests actual native snapshot or graph storage");
        Check(descriptor.m_baseResourceDescriptors[0].m_alignment==16 && allocator.names[index]==nullptr,
              "original descriptor alignment and null allocation name are retained");
        bool lanes=true;
        for (unsigned lane=1;lane<rw::KU_RESOURCE_LANE_COUNT;++lane)
            lanes&=descriptor.m_baseResourceDescriptors[lane].m_size==0
                && descriptor.m_baseResourceDescriptors[lane].m_alignment==1;
        Check(lanes,"all four unused descriptor lanes retain the original identity");
    }
    Check(debug.mpSerialisers==allocator.blocks[0].data && debug.mpWriteSlotsUsedGraph==allocator.blocks[1].data
          && debug.mpWriteBufferUsedGraph==allocator.blocks[2].data && debug.mpReadGraph==allocator.blocks[3].data,
          "all four returned resources are retained in original order");
    for (auto* graph:{debug.mpWriteSlotsUsedGraph,debug.mpWriteBufferUsedGraph,debug.mpReadGraph})
    {
        Check(graph->mBuffer.GetLength()==0 && graph->mBuffer.GetMaxLength()==256
              && graph->mBuffer.mpData==graph->mBuffer.maData,
              "real fixed-ring metadata attaches its own native sample storage");
        Check(static_cast<unsigned char>(graph->macName[0])==0x5A
              && reinterpret_cast<const unsigned char*>(&graph->mfMin)[0]==0x5A
              && reinterpret_cast<const unsigned char*>(graph->mBuffer.maData)[0]==0x5A,
              "graph construction retains unpublished name, range and sample bytes");
        for (unsigned index=0;index<260;++index)
        {
            const f32 value=static_cast<f32>(index);graph->mBuffer.Push(&value);
        }
        Check(graph->mBuffer.GetLength()==256 && graph->mBuffer[0]==4.0f && graph->mBuffer[255]==259.0f,
              "canonical graph buffer wraps and retains its live sample window");
        unsigned char samples[sizeof(graph->mBuffer.maData)];
        std::memcpy(samples,graph->mBuffer.maData,sizeof(samples));
        debug.ClearGraph(graph);
        Check(graph->macName[0]==0 && static_cast<unsigned char>(graph->macName[1])==0x5A
              && graph->mfMin==0 && graph->mfMax==0 && graph->mBuffer.GetLength()==0,
              "original graph clear resets name prefix, min/max and cursors");
        Check(std::memcmp(samples,graph->mBuffer.maData,sizeof(samples))==0
              && graph->mBuffer.GetMaxLength()==256 && graph->mBuffer.mpData==graph->mBuffer.maData,
              "graph clear retains every sample, native pointer and capacity");
    }
    // The true PreUpdateRecord body is not substituted here: this checks the
    // constructor's full eleven-record destination capacity only.
    for (unsigned index=0;index<11;++index)
        std::memset(&debug.mpSerialisers[index],0x37,sizeof(BrnReplays::DebugSerialiserInfo));
    for (const auto& block:allocator.blocks)
    {
        bool guards=true;
        for (unsigned index=0;index<16;++index)
            guards&=block.allocation[index]==0xC3 && block.allocation[16+block.size+index]==0xC3;
        Check(guards,"all actual allocations retain their surrounding guard bytes");
    }
    Check(asserts==0 && unexpectedCalls==0,"constructor and graph path invoke no unrelated engine/UI bodies");
    std::printf("ReplayDebugConstruction: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
