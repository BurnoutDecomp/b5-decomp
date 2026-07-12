#include "SDKs/XGraphics/XGraphicsVRegTable.h"

#include "SDKs/XGraphics/XGraphicsInternalHashTable.h" // InternalHashTable::Lookup/Insert/Remove
#include "SDKs/XGraphics/XGraphicsVReg.h"              // VRegInfo (miIndex/miType, ::Make)
#include "SDKs/XGraphics/XGraphicsIRInst.h"            // IRInst (maUnk3A0 const-component array)

// ===========================================================================
// XGRAPHICS::VRegTable -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsVRegTable.h for the attested layout. Each store / branch below is
// store-for-store with the X360 asm; every member is accessed by NAME.
//
// The hash table stores each vreg by its pointer used as a u32 key (X360 is
// 32-bit); the truncating cast on insert and the widening cast on lookup are the
// same idiom used across this SDK's u32-slot containers (see XGraphicsVReg.cpp).
//
// RemoveConstant @ 0x82C28530 -- reconstructed now that IRInst names the +0x3A0
// component array (maUnk3A0) and InternalHashTable::Remove is homed. It counts the
// const node's live components (1..4), snapshots the four per-count constant
// tables, and removes the node (used as the u32 key -- the same 32-bit-slot idiom
// as the vreg tables) from the table for its component count.
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

void VRegTable::RemoveConstant(IRInst* apConst)
{
    // Count the const node's live components: its +0x3A0 component array holds up
    // to four words, non-null for each live component (1 == float .. 4 == vec4).
    // Faithful do-while: word 0 is always probed, so a genuine constant (the only
    // caller, IRLoadConst::Kill, gates on a real constant operand) yields >= 1.
    s32 liCount = 0;
    const s32* lpComponent = apConst->maUnk3A0;
    do
    {
        if (*lpComponent == 0)
            break;
        ++liCount;
        ++lpComponent;
    }
    while (liCount < 4);

    // Snapshot the four per-component-count constant tables (the asm materialises
    // them into a local array before indexing) and remove the node from the one
    // for its component count. The node pointer is the u32 hash key (32-bit slot).
    InternalHashTable* lapTables[4];
    lapTables[0] = mapConstTables[0];
    lapTables[1] = mapConstTables[1];
    lapTables[2] = mapConstTables[2];
    lapTables[3] = mapConstTables[3];

    lapTables[liCount - 1]->Remove(
        static_cast<u32>(reinterpret_cast<uintptr_t>(apConst)));
}

} // namespace XGRAPHICS
