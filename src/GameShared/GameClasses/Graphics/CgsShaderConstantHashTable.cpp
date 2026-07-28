#include "GameShared/GameClasses/Graphics/CgsShaderConstantHashTable.h"

// =============================================================================================
// SERIALISED-BLOCK SEAM (same class of bug, same remedy, as CgsShaderConstants.cpp's banner)
//
// A ShaderConstantHashTable is never constructed by the engine: it is a 12-BYTE WINDOW INSIDE a
// streamed resource image (ShaderTechniqueResourceType::FixUp @0x827EEB30 relocates the one at
// blob+128, between the six constant sub-blocks at +8..+96 and the sampler table at +140/+144).
// That image is the CONSOLE word layout -- three u32 slots at +0/+4/+8 -- on every platform,
// including the converted platform-4 bundles (tools/assets/shaders/FORMAT_MAP.md section 2:
// "the platform-4 data keeps the 32-bit console layout, byteswapped").
//
// The struct in the header is the HOST-width (x64) shape, so walking a streamed table through
// its members reads mppcNames across the +0x04/+0x08 word pair (the name-array offset spliced
// with muSize) and muSize out of the next block entirely. That is what faulted on the first
// staged SHADERS.BNDL load: FixUp dereferenced 0x00000254_03d53261 (a splice) inside the
// per-name relocation loop.
//
// So FixUp/FixDown/GetName -- everything that touches a table AS LOADED FROM DISC -- goes
// through the SerialisedHashTable view: the console word layout, with the u32 slots resolved
// through the project's low-4 GB PointerFromU32 convention (CgsMemory::LowMemory reserves the
// engine's whole root allocation below 4 GB).
// =============================================================================================
namespace
{
    // Console word layout of ShaderConstantHashTable (3 words).
    struct SerialisedHashTable
    {
        u32 muHashKeys;   // +0x00  u32*    sorted ascending, muSize entries
        u32 muNames;      // +0x04  char**  parallel name array, muSize entries
        u32 muSize;       // +0x08
    };

    inline u32* SlotU32Array(u32 luSlot)
    {
        return reinterpret_cast<u32*>(static_cast<uintptr_t>(luSlot));
    }
    inline const char* SlotString(u32 luSlot)
    {
        return reinterpret_cast<const char*>(static_cast<uintptr_t>(luSlot));
    }
}

namespace CgsGraphics
{

// CgsShaderConstantHashTable.cpp:33
// FixUp @ 0x827E9B20
// Relocate the streamed-in hash table: the key array, the name-pointer array,
// and every individual name pointer were saved as file-relative offsets; add
// the load-time base (lpBaseData) to turn them back into absolute pointers.
// In the X360 pseudocode the object is read as result[] DWORDs:
//   result[0] = mpuHashKeys (offset 0x00)
//   result[1] = mppcNames   (offset 0x04)
//   result[2] = muSize      (offset 0x08)
// -- which is exactly the SerialisedHashTable view (see the banner).
void ShaderConstantHashTable::FixUp(u8* lpBaseData)
{
    SerialisedHashTable* const lpTable = reinterpret_cast<SerialisedHashTable*>(this);
    const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lpBaseData));

    lpTable->muHashKeys += luDelta;
    lpTable->muNames    += luDelta;

    if (lpTable->muSize)
    {
        u32* const lpaNames = SlotU32Array(lpTable->muNames);
        for (u32 luI = 0; luI < lpTable->muSize; ++luI)
        {
            lpaNames[luI] += luDelta;
        }
    }
}

// CgsShaderConstantHashTable.cpp:67
// GetName @ 0x827E9B78
// Resolve a shader-constant hash to its debug name. mpuHashKeys is sorted
// ascending, so this does a binary search while the candidate window is wider
// than four entries, then falls back to a linear scan over the final small
// window. Returns the parallel mppcNames[] entry on a hit, or null if absent.
const char* ShaderConstantHashTable::GetName(u32 luHash) const
{
    // SERIALISED-BLOCK SEAM (see the banner): walk the console word layout.
    const SerialisedHashTable* const lpTable =
        reinterpret_cast<const SerialisedHashTable*>(this);
    const u32* const lpaHashKeys = SlotU32Array(lpTable->muHashKeys);
    const u32* const lpaNames    = SlotU32Array(lpTable->muNames);

    s32 liMinIndex = 0;
    s32 liMaxIndex = static_cast<s32>(lpTable->muSize) - 1;
    s32 liSearchSize = liMaxIndex;

    // Binary search while the window spans more than four entries.
    while (liSearchSize > 4)
    {
        const s32 liPivot = liSearchSize / 2 + liMinIndex;
        const u32 luPivotHash = lpaHashKeys[liPivot];

        if (luHash == luPivotHash)
        {
            return SlotString(lpaNames[liPivot]);
        }

        if (luHash <= luPivotHash)
        {
            liMaxIndex = liPivot - 1;
        }
        else
        {
            liMinIndex = liPivot + 1;
        }

        liSearchSize = liMaxIndex - liMinIndex;
    }

    // Linear scan over the remaining (<= 4 entry) window [liMinIndex..liMaxIndex].
    if (liMinIndex > liMaxIndex)
    {
        return 0;
    }

    s32 liI = liMinIndex;
    for (const u32* lpuKey = &lpaHashKeys[liMinIndex]; luHash != *lpuKey; ++lpuKey)
    {
        if (++liI > liMaxIndex)
        {
            return 0;
        }
    }

    return SlotString(lpaNames[liI]);
}

} // namespace CgsGraphics
