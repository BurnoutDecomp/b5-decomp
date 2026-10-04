#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <algorithm>   // std::sort (ARTIST RadixSortJob::Execute uses std::_Sort)
#include <cstdint>     // uintptr_t (128-byte alignment of the flat key array)
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"

// =============================================================================
// CgsGraphicsDispatchList.cpp
//
// Bodies for the CgsGraphics::DispatchList sort-key bucket, reconstructed
// store-for-store from the BURNOUT_X360_ARTIST.XEX disassembly:
//
//   CgsGraphics::DispatchList::ReserveKey         @ 0x822A0788
//   CgsGraphics::DispatchList::Submit             @ 0x822A0808
//   CgsGraphics::DispatchList::AllocateKeyBlock   @ 0x827FA730
//   CgsGraphics::DispatchList::PrepareSortJobInfo @ 0x827FA7D0
//   (+ SortForDispatch, the PC synchronous fallback for the RadixSort job)
//
// A DispatchList accumulates 64-bit sort records (one per Submit) into a chain of
// fixed-capacity KeyBlocks. ReserveKey guarantees the tail block has room (rolling
// to a fresh block via AllocateKeyBlock when full); Submit packs the sort key with
// the packet's bin-local quad-word offset and stores it at the tail's write head.
// =============================================================================

namespace CgsGraphics
{

// @ 0x822A0788
// Ensure the tail key block can hold one more record: allocate a fresh block when
// the tail is missing or full, then assert the tail exists.
DispatchList* DispatchList::ReserveKey()
{
    KeyBlock* lpTail = mpBlockListTail;
    if (lpTail == NULL || lpTail->muCount >= lpTail->muCapacity)
    {
        AllocateKeyBlock();
    }

    CGS_ASSERT(mpBlockListTail != NULL, "mpBlockListTail != NULL");
    return this;
}

// @ 0x822A0808
// Submit a packet under a sort key: pack (sortKey << 20 | packetLocalOffset) into
// the tail block's next slot, then advance the block and list counts.
void DispatchList::Submit(u64 lu64SortKey, DispatchCommand* lpPacket)
{
    CGS_ASSERT(lpPacket != NULL, "lpPacket != NULL");

    ReserveKey();

    CGS_ASSERT(mpBlockListTail != NULL, "mpBlockListTail != NULL");

    // Packet offset within the bin, in quad-words (DispatchCommand pointer diff
    // already scales by sizeof(DispatchCommand) == 16 == the PPC `>> 4`).
    const u32 luPacketLocalOffset = static_cast<u32>(lpPacket - m_pBinBase);

    const u64 luKey =
        (lu64SortKey << SortKey::KU_SHIFT_KEY)
        | (luPacketLocalOffset & SortKey::KU_MASK_OFFSET);

    // Store the record first, then range-check the offset (matches the asm order).
    KeyBlock* lpTail = mpBlockListTail;
    lpTail->mpKeys[lpTail->muCount] = luKey;

    CGS_ASSERT(luPacketLocalOffset <= SortKey::KU_MASK_OFFSET,
               "uint32_t(liPacketLocalOffset) <= SortKey::KU_MASK_OFFSET");

    ++lpTail->muCount;
    ++muCount;
}

// @ 0x827FA730
// Append a fresh 64-record KeyBlock to the chain and make it the tail. The block
// is carved from the list's bin: X360 allocates 33 quad-words (= the 16-byte block
// header + 64 * 8-byte keys) with the key array starting right after the header.
// The x64 gate sizes the header by the host struct (semantic parity: header first,
// then the 64-key array, whole carve rounded up to quad-words).
DispatchList* DispatchList::AllocateKeyBlock()
{
    CGS_ASSERT(mpDispatchBin != NULL, "mpDispatchBin != NULL");

    const u32 luHeaderBytes = (sizeof(void*) == 4) ? 16u : static_cast<u32>((sizeof(KeyBlock) + 15u) & ~15u);
    const u32 luCarveQwords = (luHeaderBytes + KU_KEYBLOCK_CAPACITY * 8u) >> 4;

    KeyBlock* lpBlock = reinterpret_cast<KeyBlock*>(mpDispatchBin->AllocateMemoryFast(luCarveQwords));

    if (mpBlockListTail != 0)
    {
        mpBlockListTail->mpNext = lpBlock;
    }
    else
    {
        mpBlockListHead = lpBlock;
    }
    mpBlockListTail = lpBlock;

    lpBlock->mpKeys     = reinterpret_cast<u64*>(reinterpret_cast<u8*>(lpBlock) + luHeaderBytes);
    lpBlock->muCount    = 0;
    lpBlock->muCapacity = KU_KEYBLOCK_CAPACITY;
    lpBlock->mpNext     = 0;
    return this;
}

// @ 0x827FA7D0
// Flatten every key block into one 128-byte-aligned record array carved from the
// list's own bin, collapse the block chain onto that array (the head block is
// rewritten to cover it: count == capacity == muCount, next == NULL, tail == head),
// publish it as mpSortedKeys and fill the sort-job parameter block.
DispatchList* DispatchList::PrepareSortJobInfo(SortJobInfo* lpJobInfo)
{
    lpJobInfo->mpaFlatKeys     = 0;
    lpJobInfo->muKeyCount      = muCount;
    lpJobInfo->mpBlockListHead = mpBlockListHead;

    mpSortedKeys = 0;

    if (muCount != 0)
    {
        // X360 carve: (8*count + 399) >> 4 quad-words, then align the pointer up to
        // 128 (the extra 399/16 qwords carry the alignment slop).
        void* lpRaw = mpDispatchBin->AllocateMemoryFast((8u * muCount + 399u) >> 4);
        u64*  lpaFlat = reinterpret_cast<u64*>(
            (reinterpret_cast<uintptr_t>(lpRaw) + 127u) & ~static_cast<uintptr_t>(127u));
        lpJobInfo->mpaFlatKeys = lpaFlat;

        u32 luKeysCopied = 0;
        for (KeyBlock* lpBlock = mpBlockListHead; lpBlock != 0; lpBlock = lpBlock->mpNext)
        {
            for (u32 luKey = 0; luKey < lpBlock->muCount; ++luKey)
            {
                lpaFlat[luKeysCopied + luKey] = lpBlock->mpKeys[luKey];
            }
            luKeysCopied += lpBlock->muCount;
        }
        CGS_ASSERT(luKeysCopied == muCount, "luKeysCopied == muTotalKeyCount");

        mpBlockListHead->mpKeys     = lpaFlat;
        mpBlockListHead->muCount    = muCount;
        mpBlockListHead->muCapacity = muCount;
        mpBlockListHead->mpNext     = 0;
        mpBlockListTail = mpBlockListHead;
        mpSortedKeys    = lpaFlat;
    }
    return this;
}

// [PC leaf] The X360 sorts each prepared list on a RadixSort job (RadixSortEntry
// @0x82AD2020, packaged by BrnRendererModule sub_823F5EA0). This remains the
// fallback when native sort jobs are disabled. Despite the
// job name, ARTIST RadixSortJob::Execute @0x82AD2818 calls std::_Sort<u64*,int>.
// Sorting the complete records also orders equal material keys by packet offset;
// identical records refer to the same packet and require no stable-sort storage.
void DispatchList::SortForDispatch()
{
    renderengine::FrameProfile::Scope lSortProfile(renderengine::FrameProfile::DISPATCH_SORT);
    SortJobInfo lJobInfo;
    PrepareSortJobInfo(&lJobInfo);
    if (mpSortedKeys != 0 && muCount > 1)
    {
        std::sort(mpSortedKeys, mpSortedKeys + muCount);
    }
}

// ARTIST827EE868..96C, recovered directly from the image (the JSON export has a
// hole). Save the current chain, then clear it for the next shared block. r6 is
// unused on X360: keys already carry offsets from the master bin.
void DispatchList::RelocateForMainMemory(uintptr_t luBinBase, uintptr_t luBinOutput,
                                        uintptr_t /*luBinMaster*/)
{
    const uintptr_t luDelta = luBinOutput - luBinBase;
    if (luDelta != 0)
    {
        for (KeyBlock* lpBlock = mpBlockListHead; lpBlock != nullptr;)
        {
            KeyBlock* lpNext = lpBlock->mpNext;
            if (lpBlock->mpKeys)
                lpBlock->mpKeys = reinterpret_cast<u64*>(reinterpret_cast<uintptr_t>(lpBlock->mpKeys) + luDelta);
            if (lpNext)
                lpBlock->mpNext = reinterpret_cast<KeyBlock*>(reinterpret_cast<uintptr_t>(lpNext) + luDelta);
            lpBlock = lpNext;
        }
        if (mpBlockListHead)
            mpBlockListHead = reinterpret_cast<KeyBlock*>(reinterpret_cast<uintptr_t>(mpBlockListHead) + luDelta);
        if (mpBlockListTail)
            mpBlockListTail = reinterpret_cast<KeyBlock*>(reinterpret_cast<uintptr_t>(mpBlockListTail) + luDelta);
    }
    if (mpBlockListHead)
    {
        if (muChainBlockCount < KU_MAX_BLOCKS_PER_CHAIN)
        {
            mapChainBlockArray[muChainBlockCount++] = mpBlockListHead;
        }
        else
        {
            // FLAG PC-platform leaf: the original 64-head array is too small
            // for extended-detail PC scenes. Coalesce additional chains into
            // its last entry. The previous block is already copied to shared
            // memory; do not dereference this block's relocated tail until its
            // own FlushBlockToSharedMemory copy has completed. The consumer
            // still traverses ordinary KeyBlocks, after all jobs have joined.
            CGS_ASSERT(muChainBlockCount == KU_MAX_BLOCKS_PER_CHAIN && mpRelocatedChainTailPC,
                       "Full saved chain array has a relocated tail");
            mpRelocatedChainTailPC->mpNext = mpBlockListHead;
        }
        mpRelocatedChainTailPC = mpBlockListTail;
    }
    muCount = 0;
    mpBlockListHead = mpBlockListTail = nullptr;
}

// ARTIST827E9438: concatenate saved chains and recount their keys after joining
// the conversion jobs. Each chain still contains ordinary 64-key blocks.
void DispatchList::ReconnectChainBlocks()
{
    CGS_ASSERT(mpBlockListHead == nullptr, "mpBlockListHead == NULL");
    CGS_ASSERT(mpBlockListTail == nullptr, "mpBlockListTail == NULL");
    muCount = 0;
    for (u32 luChain = 0; luChain < muChainBlockCount; ++luChain)
    {
        KeyBlock* lpBlock = mapChainBlockArray[luChain];
        for (;;)
        {
            CGS_ASSERT(lpBlock->muCapacity <= KU_KEYBLOCK_CAPACITY,
                       "lpBlockWithinChain->muKeyCapacity <= KU_MAX_KEYS_PER_BLOCK");
            muCount += lpBlock->muCount;
            if (!lpBlock->mpNext) break;
            lpBlock = lpBlock->mpNext;
        }
        if (luChain + 1u < muChainBlockCount)
            lpBlock->mpNext = mapChainBlockArray[luChain + 1u];
        else
            mpBlockListTail = lpBlock;
    }
    if (muChainBlockCount)
        mpBlockListHead = mapChainBlockArray[0];
}

// ARTIST827E9590..96A0: Append transfers ownership of the source chain.
void DispatchList::Append(DispatchList* lpOther)
{
    CGS_ASSERT(lpOther != nullptr, "lpOther != NULL");
    CGS_ASSERT(lpOther != this, "lpOther != this");
    if (lpOther->mpBlockListHead)
    {
        if (mpBlockListTail)
        {
            CGS_ASSERT(mpBlockListTail->mpNext == nullptr, "mpBlockListTail->mpNext == NULL");
            mpBlockListTail->mpNext = lpOther->mpBlockListHead;
        }
        else
        {
            CGS_ASSERT(mpBlockListHead == nullptr, "mpBlockListHead == NULL");
            mpBlockListHead = lpOther->mpBlockListHead;
        }
        mpBlockListTail = lpOther->mpBlockListTail;
        muCount += lpOther->muCount;
        lpOther->muCount = 0;
        lpOther->mpBlockListHead = lpOther->mpBlockListTail = nullptr;
    }
}

} // namespace CgsGraphics
