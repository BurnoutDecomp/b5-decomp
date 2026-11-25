#include "SDKs/RenderEngineClub/MAIN/components/src/texture2d.h"
#include "rw/rwcore_structs.h"

namespace renderengine
{
// ARTIST 823FF848. Keep descriptor construction separate from the GPU size owners.
rw::ResourceDescriptor Texture::GetResourceDescriptor(const Parameters& parameters)
{
    rw::ResourceDescriptor descriptor(sizeof(Texture), 4);
    if (!(parameters.muFlags & FLAG_DONTALLOCATE))
    {
        const u32 size = GetPixelDataSize(parameters.meType, parameters.muWidth,
            parameters.muHeight, parameters.muDepth, parameters.meFormat, parameters.muNumLevels);
        descriptor.m_baseResourceDescriptors[2] = rw::BaseResourceDescriptor(size, D3DTEXTURE_ALIGNMENT);
    }
    return descriptor;
}

// ARTIST 823C4CB8. The same lane layout uses the specialized 2D size query.
Texture2D::ResourceDescriptor Texture2D::GetResourceDescriptor(const Parameters& parameters)
{
    ResourceDescriptor descriptor(sizeof(Texture2D), 4);
    if (!(parameters.muFlags & FLAG_DONTALLOCATE))
    {
        const u32 size = Xbox2GetPhysicalMemorySize(parameters);
        descriptor.m_baseResourceDescriptors[2] = rw::BaseResourceDescriptor(size, D3DTEXTURE_ALIGNMENT);
    }
    return descriptor;
}
}
