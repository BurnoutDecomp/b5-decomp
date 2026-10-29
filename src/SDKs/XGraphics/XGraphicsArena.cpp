#include "SDKs/XGraphics/XGraphicsArena.h"
#include "SDKs/XGraphics/XGraphicsCompiler.h"

#include <cstring>

namespace XGRAPHICS
{
// ARTIST82C26AC8; the SDK signature is unsigned, but the minimum-size test is signed.
void Arena::Grow(unsigned int size) const
{
    if (static_cast<s32>(size) < 12248)
        size = 12248;

    const u32 blockSize = size + sizeof(Chunk);
    Chunk* previous = chunk;
    Chunk* block = static_cast<Chunk*>(context->malloc(context->handle, blockSize));
    chunk = block;
    chunk->previous = previous;
    chunk->size = blockSize;
    char* base = reinterpret_cast<char*>(chunk);
    hwm = base + sizeof(Chunk);
    amax = base + blockSize;
    old = base + sizeof(Chunk);
}

// ARTIST82C21EC0.
void* Arena::Malloc(unsigned int size) const
{
    size = (size + 3u) & ~3u;
    if (hwm + size > amax)
        Grow(size);

    char* allocation = hwm;
    old = allocation;
    if (recycle && allocation < hwm_max)
    {
        ++reused;
        reused_bytes += allocation + size >= hwm_max
            ? static_cast<u32>(hwm_max - allocation) : size;
    }

    char* next = allocation + size;
    hwm = next;
    hwm_max = hwm_max < next ? next : hwm_max;
    return old;
}

// ARTIST82C21F88: statistics and cursor are reloaded after the external memset.
void Arena::Free(void* block) const
{
    if (recycle && block == old)
    {
        std::memset(block, 0, static_cast<u32>(hwm - old));
        const u32 freed = static_cast<u32>(hwm - old);
        hwm = old;
        ++reclaimed;
        reclaimed_bytes += freed;
    }
}
}
