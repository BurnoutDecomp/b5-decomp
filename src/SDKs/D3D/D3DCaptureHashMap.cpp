#include "SDKs/D3D/D3DCaptureHashMap.h"

#include <cstddef> // offsetof (uncalled _CCaptureHashMapAssertLayout), size_t
#include <cstdint> // uintptr_t (X360 CPU-address -> host pointer for the page payload)
#include <cstring> // memcpy / memset (WriteData bounce buffer, End record)

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

// --- @ 0x8295BC70 -----------------------------------------------------------
// Ensure this bucket has room for one more (page, flags) entry, then commit it.
// If count+1 exceeds the capacity the entry array is grown via Reserve(count+1)
// (which writes the status word); if the status is non-negative the live count
// is incremented. The returned value merely threads r3 (the bucket pointer) and
// is not consumed. Reproduced exactly from the X360 asm.
s32 CCapture::HashMap::Bucket::AddEntry(s32* piStatus)
{
    s32 iResult = 0; // r3 thread (bucket ptr on the X360); not consumed

    const u32 uNeeded = muWord1 + 1;      // count + 1
    if (uNeeded > muWord2)                 // exceeds capacity -> grow (unsigned)
        iResult = Reserve(uNeeded, piStatus);

    if (*piStatus >= 0)                    // grow succeeded (or was unnecessary)
        ++muWord1;                         // commit the new entry

    return iResult;
}

// ===========================================================================
// D3D::CCapture -- the outer GPU-capture object (8 of its 11 X360 functions
// homed here). Source-of-truth: X360 asm only (no DWARF, no reference source).
//
// MarkUsedPages @ 0x8295BD50 is now homed (below): its two HashMap helpers were
// resolved -- Bucket::AddEntry (@ 0x8295BC70) is bodied faithfully from asm, and
// Bucket::Reserve (@ 0x8295B028, the truncated "D3D::CCaptu" grow) is DECLARED
// from its attested signature but left unbodied (0x8295B028 has no disassembly
// export, so its allocate/copy/free body would have to be fabricated -- it is
// its own future TU). Under the per-TU compile gate the declaration suffices.
//
// The remaining three CCapture functions stay BLOCKED and unbodied:
//   * ParseRegisters      @ 0x82959948 -- indexes a 2-entry rodata table
//     (unk_8210626C: {registerOffset, targetOffset} pairs) whose values are
//     un-exported X360 rodata; reconstructing them would fabricate constants.
//   * HandleType3Opcode   @ 0x82959CE8 -- dispatches on two rodata pointer tables
//     (unk_82103040 / unk_82103020) AND fans out through the CCapture vtable
//     (word0), whose virtuals are set by the un-homed ctor; both are un-recoverable
//     here (rodata + phantom vtable slots).
//   * PreSubmit           @ 0x8295A148 -- drives six distinct CCapture vtable slots
//     (**this, this->vt[4/8/12/24/32]); the virtual method set is not homed, so a
//     faithful body would fabricate vtable slots.
// ===========================================================================

// ---- D3D graphics-SDK free functions used by CCapture (bodies are own TUs) --
extern u32 GetCPUAddress(u32 uGpuAddress);
extern s32 FlushCachedMemory(u32 uStartPhys, u32 uEndPhys, s32 iFlags);

// ---- default capture callbacks (installed by Initialize when the caller passes
// null; their addresses are the lis/addi symbols D3D__Capture{Write,Alloc,Free}).
extern s32   CaptureWrite(const void* pData, s32 iSize);
extern void* CaptureAlloc(u32 uSize);
extern void  CaptureFree(void* pData);

// --- @ 0x8295A678 -----------------------------------------------------------
// Reset every capture-state word to its initial value and install the callback
// trio (write/alloc/free), each defaulting to the D3D built-in when the caller
// passes null. The flag word keeps only its low 26 bits (clrlwi ...,6). Returns
// `this` (r3 threaded straight through).
CCapture* CCapture::Initialize(void* pDevice, u32 uBase, TWriteFn pfnWrite,
                               TAllocFn pfnAlloc, TFreeFn pfnFree)
{
    const u32 uFlags = muFlags;         // read before it is masked below

    muCurrentBase = uBase;              // result[204] = a3
    mpDevice      = pDevice;            // result[206] = a2
    muWord196     = 0;
    muWord198     = 0;
    muFlags       = uFlags & 0x03FFFFFFu; // result[197] = v8 & 0x3FFFFFF
    muWord199     = 0;
    muWord200     = 0;
    muWord201     = 0;
    muWord202     = 0;
    muWord203     = 0;
    muWord208     = 0;
    muWord209     = 0;
    muWord210     = 0;
    muWord211     = 0;
    muWord212     = 0;
    muWord213     = 0;
    muWord205     = 0;
    muPendingDpc  = 0;                  // result[207]

    mpfnWrite = pfnWrite ? pfnWrite : &CaptureWrite;
    mpfnAlloc = pfnAlloc ? pfnAlloc : &CaptureAlloc;
    mpfnFree  = pfnFree  ? pfnFree  : &CaptureFree;
    return this;
}

// --- @ 0x8295A4C0 -----------------------------------------------------------
// Begin a capture. When the object is not already flagged as errored (flag word
// sign bit clear) set the "active" bit (0x10000000), emit the opening record,
// and seed the run-length bookkeeping. Otherwise set the error bit and fail.
s32 CCapture::Start()
{
    const u32 uFlags = muFlags;
    if (static_cast<s32>(uFlags) >= 0)
    {
        const u32 uBase = muCurrentBase;
        muFlags = uFlags | 0x10000000u;

        u32 uRecord = 6534;            // 0x1986 -- opening record payload (one word)
        WriteData(1, &uRecord, 4, 0x0ABAD0BA, 0x0ABAD0BA, static_cast<s32>(uBase));

        muWord202 = 0xFFFFFFFFu;       // -1
        muWord200 = 0;
        muWord201 = 0x800;             // 2048
        muWord203 = 0xFFFFFFFFu;       // -1
        return 0;
    }

    muFlags = uFlags | 0x80000000u;
    return static_cast<s32>(0x8007000Eu); // -2147024882
}

// --- @ 0x8295A748 -----------------------------------------------------------
// End a capture. Fails (0x80004005) when the active bit is not set. Otherwise
// clear the active bit, emit the closing 20-byte record (unless the no-write bit
// 0x20000000 is set), then empty the page table by zeroing every bucket's live
// count. A final re-read of the flag word turns a set error/no-write bit into
// the 0x8007000E failure code.
s32 CCapture::End()
{
    const u32 uFlags = muFlags;
    if ((uFlags & 0x10000000u) == 0)
        return static_cast<s32>(0x80004005u); // -2147467259

    const u32 uCleared = uFlags & 0xEFFFFFFFu;
    muFlags = uCleared;

    u32 aRecord[5];
    memset(aRecord, 0, 20);
    if ((uCleared & 0x20000000u) == 0)
        mpfnWrite(aRecord, 20);

    for (s32 i = 0; i < HashMap::KI_BUCKET_COUNT; ++i)
        mHashMap.maBuckets[i].muWord1 = 0; // reset each bucket's live entry count

    const u32 uFlags2 = muFlags;
    if (static_cast<s32>(uFlags2) < 0 || (uFlags2 & 0x20000000u) != 0)
        return static_cast<s32>(0x8007000Eu); // -2147024882
    return 0;
}

// --- @ 0x82959B30 -----------------------------------------------------------
// Serialise one record. The 20-byte header {type,size,param0,param1,param2} is
// pushed through the write callback, then (for a non-zero payload) the payload
// itself: type 8 is bounce-buffered through a temporary XDK heap block, every
// other type is written straight from the caller's buffer. The whole body is
// skipped when the no-write bit (0x20000000) is set. The return value mirrors r3
// and is not consumed by any caller.
s32 CCapture::WriteData(s32 iType, const void* pPayload, s32 iSize,
                        s32 iParam0, s32 iParam1, s32 iParam2)
{
    s32 iResult = 0;
    const u32 uFlags = muFlags;

    u32 aRecord[5];
    aRecord[0] = static_cast<u32>(iType);
    aRecord[1] = static_cast<u32>(iSize);
    aRecord[2] = static_cast<u32>(iParam0);
    aRecord[3] = static_cast<u32>(iParam1);
    aRecord[4] = static_cast<u32>(iParam2);

    if ((uFlags & 0x20000000u) == 0)
    {
        iResult = mpfnWrite(aRecord, 20);
        if (iSize != 0)
        {
            if (iType == 8)
            {
                // lis r4,0x2480 -> 0x24800000 alloc/free attributes.
                void* pTemp = XMemAlloc(static_cast<u32>(iSize), 0x24800000u);
                memcpy(pTemp, pPayload, static_cast<size_t>(iSize));
                mpfnWrite(pTemp, iSize);
                XMemFree(pTemp, 0x24800000u);
            }
            else
            {
                iResult = mpfnWrite(pPayload, iSize);
            }
        }
    }
    return iResult;
}

// --- @ 0x82959C00 -----------------------------------------------------------
// Flush every dirty page recorded in the hash table. Walk all 64 buckets in
// order; within each bucket scan its (address, flags) entries back-to-front. An
// entry flagged 0x400000 has its flags rewritten to carry the current page tag
// (word198) plus 0x100000; an entry flagged 0x200000 is a dirty page, so its
// 4 KB is cache-flushed, written out as a type-5 record, and flushed again.
// Finally the last-saved page tag (word199) is advanced to the current tag.
// The return value mirrors r3 and is not consumed by any caller.
s32 CCapture::SavePages()
{
    s32 iResult = 0;
    for (s32 iBucket = 0; iBucket < HashMap::KI_BUCKET_COUNT; ++iBucket)
    {
        HashMap::Bucket& rBucket = mHashMap.maBuckets[iBucket];
        u32* pBase = static_cast<u32*>(rBucket.mpData);
        u32* pCur  = pBase + 2u * rBucket.muWord1; // base + 8*count bytes (2 words/entry)
        while (pCur > pBase)
        {
            pCur -= 2; // step back one 8-byte (page, flags) entry
            const u32 uEntryFlags = pCur[1];

            if ((uEntryFlags & 0x00400000u) != 0)
            {
                // Two stores, exactly as the asm emits them.
                pCur[1] = uEntryFlags & 0xFF900000u;
                pCur[1] = muWord198 | (uEntryFlags & 0xFF900000u) | 0x00100000u;
            }

            if ((uEntryFlags & 0x00200000u) != 0)
            {
                const u32 uGpuAddr = pCur[0];
                const u32 uCpuAddr = GetCPUAddress(pCur[0]);
                FlushCachedMemory(uCpuAddr, uCpuAddr + 4096, 1);
                WriteData(5,
                          reinterpret_cast<const void*>(static_cast<uintptr_t>(uCpuAddr)),
                          4096,
                          static_cast<s32>(uGpuAddr),
                          static_cast<s32>(uEntryFlags & 0x000FFFFFu),
                          static_cast<s32>(muWord199));
                iResult = FlushCachedMemory(uCpuAddr, uCpuAddr + 4096, 1);
            }
        }
    }

    muWord199 = muWord198; // roll the last-saved tag forward
    return iResult;
}

// --- @ 0x8295BD50 -----------------------------------------------------------
// Mark every 4 KB page spanned by [uAddress, uAddress+uSize) as used. The range
// is first gated (biased-range floor, non-zero addr/size, addr below the
// 0x20000000 window) and its length clamped to the window. For each page: locate
// its hash bucket (index = (page >> 12) & 0x3F); if the bucket already has
// capacity, scan its (page, flags) entries back-to-front for a match; otherwise
// seed the bucket (Bucket::Reserve(0x10)). A miss appends a fresh zero-flag entry
// via Bucket::AddEntry. The entry's flags then get bit 0x400000 set; page 0 is
// special -- on its first touch a one-shot type-3 record carrying uTag is emitted
// (guarded by flag bit 0x40000000) and the flags word is NOT written back;
// otherwise a not-yet-dirty entry (bits 0x300000 clear) is marked dirty (0x200000)
// and stored. A failed Reserve/AddEntry sets the object's error bit (0x80000000)
// and aborts. The returned r3 thread is not consumed by any caller (xrefs_to
// empty). Reproduced from the X360 asm (0x8295BD50.json).
s32 CCapture::MarkUsedPages(u32 uAddress, u32 uSize, s32 uTag)
{
    s32 iResult = 0; // r3 thread; not consumed by any caller

    if (uAddress + 0x3FF00000u > 0x07EFFFFFu   // biased-range floor (u32 wrap)
        && uAddress != 0 && uSize != 0 && uAddress < 0x20000000u)
    {
        u32 uLen = uSize;
        if (uAddress + uLen > 0x20000000u)
            uLen = 0x20000000u - uAddress;         // clamp to the 0x20000000 window

        const u32 uEnd  = uAddress + uLen;         // exclusive end
        u32       uPage = uAddress & 0xFFFFF000u;  // first 4 KB page (clrrwi ...,12)

        if (uPage < uEnd)                          // at least one page spanned
        {
            do
            {
                HashMap::Bucket& rBucket =
                    mHashMap.maBuckets[(uPage >> 12) & 0x3Fu];
                u32* pEntry = nullptr;
                bool bFound = false;

                // Single status slot shared by Reserve and AddEntry, mirroring the
                // one X360 stack word (v16): a just-seeded bucket carries the
                // seed's success into AddEntry. A failing Reserve breaks the loop
                // before the slot could ever be re-read, so a per-iteration >=0
                // default is behaviourally identical to the shared X360 slot.
                s32 iStatus = 0;

                if (rBucket.muWord2 != 0)          // capacity != 0 -> search
                {
                    u32* pBase = static_cast<u32*>(rBucket.mpData);
                    pEntry = pBase + 2u * rBucket.muWord1;  // one past the last entry
                    while (pEntry > pBase)
                    {
                        pEntry -= 2;               // step back one 8-byte entry
                        if (pEntry[0] == uPage)    // matching page address
                        {
                            bFound = true;
                            break;
                        }
                    }
                }
                else
                {
                    iResult = rBucket.Reserve(0x10u, &iStatus);  // seed the bucket
                    if (iStatus < 0)
                    {
                        muFlags |= 0x80000000u;
                        break;
                    }
                }

                if (!bFound)
                {
                    iResult = rBucket.AddEntry(&iStatus);        // reserve+commit
                    if (iStatus < 0)
                    {
                        muFlags |= 0x80000000u;
                        break;
                    }
                    u32* pBase = static_cast<u32*>(rBucket.mpData);
                    pEntry = pBase + 2u * rBucket.muWord1 - 2;   // the new last slot
                    pEntry[0] = uPage;             // entry.page
                    pEntry[1] = 0;                 // entry.flags
                }

                u32* pFlags = &pEntry[1];
                if (pFlags == nullptr)             // degenerate (null data) -> fail
                {
                    muFlags |= 0x80000000u;
                    break;
                }

                u32  uEntryFlags = *pFlags;
                bool bStoreFlags = true;
                if ((uEntryFlags & 0x00400000u) == 0)
                {
                    uEntryFlags |= 0x00400000u;
                    if (uPage == 0)
                    {
                        const u32 uFlags = muFlags;
                        if ((uFlags & 0x40000000u) == 0)
                        {
                            u32 aRecord[3];
                            aRecord[0] = muWord196;               // record base
                            aRecord[1] = static_cast<u32>(uTag);
                            aRecord[2] = 0;
                            muFlags = uFlags | 0x40000000u;       // one-shot latch
                            iResult = WriteData(3, aRecord, 12, 0, 0, 0);
                        }
                        bStoreFlags = false;       // the page-0 path never stores back
                    }
                    else if ((uEntryFlags & 0x00300000u) == 0)
                    {
                        uEntryFlags |= 0x00200000u; // mark dirty
                    }
                }

                if (bStoreFlags)
                    *pFlags = uEntryFlags;

                uPage += 4096;
            }
            while (uPage < uEnd);
        }
    }

    return iResult;
}

// --- @ 0x829599E0 -----------------------------------------------------------
// Empty on the X360 (prologue/epilogue only).
void CCapture::PostSubmit()
{
}

// --- @ 0x8295BC48 -----------------------------------------------------------
// Empty on the X360 (prologue/epilogue only).
void CCapture::ShadowShaderProgram()
{
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
