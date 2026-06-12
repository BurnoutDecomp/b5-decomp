#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828EDF70
//   (CgsResource::TextFileResourceType::GetSerialisedResourceDescriptor)
//
// Reconstructed from the PPC disassembly — the Hex-Rays pseudocode is MISLEADING
// here (it renders the even-index zero stores as `v3` and hides the 64-bit final
// store). Ground-truth asm trace:
//
//   r9 = *lpResource + 5                 ; size (v3)
//   stw r11(=1) -> result[1],[3],[5],[7],[9]
//   r8 = 4 -> var_10+4 (LODWORD of v4)
//   r9 = v3 -> var_10   (HIDWORD of v4)
//   ld  r11, var_10                      ; r11 = v4 = (v3<<32) | 4  (big-endian)
//   stw r10(=0) -> result[0],[2],[4],[6],[8]
//   std r11, 0(r3)                       ; result[0]=HIDWORD(v4)=v3, result[1]=LODWORD(v4)=4
//
// Net: 5 descriptor entries ({size, alignment} u32 pairs):
//   entry 0 : { size = *lpResource + 5, alignment = 4 }
//   entry 1..4 : { size = 0,            alignment = 1 }

namespace CgsResource
{
    struct ResourceDescriptorEntry
    {
        u32 muSize;
        u32 muAlignment;
    };

    class TextFileResourceType
    {
    public:
        ResourceDescriptorEntry* GetSerialisedResourceDescriptor(
            ResourceDescriptorEntry* lpaResult, const u32* lpResource) const;
    };

    ResourceDescriptorEntry* TextFileResourceType::GetSerialisedResourceDescriptor(
        ResourceDescriptorEntry* lpaResult, const u32* lpResource) const
    {
        const u32 luSize = lpResource[0] + 5u;

        lpaResult[0].muSize      = luSize;
        lpaResult[0].muAlignment = 4u;
        for (int liEntry = 1; liEntry < 5; ++liEntry)
        {
            lpaResult[liEntry].muSize      = 0u;
            lpaResult[liEntry].muAlignment = 1u;
        }
        return lpaResult;
    }
}
