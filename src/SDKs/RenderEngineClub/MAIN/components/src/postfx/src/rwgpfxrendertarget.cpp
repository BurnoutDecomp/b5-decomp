#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/pixelbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/rendertargetstate.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <new>

namespace rw { namespace graphics { namespace postfx {

// ARTIST 827E73B0 inlines the five Target parameter constructors.
Target::Parameters::Parameters()
    : mpAllocator(0), mu32Width(0), mu32Height(0), mu32Pitch(0),
      mBufferFormat(~0u), mTextureFormat(~0u), mTextureType(1),
      mu32MultiSampleFormat(0), mFilterMode(1), mbUseStencil(false),
      mn32BaseEDRAM(0), mn32HierarchicalZAddress(0), mn32CompressionBase(-1),
      mbSystemMemory(false), mpBufferAddress(0), mn8TileIndex(-1),
      mn8ZCullIndex(-1), mn32ZCullAddress(-1), mu8NumSections(1)
{}

RenderTarget::Parameters::Parameters()
    : mpAllocator(0), mu32Width(0), mu32Height(0), mu32NumMipMaps(1),
      mColourMode(1), mDepthStencilMode(1), mbUseDepthStencilAsTexture(false),
      mu8NumSections(1), mpProvidedDepthStencilTarget(0)
{
    for (u32 i = 0; i < 4; ++i) mapProvidedColourTarget[i] = 0;
}

// ARTIST 82403420. Both descriptors and allocations have five native lanes.
static renderengine::Texture* AllocateAndInitializeTexture(
    const Target::Parameters& target, const renderengine::Texture::Parameters& parameters)
{
    const rw::ResourceDescriptor descriptor = renderengine::Texture::GetResourceDescriptor(parameters);
    const rw::Resource resource = target.mpAllocator->DoAllocate(descriptor, 0);
    return renderengine::Texture::Initialize(resource, parameters);
}

// ARTIST 824034D0.
void Target::CreateColor(const Parameters& parameters)
{
    mu8NumSections = parameters.mu8NumSections;
    renderengine::Texture::Parameters texture;
    texture.meType = static_cast<renderengine::Texture::Type>(parameters.mTextureType);
    texture.muFlags = 0;
    texture.muWidth = parameters.mu32Width;
    texture.muHeight = parameters.mu32Height;
    texture.muDepth = 1;
    texture.muNumLevels = 1;
    texture.meFormat = static_cast<renderengine::PixelFormat>(parameters.mTextureFormat);
    mpTexture = AllocateAndInitializeTexture(parameters, texture);
    if (mpTexture)
    {
        renderengine::TextureState::Parameters state;
        state.muAddressU = 2;
        state.muAddressV = 2;
        state.muMagFilter = parameters.mFilterMode;
        state.muMinFilter = parameters.mFilterMode;
        state.muMipFilter = 2;
        state.mpTexture = mpTexture;
        const rw::ResourceDescriptor descriptor = renderengine::TextureState::GetResourceDescriptor(state);
        const rw::Resource resource = parameters.mpAllocator->DoAllocate(descriptor, 0);
        mpTextureState = renderengine::TextureState::Initialize(resource, state);
    }
    renderengine::PixelBuffer::Parameters buffer;
    buffer.type = renderengine::PixelBuffer::TYPE_RENDERTARGET;
    buffer.flags = 1;
    buffer.width = parameters.mu32Width;
    buffer.height = parameters.mu32Height;
    buffer.format = static_cast<renderengine::PixelFormat>(parameters.mBufferFormat);
    buffer.multiSampleType = parameters.mu32MultiSampleFormat;
    buffer.colorExpBias = 0;
    const rw::ResourceDescriptor descriptor = renderengine::PixelBuffer::GetResourceDescriptor(buffer);
    const rw::Resource resource = parameters.mpAllocator->DoAllocate(descriptor, 0);
    mpPixelBuffer = renderengine::PixelBuffer::Initialize(resource, buffer);
    mpPixelBuffer->Xbox2SetBaseEDRAM(static_cast<s16>(parameters.mn32BaseEDRAM));
}

// ARTIST 82403688.
void Target::CreateDepth(const Parameters& parameters)
{
    renderengine::Texture::Parameters texture;
    texture.meType = renderengine::Texture::TYPE_2D;
    texture.muFlags = 0;
    texture.muWidth = parameters.mu32Width;
    texture.muHeight = parameters.mu8NumSections * parameters.mu32Height;
    texture.muDepth = 1;
    texture.muNumLevels = 1;
    texture.meFormat = static_cast<renderengine::PixelFormat>(parameters.mTextureFormat);
    mpTexture = AllocateAndInitializeTexture(parameters, texture);
    mpHiZTexture = 0;
    if (parameters.mbUseStencil)
    {
        texture.muFlags = 1;
        texture.meFormat = static_cast<renderengine::PixelFormat>(0x18280186);
        mpHiZTexture = AllocateAndInitializeTexture(parameters, texture);
        mpHiZTexture->Format.BaseAddress = mpTexture->Format.BaseAddress;
        mpHiZTexture->Xbox2CheckPhysicalMemoryFlags();
    }
    mu8NumSections = parameters.mu8NumSections;
    renderengine::PixelBuffer::Parameters buffer;
    buffer.type = renderengine::PixelBuffer::TYPE_DEPTHSTENCIL;
    buffer.flags = 1;
    buffer.width = parameters.mu32Width;
    buffer.height = parameters.mu32Height;
    buffer.format = static_cast<renderengine::PixelFormat>(parameters.mBufferFormat);
    buffer.multiSampleType = parameters.mu32MultiSampleFormat;
    buffer.colorExpBias = 0;
    const rw::ResourceDescriptor descriptor = renderengine::PixelBuffer::GetResourceDescriptor(buffer);
    const rw::Resource resource = parameters.mpAllocator->DoAllocate(descriptor, 0);
    mpPixelBuffer = renderengine::PixelBuffer::Initialize(resource, buffer);
    mpPixelBuffer->Xbox2SetBaseHierarchicalZ(parameters.mn32HierarchicalZAddress);
    mpPixelBuffer->Xbox2SetBaseEDRAM(static_cast<s16>(parameters.mn32BaseEDRAM));
}

// ARTIST 823F9118.
void Target::Resolve()
{
    if (mpTexture)
        mpPixelBuffer->Xbox2ResolveTo(mpTexture, 0, 0, 0, 0, 0, 0, 1.0f, 0);
}

// ARTIST 823F9170: a single section resolves to the destination face;
// multiple sections use a vertical atlas and the real XDK surface dimensions.
void Target::Resolve(u32 section)
{
    if (!mpTexture) return;
    if (mu8NumSections <= 1)
    {
        mpPixelBuffer->Xbox2ResolveTo(mpTexture, 0, 0, 0, 0, section, 0, 1.0f, 0);
        return;
    }
    D3DRECT source;
    source.x1 = 0; source.y1 = 0;
    source.x2 = mpPixelBuffer->Width + 1;
    source.y2 = mpPixelBuffer->Height + 1;
    D3DPOINT destination;
    destination.x = 0;
    destination.y = (mpPixelBuffer->Height + 1) * section;
    D3DDevice_Resolve(gpD3DDevice, 4, &source, mpTexture, &destination, 0, 0, 0, 1.0f, 0, 0);
}

// ARTIST 8240A548/5D8/5E0 and 8240A5E4 inline these accessors.
renderengine::Texture* RenderTarget::GetTexture(u32 index)
{
    return maColourTargets[index].mpTexture;
}

renderengine::TextureState* RenderTarget::GetDepthTextureState()
{
    return mDepthTarget.mpTextureState;
}

// ARTIST 823F9250.
void RenderTarget::Begin(u32 section)
{
    const renderengine::RenderTargetState* state = muColourMode == 2
        ? renderengine::Device::GetDefaultRenderTargetState() : (&mpSection0State)[section];
    shadow::Device::SetState(state);
    D3DVIEWPORTF9 viewport;
    viewport.X = 0.0f; viewport.Y = 0.0f;
    viewport.Width = static_cast<float>(muWidth);
    viewport.Height = static_cast<float>(muHeight);
    viewport.MinZ = 0.0f; viewport.MaxZ = 1.0f; viewport.Flags = 0;
    D3DDevice_SetViewportF(gpD3DDevice, &viewport);
    RECT scissor;
    scissor.left = 0; scissor.top = 0;
    scissor.right = muWidth; scissor.bottom = muHeight;
    D3DDevice_SetScissorRect(gpD3DDevice, &scissor);
}

// ARTIST 823FE648 and 823F9338.
void RenderTarget::End(bool resolve)
{
    if (resolve) Resolve(true, true);
}
void RenderTarget::Resolve(bool resolveDepth, bool resolveColour)
{
    if (resolveColour && maColourTargets[0].mpTexture)
        maColourTargets[0].Resolve();
    if (mu8HasColour && resolveDepth)
        mDepthTarget.Resolve();
}

// ARTIST 824037E8. The depth sampler is Target::mpTextureState at +8C.
void RenderTarget::CreateStates(const Parameters& parameters)
{
    mu8NumSections = parameters.mu8NumSections;
    if (muColourMode == 1 || (muColourMode == 0 && muDepthStencilMode != 0))
    {
        renderengine::RenderTargetState::Parameters state;
        for (u32 i = 0; i < 5; ++i) state.targets[i] = 0;
        if (muColourMode == 1) state.targets[0] = maColourTargets[0].mpPixelBuffer;
        if (muDepthStencilMode == 1 || muDepthStencilMode == 4)
            state.targets[4] = mDepthTarget.mpPixelBuffer;
        else if (muDepthStencilMode == 2)
            state.targets[4] = &renderengine::gOriginalDepthStencilBuffer;
        const rw::ResourceDescriptor descriptor = renderengine::RenderTargetState::GetResourceDescriptor(state);
        const rw::Resource resource = mpAllocator->DoAllocate(descriptor, 0);
        mpSection0State = renderengine::RenderTargetState::Initialize(resource, state);
        if (muColourMode == 0 && parameters.mu8NumSections > 1)
        {
            CGS_ASSERT(parameters.maColourTargetParams[0].mTextureType != renderengine::Texture::TYPE_CUBE,
                       "params.GetColorTargetParameters(0).GetTextureType() != Texture::TYPE_CUBE");
            CGS_ASSERT(muDepthStencilMode != 2, "m_depthStencilMode != eRenderTarget_USE_DEVICE_FOR_WRITE");
        }
    }
    if (!parameters.mbUseDepthStencilAsTexture || muDepthStencilMode == 4)
    {
        mDepthTarget.mpTextureState = 0;
        return;
    }
    renderengine::TextureState::Parameters state;
    state.mpTexture = mDepthTarget.mpHiZTexture ? mDepthTarget.mpHiZTexture : mDepthTarget.mpTexture;
    if (muColourMode == 0 && muDepthStencilMode != 0)
    {
        state.mbWhiteBorder = 1;
        state.muAddressU = 6;
        state.muAddressV = 6;
    }
    else
    {
        state.muAddressU = 2;
        state.muAddressV = 2;
    }
    state.muMipFilter = 2;
    const rw::ResourceDescriptor descriptor = renderengine::TextureState::GetResourceDescriptor(state);
    const rw::Resource resource = mpAllocator->DoAllocate(descriptor, 0);
    mDepthTarget.mpTextureState = renderengine::TextureState::Initialize(resource, state);
}

// ARTIST 824088B0; four colour records followed by the depth record.
RenderTarget::RenderTarget(const Parameters& parameters)
{
    mpAllocator = 0;
    muWidth = parameters.mu32Width;
    muHeight = parameters.mu32Height;
    muColourMode = parameters.mColourMode;
    muDepthStencilMode = parameters.mDepthStencilMode;
    mu8HasColour = parameters.mbUseDepthStencilAsTexture;
    for (u32 i = 0; i < 4; ++i)
    {
        maColourTargets[i].mpPixelBuffer = 0;
        maColourTargets[i].mpTexture = 0;
        maColourTargets[i].mpTextureState = 0;
        maColourTargets[i].mpHiZTexture = 0;
        maColourTargets[i].mbIsShared = false;
    }
    mDepthTarget.mpPixelBuffer = 0;
    mDepthTarget.mpTexture = 0;
    mDepthTarget.mpTextureState = 0;
    mDepthTarget.mpHiZTexture = 0;
    mDepthTarget.mbIsShared = false;
    mpAllocator = parameters.mpAllocator ? parameters.mpAllocator : rw::ResourceAllocatorRegistry::GetDefaultAllocator();
    Target::Parameters target = parameters.maColourTargetParams[0];
    target.mpAllocator = mpAllocator;
    target.mu32Width = parameters.mu32Width;
    target.mu32Height = parameters.mu32Height;
    target.mu8NumSections = parameters.mu8NumSections;
    if (muColourMode == 1)
        maColourTargets[0].CreateColor(target);
    else if (muColourMode == 2)
        maColourTargets[0].mpPixelBuffer = &renderengine::gOriginalColorBuffer;
    if (muDepthStencilMode == 1)
    {
        target = parameters.mDepthStencilParams;
        target.mpAllocator = mpAllocator;
        target.mu32Width = parameters.mu32Width;
        target.mu32Height = parameters.mu32Height;
        target.mbUseStencil = parameters.mbUseDepthStencilAsTexture;
        target.mu8NumSections = parameters.mu8NumSections;
        mDepthTarget.CreateDepth(target);
    }
    else if (muDepthStencilMode == 4)
    {
        mDepthTarget = *parameters.mpProvidedDepthStencilTarget;
        mDepthTarget.mbIsShared = true;
    }
    mpSection0State = 0;
    CreateStates(parameters);
}

// ARTIST 82409B60.
RenderTarget* RenderTarget::Initialize(const Parameters& parameters)
{
    rw::ResourceDescriptor descriptor;
    descriptor.m_baseResourceDescriptors[0].m_size = sizeof(RenderTarget);
    descriptor.m_baseResourceDescriptors[0].m_alignment = 16;
    rw::IResourceAllocator* allocator = parameters.mpAllocator
        ? parameters.mpAllocator : rw::ResourceAllocatorRegistry::GetDefaultAllocator();
    const rw::Resource resource = allocator->DoAllocate(descriptor, 0);
    void* memory = resource.m_baseResources[0];
    return memory ? new (memory) RenderTarget(parameters) : 0;
}

}}}
