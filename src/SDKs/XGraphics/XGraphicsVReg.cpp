#include "SDKs/XGraphics/XGraphicsVReg.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h"          // Arena, ArenaAllocPrefixed/Free
#include "SDKs/XGraphics/XGraphicsCFG.h"            // CFG (owning context)
#include "SDKs/XGraphics/XGraphicsDefStack.h"       // DefStack
#include "SDKs/XGraphics/XGraphicsInternalVector.h" // InternalVector
#include "SDKs/XGraphics/XGraphicsIRInst.h"         // IRInst (BumpUses operand scan)

// ===========================================================================
// XGRAPHICS::VRegInfo -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsVReg.h for the attested layout. Each store / branch below is
// store-for-store with the X360 asm.
//
// NOTE (X360 pointer width): the use/def InternalVectors are generic u32-slot
// arrays; on X360 (32-bit) a slot holds an IRInst*. The stored value is written
// with an explicit truncating cast so it compiles on the x64 PC target. FLAG:
// widening these shared containers to pointer width is deferred -- InternalVector
// is also used for plain u32 indices elsewhere, so it stays a u32 container here.
// ===========================================================================

namespace XGRAPHICS
{

VRegInfo::VRegInfo(s32 aiIndex, s32 aiType, CFG* apContext)
{
    // Scalar init (the compiler sets the vptr on entry; +0x30..+0x50 are left to
    // the derived value-kind ctors, matching the asm which does not touch them).
    miIndex     = aiIndex; // +0x0C
    mbUnk04     = 0;       // +0x04
    mbUnk05     = 0;       // +0x05
    miUsage     = -1;      // +0x10
    miUsesCount = 0;       // +0x14
    miDefsCount = 0;       // +0x18
    mbUnk1C     = 0;       // +0x1C
    mbUnk1D     = 0;       // +0x1D
    miType      = aiType;  // +0x20

    Arena* lpArena = apContext->mpArena;

    // Def stack first (asm order), then the use and def instruction vectors --
    // each carved from the arena with a leading owner-arena pointer.
    void* lpStackMem = ArenaAllocPrefixed(lpArena, sizeof(DefStack));
    mpDefStack = lpStackMem ? new (lpStackMem) DefStack(apContext) : nullptr;

    void* lpUsesMem = ArenaAllocPrefixed(lpArena, sizeof(InternalVector));
    mpUses = lpUsesMem ? new (lpUsesMem) InternalVector(lpArena) : nullptr;

    void* lpDefsMem = ArenaAllocPrefixed(lpArena, sizeof(InternalVector));
    mpDefs = lpDefsMem ? new (lpDefsMem) InternalVector(lpArena) : nullptr;

    // Stamp this vreg with the graph's next id and advance the counter.
    miId = apContext->miNextVRegId++;
}

VRegInfo::~VRegInfo()
{
    // Only the def stack owns arena memory ~VRegInfo returns; the use/def vectors
    // are released with the arena in bulk (the asm frees only +0x2C).
    if (mpDefStack)
    {
        mpDefStack->~DefStack();
        ArenaFreePrefixed(mpDefStack);
    }
}

void VRegInfo::BumpDefs(IRInst* apInst)
{
    u32* lpSlot = mpDefs->Grow(mpDefs->muCount);
    *lpSlot = static_cast<u32>(reinterpret_cast<uintptr_t>(apInst)); // FLAG X360 32-bit ptr
    ++miDefsCount;
}

void VRegInfo::BumpUses(s32 aiNumOperands, IRInst* apInst)
{
    // Dedup: if this vreg already appears in one of the instruction's input
    // operand slots (1 .. aiNumOperands-1) it is already recorded as a use of
    // that instruction -- do not add it a second time.
    for (s32 liSlot = 1; liSlot < aiNumOperands; ++liSlot)
    {
        if (apInst->maRegs[liSlot] == this)
            return;
    }

    // First occurrence: record the using instruction.
    u32* lpSlot = mpUses->Grow(mpUses->muCount);
    *lpSlot = static_cast<u32>(reinterpret_cast<uintptr_t>(apInst)); // FLAG X360 32-bit ptr
    ++miUsesCount;
}

} // namespace XGRAPHICS
