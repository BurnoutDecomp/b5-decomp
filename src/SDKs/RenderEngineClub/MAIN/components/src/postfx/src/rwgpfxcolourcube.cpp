#include "types.hpp"

#include "rw/rwcore_structs.h"  // rw::BaseResourceDescriptors<5>
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.h"  // rw::graphics::postfx::ColourCube

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::ColourCube::GetResourceDescriptor @ 0x82402C48
//
// A ColourCube is the post-fx colour-grading lookup volume: an N x N x N RGB cube (3 bytes per cell)
// preceded by a 16-byte header. GetResourceDescriptor sizes the rw resource for it from the cube
// edge length carried in the Parameters block: it seeds a five-entry serialised resource descriptor
// (rw::BaseResourceDescriptors<5>) to {size=0, align=1} in every slot, then writes slot0 =
// {size = 3*N*N*N + 16, align = 16} -- the packed RGB cell data (3 bytes * N^3) plus the header,
// 16-byte aligned. Its caller is CgsResource::RwColourCubeResourceType::GetSerialisedResourceDescriptor.
//
// Layout / store order follow the X360 asm (it overrides DWARF). The 64-bit slot0 store is a single
// big-endian std at result[0]: on the X360 target the high 32 bits land in result[0] (m_size) and the
// low 32 bits in result[1] (m_alignment), so m_size = 3*N^3 + 16 and m_alignment = 16. (Hex-Rays'
// LODWORD/HIDWORD framing is little-endian; the big-endian image stores {size, align} = {3N^3+16, 16}.)
//
// rw::graphics::postfx is the committed post-fx namespace (see RwVignetteParameters.h). The ColourCube
// class declaration carrying GetResourceDescriptor now lives in the shared header rwgpfxcolourcube.h
// so the game-side resource-type handler (CgsRwColourCubeResourceType.cpp) can share it.

namespace rw::graphics::postfx
{
    // X360 0x82402C48.
    rw::BaseResourceDescriptors<5>* ColourCube::GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpResult,
                                                                      const Parameters* lpParameters)
    {
        const u32 luEdge = lpParameters->muEdgeLength;   // r9 = *a2

        // Seed all five entries to {size=0, align=1}.
        for (u32 luSlot = 0; luSlot < 5; ++luSlot)
        {
            lpResult->m_baseResourceDescriptors[luSlot].m_size      = 0;
            lpResult->m_baseResourceDescriptors[luSlot].m_alignment = 1;
        }

        // slot0: the packed RGB cell volume (3 bytes per cell, N^3 cells) plus the 16-byte header,
        // 16-byte aligned. mullw/mullw build N^3, slwi+add build 3*N^3, addi adds the 0x10 header.
        const u32 luSize = 3u * luEdge * luEdge * luEdge + 16u;
        lpResult->m_baseResourceDescriptors[0].m_size      = luSize;  // big-endian std high word
        lpResult->m_baseResourceDescriptors[0].m_alignment = 16;      // big-endian std low word
        return lpResult;
    }
}
