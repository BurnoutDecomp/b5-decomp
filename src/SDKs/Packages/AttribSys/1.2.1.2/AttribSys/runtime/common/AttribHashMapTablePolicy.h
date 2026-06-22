#pragma once

#include "types.hpp"

// Attrib::HashMapTablePolicy -- the AttribSys hash-map's allocation policy.
//
// AttribSys's internal HashMapTable (the open-addressing bucket array behind a
// Collection's / Class's attribute lookup) is parameterised on an allocation
// policy, EASTL-style. HashMapTablePolicy is that policy: a namespace-of-statics
// helper that routes every bucket-array (re)allocation through the engine's
// AttribSys package allocator and keeps a running byte census so the AttribSys
// memory report can show the hash-map overhead.
//
// Recovered from BURNOUT_X360_ARTIST.XEX. Only Free is X360-ledger attested:
//   Free(void*, size_t)  @ 0x82804690
// Allocate (the malloc-side counterpart that bumps smCurrentMemory the other way
// and forwards onto AttribSysPackageAllocator::Malloc) is declared so the
// HashMapTable container compiles against the real policy interface; its body
// lives in its own TU and is not part of this group.
//
// The two byte counters are the X360 file-scope statics that sit beside the
// AttribSys database singleton pointer:
//   smCurrentMemory  (dword_83011BFC)  -- live bytes the hash-map policy holds
//   smPeakMemory     (dword_83011BF8)  -- high-water mark of smCurrentMemory
// The "Attrib::HashMapTable" string is the diagnostic tag handed to the package
// allocator's tagged Free overload on release.
namespace Attrib
{
    class HashMapTablePolicy
    {
    public:
        // X360 0x82804690 -- return liSize bytes (previously handed out by Allocate)
        // to the AttribSys package allocator and decrement the live-byte census.
        // The peak counter is refreshed against the post-decrement total to mirror
        // the shared census update the X360 emits. lpBlock/liSize are only forwarded
        // to the allocator when both are non-zero. Returns the freed block pointer
        // (the X360 returns r3 from the inner free, which is the block; the
        // Collection/ClassPrivate destructor thunks thread it back as their own
        // this-return). Modelled as void* so it is host-pointer-width correct (the
        // X360 r3 is a 32-bit pointer).
        static void* Free(void* lpBlock, size_t liSize);

        // The malloc-side counterpart (bumps smCurrentMemory / smPeakMemory the other
        // way and forwards onto AttribSysPackageAllocator::Malloc). Body in its own TU.
        static void* Allocate(size_t liSize, size_t liAlignment, size_t liOffset, int liFlags);

    private:
        // Live bytes currently held by the hash-map policy (X360 dword_83011BFC).
        static u32 smCurrentMemory;
        // High-water mark of smCurrentMemory (X360 dword_83011BF8).
        static u32 smPeakMemory;
    };
}
