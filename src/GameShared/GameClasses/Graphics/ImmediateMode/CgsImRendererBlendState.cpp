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
    const renderengine::RenderTargetState::TargetID target = renderengine::RenderTargetState::TARGETID_COLOR0;
    parameters.SetOperation(target, operation);
    parameters.SetDestination(target, destination);
    parameters.SetSource(target, source);
    parameters.SetBlendEnable(true);

    const rw::ResourceDescriptor descriptor =
        renderengine::BlendState::GetResourceDescriptor(parameters);
    const rw::Resource resource = allocator->Allocate(descriptor, 0);
    return renderengine::BlendState::Initialize(resource, parameters);
}
}
