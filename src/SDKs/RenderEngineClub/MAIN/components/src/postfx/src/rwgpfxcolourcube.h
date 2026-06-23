#ifndef RW_GPFX_COLOUR_CUBE_H
#define RW_GPFX_COLOUR_CUBE_H

#include "types.hpp"
#include "rw/rwcore_structs.h"  // rw::BaseResourceDescriptors<5>

// rw::graphics::postfx::ColourCube -- the post-fx colour-grading lookup volume: an N x N x N RGB
// cube (3 bytes per cell) preceded by a 16-byte header. GetResourceDescriptor sizes the rw
// resource for it from the cube edge length carried in the Parameters block.
//
// Definition lives here so both the SDK home (rwgpfxcolourcube.cpp) and the game-side resource-
// type handler (CgsRwColourCubeResourceType.cpp) share one declaration.
namespace rw::graphics::postfx
{
    class ColourCube
    {
    public:
        // The colour-cube build parameters. Only the leading edge-length word is read by
        // GetResourceDescriptor (X360 *a2); the remaining fields are opaque to it and modelled
        // as a tail.
        struct Parameters
        {
            u32 muEdgeLength;   // +0x00 N: the cube is N x N x N RGB cells
        };

        // X360 0x82402C48 -- size the rw resource for the colour cube.
        static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpResult,
                                                                     const Parameters* lpParameters);
    };
}

#endif // RW_GPFX_COLOUR_CUBE_H
