#pragma once

// CgsMemory::LowMemory -- the x64 port's low-4GB backing-memory reservation.
//
// FLAG PC-platform leaf: THIS HAS NO X360 COUNTERPART BY CONSTRUCTION. The X360 is a
// 32-bit address space, so every address in the game fits a u32 and the shipped
// serialised resource format stores POINTERS AS 4-BYTE SLOTS -- the resource FixUp
// passes (and the Attrib vault's PtrN type-3/type-4 fixup, attribloadandgo.cpp) write
// a live host pointer straight into a 4-byte on-disc field, and every consumer reads
// it back with the committed PointerFromU32 idiom
// (`reinterpret_cast<T*>(static_cast<uintptr_t>(slot))`).
//
// That convention is data-format, not code: the slots ARE 4 bytes wide in the shipped
// bundles, so it can only be honoured on x64 by making the pointed-to memory live below
// 4 GB. On Windows x64 the CRT heap hands out addresses anywhere in the 128 TB user
// space (measured on the dev machine: malloc -> 0x000001C32DB21F80), so a fixup silently
// truncates and the first dereference faults. This module is the structural consequence
// of the 32-bit -> 64-bit port and nothing else: it reserves the engine's ROOT backing
// block inside the low 4 GB so every pointer the resource system ever stores into a
// 4-byte slot round-trips exactly.
//
// Everything the resource system uses is carved from that one root block
// (BrnResource::Allocators::mpInternalDebugAllocator -> the MemoryModule root banks ->
// the 27 pools + the 24 memory-map allocators, including the AttribSys package heap),
// so a single low reservation makes the whole convention sound at once.

#include "types.hpp"
#include <cstddef>   // size_t

namespace CgsMemory
{
    namespace LowMemory
    {
        // Reserve + commit lnBytes of read/write memory whose whole extent lies below
        // 0x1_0000_0000. Returns NULL only if the OS cannot serve the request at all.
        //
        // If no low region is available the call still returns usable memory (from
        // anywhere) and logs loudly -- a high block is exactly the pointer-truncation
        // failure mode above, so it must be diagnosable rather than silent.
        void* Reserve(size_t lnBytes);

        // Release a block obtained from Reserve.
        void Release(void* lpBlock);

        // True when the last Reserve() satisfied the low-4GB requirement. Diagnostics.
        bool IsLowAddress(const void* lpBlock);
    }
}
