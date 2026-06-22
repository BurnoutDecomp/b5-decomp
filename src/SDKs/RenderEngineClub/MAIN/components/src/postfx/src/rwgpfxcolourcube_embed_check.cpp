// Tiny embed check: re-declare the ColourCube home surface and force ODR use of the reconstructed
// GetResourceDescriptor so the compile gate exercises its signature end to end. Compile-only
// (cl /c); the real body + layout live in rwgpfxcolourcube.cpp.
#include "types.hpp"
#include "rw/rwcore_structs.h"

namespace rw::graphics::postfx
{
    class ColourCube
    {
    public:
        struct Parameters
        {
            u32 muEdgeLength;
        };

        static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpResult,
                                                                     const Parameters* lpParameters);
    };
}

void rwgpfxcolourcube_embed_check()
{
    rw::BaseResourceDescriptors<5> lDescriptor{};
    rw::graphics::postfx::ColourCube::Parameters lParameters{};
    lParameters.muEdgeLength = 16;
    (void)rw::graphics::postfx::ColourCube::GetResourceDescriptor(&lDescriptor, &lParameters);
}
