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
    // ---- capture-stream I/O callbacks (function-pointer members word1..word3) --
    // Only the WRITE callback is called in this TU (WriteData/End): it takes a
    // byte blob + size and returns a status int -- signature attested by the asm
    // (mtctr/bctrl with r3=data, r4=size, result in r3). The alloc/free callbacks
    // are stored by Initialize but not invoked here; their signatures are the
    // natural alloc(size)->ptr / free(ptr) pair.
    typedef s32   (*TWriteFn)(const void* pData, s32 iSize);
    typedef void* (*TAllocFn)(u32 uSize);
    typedef void  (*TFreeFn)(void* pData);

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
            void* mpData;   // +0x00  heap block (HashMemAlloc): base of the
                            //        (pageAddress, pageFlags) entry array; freed in ~HashMap
            u32   muWord1;  // +0x04  live entry count (each entry = 2 words / 8 bytes)
            u32   muWord2;  // +0x08  entry capacity

            // @ 0x8295B028 (X360 name truncated to "D3D::CCaptu") -- grow the
            // (pageAddress, pageFlags) entry array to hold uCapacity entries and
            // write a signed status (<0 == failure) to *piStatus. The int it
            // returns just threads r3 and is not meaningfully consumed.
            //   BLOCKED BODY: 0x8295B028 has NO disassembly export -- it sits
            //   between D3D::GetTextureSizes @0x8295AF50 and HashMap::HashMap
            //   @0x8295B0C0 with no standalone dump, so the allocate/copy/free
            //   body cannot be reconstructed without fabrication. Declared here
            //   (signature attested by BOTH call sites -- AddEntry @0x8295BCAC and
            //   MarkUsedPages @0x8295BDF8 pass this=&bucket, r4=capacity,
            //   r5=&status) so AddEntry / MarkUsedPages compile; the body stays
            //   its own future TU.
            s32 Reserve(u32 uCapacity, s32* piStatus);

            // @ 0x8295BC70 -- ensure room for one more entry: when count+1 exceeds
            // the capacity, grow via Reserve(count+1, piStatus); then, if the
            // status is non-negative, commit the slot by bumping the live count.
            // Faithfully recovered from the X360 asm (0x8295BC70.json).
            s32 AddEntry(s32* piStatus);
        };

        Bucket maBuckets[KI_BUCKET_COUNT];  // +0x000  0x300 bytes on the X360

        friend void _CCaptureHashMapAssertLayout();
        // CCapture drives its own bucket table directly (End resets the counts,
        // SavePages walks the entries) -- attested by the X360 asm.
        friend class CCapture;
    };

    // ======================================================================
    // CCapture instance -- the outer GPU-capture object. Layout reconstructed
    // purely from the byte offsets the X360 asm loads/stores (no DWARF, no
    // reference source). word0 is the vtable pointer (the class has virtuals,
    // dispatched in the not-yet-homed PreSubmit/HandleType3Opcode); the three
    // callbacks sit at word1..word3; the 64-bucket HashMap occupies word4..word195
    // (0x10..0x30F); the trailing state words start at word196 (+0x310). Past
    // word0 the absolute byte offsets shift on an LLP64 host (4->8 pointer widening
    // + the wider HashMap), so nothing here is offset-asserted -- every member is
    // reached by name.
    // ======================================================================

    // -------- FAITHFULLY BODIED (recovered from X360 asm) --------

    // @ 0x8295A678 -- reset all capture state and install the write/alloc/free
    // callbacks (falling back to the D3D defaults when null). Returns `this`.
    CCapture* Initialize(void* pDevice, u32 uBase, TWriteFn pfnWrite,
                         TAllocFn pfnAlloc, TFreeFn pfnFree);

    // @ 0x8295A4C0 -- begin a capture: set the active flag and emit the opening
    // record. Fails (0x8007000E) when already in the error state.
    s32 Start();

    // @ 0x8295A748 -- end a capture: clear the active flag, emit the closing
    // record, and empty the page hash table (reset every bucket's count).
    s32 End();

    // @ 0x82959B30 -- serialise one record: emit the 20-byte header via the write
    // callback, then the payload (bounce-buffered through the XDK heap for type 8).
    s32 WriteData(s32 iType, const void* pPayload, s32 iSize,
                  s32 iParam0, s32 iParam1, s32 iParam2);

    // @ 0x82959C00 -- write out every dirty page tracked in the hash table and
    // roll the page-tag generation forward.
    s32 SavePages();

    // @ 0x8295BD50 -- mark each 4 KB page spanned by [uAddress, uAddress+uSize)
    // (clamped to the low 0x20000000 window) as used: find or insert the page in
    // its hash bucket (via Bucket::Reserve / Bucket::AddEntry) and OR the tracking
    // flags into the entry. When page 0 is first touched it also emits a type-3
    // record carrying uTag. The returned r3 thread is not consumed by any caller.
    s32 MarkUsedPages(u32 uAddress, u32 uSize, s32 uTag);

    // @ 0x829599E0 -- no-op hook (empty on the X360).
    void PostSubmit();

    // @ 0x8295BC48 -- no-op hook (empty on the X360).
    void ShadowShaderProgram();

private:
    void*    mpVTable;       // +0x000 word0   vtable pointer (virtuals not homed here)
    TWriteFn mpfnWrite;      // +0x004 word1   capture-stream write callback
    TAllocFn mpfnAlloc;      // +0x008 word2   allocation callback (stored; unused here)
    TFreeFn  mpfnFree;       // +0x00C word3   free callback (stored; unused here)
    HashMap  mHashMap;       // +0x010 word4   64-bucket used-page table (0x300 B on X360)
    u32      muWord196;      // +0x310 word196 record base value (Init=0)
    u32      muFlags;        // +0x314 word197 capture state / flag bits
    u32      muWord198;      // +0x318 word198 duplicate-address counter / current page tag
    u32      muWord199;      // +0x31C word199 last-saved page tag
    u32      muWord200;      // +0x320 word200
    u32      muWord201;      // +0x324 word201
    u32      muWord202;      // +0x328 word202
    u32      muWord203;      // +0x32C word203
    u32      muCurrentBase;  // +0x330 word204 current pushbuffer base address
    u32      muWord205;      // +0x334 word205
    void*    mpDevice;       // +0x338 word206 device / context pointer
    u32      muPendingDpc;   // +0x33C word207 pending PIX worker-DPC argument
    u32      muWord208;      // +0x340 word208 register mask A
    u32      muWord209;      // +0x344 word209 register mask B
    u32      muWord210;      // +0x348 word210 register value A
    u32      muWord211;      // +0x34C word211 register value B
    u32      muWord212;      // +0x350 word212 DPC callback function pointer
    u32      muWord213;      // +0x354 word213 DPC callback argument

    friend void _CCaptureHashMapAssertLayout();
};

} // namespace D3D
