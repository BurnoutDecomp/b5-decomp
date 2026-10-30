#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"

namespace CgsGraphics
{
// ARTIST827ED118; the first argument is the actual resource allocator.
// Xenos source/destination/operation enum values come from the native SDK.
const BlendState* ImRendererBase::ConstructBlendState(rw::IResourceAllocator* allocator,
    renderengine::BlendState::Input source, renderengine::BlendState::Input destination,
    renderengine::BlendState::Operation operation)
{
    renderengine::BlendState::Parameters parameters;
    parameters.maBlendFactor[0] =
        (static_cast<u32>(source) & 0x1Fu)
        | ((32u * (((8u * static_cast<u32>(destination)) & 0x7F8u)
            | (static_cast<u32>(operation) & 0xFFFFF807u))) & 0xFFE0u)
        | 0x07060000u;
    parameters.mbHasCustomBlendFactors = 1;
    const rw::ResourceDescriptor descriptor =
        renderengine::BlendState::GetResourceDescriptor(parameters);
    const rw::Resource resource = allocator->DoAllocate(descriptor, 0);
    return renderengine::BlendState::Initialize(resource, parameters);
}
}
