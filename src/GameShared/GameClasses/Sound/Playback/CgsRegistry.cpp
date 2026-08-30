#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"
#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"

#include <cstddef>
#include <cstring>

// CgsSound::Playback::Registry out-of-line members.
//
// Reconstructed store-for-store from the X360:
//   Registry::Registry(const RegistrySpec&)  @ 0x82692C38
//   Registry::AddEntity(const Entity&)        @ 0x82692DA0
//
// HOST-WIDTH FLAG: on X360 the table is carved in place out of the Registry's own
// memory image -- the slot array, the entity data blob, and the string table all
// live contiguously after the 7-word header (the asm forms mpu8Data as
// `this + (capacity + 7) words` and mpcStringTable as `mpu8Data + muDataSize`).
// That byte arithmetic is 4-byte-pointer specific. The reconstruction keeps the
// SEMANTICS by NAME (slots cleared, data ptr just past the slots, string-table
// ptr just past the data) but derives the data pointer from the named slot array,
// not from a hand-computed `(capacity + 7) * 4` offset, so it is faithful on the
// 64-bit host. FLAGGED where the absolute X360 offset would have been assumed.
namespace CgsSound
{
namespace Playback
{

// =============================================================================
// Registry::Registry  @ 0x82692C38
// =============================================================================
Registry::Registry(const RegistrySpec& lSpec)
{
    // *a1 = 0; a1[1] = *a2; a1[2] = a2[1]; a1[4] = a2[2].
    mu32EntityCount    = 0;
    mu32EntityCapacity = lSpec.mu32EntityCount;
    muDataSize         = lSpec.muDataSize;
    muStringTableSize  = lSpec.muStringTableSize;

    // Zero every hash-table slot. X360: the `do { *v3 = 0; ... } while (v4 <
    // capacity)` loop over a1[7..7+capacity).
    const Entity** lppSlot = GetFirstEntity();
    for (u32 luI = 0; luI < mu32EntityCapacity; ++luI)
    {
        lppSlot[luI] = 0;
    }

    // mpu8Data = address just past the slot array (X360: this + (capacity + 7)
    // words). FLAG (host-width): derived from the named slot array end rather than
    // the X360 `4 * (capacity + 7)` byte offset.
    mpu8Data = GetDataStart();

    // mpcStringTable = mpu8Data + muDataSize when a string table exists, else null.
    // X360: `if (muStringTableSize) v9 = a1 + muDataSize + v7; else v9 = 0`.
    if (muStringTableSize)
    {
        mpcStringTable = GetStringTableStart();
    }
    else
    {
        mpcStringTable = 0;
    }

    // assert rw::IsMemAligned(mpu8Data, KU32_DEFAULT_ALIGNMENT) (CgsRegistry.h:289).
    // X360: `(v8 & 3) != 0` -> the data pointer must be 4-byte aligned. FLAG: the
    // rw::IsMemAligned/KU32_DEFAULT_ALIGNMENT pair is not yet homed in src; the
    // low-2-bits test below is the faithful X360 predicate for the default
    // (4-byte) alignment it was instantiated with.
    CGS_ASSERT((reinterpret_cast<uintptr_t>(mpu8Data) & 3u) == 0u,
               "rw::IsMemAligned(mpu8Data, KU32_DEFAULT_ALIGNMENT)");

    // assert capacity > 0 and a power of two (CgsRegistry.h:293).
    // X360: fail when `!v10 || ((v10 - 1) & v10) != 0`.
    CGS_ASSERT(mu32EntityCapacity != 0 &&
                   ((mu32EntityCapacity - 1) & mu32EntityCapacity) == 0,
               "Length of hash table must be more than 0 and a power of 2\n");

    // a1[6] = capacity - 1.
    muNameHashMask = mu32EntityCapacity - 1;
}

// =============================================================================
// Registry::AddEntity  @ 0x82692DA0
// =============================================================================
bool Registry::AddEntity(const Entity& lEntity)
{
    // if (mu32EntityCount >= mu32EntityCapacity) return false. X360: `if (*a1 <
    // a1[1])` guards the whole body; the fall-through (loc_82692F10) returns 0.
    if (mu32EntityCount >= mu32EntityCapacity)
    {
        return false;
    }

    // The entity must be initialised: its type-name must be set. X360 tests
    // `a2[1]` (mTypeName.mHash) != 0 (CgsRegistry.h:305).
    CGS_ASSERT(lEntity.mTypeName.GetValue() != 0, "Improperly initialized Entity.");

    // Masked hash = (mName.mHash >> 1) & muNameHashMask. X360: `(*a2 >> 1) & a1[6]`.
    uintptr_t luMaskedHash = (lEntity.mName.GetValue() >> 1) & muNameHashMask;

    // The masked hash must land inside the slot array (CgsRegistry.h:311).
    CGS_ASSERT(luMaskedHash < mu32EntityCapacity, "Masked hash has gone out of range\n");

    const Entity** lppSlot = GetFirstEntity();
    u32 luIndex;

    // First probe span: from the masked hash up to capacity, looking for an empty
    // slot. X360: starts at v11 = &slots[v6], scans while *v11 != 0, stops when
    // v10 reaches capacity (then wraps via LABEL_10).
    bool lbFound = false;
    if (luMaskedHash < mu32EntityCapacity)
    {
        for (luIndex = static_cast<u32>(luMaskedHash);
             luIndex < mu32EntityCapacity;
             ++luIndex)
        {
            if (lppSlot[luIndex] == 0)
            {
                lbFound = true;
                break;
            }
        }
    }

    // Wrap span: from 0 up to the masked hash. X360 LABEL_10: v10 = 0; if (v6)
    // scan slots[0..v6) for an empty slot. If v6 == 0 (nothing to wrap into) the
    // table is full -> return 0.
    if (!lbFound)
    {
        if (luMaskedHash == 0)
        {
            return false;
        }
        for (luIndex = 0; luIndex < static_cast<u32>(luMaskedHash); ++luIndex)
        {
            if (lppSlot[luIndex] == 0)
            {
                lbFound = true;
                break;
            }
        }
        if (!lbFound)
        {
            return false;
        }
    }

    // Insert. X360 LABEL_16: slots[idx] = &entity; ++count; return 1.
    lppSlot[luIndex] = &lEntity;
    ++mu32EntityCount;
    return true;
}

bool Registry::Contains(const Entity& arEntity) const
{
    const u8* lpu8Entity = reinterpret_cast<const u8*>(&arEntity);
    return lpu8Entity >= GetDataStart() && lpu8Entity < mpu8Data;
}

void Registry::FixUp()
{
    const uintptr_t luBase = reinterpret_cast<uintptr_t>(this);
    mpcStringTable = reinterpret_cast<char*>(
        luBase + reinterpret_cast<uintptr_t>(mpcStringTable));
    mpu8Data = reinterpret_cast<u8*>(
        luBase + reinterpret_cast<uintptr_t>(mpu8Data));

    const Entity** lppEntity = GetFirstEntity();
    for (u32 luI = 0; luI < mu32EntityCapacity; ++luI)
    {
        if (lppEntity[luI] == 0)
            continue;

        lppEntity[luI] = reinterpret_cast<const Entity*>(
            luBase + reinterpret_cast<uintptr_t>(lppEntity[luI]));
        Entity& lrEntity = *const_cast<Entity*>(lppEntity[luI]);
        const IEntityFixer* lpFixer = IEntityFixer::GetFixer(lrEntity.mTypeName);
        CGS_ASSERT(lpFixer != 0, "lpFixer");
        if (lpFixer != 0)
            lpFixer->FixUp(lrEntity);
    }
}

void Registry::Resolve(const Registry& arRegistry)
{
    const Entity* const* lppEntity = GetFirstEntity();
    for (u32 luI = 0; luI < mu32EntityCapacity; ++luI)
    {
        if (lppEntity[luI] == 0)
            continue;

        Entity& lrEntity = *const_cast<Entity*>(lppEntity[luI]);
        const IEntityFixer* lpFixer = IEntityFixer::GetFixer(lrEntity.mTypeName);
        CGS_ASSERT(lpFixer != 0, "lpFixer");
        if (lpFixer != 0)
            lpFixer->Resolve(lrEntity, arRegistry);
    }
}

const Entity* Registry::Import(u8* apu8Base, const Entity& arEntity,
                               const Registry& arFrom) const
{
    const std::ptrdiff_t liOffset =
        reinterpret_cast<const u8*>(&arEntity) - arFrom.GetDataStart();
    Entity* lpImported = reinterpret_cast<Entity*>(apu8Base + liOffset);
    const IEntityFixer* lpFixer = IEntityFixer::GetFixer(arEntity.mTypeName);
    CGS_ASSERT(lpFixer != 0, "lpFixer");
    if (lpFixer != 0)
        lpFixer->Relocate(*lpImported, apu8Base, *this, arFrom);
    return lpImported;
}

Registry& Registry::operator+=(Registry& arOther)
{
    arOther.Resolve(*this);

    CGS_ASSERT(mpu8Data != 0, "mpu8Data");
    CGS_ASSERT(arOther.mpu8Data != 0, "lOther.mpu8Data");
    CGS_ASSERT(arOther.mu32EntityCount <=
                   (mu32EntityCapacity - mu32EntityCount),
               "The Registry has overgrown it's entity limits");

    const size_t luOtherDataUsed = static_cast<size_t>(
        arOther.mpu8Data - arOther.GetDataStart());
    const size_t luDataRemaining = static_cast<size_t>(
        GetDataStart() + muDataSize - mpu8Data);
    CGS_ASSERT(luOtherDataUsed <= luDataRemaining,
               "The Registry has overgrown it's size");

    u8* lpu8DestinationBase = mpu8Data;
    std::memcpy(lpu8DestinationBase, arOther.GetDataStart(), luOtherDataUsed);
    mpu8Data += luOtherDataUsed;

    const size_t luOtherStringsUsed = arOther.muStringTableSize != 0
        ? static_cast<size_t>(arOther.mpcStringTable - arOther.GetStringTableStart())
        : 0u;
    const size_t luStringsUsed = muStringTableSize != 0
        ? static_cast<size_t>(mpcStringTable - GetStringTableStart())
        : 0u;
    // ARTIST @0x826C0650 conditionally copies the string arena only when it
    // fits; the main/factory registries deliberately have zero string capacity
    // because Module::ImportStringTable owns those debug-name strings.  There is
    // no assert on the non-fitting branch.
    const size_t luStringsRemaining =
        muStringTableSize >= luStringsUsed
            ? muStringTableSize - luStringsUsed
            : 0u;
    if (luOtherStringsUsed != 0 && luOtherStringsUsed <= luStringsRemaining)
    {
        std::memcpy(mpcStringTable, arOther.GetStringTableStart(), luOtherStringsUsed);
        mpcStringTable += luOtherStringsUsed;
    }

    const Entity* const* lppOtherEntity = arOther.GetFirstEntity();
    for (u32 luI = 0; luI < arOther.mu32EntityCapacity; ++luI)
    {
        if (lppOtherEntity[luI] == 0)
            continue;

        const Entity* lpImported =
            Import(lpu8DestinationBase, *lppOtherEntity[luI], arOther);
        AddEntity(*lpImported);
    }
    return *this;
}

}
}
