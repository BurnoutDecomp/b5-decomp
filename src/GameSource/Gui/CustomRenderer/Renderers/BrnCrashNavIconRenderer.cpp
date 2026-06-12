#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E0CA0
//   (BrnGui::CrashNavIconRenderer::SetRenderEnabled)
//
// Behaviour-faithful to the X360 pseudocode:
//     *(result + 4)    = a2;             // byte: render-enabled flag
//     *(result + 336)  = 0x400000000LL;  // 64-bit field
//     *(result + 344)  = 0x400000000LL;  // 64-bit field
//     *(result + 356)  = 0;
//     *(result + 352)  = 0;
//     *(result + 456)  = 0;
//     *(result + 452)  = 0;
//     *(result + 5444) = 0;
//     *(result + 436)  = 4;
//     return result;
//
// Stores the enabled flag, then resets the icon's animation/placement state: two
// 64-bit fields to 0x4_00000000, several counters/handles to 0, and a mode word to
// 4. The renderer object is large and opaque, so the touched members are addressed
// by their original byte offsets through a raw view of `this`. The 0x400000000
// stores are kept as 64-bit writes (the logical value), not byte-matched.

namespace BrnGui
{
    struct CrashNavIconRenderer
    {
        CrashNavIconRenderer* SetRenderEnabled(bool lbEnabled);
    };

    CrashNavIconRenderer* CrashNavIconRenderer::SetRenderEnabled(bool lbEnabled)
    {
        u8* lpThis = reinterpret_cast<u8*>(this);

        lpThis[4] = static_cast<u8>(lbEnabled);

        *reinterpret_cast<u64*>(lpThis + 336) = 0x400000000ull;
        *reinterpret_cast<u64*>(lpThis + 344) = 0x400000000ull;
        *reinterpret_cast<u32*>(lpThis + 352) = 0;
        *reinterpret_cast<u32*>(lpThis + 356) = 0;
        *reinterpret_cast<u32*>(lpThis + 452) = 0;
        *reinterpret_cast<u32*>(lpThis + 456) = 0;
        *reinterpret_cast<u32*>(lpThis + 5444) = 0;
        *reinterpret_cast<u32*>(lpThis + 436) = 4;

        return this;
    }
}
