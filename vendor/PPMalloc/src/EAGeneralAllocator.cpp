#include "ppmalloc/EAGeneralAllocator.h"
#include <cstring>   // memset / memcpy
#include <cstddef>   // offsetof
#include <cstdint>   // uintptr_t
#include <cstdio>    // snprintf: the original bounded trace fields
#include "types.hpp" // u32

#include <windows.h> // CRITICAL_SECTION for PPMAutoMutex

// EA::Allocator::GeneralAllocator - PPMalloc dlmalloc-style heap allocator. This TU lands the
// FOUNDATION: the ctor + Init (the empty-allocator setup) + the default assertion/trace sinks +
// the dtor. The allocation engine (AddCore / MallocInternal / MallocAlignedInternal / FreeInternal
// / Realloc / Calloc / bin management) is reconstructed in follow-on passes on top of this.
//
// IMPORTANT: the X360 (PPC, 32-bit ptr), the Spore x86 PDB, and our PC x64 target all lay this
// class out DIFFERENTLY (vptr + pointer-width + padding differences), so the bodies are
// reconstructed by NAMED MEMBER from the X360 *behaviour* + the recognizable PPMalloc default
// constants, NOT by transcribing raw byte offsets. Constants taken from the X360 ctor 0x82B4FF58 /
// Init 0x82B4FA30 (hex values are unambiguous); a few fields the X360 sets that don't map cleanly
// to a named member yet are left zero-initialised and flagged [VERIFY] for a later pass.
namespace EA
{
namespace Allocator
{
    // Uncalled layout pin. The X360 (PPC, 32-bit ptr) and the PC x64 gate host lay this class out
    // differently once pointer-width widens 4B->8B, so only POINTER-INVARIANT facts are asserted as
    // absolute byte offsets (the pointer-free leading scalars of each struct + the X360 chunk-header
    // encoding the engine reads by name). Member ORDER invariants are checked relationally. This is
    // never called; it exists purely to trip the build if the recovered layout drifts.
    void GeneralAllocator::_AssertLayout()
    {
        // Chunk: the dlmalloc boundary-tag header. mnPriorSize / mnSize are the two u32 size words
        // the X360 engine masks (size & 0x7FFFFFF8, low bits = in-use/mmapped flags); they precede
        // the fd/bk pointers and so are pointer-free + offset-stable across X360/x64.
        static_assert(offsetof(GeneralAllocator::Chunk, mnPriorSize) == 0, "Chunk.mnPriorSize must be first");
        static_assert(offsetof(GeneralAllocator::Chunk, mnSize)      == 4, "Chunk.mnSize follows mnPriorSize");
        // CoreBlock: mpCore is the first member (a pointer); the bool flag run + the X360
        // prev/next list slots come after it, so only the relative ordering is invariant on LLP64.
        static_assert(offsetof(GeneralAllocator::CoreBlock, mpCore)          == 0,
                      "CoreBlock.mpCore must be first");
        static_assert(offsetof(GeneralAllocator::CoreBlock, mbMMappedMemory) <
                      offsetof(GeneralAllocator::CoreBlock, mpPrevCoreBlock),
                      "CoreBlock flag run precedes the prev/next list slots");
        static_assert(offsetof(GeneralAllocator::CoreBlock, mpPrevCoreBlock) <
                      offsetof(GeneralAllocator::CoreBlock, mpNextCoreBlock),
                      "CoreBlock mpPrevCoreBlock precedes mpNextCoreBlock (UnlinkCoreBlock splice order)");
        // Allocator: mbInitialized is the FIRST member (offset 0, non-polymorphic class, no vptr --
        // asm ctor @0x82B4FF58 stores it as the first byte); mnMaxFastBinChunkSize follows it, then
        // the fast-bin array - the ctor/Init store order the engine relies on.
        static_assert(offsetof(GeneralAllocator, mbInitialized) == 0,
                      "mbInitialized is the first member (offset 0, no vptr)");
        static_assert(offsetof(GeneralAllocator, mbInitialized) <
                      offsetof(GeneralAllocator, mnMaxFastBinChunkSize),
                      "mbInitialized precedes mnMaxFastBinChunkSize");
        static_assert(offsetof(GeneralAllocator, mnMaxFastBinChunkSize) <
                      offsetof(GeneralAllocator, mpFastBinArray),
                      "mnMaxFastBinChunkSize precedes mpFastBinArray");
        static_assert(offsetof(GeneralAllocator, mpFastBinArray) <
                      offsetof(GeneralAllocator, mpBinArray),
                      "fast bins precede the regular bins");
        static_assert(offsetof(GeneralAllocator, mpBinArray) <
                      offsetof(GeneralAllocator, mpTopChunk),
                      "regular bins precede the top chunk");

        // DecFIGS BlockInfo declares native size_t fields; copying the console's u32
        // size slots would move the callback's later members on x64.
        static_assert(sizeof(BlockInfo::mnBlockSize) == sizeof(size_t),
                      "BlockInfo.mnBlockSize is size_t");
        static_assert(sizeof(BlockInfo::mnDataSize) == sizeof(size_t),
                      "BlockInfo.mnDataSize is size_t");
        static_assert(offsetof(BlockInfo, mpCore) < offsetof(BlockInfo, mpBlock) &&
                      offsetof(BlockInfo, mpBlock) < offsetof(BlockInfo, mnBlockSize) &&
                      offsetof(BlockInfo, mnBlockSize) < offsetof(BlockInfo, mpData) &&
                      offsetof(BlockInfo, mpData) < offsetof(BlockInfo, mnDataSize) &&
                      offsetof(BlockInfo, mnDataSize) < offsetof(BlockInfo, mBlockType) &&
                      offsetof(BlockInfo, mBlockType) < offsetof(BlockInfo, mbMemoryMapped),
                      "BlockInfo callback member order follows DecFIGS");
        static_assert(sizeof(Snapshot) <= sizeof(SnapshotImage),
                      "SnapshotImage holds a native dynamic report context");
    }

    // Existing assertion sink; the trace producer does not route through it.
    void GeneralAllocator::AssertionFailureFunctionDefault(const char* /*pMessage*/, void* /*pContext*/) {}

    // ARTIST82B4DC98 is a tail call to OutputDebugStringA with the record unchanged.
    void GeneralAllocator::TraceFunctionDefault(const char* pMessage, void* /*pContext*/)
    {
        OutputDebugStringA(pMessage);
    }

    // bin_at(i) - see the header. i is 1-based (Init self-links bins 1..127).
    GeneralAllocator::Chunk* GeneralAllocator::GetBin(int i)
    {
        return reinterpret_cast<Chunk*>(
            reinterpret_cast<char*>(&mpBinArray[2 * (i - 1)]) - offsetof(Chunk, mpPrevChunk));
    }

    // @ 0x82B4FF58 - zero the state, install the PPMalloc defaults, then Init() an empty heap.
    // (The module ctors call this as GeneralAllocator(0,0,1,0,0,0) == default-construct an empty
    // allocator; core memory is adopted later via AddCore from HeapMalloc::Construct.)
    GeneralAllocator::GeneralAllocator()
    {
        mbInitialized              = false;
        mnMaxFastBinChunkSize      = 0;
        memset(mpFastBinArray, 0, sizeof(mpFastBinArray));
        memset(mpBinArray,     0, sizeof(mpBinArray));
        memset(mBinBitmap,     0, sizeof(mBinBitmap));
        mpTopChunk                 = 0;
        mpLastRemainderChunk       = 0;
        memset(&mHeadCoreBlock, 0, sizeof(mHeadCoreBlock));
        mpHighFence                = 0;
        mbHighFenceInternallyDisabled = false;
        mbSystemAllocEnabled       = true;      // X360 ctor sets this enabled
        mnCheckChunkReentrancyCount = 0;
        mcTraceFieldDelimiter      = 9;         // '\t'
        mcTraceRecordDelimiter     = 10;        // '\n'
        mAutoHeapValidationLevel   = kHeapValidationLevelNone;
        mnAutoHeapValidationFrequency = 0;
        mnAutoHeapValidationEventCount = 0;
        mbHeapValidationActive     = false;
        mnMMapCount                = 0;
        mnMMapMallocTotal          = 0;
        mnMMapMaxAllowed           = 0;
        mnMMapThreshold            = 0;
        mbMMapTopDown              = false;
        memset(&mHeadMMapChunk, 0, sizeof(mHeadMMapChunk));
        mpHookFunction             = 0;
        mpHookFunctionContext      = 0;
        mpMallocFailureFunction    = 0;
        mpMallocFailureFunctionContext = 0;
        mnMaxMallocFailureCount    = 0;
        mpAssertionFailureFunction = &AssertionFailureFunctionDefault;
        mpAssertionFailureFunctionContext = this;
        mpTraceFunction            = &TraceFunctionDefault;
        mpTraceFunctionContext     = this;
        mnTrimThreshold            = 0;
        mnTopPad                   = 0;
        mpInitialTopChunk          = 0;
        mnPageSize                 = 4096;
        mnNewCoreSize              = 0x01000000;  // 16 MB (X360 ctor)
        mnCoreIncrementSize        = 0x00400000;  // 4 MB  (X360 ctor)
        mbTraceInternalMemory      = false;
        mpMutex                    = 0;
        memset(mpMutexData, 0, sizeof(mpMutexData));
        mnFillFree                 = 0xDD;        // X360 ctor fill bytes
        mnFillDelayedFree          = 0xDE;
        mnFillNew                  = 0xCD;
        mnFillGuard                = 0xAB;
        mnFillUnusedCore           = 0xFE;
        mpName                     = 0;
        mNotifyInitState           = 0;

        Init();
    }

    // @ 0x82B4FA30 - one-time empty-heap bring-up: self-link the 127 regular bins into empty
    // circular lists, self-link the core-block and mmap-chunk list heads, and set the dlmalloc
    // size thresholds. (Called with no core here, so no AddCore.)
    bool GeneralAllocator::Init()
    {
        if (!mbInitialized)
        {
            mbInitialized = true;

            mnMaxFastBinChunkSize = 64;   // X360 Init: *(a1+4)=64
            memset(mpFastBinArray, 0, sizeof(mpFastBinArray));
            memset(mpBinArray,     0, sizeof(mpBinArray));

            // self-link the 127 regular bins (each an empty circular free list)
            for (int i = 1; i <= 127; ++i)
            {
                Chunk* lpBin = GetBin(i);
                lpBin->mpPrevChunk = lpBin;
                lpBin->mpNextChunk = lpBin;
            }

            // core-block list head -> empty circular list
            mHeadCoreBlock.mpPrevCoreBlock = &mHeadCoreBlock;
            mHeadCoreBlock.mpNextCoreBlock = &mHeadCoreBlock;

            // mmapped-chunk list head -> empty circular list
            mHeadMMapChunk.mpPrevChunk = &mHeadMMapChunk;
            mHeadMMapChunk.mpNextChunk = &mHeadMMapChunk;

            // dlmalloc size thresholds (X360 Init hex constants)
            mnMMapMaxAllowed = 0x00010000;   // 64 KB
            mnMMapThreshold  = 0x00020000;   // 128 KB
            mnTrimThreshold  = 0x00040000;   // 256 KB
            mnTopPad         = 0x00010000;   // 64 KB
            mnPageSize       = 4096;
            // [VERIFY] the X360 also seeds an initial top sentinel + a flags word here that don't
            // map cleanly to a named member across the x86/PPC/x64 layouts; left for the pass that
            // reconstructs MallocInternal (which is what consumes them).
        }
        return true;
    }

    // ====================================================================================
    // [PC-LEAF ENGINE] AddCore / Malloc / MallocAligned / Free / Calloc / Realloc.
    //
    // The faithful GeneralAllocator LAYOUT + ctor/Init above are kept; only the allocation
    // ENGINE is a PC leaf. The real X360 bodies (MallocInternal 0x82B502C0, FreeInternal
    // 0x82B4F5D8, AddCoreInternal 0x82B4EF08, the fast/regular bins) implement dlmalloc with
    // raw chunk-header bit packing whose bit-exact behaviour assumes the X360/PPC pointer width
    // and chunk encoding; porting that to x64 is high-risk and behaviourally invisible (any
    // correct allocator over the same fixed buffer yields identical gameplay). So this is a
    // clean boundary-tag free-list allocator over the adopted core buffer: it preserves the
    // fixed-memory-budget semantics (allocations come out of [mpCore, mpCore+mnSize)) and
    // 16-byte alignment, supports Free with full coalescing, but is NOT the bit-exact dlmalloc.
    // (User-approved 2026-06-20.) The faithful bin/chunk members above are vestigial for the
    // PC engine; reinstate the real engine here if bit-exact heap fidelity is ever needed.
    // ====================================================================================
    namespace
    {
        const u32 KU_PC_ALIGN  = 16;                 // base alignment / header size
        const u32 KU_PC_MAGIC  = 0x504C4B42u;        // 'PLKB' - used/free block tag

        struct PcBlock                               // 16-byte header; data follows at +16
        {
            u32 mnSize;     // total block size incl this header (multiple of 16)
            u32 mbFree;     // 1 = free
            u32 mnMagic;    // KU_PC_MAGIC
            u32 mnPad;      // report-owned allocations use bit 4 as the internal marker
        };

        static_assert(sizeof(PcBlock) == KU_PC_ALIGN, "PC block header matches the allocation engine");

        enum
        {
            kReportAllocated = 2,
            kReportFree = 4,
            kReportCore = 8,
            kReportInternal = 16,
            kPcBlockInternal = 4,
            kSnapshotMagic = 0x534E4150,
            kSnapshotSpareRecords = 4
        };

        // FLAG PC-platform leaf: the approved PC engine uses PcBlock, not ARTIST's
        // dlmalloc Chunk/fence/core lists. Read its actual boundary tags without
        // changing allocation/free behavior or maintaining a second allocation list.
        bool PcBlockIsValid(const PcBlock* pBlock, const char* pCoreEnd)
        {
            const char* const pBlockAddress = reinterpret_cast<const char*>(pBlock);
            const size_t nRemaining = static_cast<size_t>(pCoreEnd - pBlockAddress);
            return nRemaining >= sizeof(PcBlock) && pBlock->mnMagic == KU_PC_MAGIC &&
                   pBlock->mnSize >= sizeof(PcBlock) &&
                   (pBlock->mnSize & (KU_PC_ALIGN - 1)) == 0 &&
                   pBlock->mnSize <= nRemaining;
        }

        bool PcBlockIsInternal(const PcBlock* pBlock)
        {
            return !pBlock->mbFree && (pBlock->mnPad & kPcBlockInternal) != 0;
        }

        bool PcBlockMatchesReport(const PcBlock* pBlock, int nBlockTypeFlags)
        {
            // ARTIST ChunkMatchesBlockType @82B4D9C0 suppresses internal chunks
            // unless bit 16 is included; allocation/free selection still uses bits 2/4.
            return (!PcBlockIsInternal(pBlock) || (nBlockTypeFlags & kReportInternal)) &&
                   (nBlockTypeFlags & (pBlock->mbFree ? kReportFree : kReportAllocated));
        }

        void PcGetBlockInfo(const PcBlock* pBlock, GeneralAllocator::BlockInfo* pBlockInfo,
                            const void* pCore)
        {
            pBlockInfo->mpCore = pCore;
            pBlockInfo->mpBlock = pBlock;
            pBlockInfo->mnBlockSize = pBlock->mnSize;
            pBlockInfo->mpData = const_cast<char*>(reinterpret_cast<const char*>(pBlock)) + sizeof(PcBlock);
            pBlockInfo->mnDataSize = pBlock->mnSize - sizeof(PcBlock);
            pBlockInfo->mBlockType = static_cast<char>(pBlock->mbFree ? kReportFree :
                (PcBlockIsInternal(pBlock) ? kReportInternal : kReportAllocated));
            pBlockInfo->mbMemoryMapped = false;
        }

        bool PcCaptureReport(const GeneralAllocator::CoreBlock& coreBlock, int nBlockTypeFlags,
                             GeneralAllocator::BlockInfo* pBlockInfo, size_t nCapacity,
                             size_t& nBlockInfoCount)
        {
            nBlockInfoCount = 0;
            if (!coreBlock.mpCore || !coreBlock.mnSize)
                return true;

            // Copied ARTIST snapshots emit the core record before its chunks.
            // Dynamic ReportNext emits chunks only, even when bit 8 was requested.
            if (nBlockTypeFlags & kReportCore)
            {
                if (pBlockInfo)
                {
                    if (!nCapacity)
                        return false;
                    pBlockInfo->mpCore = coreBlock.mpCore;
                    pBlockInfo->mpBlock = coreBlock.mpCore;
                    pBlockInfo->mnBlockSize = coreBlock.mnSize;
                    pBlockInfo->mpData = 0;
                    pBlockInfo->mnDataSize = 0;
                    pBlockInfo->mBlockType = kReportCore;
                    pBlockInfo->mbMemoryMapped = false;
                }
                ++nBlockInfoCount;
            }

            if (nBlockTypeFlags & (kReportAllocated | kReportFree))
            {
                const char* const pCoreEnd = coreBlock.mpCore + coreBlock.mnSize;
                for (const char* pCursor = coreBlock.mpCore; pCursor < pCoreEnd; )
                {
                    const PcBlock* const pBlock = reinterpret_cast<const PcBlock*>(pCursor);
                    if (!PcBlockIsValid(pBlock, pCoreEnd))
                        return false;
                    if (PcBlockMatchesReport(pBlock, nBlockTypeFlags))
                    {
                        if (pBlockInfo)
                        {
                            if (nBlockInfoCount >= nCapacity)
                                return false;
                            PcGetBlockInfo(pBlock, pBlockInfo + nBlockInfoCount, coreBlock.mpCore);
                        }
                        ++nBlockInfoCount;
                    }
                    pCursor += pBlock->mnSize;
                }
            }
            return true;
        }

        inline u32 PcRoundUp(u32 lu, u32 luAlign) { return (lu + luAlign - 1) & ~(luAlign - 1); }

        // Align a POINTER up to luAlign. Must use uintptr_t (64-bit on x64) -- rounding the pointer as a
        // u32 truncates the high address bits and yields a garbage pointer.
        inline char* PcAlignPtr(char* lpPtr, u32 luAlign)
        {
            const uintptr_t lu = (reinterpret_cast<uintptr_t>(lpPtr) + (luAlign - 1)) & ~(static_cast<uintptr_t>(luAlign) - 1);
            return reinterpret_cast<char*>(lu);
        }
    }

    // @ 0x82B4F800 - adopt [pCore, pCore+nSize) as one big free block. (PC leaf: one core block.)
    bool GeneralAllocator::AddCore(void* pCore, size_t nSize, bool /*bShouldFree*/, bool /*bShouldFreeOnShutdown*/)
    {
        if (!pCore || nSize < KU_PC_ALIGN * 2)
            return false;

        char* lpBase   = reinterpret_cast<char*>(pCore);
        char* lpAligned = PcAlignPtr(lpBase, KU_PC_ALIGN);
        u32   luUsable = static_cast<u32>(nSize) - static_cast<u32>(lpAligned - lpBase);
        luUsable &= ~(KU_PC_ALIGN - 1);

        mHeadCoreBlock.mpCore = lpAligned;
        mHeadCoreBlock.mnSize = luUsable;

        PcBlock* lpFree = reinterpret_cast<PcBlock*>(lpAligned);
        lpFree->mnSize  = luUsable;
        lpFree->mbFree  = 1;
        lpFree->mnMagic = KU_PC_MAGIC;
        lpFree->mnPad   = 0;
        return true;
    }

    void* GeneralAllocator::MallocAlignedInternal(size_t nSize, size_t nAlignment, size_t /*nAlignmentOffset*/, int /*nAllocationFlags*/)
    {
        if (!mHeadCoreBlock.mpCore)
            return 0;
        u32 luAlign   = nAlignment < KU_PC_ALIGN ? KU_PC_ALIGN : static_cast<u32>(nAlignment);
        u32 luDataNeed = PcRoundUp(nSize == 0 ? 1 : static_cast<u32>(nSize), KU_PC_ALIGN);

        char* lpBase = static_cast<char*>(mHeadCoreBlock.mpCore);
        char* lpEnd  = lpBase + mHeadCoreBlock.mnSize;
        for (char* lpCur = lpBase; lpCur < lpEnd; )
        {
            PcBlock* lpBlk = reinterpret_cast<PcBlock*>(lpCur);
            // Boundary-tag sanity. Without this the walk trusts whatever bytes sit at
            // lpCur: one out-of-bounds write into a block header desynchronises the
            // chain, the walk lands mid-payload, reads a non-zero word as `mbFree`, and
            // hands the caller a block carved ON TOP of live data -- two overlapping
            // LIVE allocations from one heap (the CrashNav main-map corruption,
            // 2026-08-29). A corrupt chain must fail the allocation instead: the caller
            // (AptDataHandler::AptAlloc) already asserts loudly on a null result.
            if (lpBlk->mnMagic != KU_PC_MAGIC ||
                lpBlk->mnSize < KU_PC_ALIGN ||
                (lpBlk->mnSize & (KU_PC_ALIGN - 1)) != 0 ||
                lpCur + lpBlk->mnSize > lpEnd)
            {
                return 0;
            }
            if (lpBlk->mbFree)
            {
                // place the (16-aligned) data at the next `luAlign` boundary at/after lpCur+16
                char* lpAlignedData = PcAlignPtr(lpCur + KU_PC_ALIGN, luAlign);
                char* lpUsedHdr     = lpAlignedData - KU_PC_ALIGN;
                u32   luFrontPad    = static_cast<u32>(lpUsedHdr - lpCur);   // multiple of 16 (0 or >=16)
                u32   luNeedFromCur = luFrontPad + KU_PC_ALIGN + luDataNeed; // front-block + used header + data

                if (lpBlk->mnSize >= luNeedFromCur)
                {
                    u32 luBlkSize = lpBlk->mnSize;

                    // optional free block in front (to honour alignment)
                    if (luFrontPad >= KU_PC_ALIGN)
                    {
                        lpBlk->mnSize = luFrontPad;            // stays free
                        // (lpBlk already free + magic)
                    }

                    PcBlock* lpUsed = reinterpret_cast<PcBlock*>(lpUsedHdr);
                    u32 luUsedSpan  = luBlkSize - luFrontPad;  // from lpUsedHdr to block end
                    u32 luUsedSize  = KU_PC_ALIGN + luDataNeed;

                    // split a free tail if there's room for a header + min data
                    if (luUsedSpan - luUsedSize >= KU_PC_ALIGN * 2)
                    {
                        PcBlock* lpTail = reinterpret_cast<PcBlock*>(lpUsedHdr + luUsedSize);
                        lpTail->mnSize  = luUsedSpan - luUsedSize;
                        lpTail->mbFree  = 1;
                        lpTail->mnMagic = KU_PC_MAGIC;
                        lpTail->mnPad   = 0;
                        lpUsed->mnSize  = luUsedSize;
                    }
                    else
                    {
                        lpUsed->mnSize  = luUsedSpan;          // absorb the slack
                    }
                    lpUsed->mbFree  = 0;
                    lpUsed->mnMagic = KU_PC_MAGIC;
                    lpUsed->mnPad   = 0;
                    return lpUsedHdr + KU_PC_ALIGN;            // == lpAlignedData
                }
            }
            lpCur += lpBlk->mnSize;
        }
        return 0;   // out of memory
    }

    void* GeneralAllocator::MallocInternal(size_t nSize, int nAllocationFlags)
    {
        return MallocAlignedInternal(nSize, KU_PC_ALIGN, 0, nAllocationFlags);
    }

    void* GeneralAllocator::Malloc(size_t nSize, int nAllocationFlags)
    {
        return MallocInternal(nSize, nAllocationFlags);
    }

    void* GeneralAllocator::MallocAligned(size_t nSize, size_t nAlignment, size_t nAlignmentOffset, int nAllocationFlags)
    {
        return MallocAlignedInternal(nSize, nAlignment, nAlignmentOffset, nAllocationFlags);
    }

    // @ 0x82B4FC08 - mark the block free, then coalesce all adjacent free blocks (full pass).
    void GeneralAllocator::Free(void* pData)
    {
        if (!pData || !mHeadCoreBlock.mpCore)
            return;
        PcBlock* lpBlk = reinterpret_cast<PcBlock*>(static_cast<char*>(pData) - KU_PC_ALIGN);
        if (lpBlk->mnMagic != KU_PC_MAGIC)
            return;     // not one of ours
        lpBlk->mbFree = 1;

        // coalesce adjacent free blocks (single forward pass over the core)
        char* lpBase = static_cast<char*>(mHeadCoreBlock.mpCore);
        char* lpEnd  = lpBase + mHeadCoreBlock.mnSize;
        char* lpCur  = lpBase;
        while (lpCur < lpEnd)
        {
            PcBlock* lpA = reinterpret_cast<PcBlock*>(lpCur);
            // Same boundary-tag sanity as the Malloc walk: never coalesce across a
            // header the chain no longer vouches for (that would merge live payload
            // bytes into a free block).
            if (lpA->mnMagic != KU_PC_MAGIC || lpA->mnSize < KU_PC_ALIGN ||
                (lpA->mnSize & (KU_PC_ALIGN - 1)) != 0 || lpCur + lpA->mnSize > lpEnd)
            {
                break;
            }
            char* lpNext = lpCur + lpA->mnSize;
            if (lpA->mbFree && lpNext < lpEnd)
            {
                PcBlock* lpB = reinterpret_cast<PcBlock*>(lpNext);
                if (lpB->mnMagic != KU_PC_MAGIC || lpB->mnSize < KU_PC_ALIGN ||
                    (lpB->mnSize & (KU_PC_ALIGN - 1)) != 0 || lpNext + lpB->mnSize > lpEnd)
                {
                    break;
                }
                if (lpB->mbFree)
                {
                    lpA->mnSize += lpB->mnSize;   // merge B into A; re-check A against the new next
                    continue;
                }
            }
            lpCur = lpNext;
        }
    }

    // FLAG PC-platform leaf: the approved boundary-tag engine's Free is the
    // internal free operation too; no second allocator or bookkeeping is introduced.
    void GeneralAllocator::FreeInternal(void* pData)
    {
        Free(pData);
    }

    // @82B511B0 - lock while constructing a dynamic iterator or a two-pass copy.
    // FLAG PC-platform leaf: enumerate actual PcBlock tags in the adopted PC core.
    // ARTIST's 8-byte Chunk data header / free-list links / fenceposts do not exist
    // in this engine. PcGetBlockInfo reports its real 16-byte header and capacity;
    // all pointers and storage extents are native, never console byte offsets.
    void* GeneralAllocator::TakeSnapshot(int nBlockTypeFlags, bool bMakeCopy,
                                         void* pStorage, size_t nStorageSize)
    {
        PPMAutoMutex lock(mpMutex);
        size_t nBlockInfoCapacity = 0;
        if (bMakeCopy)
        {
            if (mnMaxFastBinChunkSize & 1)
                ClearFastBins();
            if (!PcCaptureReport(mHeadCoreBlock, nBlockTypeFlags, 0, 0, nBlockInfoCapacity))
                return 0;
            // The snapshot allocation itself can split a free block. ARTIST adds
            // four records between its count and fill passes for that change.
            if (nBlockInfoCapacity > (static_cast<size_t>(-1) - sizeof(Snapshot)) /
                                     sizeof(BlockInfo) - kSnapshotSpareRecords)
                return 0;
            nBlockInfoCapacity += kSnapshotSpareRecords;
        }

        const size_t nSnapshotSize = sizeof(Snapshot) + nBlockInfoCapacity * sizeof(BlockInfo);
        if (pStorage && nStorageSize < nSnapshotSize)
            pStorage = 0;
        // PcBlock's actual span is u32. Do not let its existing size conversion
        // wrap a snapshot request that cannot be represented by the PC engine.
        if (!pStorage && nSnapshotSize > static_cast<u32>(-1) - KU_PC_ALIGN * 2)
            return 0;
        Snapshot* const pSnapshot = static_cast<Snapshot*>(pStorage ? pStorage :
            MallocInternal(nSnapshotSize, static_cast<int>(0x80000000u)));
        if (!pSnapshot)
            return 0;

        if (!pStorage)
        {
            PcBlock* const pBlock = reinterpret_cast<PcBlock*>(
                reinterpret_cast<char*>(pSnapshot) - sizeof(PcBlock));
            pBlock->mnPad |= kPcBlockInternal;
        }
        memset(pSnapshot, 0, nSnapshotSize);
        pSnapshot->mnMagicNumber = kSnapshotMagic;
        pSnapshot->mnSizeOfThis = nSnapshotSize;
        pSnapshot->mnBlockTypeFlags = nBlockTypeFlags;
        pSnapshot->mbUserAllocated = pStorage != 0;
        pSnapshot->mbDynamic = !bMakeCopy;

        if (bMakeCopy)
        {
            pSnapshot->mnBlockInfoCount = nBlockInfoCapacity;
            if (nBlockInfoCapacity != kSnapshotSpareRecords)
            {
                if (!PcCaptureReport(mHeadCoreBlock, nBlockTypeFlags, pSnapshot->mBlockInfo,
                                     nBlockInfoCapacity, pSnapshot->mnBlockInfoCount))
                {
                    FreeSnapshot(pSnapshot);
                    return 0;
                }
            }
        }
        else
        {
            // The approved PC engine owns one core directly in mHeadCoreBlock;
            // its console circular lists and mmap list remain unused.
            pSnapshot->mpCurrentCoreBlock = mHeadCoreBlock.mpCore ? &mHeadCoreBlock : 0;
        }
        return pSnapshot;
    }

    // FreeSnapshot / ReportBegin / ReportEnd are inlined in ARTIST ReportHeap
    // @82B51848 and TraceAllocatedMemory @82B516F0. DecFIGS pins their API shape.
    void GeneralAllocator::FreeSnapshot(void* pSnapshot)
    {
        Snapshot* const pSnapshotData = static_cast<Snapshot*>(pSnapshot);
        if (pSnapshotData && pSnapshotData->mnMagicNumber == kSnapshotMagic &&
            !pSnapshotData->mbUserAllocated)
            FreeInternal(pSnapshotData);
    }

    const void* GeneralAllocator::ReportBegin(void* pSnapshot, int nBlockTypeFlags,
                                             bool bMakeCopy, void* pStorage, size_t nStorageSize)
    {
        Snapshot* const pReport = static_cast<Snapshot*>(pSnapshot ? pSnapshot :
            TakeSnapshot(nBlockTypeFlags, bMakeCopy, pStorage, nStorageSize));
        if (pReport)
            pReport->mbReport = pSnapshot == 0;
        return pReport;
    }

    // @82B4F3A8 - reject invalid contexts, intersect masks for a live walk, and
    // advance the copied-record index before returning a matching record.
    // FLAG PC-platform leaf: the live cursor carries an opaque PcBlock address
    // in mpCurrentChunk; it is converted back before any metadata is read.
    const GeneralAllocator::BlockInfo* GeneralAllocator::ReportNext(const void* pContext,
                                                                    int nBlockTypeFlags)
    {
        Snapshot* const pSnapshot = const_cast<Snapshot*>(static_cast<const Snapshot*>(pContext));
        if (!pSnapshot || pSnapshot->mnMagicNumber != kSnapshotMagic)
            return 0;

        if (!pSnapshot->mbDynamic)
        {
            while (pSnapshot->mnBlockInfoIndex < pSnapshot->mnBlockInfoCount)
            {
                const BlockInfo* const pBlockInfo =
                    pSnapshot->mBlockInfo + pSnapshot->mnBlockInfoIndex++;
                if (pBlockInfo->mBlockType & nBlockTypeFlags)
                    return pBlockInfo;
            }
            return 0;
        }

        CoreBlock* const pCoreBlock = pSnapshot->mpCurrentCoreBlock;
        if (!pCoreBlock || !pCoreBlock->mpCore || !pCoreBlock->mnSize)
            return 0;
        const char* const pCoreEnd = pCoreBlock->mpCore + pCoreBlock->mnSize;
        const char* pCursor = pCoreBlock->mpCore;
        if (pSnapshot->mpCurrentChunk)
        {
            const PcBlock* const pPrevious = reinterpret_cast<const PcBlock*>(pSnapshot->mpCurrentChunk);
            pCursor = reinterpret_cast<const char*>(pPrevious);
            if (pCursor < pCoreBlock->mpCore || pCursor >= pCoreEnd ||
                !PcBlockIsValid(pPrevious, pCoreEnd))
            {
                pSnapshot->mpCurrentCoreBlock = 0;
                return 0;
            }
            pCursor += pPrevious->mnSize;
        }

        const int nEffectiveFlags = pSnapshot->mnBlockTypeFlags & nBlockTypeFlags;
        while (pCursor < pCoreEnd)
        {
            const PcBlock* const pBlock = reinterpret_cast<const PcBlock*>(pCursor);
            if (!PcBlockIsValid(pBlock, pCoreEnd))
                break;
            pSnapshot->mpCurrentChunk = reinterpret_cast<Chunk*>(const_cast<PcBlock*>(pBlock));
            if (PcBlockMatchesReport(pBlock, nEffectiveFlags))
            {
                PcGetBlockInfo(pBlock, pSnapshot->mBlockInfo, pCoreBlock->mpCore);
                return pSnapshot->mBlockInfo;
            }
            pCursor += pBlock->mnSize;
        }
        pSnapshot->mpCurrentCoreBlock = 0;
        pSnapshot->mpCurrentChunk = 0;
        return 0;
    }

    void GeneralAllocator::ReportEnd(const void* pContext)
    {
        Snapshot* const pSnapshot = const_cast<Snapshot*>(static_cast<const Snapshot*>(pContext));
        if (pSnapshot && pSnapshot->mnMagicNumber == kSnapshotMagic)
        {
            if (pSnapshot->mbReport)
                FreeSnapshot(pSnapshot);
            else
                pSnapshot->mnBlockInfoIndex = 0;
        }
    }

    // @82B51848 - success for an empty valid report; false for a null callback,
    // snapshot allocation failure, or a callback that stops the report. ARTIST
    // advances ReportNext once even after a false callback, then ends the report.
    bool GeneralAllocator::ReportHeap(bool (*pReportFunction)(const BlockInfo*, void*),
                                      void* pContext, int nBlockTypeFlags, bool bMakeCopy,
                                      void* pStorage, size_t nStorageSize)
    {
        if (!pReportFunction)
            return false;
        const void* const pReport = ReportBegin(0, nBlockTypeFlags, bMakeCopy, pStorage, nStorageSize);
        if (!pReport)
            return false;

        bool bContinue = true;
        const BlockInfo* pBlockInfo = ReportNext(pReport, nBlockTypeFlags);
        while (pBlockInfo)
        {
            bContinue = pReportFunction(pBlockInfo, pContext);
            pBlockInfo = ReportNext(pReport, nBlockTypeFlags);
            if (!bContinue)
                break;
        }
        ReportEnd(pReport);
        return bContinue;
    }

    // True if pData is inside the adopted core block.
    bool GeneralAllocator::Owns(const void* pData) const
    {
        const char* lpCore = static_cast<const char*>(mHeadCoreBlock.mpCore);
        if (!lpCore || !pData)
            return false;
        const char* lpP = static_cast<const char*>(pData);
        return lpP >= lpCore && lpP < lpCore + mHeadCoreBlock.mnSize;
    }

    // DWARF EAGeneralAllocator.h:819 - walk the heap and report whether its bookkeeping is
    // intact. The X360 engine validates the dlmalloc bin/chunk invariants at increasing depth
    // per HeapValidationLevel; the PC-leaf engine here keeps a single boundary-tag list, so the
    // equivalent walk is: every block from the core base carries the magic, has a non-degenerate
    // size, and the chain lands exactly on the core end. kHeapValidationLevelNone short-circuits
    // true, matching the console's own early-out. Callers use this only inside asserts
    // (CgsNetwork::NetworkTexture::Prepare, BrnGameState::CarData::Prepare, the DXT compressor).
    bool GeneralAllocator::ValidateHeap(HeapValidationLevel heapValidationLevel)
    {
        if (heapValidationLevel == kHeapValidationLevelNone)
            return true;
        if (!mHeadCoreBlock.mpCore)
            return true;   // nothing adopted yet -- vacuously intact

        char* lpBase = static_cast<char*>(mHeadCoreBlock.mpCore);
        char* lpEnd  = lpBase + mHeadCoreBlock.mnSize;
        char* lpCur  = lpBase;
        while (lpCur < lpEnd)
        {
            const PcBlock* lpBlk = reinterpret_cast<const PcBlock*>(lpCur);
            if (lpBlk->mnMagic != KU_PC_MAGIC)
                return false;
            if (lpBlk->mnSize < KU_PC_ALIGN)
                return false;
            lpCur += lpBlk->mnSize;
        }
        return lpCur == lpEnd;
    }

    // @ 0x82B4E0B0 - consolidate the fast bins back into the regular free list. The real X360
    // engine caches small frees in mpFastBinArray and flushes them here (from SetOption /
    // TraceAllocatedMemory / GetLargestFreeBlock). The PC-leaf engine never populates the fast
    // bins (Free coalesces directly in the boundary-tag list), so there is nothing to flush --
    // an inert but faithful entry point. (Reinstate the real flush if the bit-exact dlmalloc
    // engine is ever restored.)
    void GeneralAllocator::ClearFastBins()
    {
        // PC leaf: no fast-bin chunks are ever created; the boundary-tag list is always consolidated.
    }

    // @ 0x82B4E2E8 - the largest single block that can currently be allocated. The X360 flushes the
    // fast bins (when bClearCache) then walks the free bins for the biggest free chunk. PC leaf:
    // flush (no-op) then scan the boundary-tag core for the largest free PcBlock, returning its
    // usable byte count (block size minus the 16-byte header).
    size_t GeneralAllocator::GetLargestFreeBlock(bool bClearCache)
    {
        if (bClearCache)
            ClearFastBins();
        if (!mHeadCoreBlock.mpCore)
            return 0;

        u32 luLargest = 0;
        char* lpBase = static_cast<char*>(mHeadCoreBlock.mpCore);
        char* lpEnd  = lpBase + mHeadCoreBlock.mnSize;
        for (char* lpCur = lpBase; lpCur < lpEnd; )
        {
            PcBlock* lpBlk = reinterpret_cast<PcBlock*>(lpCur);
            if (lpBlk->mnMagic != KU_PC_MAGIC || lpBlk->mnSize < KU_PC_ALIGN)
                break;   // corrupt / past the end
            if (lpBlk->mbFree && lpBlk->mnSize > luLargest)
                luLargest = lpBlk->mnSize;
            lpCur += lpBlk->mnSize;
        }
        return luLargest >= KU_PC_ALIGN ? static_cast<size_t>(luLargest - KU_PC_ALIGN) : 0;
    }

    void* GeneralAllocator::Calloc(size_t nElementCount, size_t nElementSize, int nAllocationFlags)
    {
        size_t lnTotal = nElementCount * nElementSize;
        void* lpData = MallocInternal(lnTotal, nAllocationFlags);
        if (lpData)
            memset(lpData, 0, lnTotal);
        return lpData;
    }

    void* GeneralAllocator::Realloc(void* pData, size_t nNewSize, int nAllocationFlags)
    {
        if (!pData)
            return MallocInternal(nNewSize, nAllocationFlags);
        if (nNewSize == 0)
        {
            Free(pData);
            return 0;
        }
        PcBlock* lpBlk = reinterpret_cast<PcBlock*>(static_cast<char*>(pData) - KU_PC_ALIGN);
        u32 luOldData = lpBlk->mnSize - KU_PC_ALIGN;
        if (luOldData >= nNewSize)
            return pData;   // shrink-in-place
        void* lpNew = MallocInternal(nNewSize, nAllocationFlags);
        if (lpNew)
        {
            memcpy(lpNew, pData, luOldData);
            Free(pData);
        }
        return lpNew;
    }

    // @ 0x82B500F8 - tear the heap down. PC leaf: the core buffer is owned by the caller
    // (HeapMalloc / the module); just drop our reference and mark uninitialised.
    bool GeneralAllocator::Shutdown()
    {
        mHeadCoreBlock.mpCore = 0;
        mHeadCoreBlock.mnSize = 0;
        mbInitialized = false;
        return true;
    }

    // @ 0x82B511A8 - destroy: release any adopted core. (Full body lands with AddCore/FreeCore.)
    GeneralAllocator::~GeneralAllocator()
    {
        Shutdown();
    }

    // ====================================================================================
    // FULL-DWARF SURFACE (this round): the core-init ctor/Init overload, SetOption, the
    // PPMAutoMutex lock, TraceAllocatedMemory, and the trivial accessors / callback setters.
    // The complex bin/coalescing internals stay declare-only (next round).
    // ====================================================================================

    // Global head of the init-callback list (X360 dword_832500B0). Empty here.
    volatile GeneralAllocator::InitCallbackNode* GeneralAllocator::mpInitCallbackNode = 0;

    // @ 0x82B4DF08 / dtor - enter/leave the allocator's mutex (a CRITICAL_SECTION) if present.
    // The X360 also bumps a recursion counter inside the mutex; the CRITICAL_SECTION tracks its
    // own recursion. A null mutex is a no-op (the thread-safety option was never enabled).
    GeneralAllocator::PPMAutoMutex::PPMAutoMutex(void* pMutex)
        : mpMutex(pMutex)
    {
        if (mpMutex)
            EnterCriticalSection(static_cast<CRITICAL_SECTION*>(mpMutex));
    }

    GeneralAllocator::PPMAutoMutex::~PPMAutoMutex()
    {
        if (mpMutex)
            LeaveCriticalSection(static_cast<CRITICAL_SECTION*>(mpMutex));
    }

    // @ 0x82B4FF58 - core-adopting ctor: zero the state + install PPMalloc defaults (delegating to
    // the default-ctor body), then Init() over the supplied initial core. The X360 inlines the
    // member init here; we reuse the default ctor's faithful field setup, then adopt the core.
    GeneralAllocator::GeneralAllocator(void* pInitialCore, size_t nInitialCoreSize,
                                       bool bShouldFreeInitialCore, bool bShouldTrace,
                                       unsigned int (*pCoreAddFunction)(GeneralAllocator*, void*, unsigned int, void*),
                                       void* pCoreAddFunctionContext)
        : GeneralAllocator()
    {
        mbTraceInternalMemory = bShouldTrace;
        Init(pInitialCore, nInitialCoreSize, bShouldFreeInitialCore, bShouldTrace,
             pCoreAddFunction, pCoreAddFunctionContext);
    }

    // @ 0x82B4FA30 - full core-init overload: bring up the empty heap (idempotent), then adopt the
    // supplied initial core if one was given, then fire the registered init callbacks once.
    bool GeneralAllocator::Init(void* pInitialCore, size_t nInitialCoreSize,
                                bool bShouldFreeInitialCore, bool /*bShouldTrace*/,
                                unsigned int (*/*pCoreAddFunction*/)(GeneralAllocator*, void*, unsigned int, void*),
                                void* /*pCoreAddFunctionContext*/)
    {
        if (!mbInitialized)
        {
            // X360 Init enables thread-safety option 1 first, then self-links the bins; the
            // 0-arg Init() below performs the faithful empty-heap bring-up.
            SetOption(kOptionEnableThreadSafety, 1);
            Init();
        }

        // adopt the initial core (X360: `if (a2 || a3) AddCore(...)`)
        if (pInitialCore || nInitialCoreSize)
            AddCore(pInitialCore, nInitialCoreSize, bShouldFreeInitialCore, bShouldFreeInitialCore);

        // fire the one-time init callbacks (X360 tracks this with the +1300 mNotifyInitState byte)
        if (!mNotifyInitState)
        {
            mNotifyInitState = 1;
            for (volatile InitCallbackNode* lpNode = mpInitCallbackNode; lpNode; lpNode = lpNode->mpNext)
                lpNode->mpInitCallbackFunction(this, true, lpNode->mpContext);
        }
        return true;
    }

    // @ 0x82B4DF60 - runtime tunables. The X360 dispatches options 1..13 through a jump table;
    // each case stores nValue into the matching field (a few normalise/clamp it). Reconstructed
    // as a clean switch over enum Option from the per-case stores in the asm.
    void GeneralAllocator::SetOption(int option, int nValue)
    {
        switch (option)
        {
        case kOptionEnableThreadSafety:
            // create the mutex on enable / tear it down on disable (X360 PPMMutexCreate path).
            if (nValue)
            {
                if (!mpMutex)
                    mpMutex = mpMutexData;   // back the CRITICAL_SECTION with the in-object storage
            }
            else
            {
                mpMutex = 0;
            }
            break;

        case kOptionEnableHighAllocation:
            mbHighFenceInternallyDisabled = (nValue == 0);
            break;

        case kOptionEnableSystemAlloc:
            mbSystemAllocEnabled = (nValue != 0);
            break;

        case kOptionEnableMallocFailureAssert:
            // (X360 stores a boolean flag; folded into the failure-function policy.)
            break;

        case kOptionMaxMallocFailureCount:
            mnMaxMallocFailureCount = static_cast<unsigned int>(nValue);
            break;

        case kOptionMaxFastBinRequestSize:
        {
            // clamp request -> chunk size, rounded down to 8, min 16 (X360 0x82B4E138 path).
            ClearFastBins();
            unsigned int luRequest = static_cast<unsigned int>(nValue);
            if (luRequest)
            {
                if (luRequest > 0x50)
                    luRequest = 0x50;
                unsigned int luChunk = (luRequest + 0x0B) & ~7u;
                if (luChunk < 0x10)
                    luChunk = 0x10;
                mnMaxFastBinChunkSize = luChunk;
            }
            else
            {
                mnMaxFastBinChunkSize = 0;
            }
            break;
        }

        case kOptionTrimThreshold:
            mnTrimThreshold = static_cast<unsigned int>(nValue);
            break;

        case kOptionTopPad:
            mnTopPad = static_cast<unsigned int>(nValue);
            break;

        case kOptionMMapThreshold:
            mnMMapThreshold = static_cast<unsigned int>(nValue);
            break;

        case kOptionMMapMaxAllowed:
            mnMMapMaxAllowed = nValue;
            break;

        case kOptionNewCoreSize:
            mnNewCoreSize = static_cast<unsigned int>(nValue);
            break;

        case kOptionCoreIncrementSize:
            mnCoreIncrementSize = static_cast<unsigned int>(nValue);
            break;

        case kOptionMMapTopDown:
            mbMMapTopDown = (nValue != 0);
            break;

        default:
            break;
        }
    }

    // ARTIST82B4DA38; XB1 sub_140910EB0 corroborates native counts and the
    // two-byte wchar_t branch. The preview is binary hex plus printable bytes;
    // even the wide output returns kDataTypeBinary. Preserve its byte-filled
    // wide padding and its distinct DEL handling.
    GeneralAllocator::DataType GeneralAllocator::GetDataPreview(
        const void* pData, size_t nDataSize, char* pBuffer, wchar_t* pBufferW,
        size_t nBufferLength) const
    {
        if (nBufferLength < 5)
        {
            if (nBufferLength)
            {
                if (pBuffer)
                    pBuffer[0] = 0;
                if (pBufferW)
                    pBufferW[0] = 0;
            }
            return kDataTypeBinary;
        }

        size_t nPreviewSize = (nBufferLength - 2) / 4;
        if (nPreviewSize > nDataSize)
            nPreviewSize = nDataSize;
        // FLAG PC-platform leaf: do not form the original 3*N-1 separator
        // address when no byte fits. Real live PcBlock payloads are nonempty;
        // TraceAllocatedMemory uses the original 256-character preview buffer.
        if (!nPreviewSize)
        {
            if (pBuffer)
                pBuffer[0] = 0;
            if (pBufferW)
                pBufferW[0] = 0;
            return kDataTypeBinary;
        }

        if (pBuffer)
        {
            memset(pBuffer, ' ', nBufferLength);
            pBuffer[nBufferLength - 1] = 0;
            pBuffer[3 * nPreviewSize - 1] = '\t';
        }
        if (pBufferW)
        {
            memset(pBufferW, ' ', nBufferLength * sizeof(wchar_t));
            pBufferW[nBufferLength - 1] = 0;
            pBufferW[3 * nPreviewSize - 1] = L'\t';
        }

        const unsigned char* const pBytes = static_cast<const unsigned char*>(pData);
        const char kDigits[] = "0123456789abcdef";
        for (size_t i = 0; i < nPreviewSize; ++i)
        {
            const unsigned char nByte = pBytes[i];
            const char cHigh = kDigits[nByte >> 4];
            const char cLow = kDigits[nByte & 15];
            const signed char cByte = static_cast<signed char>(nByte);
            if (pBuffer)
            {
                pBuffer[3 * i] = cHigh;
                pBuffer[3 * i + 1] = cLow;
                pBuffer[3 * nPreviewSize + i] =
                    (cByte < 32 || cByte >= 127 || cByte == '"' || cByte == '\'') ? '.' : static_cast<char>(cByte);
            }
            if (pBufferW)
            {
                pBufferW[3 * i] = static_cast<wchar_t>(cHigh);
                pBufferW[3 * i + 1] = static_cast<wchar_t>(cLow);
                pBufferW[3 * nPreviewSize + i] =
                    (cByte < 32 || cByte == '"' || cByte == '\'') ? L'.' : static_cast<wchar_t>(cByte);
            }
        }
        return kDataTypeBinary;
    }

    // ARTIST82B4DCA0 supplies the address/size/data/attribute fields and the
    // allocator's two delimiters. XB1 sub_14090FD90 pins the native address
    // spelling and 0x1c capacity gate; its later <space> field is not in ARTIST.
    // FLAG PC-platform leaf: ReportNext carries an opaque PcBlock address in
    // Chunk*. Convert that address before metadata access, using the same real
    // PcGetBlockInfo producer as reporting. No console Chunk field is read.
    size_t GeneralAllocator::DescribeChunk(const Chunk* pChunk, char* pBuffer,
                                           size_t nBufferLength, bool bAppendLineEnd) const
    {
        // The actual trace passes4000. Short no-append output leaves bytes intact;
        // only append mode needs its record delimiter and terminator.
        if (nBufferLength < 2)
        {
            if (nBufferLength && bAppendLineEnd)
                pBuffer[0] = 0;
            return 0;
        }
        const PcBlock* const pBlock = reinterpret_cast<const PcBlock*>(pChunk);
        BlockInfo info;
        PcGetBlockInfo(pBlock, &info, mHeadCoreBlock.mpCore);
        const unsigned int nDataSize = static_cast<unsigned int>(info.mnDataSize);
        char* pWrite = pBuffer;
        size_t nRemaining = nBufferLength;
        if (nRemaining >= 0x1c)
        {
            const int nWritten = std::snprintf(pWrite, nRemaining, "addr: 0x%016llx%c",
                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(info.mpData)), mcTraceFieldDelimiter);
            pWrite += nWritten;
            nRemaining -= nWritten;
        }
        if (nRemaining >= 0x1f)
        {
            const int nWritten = std::snprintf(pWrite, nRemaining, "size: %10u (%8x)%c",
                nDataSize, nDataSize, mcTraceFieldDelimiter);
            pWrite += nWritten;
            nRemaining -= nWritten;
        }
        if (nRemaining >= 0x10a)
        {
            char data[256] = {};
            GetDataPreview(info.mpData, info.mnDataSize, data, 0, sizeof(data));
            const int nWritten = std::snprintf(pWrite, nRemaining, "data: %s%c", data, mcTraceFieldDelimiter);
            pWrite += nWritten;
            nRemaining -= nWritten;
        }
        const bool bInternal = (info.mBlockType & kReportInternal) != 0;
        if ((bInternal || info.mbMemoryMapped) && nRemaining >= 0x19)
        {
            const int nWritten = std::snprintf(pWrite, nRemaining, "attr: %s %s%c",
                bInternal ? "internal" : "", info.mbMemoryMapped ? "mapped" : "", mcTraceFieldDelimiter);
            pWrite += nWritten;
        }
        if (bAppendLineEnd)
        {
            *pWrite++ = static_cast<char>(mcTraceRecordDelimiter);
            *pWrite = 0;
        }
        return static_cast<size_t>(pWrite - pBuffer);
    }

    // ARTIST82B516F0: lock, conditionally consolidate fast bins, independently
    // default the callback and context, then trace the real allocated-block
    // report. ReportBegin/End are the recovered inline snapshot lifecycle.
    void GeneralAllocator::TraceAllocatedMemory(void (*pTraceCallback)(const char*, void*),
                                                void* pTraceCallbackContext,
                                                void* pStorage, size_t nStorageSize)
    {
        PPMAutoMutex lock(mpMutex);
        if (mnMaxFastBinChunkSize & 1)
            ClearFastBins();
        if (!pTraceCallback)
            pTraceCallback = mpTraceFunction;
        if (!pTraceCallbackContext)
            pTraceCallbackContext = mpTraceFunctionContext;
        if (pTraceCallback)
        {
            const void* const pReport = ReportBegin(0, kReportAllocated, false, pStorage, nStorageSize);
            const BlockInfo* pBlockInfo;
            char record[0xfa0];
            while ((pBlockInfo = ReportNext(pReport, 0x1f)) != 0)
            {
                if (mbTraceInternalMemory || (pBlockInfo->mBlockType & kReportInternal) == 0)
                {
                    DescribeChunk(static_cast<const Chunk*>(pBlockInfo->mpBlock), record, sizeof(record), true);
                    pTraceCallback(record, pTraceCallbackContext);
                }
            }
            ReportEnd(pReport);
        }
    }

    // ------------------------------------------------------------------------------------
    // Trivial accessors / callback setters (DWARF surface; store-for-store from the X360).
    // ------------------------------------------------------------------------------------
    void GeneralAllocator::SetName(const char* pName)
    {
        mpName = pName;
    }

    const char* GeneralAllocator::GetName() const
    {
        return mpName;
    }

    void GeneralAllocator::SetMallocFailureFunction(
        bool (*pMallocFailureFunction)(GeneralAllocator*, size_t, size_t, void*), void* pContext)
    {
        // The public setter takes size_t (DWARF); the stored slot is unsigned int (the X360 layout
        // member). Same calling convention on the target - reinterpret to the slot type.
        mpMallocFailureFunction        = reinterpret_cast<bool (*)(GeneralAllocator*, unsigned int, unsigned int, void*)>(pMallocFailureFunction);
        mpMallocFailureFunctionContext = pContext;
    }

    void GeneralAllocator::SetAssertionFailureFunction(void (*pAssertionFailureFunction)(const char*, void*), void* pContext)
    {
        mpAssertionFailureFunction        = pAssertionFailureFunction;
        mpAssertionFailureFunctionContext = pContext;
    }

    void GeneralAllocator::AssertionFailure(const char* pExpression) const
    {
        if (mpAssertionFailureFunction)
            mpAssertionFailureFunction(pExpression, mpAssertionFailureFunctionContext);
    }

    void GeneralAllocator::SetHookFunction(void (*pHookFunction)(const HookInfo*, void*), void* pContext)
    {
        // mpHookFunction is typed non-const in the layout (X360 store); the public setter takes the
        // const-HookInfo signature - reinterpret to the stored slot type (same calling convention).
        mpHookFunction        = reinterpret_cast<void (*)(HookInfo*, void*)>(pHookFunction);
        mpHookFunctionContext = pContext;
    }

    void GeneralAllocator::SetTraceFunction(void (*pTraceFunction)(const char*, void*), void* pContext)
    {
        mpTraceFunction        = pTraceFunction;
        mpTraceFunctionContext = pContext;
    }

    void GeneralAllocator::SetTraceFieldDelimiters(unsigned char fieldDelimiter, unsigned char recordDelimiter)
    {
        mcTraceFieldDelimiter  = fieldDelimiter;
        mcTraceRecordDelimiter = recordDelimiter;
    }

    void GeneralAllocator::SetAutoHeapValidation(HeapValidationLevel heapValidationLevel, size_t nFrequency)
    {
        mAutoHeapValidationLevel       = heapValidationLevel;
        mnAutoHeapValidationFrequency  = static_cast<unsigned int>(nFrequency);
        mnAutoHeapValidationEventCount = 0;
    }

    // @ 0x82B4DE08 - splice pCoreBlock out of the circular core-block list. The X360 body is the
    // classic two-store doubly-linked-list unlink against the CoreBlock prev/next slots (offsets
    // +24 / +28 == mpPrevCoreBlock / mpNextCoreBlock); reconstructed here BY NAMED MEMBER so it is
    // layout-portable. (Structural op shared by the real Shutdown / TrimCore teardown paths; the
    // PC-leaf engine keeps a single core block, so it is a faithful but currently-unexercised
    // entry point.)
    void GeneralAllocator::UnlinkCoreBlock(CoreBlock* pCoreBlock)
    {
        pCoreBlock->mpPrevCoreBlock->mpNextCoreBlock = pCoreBlock->mpNextCoreBlock;
        pCoreBlock->mpNextCoreBlock->mpPrevCoreBlock = pCoreBlock->mpPrevCoreBlock;
    }
}
}
