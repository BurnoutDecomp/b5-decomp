#pragma once
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"

namespace renderengine
{
class Texture2D : public Texture
{
public:
    // Five words read by ARTIST 82B610D8 and 82B62E78. Depth belongs only
    // to Texture::Parameters; a 2D texture always supplies depth one.
    struct Parameters
    {
        u32 muFlags;
        u32 muWidth;
        u32 muHeight;
        u32 muNumLevels;
        PixelFormat meFormat;
    };
    typedef ResourceDescriptor5 ResourceDescriptor;
    static ResourceDescriptor* GetResourceDescriptor(ResourceDescriptor*, const Parameters*);
    static Texture2D* Initialize(const rw::Resource*, const Parameters*);
    static u32 Xbox2GetPhysicalMemorySize(const Parameters*);
};
CGS_STATIC_ASSERT((sizeof(Texture2D) == 0x34), "ARTIST 2D texture header");
CGS_STATIC_ASSERT((sizeof(Texture2D::Parameters) == 20), "ARTIST 2D texture parameters");
CGS_STATIC_ASSERT((offsetof(Texture2D::Parameters, meFormat) == 16), "ARTIST 2D format");
}
