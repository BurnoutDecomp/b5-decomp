#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827F78C8 (Shader::ParameterBlock)
// First-pass reconstruction: behaviour-faithful to the X360 pseudocode; struct
// layout inferred (offsets are from the 32-bit source, not byte-matched on host).

namespace Shader
{
    struct ResourceDescriptor
    {
        u32 entries[10];
    };

    // One shader parameter: a 28-byte record. Two dimensions drive the size.
    // FLAG: ARTIST 0x827F78C8 reads dimA as a HALFWORD (lhz 0(entry)) and dimB as a
    // single BYTE (lbz 2(entry)); the original is not two u16s. Entry stride = 0x1C (28).
    struct ParameterEntry
    {
        u16 dimA;       // [0] (lhz, halfword)
        u8  dimB;       // [2] (lbz, byte)
        u8  _pad0;      // [3]
        u8  _pad1[24];  // pads the record out to 28 bytes
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

        // FLAG: corrected to match ARTIST 0x827F78C8. The descriptor is filled as five
        // (size,alignment) pairs: pair 0 = (size, 16) (written via the trailing `std`
        // of the (roundedSize:16) qword to entries[0]/[1]); pairs 1..4 = (0, 1). The
        // previous fill (entries[0]=16, entries[1]=size, entries[2i]=size, [2i+1]=1)
        // was wrong on every dword except entries[3/5/7/9].
        result->entries[0] = size;
        result->entries[1] = 16;
        for (int i = 1; i < 5; ++i)
        {
            result->entries[2 * i]     = 0;
            result->entries[2 * i + 1] = 1;
        }
        return result;
    }
}
