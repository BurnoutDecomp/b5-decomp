#include "SDKs/XGraphics/XGraphicsVRegTable.h"

#include "SDKs/XGraphics/XGraphicsInternalHashTable.h" // InternalHashTable::Lookup/Insert
#include "SDKs/XGraphics/XGraphicsVReg.h"              // VRegInfo (miIndex/miType, ::Make)

// ===========================================================================
// XGRAPHICS::VRegTable -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsVRegTable.h for the attested layout. Each store / branch below is
// store-for-store with the X360 asm; every member is accessed by NAME.
//
// The hash table stores each vreg by its pointer used as a u32 key (X360 is
// 32-bit); the truncating cast on insert and the widening cast on lookup are the
// same idiom used across this SDK's u32-slot containers (see XGraphicsVReg.cpp).
//
// NOT reconstructed here: RemoveConstant @ 0x82C28530 -- it reads the constant
// node's +0x3A0 component array to pick a per-count constant table, which needs
// the un-homed IRLoadConst layout; reproducing that with a raw-offset read of a
// live C++ object would be an invention, so it is left blocked.
// ===========================================================================

namespace XGRAPHICS
{

// Truncate a vreg pointer to the u32 the hash table keys on (X360 32-bit slot).
static inline u32 VRegKey(const VRegInfo* apReg)
{
    return static_cast<u32>(reinterpret_cast<uintptr_t>(apReg));
}

// Widen a u32 hash-table entry back to a vreg pointer.
static inline VRegInfo* VRegFromKey(u32 auKey)
{
    return reinterpret_cast<VRegInfo*>(static_cast<uintptr_t>(auKey));
}

VRegInfo* VRegTable::Find(const u32* apRegDescs, s32 aiReg)
{
    // Stamp the reusable prototype with the descriptor's index/type words, then
    // hash it into the main table. (apRegDescs is an opaque word array walked by
    // register index -- word aiReg+14 is the index, word aiReg+20 the type.)
    mpScratch->miIndex = static_cast<s32>(apRegDescs[aiReg + 14]);
    mpScratch->miType  = static_cast<s32>(apRegDescs[aiReg + 20]);
    return VRegFromKey(mpMainTable->Lookup(VRegKey(mpScratch)));
}

VRegInfo* VRegTable::Create(s32 aiType, s32 aiIndex)
{
    VRegInfo* lpReg = VRegInfo::Make(aiIndex, aiType, mpContext);
    mpMainTable->Insert(VRegKey(lpReg));
    return lpReg;
}

VRegInfo* VRegTable::FindOrCreate(s32 aiType, s32 aiIndex)
{
    // Stamp the prototype and probe the main table; on a miss, materialise it.
    mpScratch->miIndex = aiIndex;
    mpScratch->miType  = aiType;

    VRegInfo* lpReg = VRegFromKey(mpMainTable->Lookup(VRegKey(mpScratch)));
    if (lpReg == nullptr)
        lpReg = Create(aiType, aiIndex);
    return lpReg;
}

} // namespace XGRAPHICS
