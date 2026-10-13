// Exercise the production CPU stream owner with the production transactional RW allocator.
#include <Windows.h>
#include <d3d9.h>
#include "pc/gcm/renderengine/ShaderBindings.h"
#include "pc/gcm/renderengine/GeometryBindings.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"
#include "rw/rwcore_general_alloc.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <malloc.h>

static unsigned checks, failures, forbiddenCalls;
// Link-only seams for the uncalled GPU Dispatch members in the same production TU.
namespace renderengine {
IDirect3DDevice9* gDevice = nullptr;
s32 gDisplayWidth = 1, gDisplayHeight = 1;
u32 guPresentCount = 0, guDiagImBatches = 0, guDiagImFullBlack = 0;
bool gbDiagLastPresentBlack = false;
u32 FrameDumpEvery() { ++forbiddenCalls; return 0; }
HRESULT PCGetBackBuffer(IDirect3DSurface9**) { ++forbiddenCalls; return E_FAIL; }
}
namespace CgsGui { const renderengine::BlendState* gpGuiBlendStateAdditive = nullptr; }
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* text, const char*, int) { ++forbiddenCalls; std::printf("ASSERT %s\n", text); return 0; }
void* EndAssert() { return nullptr; }
} namespace Log { void WriteToLog(const char*) {} } }

static void Check(bool good, const char* label) {
    ++checks; failures += !good;
    std::printf("%s %s\n", good ? "PASS" : "FAIL", label);
}
struct MainArena {
    static constexpr unsigned guard = 256;
    unsigned char* allocation;
    unsigned char* data;
    unsigned size;
    explicit MainArena(unsigned count) : size(count) {
        allocation = static_cast<unsigned char*>(_aligned_malloc(size + 2 * guard, 128));
        if (!allocation) std::abort();
        std::memset(allocation, 0xA5, size + 2 * guard);
        data = allocation + guard;
    }
    ~MainArena() { _aligned_free(allocation); }
    bool Owns(const void* value, unsigned bytes) const {
        const uintptr_t pointer = reinterpret_cast<uintptr_t>(value);
        return pointer >= reinterpret_cast<uintptr_t>(data) &&
            pointer + bytes <= reinterpret_cast<uintptr_t>(data) + size;
    }
    bool Guards() const {
        for (unsigned i = 0; i < guard; ++i)
            if (allocation[i] != 0xA5 || data[size + i] != 0xA5) return false;
        return true;
    }
};
struct MainAllocator : rw::IResourceAllocator {
    rw::LinearResourceAllocator allocator;
    std::vector<rw::ResourceDescriptor> requests;
    unsigned frees = 0;
    explicit MainAllocator(MainArena& arena) {
        rw::Resource backing;
        backing.m_baseResources[0] = arena.data;
        rw::ResourceDescriptor capacity;
        capacity.m_baseResourceDescriptors[0].m_size = arena.size;
        capacity.m_baseResourceDescriptors[0].m_alignment = 128;
        allocator.AdoptMemory(backing, capacity);
    }
    rw::Resource DoAllocate(const rw::ResourceDescriptor& descriptor, const char* name) override {
        requests.push_back(descriptor);
        return allocator.DoAllocate(descriptor, name);
    }
    void DoFree(const rw::Resource& resource) override { ++frees; allocator.DoFree(resource); }
};
template<class V> static void PreparedStreams() {
    MainArena arena(8192);
    MainAllocator allocator(arena);
    CgsGraphics::ImRenderBuffer<V> buffer;
    buffer.Construct();
    const bool prepared = buffer.Prepare(256, 512, &allocator, false);
    Check(prepared, "main-only production Linear allocator prepares both CPU stream banks");
    bool descriptors = allocator.requests.size() == 4;
    for (unsigned i = 0; i < allocator.requests.size(); ++i) {
        const auto& request = allocator.requests[i];
        descriptors &= request.m_baseResourceDescriptors[0].m_size == (i % 2 ? 640u : 384u);
        descriptors &= request.m_baseResourceDescriptors[0].m_alignment == 128;
        for (unsigned lane = 1; lane < rw::KU_RESOURCE_LANE_COUNT; ++lane)
            descriptors &= request.m_baseResourceDescriptors[lane].m_size == 0;
    }
    Check(descriptors, "four requests retain CPU size and guard bands without unsupported secondary lanes");
    std::vector<std::pair<uintptr_t, unsigned>> ranges;
    bool owns = prepared, aligned = prepared;
    for (unsigned bank = 0; bank < 2; ++bank) {
        const auto& current = buffer.maBuffers[bank];
        owns &= arena.Owns(current.mpu8CommandBuffer, 384) && arena.Owns(current.mpu8VertexBuffer, 640);
        aligned &= current.mpu8CommandBuffer && current.mpu8VertexBuffer &&
            reinterpret_cast<uintptr_t>(current.mpu8CommandBuffer) % 128 == 0 &&
            reinterpret_cast<uintptr_t>(current.mpu8VertexBuffer) % 128 == 0;
        ranges.push_back({reinterpret_cast<uintptr_t>(current.mpu8CommandBuffer), 384});
        ranges.push_back({reinterpret_cast<uintptr_t>(current.mpu8VertexBuffer), 640});
    }
    std::sort(ranges.begin(), ranges.end());
    for (unsigned i = 1; i < ranges.size(); ++i) owns &= ranges[i-1].first + ranges[i-1].second <= ranges[i].first;
    Check(owns, "all four complete streams are disjoint and inside their real backing arena");
    Check(aligned, "all four stream bases retain absolute 128-byte alignment");
    bool primed = prepared && buffer.mpWriteBuffer == &buffer.maBuffers[0] &&
        buffer.mpDispatchBuffer == &buffer.maBuffers[1];
    if (prepared) for (const auto& bank : buffer.maBuffers)
        primed &= bank.muCommandBufferWritePos == 0 && bank.muVertexBufferWritePos == 0 &&
            bank.mpu8CommandBuffer[0] == 0 && bank.mpu8VertexBuffer[0] == 0;
    Check(primed, "both banks start empty and the producer/consumer pointers are primed");
    bool published = false;
    if (prepared) {
        buffer.BeginRendering();
        V* vertices = buffer.AllocVertices(3);
        if (vertices) std::memset(vertices, 0x3C, 3 * sizeof(V));
        buffer.EndRendering();
        buffer.Swap(); buffer.Clear();
        published = vertices && buffer.mpDispatchBuffer == &buffer.maBuffers[0] &&
            buffer.mpDispatchBuffer->muCommandBufferWritePos == 32 &&
            buffer.mpDispatchBuffer->muVertexBufferWritePos == 3 * sizeof(V) &&
            buffer.mpWriteBuffer->muCommandBufferWritePos == 0 && buffer.mpWriteBuffer->muVertexBufferWritePos == 0;
    }
    Check(published, "real command and vertex writes survive publication while the next bank clears");
    Check(arena.Guards(), "storage and producer writes preserve arena guards");
}
int main() {
    PreparedStreams<CgsGraphics::Basic2dColouredTexturedVertex>();
    PreparedStreams<CgsGraphics::Im3dVertex>();
    MainArena arena(1024); MainAllocator allocator(arena);
    rw::ResourceDescriptor missingLane;
    missingLane.m_baseResourceDescriptors[0].m_size = 128;
    missingLane.m_baseResourceDescriptors[1].m_size = 128;
    const rw::Resource rejected = allocator.allocator.DoAllocate(missingLane, nullptr);
    bool empty = true;
    for (void* value : rejected.m_baseResources) empty &= value == nullptr;
    Check(empty, "unsupported multi-lane requests still fail atomically in production RW");
    CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex> buffer;
    buffer.Construct();
    Check(!buffer.Prepare(256, 512, &allocator, true), "insufficient real backing fails preparation");
    Check(buffer.mpWriteBuffer == nullptr && buffer.mpDispatchBuffer == nullptr &&
        buffer.maBuffers[0].mpu8CommandBuffer == nullptr && buffer.maBuffers[1].mpu8VertexBuffer == nullptr &&
        arena.Guards(), "failed preparation publishes no command bank and preserves guards");
    Check(forbiddenCalls == 0, "CPU storage checks never enter GPU dispatch or assert seams");
    std::printf("PCImRenderBufferStorage: %u checks, %u failures\n", checks, failures);
    return failures != 0;
}
