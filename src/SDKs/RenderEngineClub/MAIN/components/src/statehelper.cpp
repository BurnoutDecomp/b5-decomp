#include "SDKs/RenderEngineClub/MAIN/components/src/statehelper.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/pixelbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/depthstencilstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/rasterizerstate.h"
#include "rw/rwcore_structs.h"
#include <cstring>

namespace renderengine
{
namespace StateHelper
{
BlendState* m_blendStates[4];
DepthStencilState* m_defaultDepthStencilState;
RasterizerState* m_defaultRasterizerState;
// Initial image82F87B64: zero rectangle/minZ, maxZ1, flags0.
D3DVIEWPORTF9 m_defaultViewportParams = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0 };
D3DRECT m_defaultScissorRectParams;

namespace
{
template<class State>
State* AllocateState(const typename State::Parameters& parameters)
{
    const rw::ResourceDescriptor descriptor = State::GetResourceDescriptor(parameters);
    rw::IResourceAllocator* const allocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();
    const rw::Resource resource = allocator->DoAllocate(descriptor, 0);
    return State::Initialize(resource, parameters);
}
}

// ARTIST82B64108: original publication order is blend0,1,3,2, depth, raster.
int Initialize()
{
    for (u32 i = 0; i != 4; ++i) m_blendStates[i] = 0;
    BlendState::Parameters opaque;
    m_blendStates[0] = AllocateState<BlendState>(opaque);
    BlendState::Parameters additive;
    additive.mbHasCustomBlendFactors = 1;
    additive.maBlendFactor[0] = 0x07060101u;
    m_blendStates[1] = AllocateState<BlendState>(additive);
    BlendState::Parameters colour;
    colour.mbHasCustomBlendFactors = 1;
    colour.maBlendFactor[0] = 0x07060109u;
    m_blendStates[3] = AllocateState<BlendState>(colour);
    BlendState::Parameters standard;
    standard.mbHasCustomBlendFactors = 1;
    m_blendStates[2] = AllocateState<BlendState>(standard);
    DepthStencilState::Parameters depth;
    m_defaultDepthStencilState = AllocateState<DepthStencilState>(depth);
    RasterizerState::Parameters raster;
    m_defaultRasterizerState = AllocateState<RasterizerState>(raster);
    if (Device::GetState() != Device::STATE_START) return 0;
    ResizeDefaultScissorRectAndViewportParameters();
    return 1;
}

// ARTIST82B63D30: both dimensions come from the original SDK surface header.
int ResizeDefaultScissorRectAndViewportParameters()
{
    const u32 width = gOriginalColorBuffer.Width + 1;
    const u32 height = gOriginalColorBuffer.Height + 1;
    const D3DVIEWPORTF9 viewport =
        { 0.0f, 0.0f, static_cast<f32>(width), static_cast<f32>(height), 0.0f, 1.0f, 0 };
    void* const result = std::memcpy(&m_defaultViewportParams, &viewport, sizeof(viewport));
    m_defaultScissorRectParams.x1 = 0;
    m_defaultScissorRectParams.y1 = 0;
    m_defaultScissorRectParams.x2 = width;
    m_defaultScissorRectParams.y2 = height;
    return reinterpret_cast<int>(result);
}
}
}
