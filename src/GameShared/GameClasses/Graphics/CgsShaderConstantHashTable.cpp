#include "GameShared/GameClasses/Graphics/CgsShaderConstantHashTable.h"

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
void ShaderConstantHashTable::FixUp(u8* lpBaseData)
{
    const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

    mpuHashKeys = reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(mpuHashKeys) + luBase);
    mppcNames = reinterpret_cast<char**>(reinterpret_cast<uintptr_t>(mppcNames) + luBase);

    if (muSize)
    {
        for (u32 luI = 0; luI < muSize; ++luI)
        {
            mppcNames[luI] = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(mppcNames[luI]) + luBase);
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
    s32 liMinIndex = 0;
    s32 liMaxIndex = static_cast<s32>(muSize) - 1;
    s32 liSearchSize = liMaxIndex;

    // Binary search while the window spans more than four entries.
    while (liSearchSize > 4)
    {
        const s32 liPivot = liSearchSize / 2 + liMinIndex;
        const u32 luPivotHash = mpuHashKeys[liPivot];

        if (luHash == luPivotHash)
        {
            return mppcNames[liPivot];
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
    for (const u32* lpuKey = &mpuHashKeys[liMinIndex]; luHash != *lpuKey; ++lpuKey)
    {
        if (++liI > liMaxIndex)
        {
            return 0;
        }
    }

    return mppcNames[liI];
}

} // namespace CgsGraphics
