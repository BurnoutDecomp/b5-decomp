#include "SDKs/RenderEngineClub/MAIN/components/src/states/rendertargetstate.h"
#include <cstring>

// ARTIST 82B62770 calls the XDK libcMT memset provider.
#pragma function(memset)

namespace renderengine
{
    // ARTIST 0x82B63678: five lanes, first containing a 0x18-byte state.
    // ARTIST retains the compact five-lane constructor loop here.
#pragma optimize("s", on)
    rw::ResourceDescriptor RenderTargetState::GetResourceDescriptor(const Parameters&)
    {
        rw::ResourceDescriptor descriptor(sizeof(RenderTargetState), __alignof(RenderTargetState));
        return descriptor;
    }
#pragma optimize("s", off)

    // ARTIST 0x82B62748: parameter targets 0..3 become state words 1..4,
    // target 4 becomes word 0, followed by the initialized flag.
#pragma optimize("s", on)
    RenderTargetState* RenderTargetState::Initialize(const rw::Resource& resource, const Parameters& parameters)
    {
        RenderTargetState* state = static_cast<RenderTargetState*>(resource.m_baseResources[0]);
        memset(state, 0, sizeof(*state));
        PixelBuffer* const* source = parameters.targets;
        PixelBuffer** destination = state->mpColorTargets;
        int remaining = sizeof(state->mpColorTargets) / sizeof(state->mpColorTargets[0]);
        do
        {
            PixelBuffer* const target = *source;
            ++source;
            *destination = target;
            ++destination;
        }
        while (--remaining);
        state->mpDepthTarget = parameters.targets[4];
        state->muInitialized = 1;
        return state;
    }
#pragma optimize("s", off)
}
