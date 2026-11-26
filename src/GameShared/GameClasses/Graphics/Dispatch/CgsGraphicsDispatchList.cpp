#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include <cstring>
#include <algorithm>   // std::sort (ARTIST RadixSortJob::Execute uses std::_Sort)
#include "types.hpp"     // uintptr_t (128-byte alignment of the flat key array)
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

namespace
{
    bool SortRecordLess(const SortKey& lrLeft, const SortKey& lrRight)
    {
        return lrLeft.mu64Whole < lrRight.mu64Whole;
    }
}

static const char KAC_ARTIST_ASSERT_CPP[] = "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\graphics\\Dispatch/CgsDispatcher.cpp";
static const char KAC_ARTIST_ASSERT_HEADER[] = "..\\..\\..\\GameShared\\GameClasses\\Graphics\\Dispatch\\CgsDispatcher.h";

// @ 0x822A0788
// Ensure the tail key block can hold one more record: allocate a fresh block when
// the tail is missing or full, then assert the tail exists.
void DispatchList::ReserveKey()
{
    SortKeyBlock* lpTail = mpBlockListTail;
    if (lpTail == NULL || lpTail->muKeyCount >= lpTail->muKeyCapacity)
    {
        AllocateKeyBlock();
    }

    CGS_ASSERT_AT(mpBlockListTail != NULL, "mpBlockListTail != NULL", KAC_ARTIST_ASSERT_HEADER, 559);
}

// @ 0x822A0808
// Submit a packet under a sort key: pack (sortKey << 20 | packetLocalOffset) into
// the tail block's next slot, then advance the block and list counts.
void DispatchList::Submit(u64 lu64SortKey, DispatchCommand* lpPacket)
{
    CGS_ASSERT_AT(lpPacket != NULL, "lpPacket != NULL", KAC_ARTIST_ASSERT_HEADER, 567);

    ReserveKey();

    CGS_ASSERT_AT(mpBlockListTail != NULL, "mpBlockListTail != NULL", KAC_ARTIST_ASSERT_HEADER, 571);

    // Packet offset within the bin, in quad-words (DispatchCommand pointer diff
    // already scales by sizeof(DispatchCommand) == 16 == the PPC `>> 4`).
    const s32 liPacketLocalOffset = lpPacket - m_pBinBase;

    const u64 luKey =
        (lu64SortKey << SortKey::KU_SHIFT_KEY)
        | (static_cast<u64>(liPacketLocalOffset) & SortKey::KU_MASK_OFFSET);

    // Store the record first, then range-check the offset (matches the asm order).
    SortKeyBlock* lpTail = mpBlockListTail;
    lpTail->mpKeyArray[lpTail->muKeyCount].mu64Whole = luKey;

    CGS_ASSERT_AT(static_cast<u32>(liPacketLocalOffset) <= SortKey::KU_MASK_OFFSET,
                  "uint32_t(liPacketLocalOffset) <= SortKey::KU_MASK_OFFSET",
                  KAC_ARTIST_ASSERT_HEADER, 588);

    ++mpBlockListTail->muKeyCount;
    ++muCount;
}

// @ 0x827FA730
// Append a fresh 64-record SortKeyBlock to the chain and make it the tail. The block
// is carved from the list's bin: X360 allocates 33 quad-words (= the 16-byte block
// header + 64 * 8-byte keys) with the key array starting right after the header.
// The x64 gate sizes the header by the host struct (semantic parity: header first,
// then the 64-key array, whole carve rounded up to quad-words).
void DispatchList::AllocateKeyBlock()
{
    CGS_ASSERT_AT(mpDispatchBin != NULL, "mpDispatchBin != NULL", KAC_ARTIST_ASSERT_CPP, 163);

    const u32 luCarveQwords = (sizeof(SortKeyBlock) + KU_KEYBLOCK_CAPACITY * sizeof(SortKey)) >> 4;
    SortKeyBlock* const lpBlock = static_cast<SortKeyBlock*>(mpDispatchBin->AllocateMemoryFast(luCarveQwords));
    SortKeyBlock* const lpPreviousListTail = mpBlockListTail;
    SortKey* const lpKeys = reinterpret_cast<SortKey*>(lpBlock + 1);
    if (lpPreviousListTail == 0)
        mpBlockListHead = lpBlock;
    else
        lpPreviousListTail->mpNextBlock = lpBlock;
    mpBlockListTail = lpBlock;
    lpBlock->mpKeyArray = lpKeys;
    lpBlock->muKeyCount    = 0;
    lpBlock->muKeyCapacity = KU_KEYBLOCK_CAPACITY;
    lpBlock->mpNextBlock     = 0;
}

// @ 0x827FA7D0
// Flatten every key block into one 128-byte-aligned record array carved from the
// list's own bin, collapse the block chain onto that array (the head block is
// rewritten to cover it: count == capacity == muCount, next == NULL, tail == head),
// publish it as mpaSortedKeyArray and fill the sort-job parameter block.
void DispatchList::PrepareSortJobInfo(SortJobInputOutput& lrJobInfo)
{
    lrJobInfo.mpSortedPointerArray     = 0;
    lrJobInfo.muSortedPointerCount      = muCount;
    lrJobInfo.mpKeyBlockListHeader = mpBlockListHead;

    mpaSortedKeyArray = 0;

    if (muCount != 0)
    {
        // X360 carve: (8*count + 399) >> 4 quad-words, then align the pointer up to
        // 128 (the extra 399/16 qwords carry the alignment slop).
        void* lpRaw = mpDispatchBin->AllocateMemoryFast((8u * muCount + 399u) >> 4);
        SortKey* lpaFlat = reinterpret_cast<SortKey*>(
            (reinterpret_cast<uintptr_t>(lpRaw) + 127u) & ~static_cast<uintptr_t>(127u));
        lrJobInfo.mpSortedPointerArray = lpaFlat;

        u32 luKeysCopied = 0;
        for (SortKeyBlock* lpBlock = mpBlockListHead; lpBlock != 0; lpBlock = lpBlock->mpNextBlock)
        {
            std::memcpy(lrJobInfo.mpSortedPointerArray + luKeysCopied, lpBlock->mpKeyArray,
                lpBlock->muKeyCount * sizeof(*lpBlock->mpKeyArray));
            luKeysCopied += lpBlock->muKeyCount;
        }
        CGS_ASSERT(luKeysCopied == muCount,
            CGS_ASSERT_MESSAGE_AT("luKeysCopied == muTotalKeyCount", KAC_ARTIST_ASSERT_CPP, 317));

        mpBlockListHead->mpKeyArray     = lrJobInfo.mpSortedPointerArray;
        mpBlockListHead->muKeyCount    = muCount;
        mpBlockListHead->muKeyCapacity = muCount;
        mpBlockListHead->mpNextBlock     = 0;
        mpBlockListTail = mpBlockListHead;
        mpaSortedKeyArray    = lrJobInfo.mpSortedPointerArray;
    }
}

// @0x82AD2020, packaged by BrnRendererModule sub_823F5EA0). This remains the
// fallback when native sort jobs are disabled. Despite the
// job name, ARTIST RadixSortJob::Execute @0x82AD2818 calls std::_Sort<u64*,int>.
// Sorting the complete records also orders equal material keys by packet offset;
// identical records refer to the same packet and require no stable-sort storage.
void DispatchList::SortForDispatch()
{
    SortJobInputOutput lJobInfo;
    PrepareSortJobInfo(lJobInfo);
    if (mpaSortedKeyArray != 0 && muCount > 1)
    {
        std::sort(mpaSortedKeyArray, mpaSortedKeyArray + muCount, SortRecordLess);
    }
}

// ARTIST827E9438: concatenate saved chains and recount their keys after joining
// the conversion jobs. Each chain still contains ordinary 64-key blocks.
void DispatchList::ReconnectChainBlocks()
{
    CGS_ASSERT(mpBlockListHead == NULL,
        CGS_ASSERT_MESSAGE_AT("mpBlockListHead == NULL", KAC_ARTIST_ASSERT_CPP, 237));
    CGS_ASSERT(mpBlockListTail == NULL,
        CGS_ASSERT_MESSAGE_AT("mpBlockListTail == NULL", KAC_ARTIST_ASSERT_CPP, 238));

    muCount = 0;
    if (muChainBlockCount != 0)
    {
        for (u32 luChain = 0; luChain < muChainBlockCount; ++luChain)
        {
            SortKeyBlock* lpBlockWithinChain = mapChainBlockArray[luChain];
            CGS_ASSERT(lpBlockWithinChain->muKeyCapacity <= KU_KEYBLOCK_CAPACITY,
                CGS_ASSERT_MESSAGE_AT("lpBlockWithinChain->muKeyCapacity <= KU_MAX_KEYS_PER_BLOCK",
                    KAC_ARTIST_ASSERT_CPP, 249));
            while (lpBlockWithinChain->mpNextBlock != NULL)
            {
                CGS_ASSERT(lpBlockWithinChain->muKeyCapacity <= KU_KEYBLOCK_CAPACITY,
                    CGS_ASSERT_MESSAGE_AT("lpBlockWithinChain->muKeyCapacity <= KU_MAX_KEYS_PER_BLOCK",
                        KAC_ARTIST_ASSERT_CPP, 253));
                muCount += lpBlockWithinChain->muKeyCount;
                lpBlockWithinChain = lpBlockWithinChain->mpNextBlock;
            }
            muCount += lpBlockWithinChain->muKeyCount;
            if (luChain + 1 < muChainBlockCount)
                lpBlockWithinChain->mpNextBlock = mapChainBlockArray[luChain + 1];
            else
                mpBlockListTail = lpBlockWithinChain;
        }
        mpBlockListHead = mapChainBlockArray[0];
    }
}

// ARTIST827E9590..96A0: Append transfers ownership of the source chain. Each branch takes
// the source tail itself, as the PS3 DecFIGS body (._ZN11CgsGraphics12DispatchList6AppendEPS0_)
// shows.
void DispatchList::Append(DispatchList* lpListToAppend)
{
    CGS_ASSERT_AT(lpListToAppend != 0, "lpListToAppend != NULL", KAC_ARTIST_ASSERT_CPP, 485);
    CGS_ASSERT_AT(lpListToAppend != this, "lpListToAppend != this", KAC_ARTIST_ASSERT_CPP, 486);
    if (lpListToAppend->mpBlockListHead)
    {
        if (mpBlockListTail)
        {
            CGS_ASSERT_AT(mpBlockListTail->mpNextBlock == 0, "mpBlockListTail->mpNextBlock == NULL",
                          KAC_ARTIST_ASSERT_CPP, 498);
            mpBlockListTail->mpNextBlock = lpListToAppend->mpBlockListHead;
            mpBlockListTail = lpListToAppend->mpBlockListTail;
        }
        else
        {
            CGS_ASSERT_AT(mpBlockListHead == 0, "mpBlockListHead == NULL", KAC_ARTIST_ASSERT_CPP, 505);
            mpBlockListHead = lpListToAppend->mpBlockListHead;
            mpBlockListTail = lpListToAppend->mpBlockListTail;
        }
        muCount += lpListToAppend->muCount;
        lpListToAppend->muCount = 0;
        lpListToAppend->mpBlockListHead = 0;
        lpListToAppend->mpBlockListTail = 0;
    }
}

} // namespace CgsGraphics
