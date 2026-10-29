#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"

namespace renderengine
{
    class PixelBuffer;

    class RenderTargetState
    {
    public:
        struct Parameters
        {
            PixelBuffer* targets[5];
        };

        static rw::ResourceDescriptor GetResourceDescriptor(const Parameters& parameters);
        static RenderTargetState* Initialize(const rw::Resource& resource, const Parameters& parameters);

        PixelBuffer* mpDepthTarget;
        PixelBuffer* mpColorTargets[4];
        u32 muInitialized;
    };

    CGS_STATIC_ASSERT((sizeof(RenderTargetState::Parameters) == 0x14), "ARTIST target parameters");
    CGS_STATIC_ASSERT((sizeof(RenderTargetState) == 0x18), "ARTIST target state");
    CGS_STATIC_ASSERT((offsetof(RenderTargetState, mpColorTargets) == 0x04), "ARTIST color targets");
    CGS_STATIC_ASSERT((offsetof(RenderTargetState, muInitialized) == 0x14), "ARTIST target state flag");
}
