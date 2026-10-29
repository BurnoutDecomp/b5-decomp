#pragma once

#include "types.hpp"

namespace XGRAPHICS
{
class Compiler;

// ARTIST82C26B08..B10: the backing allocation starts with this eight-byte header.
struct Chunk
{
    Chunk* previous;
    u32 size;
};

// The original XDK CodeView names and signatures agree with all three ARTIST
// routines. Allocation changes the mutable arena state through a const owner.
class Arena
{
public:
    void* Malloc(unsigned int size) const; //82C21EC0
    void Free(void* block) const;          //82C21F88
    void Grow(unsigned int size) const;    //82C26AC8

    Compiler* context;
    mutable Chunk* chunk;
    mutable char* hwm;
    mutable char* hwm_max;
    mutable char* amax;
    mutable char* old;
    mutable int reclaimed;
    mutable int reused;
    mutable u32 reclaimed_bytes;
    mutable u32 reused_bytes;
    bool recycle;
};

CGS_STATIC_ASSERT((sizeof(Chunk) == 8), "ARTIST Arena block header");
CGS_STATIC_ASSERT((offsetof(Arena, context) == 0), "ARTIST Arena Compiler owner");
CGS_STATIC_ASSERT((offsetof(Arena, chunk) == 4), "ARTIST Arena chunk");
CGS_STATIC_ASSERT((offsetof(Arena, hwm) == 8), "ARTIST Arena cursor");
CGS_STATIC_ASSERT((offsetof(Arena, hwm_max) == 0xC), "ARTIST Arena high water");
CGS_STATIC_ASSERT((offsetof(Arena, amax) == 0x10), "ARTIST Arena end");
CGS_STATIC_ASSERT((offsetof(Arena, old) == 0x14), "ARTIST Arena last allocation");
CGS_STATIC_ASSERT((offsetof(Arena, reclaimed) == 0x18), "ARTIST Arena reclaim count");
CGS_STATIC_ASSERT((offsetof(Arena, reused) == 0x1C), "ARTIST Arena reuse count");
CGS_STATIC_ASSERT((offsetof(Arena, reclaimed_bytes) == 0x20), "ARTIST Arena reclaimed bytes");
CGS_STATIC_ASSERT((offsetof(Arena, reused_bytes) == 0x24), "ARTIST Arena reused bytes");
CGS_STATIC_ASSERT((offsetof(Arena, recycle) == 0x28), "ARTIST Arena recycle flag");
CGS_STATIC_ASSERT((sizeof(Arena) == 0x2C), "ARTIST Arena size");

// Native allocation sites prepend the owning Arena pointer to their objects.
inline void* ArenaAllocPrefixed(Arena* arena, usize objectSize)
{
    void* block = arena->Malloc(static_cast<u32>(objectSize + sizeof(Arena*)));
    *static_cast<Arena**>(block) = arena;
    if (reinterpret_cast<intptr_t>(block) == -static_cast<intptr_t>(sizeof(Arena*)))
        return 0;
    return static_cast<u8*>(block) + sizeof(Arena*);
}

inline void ArenaFreePrefixed(void* object)
{
    u8* block = static_cast<u8*>(object) - sizeof(Arena*);
    Arena* arena = *reinterpret_cast<Arena**>(block);
    arena->Free(block);
}
}
