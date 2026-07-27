#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <algorithm>   // std::stable_sort (the PC synchronous RadixSort stand-in)
#include <cstdint>     // uintptr_t (128-byte alignment of the flat key array)

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
//   (+ SortForDispatch, the PC synchronous stand-in for the RadixSort job)
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
DispatchList* DispatchList::Submit(s32 li32SortKey, DispatchCommand* lpPacket)
{
    CGS_ASSERT(lpPacket != NULL, "lpPacket != NULL");

    ReserveKey();

    CGS_ASSERT(mpBlockListTail != NULL, "mpBlockListTail != NULL");

    // Packet offset within the bin, in quad-words (DispatchCommand pointer diff
    // already scales by sizeof(DispatchCommand) == 16 == the PPC `>> 4`).
    const u32 luPacketLocalOffset = static_cast<u32>(lpPacket - m_pBinBase);

    const u64 luKey =
        (static_cast<u64>(static_cast<u32>(li32SortKey)) << SortKey::KU_SHIFT_KEY)
        | (luPacketLocalOffset & SortKey::KU_MASK_OFFSET);

    // Store the record first, then range-check the offset (matches the asm order).
    KeyBlock* lpTail = mpBlockListTail;
    lpTail->mpKeys[lpTail->muCount] = luKey;

    CGS_ASSERT(luPacketLocalOffset <= SortKey::KU_MASK_OFFSET,
               "uint32_t(liPacketLocalOffset) <= SortKey::KU_MASK_OFFSET");

    ++lpTail->muCount;
    ++muCount;
    return this;
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
// @0x82AD2020, packaged by BrnRendererModule sub_823F5EA0). The PC bring-up has no
// job scheduler yet, so the same prepare + ascending stable sort runs synchronously
// here. Radix sort is a stable ascending sort over the u64 records, which
// std::stable_sort reproduces order-for-order.
void DispatchList::SortForDispatch()
{
    SortJobInfo lJobInfo;
    PrepareSortJobInfo(&lJobInfo);
    if (mpSortedKeys != 0 && muCount > 1)
    {
        std::stable_sort(mpSortedKeys, mpSortedKeys + muCount);
    }
}

// @ 0x827EE868 -- the per-list half of the SPU main-memory relocation pass
// (DispatchFrame::RelocateForMainMemory @0x827EE970 calls it once per list; that
// caller IS recovered and lives in CgsDispatcherCommands.cpp).
//
// NOT RECONSTRUCTED: 0x827EE868 carries no entry in .ida-exports/
// BURNOUT_X360_ARTIST.XEX (the address is known only as the callee name in the
// caller's xrefs_from), so there is no pseudocode/asm to reconstruct from and no
// honest body can be written. The whole relocation pass is the PS3/SPU shared-bin
// path: it rebases a job-produced list's bin/key pointers from local-store
// addresses into main memory. The PC build never produces a job-side frame
// (ConvertObjectsToMeshes runs the single-threaded fallback), so this is
// unreachable here -- it exists only to close the recovered caller's link edge.
DispatchList* DispatchList::RelocateForMainMemory(u32 /*luBinBase*/, u32 /*luBinOffset*/,
                                                  u32 /*luListOffset*/)
{
    CGS_ASSERT(false,
               "DispatchList::RelocateForMainMemory: X360 body @0x827EE868 absent from the "
               "IDA export set (SPU shared-bin relocation path; unreachable on PC)");
    return this;
}

} // namespace CgsGraphics
