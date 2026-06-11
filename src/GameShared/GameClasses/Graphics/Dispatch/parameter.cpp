#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x... (Shader::ParameterBlock)
// First-pass reconstruction: behaviour-faithful to the X360 pseudocode; struct
// layout inferred (offsets are from the 32-bit source, not byte-matched on host).

namespace Shader
{
    struct ResourceDescriptor
    {
        u32 entries[10];
    };

    // One shader parameter: a 28-byte record. Two u16 dimensions drive the size.
    struct ParameterEntry
    {
        u16 dimA;       // [0]
        u16 _pad0;
        u16 dimB;       // [2] (byte offset 4)
        u16 _pad1[11];  // pads the record out to 28 bytes
    };

    struct ParameterBlock
    {
        u32             count;    // [0]
        u32             _pad;
        const u8*       data;     // [2] (offset 8): entry array begins 24 bytes in
    };

    // result, a2 mirror the original (result, int **a2) signature.
    ResourceDescriptor* GetResourceDescriptor(ResourceDescriptor* result, ParameterBlock** a2);

    ResourceDescriptor* GetResourceDescriptor(ResourceDescriptor* result, ParameterBlock** a2)
    {
        ParameterBlock* block = *a2;
        u32 size = 16;
        u32 count = block->count;
        if (count)
        {
            const ParameterEntry* p =
                reinterpret_cast<const ParameterEntry*>(block->data + 24);
            do
            {
                size += 4u * p->dimB * p->dimA;
                ++p;
            } while (--count);
        }
        size = (size + 1039) & ~15u;

        // Descriptor: region 0 = (16, size); regions 1..4 = (size, 1).
        result->entries[0] = 16;
        result->entries[1] = size;
        for (int i = 1; i < 5; ++i)
        {
            result->entries[2 * i]     = size;
            result->entries[2 * i + 1] = 1;
        }
        return result;
    }
}
