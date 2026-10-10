#pragma once

#include <cstdint>
#include <cstdlib>

// FLAG PC-platform leaf: ARTIST 827F6948..6A7C uses no pixel program for
// opaque Z-only geometry. Keep the PC's hardware SM3 pair valid with an input-
// free shader: no texture reads, kills or depth output, and colour writes are
// disabled by the original factory state. No runtime compiler DLL is needed.
namespace renderengine::DepthOnlyPC
{
    inline bool NeedsMaterialPixelShader(bool lbZOnly, unsigned luTechniqueFlags)
    {
        return !lbZOnly || (luTechniqueFlags & 8u) != 0;
    }
    inline bool Enabled()
    {
        static const bool sbEnabled = [] {
            const char* lpcValue = std::getenv("BRN_DEPTH_SHADER");
            return !lpcValue || lpcValue[0] != '0';
        }();
        return sbEnabled;
    }
    // ps_3_0; def c0,1,1,1,1; mov oC0,c0; end. DEF is shader-local and
    // does not change device constant registers shared with material shaders.
    inline const std::uint32_t KAU_PIXEL_CODE[] = {
        0xffff0300u, 0x05000051u, 0xa00f0000u,
        0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u,
        0x02000001u, 0x800f0800u, 0xa0e40000u, 0x0000ffffu
    };
}
