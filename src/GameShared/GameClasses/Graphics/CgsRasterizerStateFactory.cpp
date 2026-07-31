#include "pc/gcm/renderengine/renderstates.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

class CgsRasterizerStateFactory
{
public:
    renderengine::RasterizerState* Construct(void* pResourceAllocator);
};

namespace
{
enum EFactoryRasterizerState
{
    E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK,
    E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT,
    E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE,
    E_FACTORY_RASTERIZER_STATE_COUNT
};

class ResourceAllocator
{
public:
    // NOT a vtable slot. Declaring this `virtual` put it at slot 0, which on the
    // rw::IResourceAllocator actually behind the reinterpret_cast is the VIRTUAL
    // DESTRUCTOR -- so the call allocated nothing and left the allocator's vptr
    // downgraded to the inert base for the rest of the run. Call the interface by
    // NAME instead; see CgsResourceAllocatorCreate.h.
    void* Create(
        void* pOut,
        ResourceAllocator* /*pAllocator*/,
        renderengine::ResourceDescriptor5* pDescriptor,
        int /*liFlags*/)
    {
        return CgsGraphics::ResourceAllocatorCreate(this, pOut, pDescriptor);
    }
};

renderengine::RasterizerState* gapRasterizerStates[E_FACTORY_RASTERIZER_STATE_COUNT];

renderengine::RasterizerState* CreateRasterizerState(
    ResourceAllocator* pAllocator,
    renderengine::RasterizerState::Parameters* pParameters)
{
    renderengine::ResourceDescriptor5 lDescriptor;
    renderengine::RasterizerState* lapStateHandles[5] = {};
    renderengine::RasterizerState* lapAllocatedHandles[5] = {};

    renderengine::RasterizerState::GetResourceDescriptor(&lDescriptor, pParameters);
    pAllocator->Create(lapAllocatedHandles, pAllocator, &lDescriptor, 0);
    for (int liHandle = 0; liHandle < 5; ++liHandle)
        lapStateHandles[liHandle] = lapAllocatedHandles[liHandle];

    return renderengine::RasterizerState::Initialize(lapStateHandles, pParameters);
}
}

namespace renderengine
{
ResourceDescriptor5* RasterizerState::GetResourceDescriptor(
    ResourceDescriptor5* lpDescriptor,
    const Parameters* /*lpParameters*/)
{
    for (ResourceDescriptor5::Entry& lEntry : lpDescriptor->maEntries)
    {
        lEntry.muSize = 0;
        lEntry.muAlignment = 1;
    }

    lpDescriptor->maEntries[0].muSize = 52;
    lpDescriptor->maEntries[0].muAlignment = 4;
    return lpDescriptor;
}

RasterizerState* RasterizerState::Initialize(RasterizerState** ppState, const Parameters* lpParameters)
{
    return (*ppState)->Initialize(lpParameters);
}
}

renderengine::RasterizerState* CgsRasterizerStateFactory::Construct(void* pResourceAllocator)
{
    renderengine::RasterizerState::Parameters lParameters = {};
    ResourceAllocator* lpAllocator = static_cast<ResourceAllocator*>(pResourceAllocator);

    lParameters.muMultisampleEnable = 0xffffffff;
    lParameters.muAntialiasedLineEnable = 0xffff;
    lParameters.mu8PaddingMode = 1;
    lParameters.mu8DepthClipEnable = 1;
    lParameters.mu8FrontCounterClockwise = 1;
    lParameters.mu8ScissorEnable = 1;

    gapRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK] = nullptr;
    gapRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT] = nullptr;
    gapRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE] = nullptr;

    lParameters.muFillMode = 0;
    lParameters.muCullMode = 2;
    lParameters.mu8ConservativeRaster = 0;
    gapRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK] =
        CreateRasterizerState(lpAllocator, &lParameters);
    CGS_ASSERT(gapRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK],
               "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeBack ]");

    lParameters.mu8ScissorEnable = 1;
    lParameters.muCullMode = (lParameters.muCullMode & 4) | 1;
    gapRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT] =
        CreateRasterizerState(lpAllocator, &lParameters);
    CGS_ASSERT(gapRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT],
               "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeFront ]");

    lParameters.mu8ScissorEnable = 1;
    lParameters.muCullMode &= 4u;
    gapRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE] =
        CreateRasterizerState(lpAllocator, &lParameters);
    renderengine::RasterizerState* lpResult =
        gapRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE];
    CGS_ASSERT(lpResult, "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeNone ]");

    return lpResult;
}
