#pragma once

#include "types.hpp"
#include <cstddef>   // size_t

// AttribSys generic allocation hooks (AttribSys v1.2.1.2). The vendor library routes
// every raw allocation/deallocation through these two hooks, which forward onto the
// engine's static AttribSys package allocator and keep the hash-map byte census.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Alloc @ 0x821F0478
//   Attrib::Free  @ 0x82804270
//
// `Attrib` is a vendor (AttribSys) library boundary, so its identifiers are preserved
// per the naming convention.
namespace Attrib
{
    // The allocation-accounting hook Alloc calls up-front (X360 own ledger function;
    // declared-not-defined here -- its body lives in its own TU). Records the request
    // against the AttribSys accounting subsystem. The second arg is a fixed 1 at the
    // Alloc call site (li r4,1).
    void AllocationAccounting(s32 liSize, int liKind);

    // @ 0x821F0478 -- the AttribSys library's generic allocation hook. Account the
    // request, short-circuit a zero-byte request to NULL, then Malloc the bytes from
    // the static AttribSys package allocator with flags 0. Takes only the size (the
    // Malloc flags argument is the literal 0 the asm stores, not a passed-through param).
    void* Alloc(size_t lnSize);

    // The tagged form the X360 emits at the vault/export-table sites (Alloc(size,
    // "Attrib::DataBlocks"/"Attrib::AssetIDs"/"Attrib::ExportPolicyPair", or a NULL
    // tag): same accounting + package-allocator Malloc; the tag is diagnostic only.
    // Body: attribsysallochooks.cpp.
    void* Alloc(size_t lnSize, const char* lpcTag);

    // @ 0x82804270 -- the AttribSys library's generic deallocation hook. Decrement the
    // shared live-byte census by lnSize, refresh the high-water mark, then (only when
    // both block and size are non-zero) hand the block back to the AttribSys package
    // allocator. Delegates to HashMapTablePolicy::FreeWithCensusIf so the census statics
    // are defined exactly once.
    void* Free(void* lpBlock, size_t lnSize, const char* lpcTag);
}
