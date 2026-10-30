#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"

namespace renderengine
{
    class PixelBuffer;

    class RenderTargetState
    {
    public:
        // ARTIST82B61E18 treats 0..3 as colour slots and 4 as all colours.
        enum TargetID { TARGETID_COLOR0 = 0, TARGETID_COLOR1 = 1, TARGETID_COLOR2 = 2,
                        TARGETID_COLOR3 = 3, TARGETID_COLOR_ALL = 4 };
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
