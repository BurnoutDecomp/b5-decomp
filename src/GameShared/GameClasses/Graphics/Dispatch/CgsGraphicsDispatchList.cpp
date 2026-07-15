#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// CgsGraphicsDispatchList.cpp
//
// Bodies for the CgsGraphics::DispatchList sort-key bucket, reconstructed
// store-for-store from the BURNOUT_X360_ARTIST.XEX disassembly:
//
//   CgsGraphics::DispatchList::ReserveKey  @ 0x822A0788
//   CgsGraphics::DispatchList::Submit      @ 0x822A0808
//
// A DispatchList accumulates 64-bit sort records (one per Submit) into a chain of
// fixed-capacity KeyBlocks. ReserveKey guarantees the tail block has room (rolling
// to a fresh block via AllocateKeyBlock when full); Submit packs the sort key with
// the packet's bin-local quad-word offset and stores it at the tail's write head.
// AllocateKeyBlock is owned by a separate TU (declared in CgsDispatcher.h).
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

} // namespace CgsGraphics
