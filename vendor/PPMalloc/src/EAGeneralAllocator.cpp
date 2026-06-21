#include "ppmalloc/EAGeneralAllocator.h"
#include <cstring>   // memset / memcpy
#include <cstddef>   // offsetof
#include <cstdint>   // uintptr_t
#include "types.hpp" // u32

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
    // @ 0x82B4EDC8 / 0x82B4DC98 - default sinks (the real X360 versions route to the platform
    // debug-print; inert here - they are only invoked on assertion/trace, which the engine paths
    // gate behind debug flags).
    void GeneralAllocator::AssertionFailureFunctionDefault(const char* /*pMessage*/, void* /*pContext*/) {}
    void GeneralAllocator::TraceFunctionDefault(const char* /*pMessage*/, void* /*pContext*/) {}

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
            u32 mnPad;
        };

        inline u32 PcRoundUp(u32 lu, u32 luAlign) { return (lu + luAlign - 1) & ~(luAlign - 1); }
    }

    // @ 0x82B4F800 - adopt [pCore, pCore+nSize) as one big free block. (PC leaf: one core block.)
    bool GeneralAllocator::AddCore(void* pCore, size_t nSize, bool /*bShouldFree*/, bool /*bShouldFreeOnShutdown*/)
    {
        if (!pCore || nSize < KU_PC_ALIGN * 2)
            return false;

        char* lpBase   = reinterpret_cast<char*>(pCore);
        char* lpAligned = reinterpret_cast<char*>(PcRoundUp(static_cast<u32>(reinterpret_cast<uintptr_t>(lpBase)), KU_PC_ALIGN));
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
            if (lpBlk->mbFree)
            {
                // place the (16-aligned) data at the next `luAlign` boundary at/after lpCur+16
                char* lpAlignedData = reinterpret_cast<char*>(PcRoundUp(static_cast<u32>(reinterpret_cast<uintptr_t>(lpCur + KU_PC_ALIGN)), luAlign));
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
            char* lpNext = lpCur + lpA->mnSize;
            if (lpA->mbFree && lpNext < lpEnd)
            {
                PcBlock* lpB = reinterpret_cast<PcBlock*>(lpNext);
                if (lpB->mbFree)
                {
                    lpA->mnSize += lpB->mnSize;   // merge B into A; re-check A against the new next
                    continue;
                }
            }
            lpCur = lpNext;
        }
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
    void GeneralAllocator::Shutdown()
    {
        mHeadCoreBlock.mpCore = 0;
        mHeadCoreBlock.mnSize = 0;
        mbInitialized = false;
    }

    // @ 0x82B511A8 - destroy: release any adopted core. (Full body lands with AddCore/FreeCore.)
    GeneralAllocator::~GeneralAllocator()
    {
        Shutdown();
    }
}
}
