#include <cstdio>
#include <cstring>
#include "rw/rwcore_structs.h"
#include "pc/gcm/renderengine/renderstates.h"
#include "pc/gcm/renderengine/texture.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"

static unsigned checks, failures, assertions;
static void Check(bool condition, const char* name)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL %s\n", name); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} }

struct TestAllocator : rw::IResourceAllocator
{
    struct alignas(16) Block {
        u8 before[16];
        alignas(renderengine::TextureState) u8 state[sizeof(renderengine::TextureState)];
        u8 after[16];
        alignas(renderengine::TextureState) u8 decoy[sizeof(renderengine::TextureState)];
    } blocks[8];
    unsigned allocations = 0, frees = 0;
    rw::Resource DoAllocate(const rw::ResourceDescriptor& descriptor, const char* name) override
    {
        Check(name == nullptr, "original allocation name is null");
        Check(descriptor.m_baseResourceDescriptors[0].m_size == sizeof(renderengine::TextureState),
              "canonical descriptor sizes actual native state");
        Check(descriptor.m_baseResourceDescriptors[0].m_alignment == alignof(renderengine::TextureState),
              "canonical descriptor aligns actual native state");
        for (u32 lane = 1; lane < rw::KU_RESOURCE_LANE_COUNT; ++lane)
            Check(descriptor.m_baseResourceDescriptors[lane].m_size == 0
                  && descriptor.m_baseResourceDescriptors[lane].m_alignment == 1,
                  "every unused descriptor lane is initialized");
        Block& block = blocks[allocations++];
        std::memset(&block, 0xA5, sizeof(block));
        // Wrong raw-lane input reads this valid decoy pointer as Resource lane 0.
        // The negative control therefore fails assertions without causing an AV.
        void* decoy = block.decoy;
        std::memcpy(block.state, &decoy, sizeof(decoy));
        rw::Resource resource;
        resource.m_baseResources[0] = block.state;
        for (u32 lane = 1; lane < rw::KU_RESOURCE_LANE_COUNT; ++lane)
            resource.m_baseResources[lane] = reinterpret_cast<void*>(UINT64_C(0x100000000) + lane);
        return resource;
    }
    void DoFree(const rw::Resource&) override { ++frees; }
    void CheckStorage(unsigned index, const renderengine::TextureState* state)
    {
        const Block& block = blocks[index];
        Check(state == reinterpret_cast<const renderengine::TextureState*>(block.state),
              "initializer returns allocator lane 0, not a raw-lane decoy or heap object");
        bool guards = true, decoy = true;
        for (u8 value : block.before) guards &= value == 0xA5;
        for (u8 value : block.after) guards &= value == 0xA5;
        for (u8 value : block.decoy) decoy &= value == 0xA5;
        Check(guards, "placement initialization stays within the state allocation");
        Check(decoy, "raw-lane decoy remains untouched");
    }
};
static TestAllocator allocator;
namespace rw {
IResourceAllocator* ResourceAllocatorRegistry::GetDefaultAllocator() { return &allocator; }
}
#include "texture_state_resource_callers.inc"

static void CheckSampler(const renderengine::TextureState* state, u32 address, u32 mip)
{
    u32 words[8]; std::memcpy(words, state->mauSamplerState, sizeof(words));
    Check(words[0] == address && words[1] == address, "caller address modes preserved");
    Check(words[3] == 1 && words[4] == 1 && words[5] == mip,
          "caller filter values preserved");
}
int main()
{
    static_assert(sizeof(void*) == 8 && sizeof(rw::Resource) == 5 * sizeof(void*),
                  "native Resource return carries five full pointer lanes");
    renderengine::Texture raster = {};
    CgsGraphics::ImRendererBase immediate;
    auto opaque = immediate.ConstructDefaultTextureState(&allocator, &raster);
    const auto* state = reinterpret_cast<const renderengine::TextureState*>(opaque);
    allocator.CheckStorage(0, state);
    Check(state->mpRaster == &raster, "immediate caller preserves the full native raster pointer");
    CheckSampler(state, 2, 0);

    CgsGui::AptRenderHandler::TextureStateCache clamp, wrap;
    clamp.Init(); wrap.Init();
    const u32 key = 0xE1231001u;
    auto* clampState = CgsGui::ResolveTextureState(clamp, key, true);
    allocator.CheckStorage(1, clampState);
    Check(clampState->mpRaster == reinterpret_cast<renderengine::Texture*>(uintptr_t(key)),
          "unchanged Apt key contract reaches the state");
    CheckSampler(clampState, 2, 2);
    const unsigned beforeHit = allocator.allocations;
    Check(CgsGui::ResolveTextureState(clamp, key, true) == clampState,
          "cache hit returns the same resource-backed state");
    Check(allocator.allocations == beforeHit, "cache hit does not allocate another resource");

    auto* wrapState = CgsGui::ResolveTextureState(wrap, key, false);
    allocator.CheckStorage(2, wrapState);
    CheckSampler(wrapState, 0, 2);
    Check(wrapState != clampState, "clamp and wrap retain separate real state allocations");
    auto* second = CgsGui::ResolveTextureState(clamp, key + 25, true);
    allocator.CheckStorage(3, second);
    Check(clamp.Find(key) == clampState && clamp.Find(key + 25) == second,
          "same-bin memoization preserves both resource-backed owners");
    Check(allocator.frees == 0, "builders leave allocator-owned backing alive for consumers");
    Check(assertions == 0, "real cache operations raise no assertions");
    std::printf("TextureStateResourceCallers: %u checks, %u failures\n", checks, failures);
    return failures != 0;
}
