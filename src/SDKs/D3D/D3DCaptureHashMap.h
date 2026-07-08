#pragma once

// ===========================================================================
// D3D::CCapture::HashMap -- the fixed 64-bucket hash table owned by the X360
// GPU-capture subsystem (CCapture / CFullCapture) inside BURNOUT_X360_ARTIST.XEX.
//
// This header is the canonical OWNING home for the nested HashMap TYPE and its
// three methods (bodied in D3DCaptureHashMap.cpp):
//
//     D3D::CCapture::HashMap::HashMap      @ 0x8295B0C0   (ctor)
//     D3D::CCapture::HashMap::~HashMap     @ 0x8295BBE8   (dtor)
//     D3D::CCapture::HashMap::HashMemAlloc @ 0x82959AE8   (static allocator)
//
// LAYOUT -- there is NO DWARF and NO reference source for this TU. The shape is
// reconstructed purely from the store/load offsets the ctor and dtor emit in the
// X360 asm: a flat array of 64 buckets, each three 32-bit words (0x300 bytes on
// the X360). The ctor walks a cursor forward in 12-byte (3-word) steps zeroing
// all three words of every bucket; the dtor walks the same buckets in REVERSE
// (cursor starts at this+0x300, steps back 12 bytes) and XMemFree()s the FIRST
// word of each -- the only word ever treated as a heap pointer. The other two
// words are zero-initialised metadata whose role is not attested by these three
// functions.
//
// CCapture is the outer class (its own methods -- the vector deleting destructor,
// AllocGpuCapture, etc. -- are not yet homed); only the shell needed to nest
// HashMap is declared here and should be EXTENDED, not re-forked, when CCapture
// is reconstructed.
//
// `D3D` is the X360 graphics-SDK boundary, so its identifiers (CCapture, HashMap,
// HashMemAlloc) are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"

namespace D3D
{

// ---- raw Xbox 360 XDK heap API (platform externs) --------------------------
// HashMemAlloc tail-calls XMemAlloc(SIZE_T,DWORD)->LPVOID and the dtor calls
// XMemFree(LPVOID,DWORD) directly (bl XMemAlloc / bl XMemFree; r3=size/pAddress,
// r4=attributes) -- i.e. the raw platform API. Declared here so this TU stands
// alone (same externs as the rwmovie codecs use).
extern void* XMemAlloc(u32 uSize, u32 uAttributes);
extern void  XMemFree(void* pAddress, u32 uAttributes);

class CCapture
{
public:
    // Fixed-size hash table: 64 buckets, each a 3-word slot.
    class HashMap
    {
    public:
        // @ 0x8295B0C0 -- zero every word of all 64 buckets.
        HashMap();

        // @ 0x8295BBE8 -- reverse-walk the buckets and XMemFree() each bucket's
        // non-null data pointer.
        ~HashMap();

        // @ 0x82959AE8 -- allocate uSize bytes from the XDK heap with the
        // capture-hash allocation attributes. Static: the asm reads no `this`
        // (r3 carries the size straight through to XMemAlloc).
        static void* HashMemAlloc(u32 uSize);

    private:
        // XMemAlloc attribute word for a bucket allocation (lis 0x6480 -> 0x64800000).
        static const u32 KU_ALLOC_ATTRIBUTES = 0x64800000u;
        // XMemFree attribute word used when releasing a bucket (lis 0x2480 -> 0x24800000).
        static const u32 KU_FREE_ATTRIBUTES  = 0x24800000u;

        // Bucket count -- ctor/dtor loop counter runs 0x3F..0 inclusive -> 64.
        static const s32 KI_BUCKET_COUNT = 64;

        // One hash bucket: 3 contiguous 32-bit words on the X360 (12 bytes there;
        // wider on the LLP64 host once mpData widens to 8 bytes -- accessed by
        // name, not byte offset).
        struct Bucket
        {
            void* mpData;   // +0x00  heap block (HashMemAlloc); freed in ~HashMap
            u32   muWord1;  // +0x04  zero-initialised metadata (role not attested)
            u32   muWord2;  // +0x08  zero-initialised metadata (role not attested)
        };

        Bucket maBuckets[KI_BUCKET_COUNT];  // +0x000  0x300 bytes on the X360

        friend void _CCaptureHashMapAssertLayout();
    };
};

} // namespace D3D
