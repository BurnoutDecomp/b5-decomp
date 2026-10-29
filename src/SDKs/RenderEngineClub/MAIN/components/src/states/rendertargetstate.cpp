#include "SDKs/RenderEngineClub/MAIN/components/src/states/rendertargetstate.h"
#include <cstring>

namespace renderengine
{
    // ARTIST 0x82B63678: five lanes, first containing a 0x18-byte state.
    rw::ResourceDescriptor RenderTargetState::GetResourceDescriptor(const Parameters&)
    {
        rw::ResourceDescriptor descriptor;
        for (u32 i = 0; i < 5; ++i)
        {
            descriptor.m_baseResourceDescriptors[i].m_size = 0;
            descriptor.m_baseResourceDescriptors[i].m_alignment = 1;
        }
        descriptor.m_baseResourceDescriptors[0].m_size = 0x18;
        descriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        return descriptor;
    }

    // ARTIST 0x82B62748: parameter targets 0..3 become state words 1..4,
    // target 4 becomes word 0, followed by the initialized flag.
    RenderTargetState* RenderTargetState::Initialize(const rw::Resource& resource, const Parameters& parameters)
    {
        RenderTargetState* state = static_cast<RenderTargetState*>(resource.m_baseResources[0]);
        std::memset(state, 0, sizeof(*state));
        for (u32 i = 0; i < 4; ++i)
            state->mpColorTargets[i] = parameters.targets[i];
        state->mpDepthTarget = parameters.targets[4];
        state->muInitialized = 1;
        return state;
    }
}
