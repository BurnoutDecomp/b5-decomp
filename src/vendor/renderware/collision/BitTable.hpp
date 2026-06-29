#pragma once

#include "types.hpp"

// ===========================================================================
// rw::BitTable -- a flat packed bit grid used by the RenderWare collision /
// scene-management layer (CgsSceneManager::CullingGroupManager builds one as a
// "culling table").
//
// OWNING HOME for the single BitTable function the X360 binary defines:
//     rw::BitTable::Initialize  @ 0x82BA7F38
//
// No DWARF hints exist for this TU, so the LAYOUT below is
// reconstructed directly from the X360 PPC asm. Initialize takes a pointer to a
// caller-allocated backing store (passed as `_DWORD **a1`, i.e. &mpStorage) and
// the grid dimensions, writes a small header into that store, then zero-fills
// the bit words.
//
// The backing store layout (the words Initialize writes), as seen in the asm:
//     +0x00  muWidth        : a2 (column count)               stw r4,0(r11)
//     +0x04  muHeight       : a3 (row count)                  stw r5,4(r11)
//     +0x08  muWordCount    : (width*height + 31) >> 5         stw r10,8(r11)
//     +0x0C  maBits[]       : muWordCount 32-bit words, all zeroed
// so the bit grid needs ceil(width*height / 32) 32-bit words after the header.
// ===========================================================================

namespace rw
{

// Forward decl -- the full descriptor lives in rw/rwcore_structs.h. GetResourceDescriptor
// takes it by pointer, so a forward declaration suffices in this header.
struct BaseResourceDescriptor;

class BitTable
{
public:
    // The 12-byte header that prefixes the packed bit words in the backing store.
    struct Storage
    {
        u32 muWidth;        // +0x00  column count
        u32 muHeight;       // +0x04  row count
        u32 muWordCount;    // +0x08  number of 32-bit words in maBits
        u32 maBits[1];      // +0x0C  first of muWordCount packed bit words
    };

    // @ 0x82BA7F38 -- initialise the bit grid in *lppStorage to liWidth x
    // liHeight, zeroing every packed word. Returns the backing store pointer
    // (null when *lppStorage is null). X360 __fastcall: r3=&storage, r4=width,
    // r5=height.
    static Storage* Initialize(Storage** lppStorage, int liWidth, int liHeight);

    // Compute the backing-store memory requirement for a liWidth x liHeight grid:
    // the 12-byte header + ceil(width*height / 32) packed 32-bit words, 4-aligned.
    // The X360 SceneManager builds a 24x24 table -> 12 + 18*4 == 84 bytes (align 4),
    // which it feeds to the rw resource allocator before Initialize. Declared as the
    // descriptor producer the CullingGroupManager uses; the body lives in BitTable's
    // own TU (declared here for the SceneManager culling-table build).
    static void GetResourceDescriptor(BaseResourceDescriptor* lpDescriptor,
                                      int liWidth, int liHeight);
};

} // namespace rw
