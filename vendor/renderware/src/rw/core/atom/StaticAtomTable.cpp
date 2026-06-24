#include "rw/core/atom/StaticAtomTable.h"

#include "rw/RwHash32String.h"  // rw::RwHash32String (the hashing primitive @ 0x82BBC458)

// ===========================================================================
// rw::core::atom::StaticAtomTable -- out-of-line member home.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//     HashIDArrayIndex  @ 0x82C478A8
//     Register          @ 0x82C47900
//     FindID            @ 0x82C47A40
//     At                @ 0x82C47B88
//
// All four are scalar integer code (no VMX). The table is an open-addressing
// linear-probe hash of NUL-terminated names to 16-bit atom ids; see the layout
// and per-member notes in rw/core/atom/StaticAtomTable.h.
// ===========================================================================

namespace rw
{
namespace core
{
namespace atom
{

// FNV-1a 32-bit offset basis, the seed the X360 loads from dword_82180F38 and
// passes to RwHash32String (-2128831035 == 0x811C9DC5).
static const uint32_t KU_FNV_OFFSET_BASIS = 0x811C9DC5u;

// ---------------------------------------------------------------------------
// HashIDArrayIndex @ 0x82C478A8
//   v3 = RwHash32String(key, 0x811C9DC5)        ; FNV-1a over the name
//   v4 = mpData->muHashSlotCount  (lhz +0x10)
//   return (v3 % v4)                              ; divwu, result masked to 16 bits
// ---------------------------------------------------------------------------
uint32_t StaticAtomTable::HashIDArrayIndex(const char* lpcKey) const
{
    const uint32_t luHash = rw::RwHash32String(lpcKey, KU_FNV_OFFSET_BASIS);
    const uint32_t luSlotCount = mpData->muHashSlotCount;
    return luHash % luSlotCount;
}

// ---------------------------------------------------------------------------
// FindID @ 0x82C47A40
//
//   slot = startSlot = HashIDArrayIndex(key)
//   loop:
//     id = mpData->mpHashSlots[slot];  *lpuOutId = id      (sth +0x0C[2*slot])
//     if (id == 0xFFFF) return false                       ; empty slot: miss
//     s = mpData->mpStringPoolBase + mpData->mpIdToOffset[id]   (offset 4*id)
//     compare s against key byte-by-byte (break at the key's NUL)
//     if (equal) return true
//     slot = (slot + 1) % mpData->muHashSlotCount          ; linear probe
//     if (slot == startSlot) return false                  ; wrapped: full miss
// ---------------------------------------------------------------------------
bool StaticAtomTable::FindID(const char* lpcKey, uint16_t* lpuOutId) const
{
    const uint32_t luStartSlot = HashIDArrayIndex(lpcKey);
    uint32_t luSlot = luStartSlot;

    while (true)
    {
        const uint16_t luId = mpData->mpHashSlots[luSlot];
        *lpuOutId = luId;
        if (luId == KU_EMPTY_SLOT)
        {
            return false;
        }

        // Resolve the stored key's bytes and compare against lpcKey. The X360
        // walks until the probe key hits its NUL; the difference at the first
        // mismatch (or at the shared NUL) decides equality.
        const uint8_t* lpStored = mpData->mpStringPoolBase + mpData->mpIdToOffset[luId];
        const uint8_t* lpProbe  = reinterpret_cast<const uint8_t*>(lpcKey);
        int liDiff;
        do
        {
            liDiff = static_cast<int>(*lpProbe) - static_cast<int>(*lpStored);
            if (*lpProbe == 0)
            {
                break;
            }
            ++lpProbe;
            ++lpStored;
        }
        while (liDiff == 0);

        if (liDiff == 0)
        {
            return true;
        }

        luSlot = (luSlot + 1) % mpData->muHashSlotCount;
        if (luSlot == luStartSlot)
        {
            return false;
        }
    }
}

// ---------------------------------------------------------------------------
// At @ 0x82C47B88
//   FindID(key, &id);  return id;                  (lhz the u16 the probe wrote)
// Returns the atom id FindID stored (0xFFFF when it stopped on an empty slot).
// ---------------------------------------------------------------------------
uint16_t StaticAtomTable::At(const char* lpcKey) const
{
    uint16_t luId;
    FindID(lpcKey, &luId);
    return luId;
}

// ---------------------------------------------------------------------------
// Register @ 0x82C47900
//
//   slot = startSlot = HashIDArrayIndex(key)
//   if (mpHashSlots[slot] == 0xFFFF) -> insert at `slot`
//   else linear-probe: for each occupied slot compare the stored key; if it
//        matches, this key is already interned -> stop; otherwise advance the
//        slot (mod slotCount) until an empty slot is found -> insert there.
//
//   insert:
//     len = strlen(key)
//     mpHashSlots[slot] = luAtomId                         (sth, 2*slot)
//     mpIdToOffset[luAtomId] = cursor - poolBase            (stw, 4*id)
//     copy key (incl. NUL) to cursor
//     cursor += len + 1
//     --mpData->muFreeSlots                                 (sth +0x12)
// ---------------------------------------------------------------------------
void StaticAtomTable::Register(const char* lpcKey, uint32_t luAtomId)
{
    const uint32_t luStartSlot = HashIDArrayIndex(lpcKey);
    uint32_t luSlot = luStartSlot;

    uint16_t luSlotId = mpData->mpHashSlots[luSlot];
    if (luSlotId != KU_EMPTY_SLOT)
    {
        // Probe occupied slots; bail out early if the key is already interned.
        while (true)
        {
            const uint8_t* lpStored = mpData->mpStringPoolBase + mpData->mpIdToOffset[luSlotId];
            const uint8_t* lpProbe  = reinterpret_cast<const uint8_t*>(lpcKey);
            int liDiff;
            do
            {
                liDiff = static_cast<int>(*lpProbe) - static_cast<int>(*lpStored);
                if (*lpProbe == 0)
                {
                    break;
                }
                ++lpProbe;
                ++lpStored;
            }
            while (liDiff == 0);

            if (liDiff == 0)
            {
                // Already present: the X360 returns without inserting.
                return;
            }

            luSlot = (luSlot + 1) % mpData->muHashSlotCount;
            luSlotId = mpData->mpHashSlots[luSlot];
            if (luSlotId == KU_EMPTY_SLOT)
            {
                break;  // found the empty slot to insert into
            }
        }
    }

    // Insert at the empty slot `luSlot`.
    const uint8_t* lpKeyEnd = reinterpret_cast<const uint8_t*>(lpcKey);
    while (*lpKeyEnd)
    {
        ++lpKeyEnd;
    }
    const uint32_t luLength =
        static_cast<uint32_t>(lpKeyEnd - reinterpret_cast<const uint8_t*>(lpcKey));

    mpData->mpHashSlots[luSlot] = static_cast<uint16_t>(luAtomId);
    mpData->mpIdToOffset[luAtomId] =
        static_cast<uint32_t>(mpData->mpStringPoolCursor - mpData->mpStringPoolBase);

    // Copy the key including its terminating NUL into the string pool.
    const uint8_t* lpSrc = reinterpret_cast<const uint8_t*>(lpcKey);
    uint8_t*       lpDst = mpData->mpStringPoolCursor;
    uint8_t        luByte;
    do
    {
        luByte  = *lpSrc++;
        *lpDst++ = luByte;
    }
    while (luByte != 0);

    mpData->mpStringPoolCursor += luLength + 1;
    --mpData->muFreeSlots;
}

}  // namespace atom
}  // namespace core
}  // namespace rw
