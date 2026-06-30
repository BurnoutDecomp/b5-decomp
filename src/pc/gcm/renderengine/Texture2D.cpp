#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/Xbox2TextureShims.h"

// X360 reconstruction of the renderengine::Texture2D EDRAM/format helper. The descriptor +
// create-path bodies (GetResourceDescriptor / Initialize) are the PC D3D9 reconstruction in
// texture.cpp; this TU homes only the X360 platform helper Xbox2GetPhysicalMemorySize, which
// sizes a texture's physical (GPU) memory by laying out a throwaway texture header. Faithful to
// the X360 ASM @ 0x82B610D8: the call is XGSetTextureHeader(Width, Height, Levels, 0, Format,
// 0, 0, -1) and the descriptor dwords are read positionally (a1[1..4]).

namespace renderengine
{
    // 0x82B610D8 -- lay out a GPU texture header for the descriptor and return its size.
    //   r3=Width=a1[1], r4=Height=a1[2], r5=Levels=a1[3], r7=Format=a1[4]; Usage/Pool/BaseOffset
    //   are 0, MipOffset is -1. The XGSetTextureHeader HRESULT-sized result is the memory size.
    u32 Texture2D::Xbox2GetPhysicalMemorySize(const u32* lpDescriptorDwords)
    {
        const int liWidth  = static_cast<int>(lpDescriptorDwords[1]);
        const int liHeight = static_cast<int>(lpDescriptorDwords[2]);
        const int liLevels = static_cast<int>(lpDescriptorDwords[3]);
        const int liFormat = static_cast<int>(lpDescriptorDwords[4]);

        return static_cast<u32>(
            XGSetTextureHeader(liWidth, liHeight, liLevels, 0, liFormat, 0, 0, -1));
    }
}
