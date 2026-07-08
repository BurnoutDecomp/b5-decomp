#include "SDKs/D3D/D3DCaptureHashMap.h"

#include <cstddef> // offsetof (uncalled _CCaptureHashMapAssertLayout)

// ===========================================================================
// D3D::CCapture::HashMap -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// D3DCaptureHashMap.h for the layout and the asm offset map.
//
// Source-of-truth: X360 asm only (no DWARF, no reference source). All three
// bodies are unambiguous load/store/compare/call sequences reproduced faithfully
// with no added control flow and no inverted branches.
// ===========================================================================

namespace D3D
{

// --- @ 0x8295B0C0 -----------------------------------------------------------
// Construct an empty table: zero all three words of every bucket. The asm walks
// a cursor forward in 12-byte (3-word) steps, storing 0 to word[0]/word[1]/word[2]
// of each of the 64 buckets (loop counter 0x3F..0 inclusive).
CCapture::HashMap::HashMap()
{
    for (s32 i = 0; i < KI_BUCKET_COUNT; ++i)
    {
        maBuckets[i].mpData  = nullptr;
        maBuckets[i].muWord1 = 0;
        maBuckets[i].muWord2 = 0;
    }
}

// --- @ 0x8295BBE8 -----------------------------------------------------------
// Release the table: walk the buckets in REVERSE (the asm cursor starts at
// this+0x300 and steps back 12 bytes each iteration, counter 0x3F..0) and
// XMemFree() each bucket's non-null data pointer.
CCapture::HashMap::~HashMap()
{
    for (s32 i = KI_BUCKET_COUNT - 1; i >= 0; --i)
    {
        if (maBuckets[i].mpData != nullptr)
            XMemFree(maBuckets[i].mpData, KU_FREE_ATTRIBUTES);
    }
}

// --- @ 0x82959AE8 -----------------------------------------------------------
// Allocate uSize bytes from the raw XDK heap with the capture-hash allocation
// attributes. A pure tail-call to XMemAlloc; the asm reads no `this`, so this is
// a static helper.
void* CCapture::HashMap::HashMemAlloc(u32 uSize)
{
    return XMemAlloc(uSize, KU_ALLOC_ATTRIBUTES);
}

// Layout facts (uncalled). mpData widens 4->8 bytes on the LLP64 host, so only
// the pointer-invariant relationships are pinned: the data pointer heads the
// bucket, the two metadata words are contiguous 32-bit slots after it, and the
// table starts at offset 0 with 64 buckets.
void _CCaptureHashMapAssertLayout()
{
    static_assert(CCapture::HashMap::KI_BUCKET_COUNT == 64, "64 buckets (loop 0x3F..0)");
    static_assert(offsetof(CCapture::HashMap::Bucket, mpData) == 0,
                  "bucket data pointer heads the slot (word[0], the freed word)");
    static_assert(offsetof(CCapture::HashMap::Bucket, muWord2)
                  - offsetof(CCapture::HashMap::Bucket, muWord1) == 4,
                  "two contiguous 32-bit metadata words");
    static_assert(offsetof(CCapture::HashMap, maBuckets) == 0,
                  "bucket table starts at offset 0");
}

} // namespace D3D
