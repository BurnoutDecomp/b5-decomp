#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827F7A10
//   (RenderableMesh::GetResourceDescriptor)
//
// Reconstructed directly from the PPC disassembly — the Hex-Rays pseudocode is
// MISLEADING here (it renders the even-index zero stores as `HIDWORD(v2)` and
// hides that the final write is a 64-bit store). Ground-truth asm trace:
//
//   r9  = (4*count + 0x3F) & ~0xF          ; aligned  (clrrwi r9,r9,4)
//   r8  = 0x10 (=16) -> stack var_10+4 (low dword of v2)
//   r9  = aligned    -> stack var_10   (high dword of v2)
//   ld  r11, var_10                        ; r11 = v2 = (aligned<<32) | 16  (big-endian)
//   stw r11(=1)  -> result[1],[3],[5],[7],[9]
//   stw r10(=0)  -> result[0],[2],[4],[6],[8]
//   std r11, 0(r3)                         ; 64-bit big-endian store of v2:
//                                          ;   result[0] = HIDWORD(v2) = aligned
//                                          ;   result[1] = LODWORD(v2) = 16
//
// Net final state (5 descriptor entries, each a {size, alignment} u32 pair):
//   entry 0 : { size = aligned, alignment = 16 }
//   entry 1 : { size = 0,       alignment = 1  }
//   entry 2 : { size = 0,       alignment = 1  }
//   entry 3 : { size = 0,       alignment = 1  }
//   entry 4 : { size = 0,       alignment = 1  }
//
// i.e. one 16-byte-aligned buffer of `aligned` bytes (sized for `count` 4-byte
// elements, rounded up), followed by four empty zero-size sections.
// where aligned = (4 * count + 63) & 0xFFFFFFF0.

struct ResourceDescriptorEntry
{
    u32 muSize;
    u32 muAlignment;
};

struct RenderableMesh
{
    static ResourceDescriptorEntry* GetResourceDescriptor(
        ResourceDescriptorEntry* lpaResult, int liCount);
};

ResourceDescriptorEntry* RenderableMesh::GetResourceDescriptor(
    ResourceDescriptorEntry* lpaResult, int liCount)
{
    const u32 luAligned = (4u * static_cast<u32>(liCount) + 63u) & 0xFFFFFFF0u;

    lpaResult[0].muSize      = luAligned;
    lpaResult[0].muAlignment = 16u;
    for (int liEntry = 1; liEntry < 5; ++liEntry)
    {
        lpaResult[liEntry].muSize      = 0u;
        lpaResult[liEntry].muAlignment = 1u;
    }
    return lpaResult;
}
