#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827F7958 (Shader::ParameterSemanticBlock)
// First-pass reconstruction: behaviour-faithful to the X360 pseudocode; struct
// layout is inferred (field roles named by use, padding offsets are from the
// 32-bit source and not byte-matched on a 64-bit host).

namespace Shader
{
    // Ten-dword GPU resource descriptor: five (offset, size) region pairs.
    struct ResourceDescriptor
    {
        u32 entries[10];
    };

    // Describes a block of shader parameter-semantic data. The per-parameter and
    // per-semantic byte sizes live in two side arrays of `count` entries each.
    struct ParameterSemanticBlock
    {
        u32  count;          // a2[0]
        u32* paramSizes;     // a2[3]  (offset 12 in the 32-bit layout)
        u32* semanticSizes;  // a2[5]  (offset 20)

        ResourceDescriptor* GetResourceDescriptor(ResourceDescriptor* result);
    };

    // Computes the total resource-buffer size for this block (a 16-byte header plus
    // 28 bytes per entry, plus every parameter/semantic size, each padded up to 16),
    // then fills the descriptor with five identically-sized regions.
    ResourceDescriptor* ParameterSemanticBlock::GetResourceDescriptor(ResourceDescriptor* result)
    {
        u32 total = ((28 * count + 15) & ~15u) + 16;
        if (count)
        {
            for (u32 i = 0; i < count; ++i)
                total += (paramSizes[i] + 15) & ~15u;
            for (u32 i = 0; i < count; ++i)
                total += (semanticSizes[i] + 15) & ~15u;
        }

        // FLAG: corrected to match ARTIST 0x827F7958. The descriptor is five
        // (size,alignment) pairs: pair 0 = (total, 16) (written via the trailing `std`
        // of the (total:16) qword to entries[0]/[1]); pairs 1..4 = (0, 1). The previous
        // fill (entries[0]=16, entries[1]=total, entries[2i]=0, entries[2i+1]=total)
        // had entries[0]/[1] swapped and put `total` where the alignment 1 belongs.
        result->entries[0] = total;
        result->entries[1] = 16;
        for (int i = 1; i < 5; ++i)
        {
            result->entries[2 * i]     = 0;
            result->entries[2 * i + 1] = 1;
        }
        return result;
    }
}
