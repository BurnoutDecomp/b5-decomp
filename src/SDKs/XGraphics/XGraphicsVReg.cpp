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
    mpDefStack = lpStackMem ? new (lpStackMem) DefStack(apContext) : 0;

    void* lpUsesMem = ArenaAllocPrefixed(lpArena, sizeof(InternalVector));
    mpUses = lpUsesMem ? new (lpUsesMem) InternalVector(lpArena) : 0;

    void* lpDefsMem = ArenaAllocPrefixed(lpArena, sizeof(InternalVector));
    mpDefs = lpDefsMem ? new (lpDefsMem) InternalVector(lpArena) : 0;

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

// FOLDED FROM VRegInfo_wM_01.cpp
#include "SDKs/XGraphics/XGraphicsVReg.h"

// ===========================================================================
// XGRAPHICS::VRegInfo::Make @ 0x82C28E58 -- reconstructed from
// BURNOUT_X360_ARTIST.XEX. This part-file carries ONLY the factory dispatch;
// the register-type-info table it reads (`gaVRegTypeInfo`, declared `extern`
// in XGraphicsVReg.h) still has no definition in the tree -- see the block
// note at the bottom of this file.
//
// The whole function is these seven instructions (MEASURED, verbatim):
//     lis   r11, off_821B5100@ha
//     mulli r10, r4, 0xC                  ; row = aiType * sizeof(VRegTypeInfo)
//     addi  r11, r11, off_821B5100@l      ; r11 = &gaVRegTypeInfo[0]
//     addi  r11, r11, 8                   ; ... + offsetof(mpfnNewItem)
//     lwzx  r11, r10, r11                 ; load the row's factory pointer
//     mtctr r11
//     bctr                                ; TAIL-call it; r3/r4/r5 untouched
//
// Consequences read straight off that listing:
//   * No `this` -- r3 is the first *declared* argument, so Make is static, as
//     the header declares it (grounded further by its sole caller
//     VRegTable::Create @ 0x82C284E0, which passes three plain values).
//   * `bctr` (not `bctrl`) with no prologue and no register shuffling: r3/r4/r5
//     -- aiIndex, aiType, apContext -- are handed to the factory unchanged, and
//     its return value IS Make's return value.
//   * There is NO bound check on aiType anywhere in the dispatch, and no
//     null-check on the loaded pointer (row 4 "EI" holds a measured NULL). The
//     reconstruction adds neither: the binary does not have them.
//
// X360 CONSOLE LITERALS -- for reference only, NOT reproduced in the code: the
// 0xC stride is the 32-bit sizeof(VRegTypeInfo) and the 8 is the 32-bit
// offsetof(VRegTypeInfo, mpfnNewItem). Both widen on the x64 PC target, so the
// indexing below is done by array subscript + named member (semantic parity).
// ===========================================================================

namespace XGRAPHICS
{

VRegInfo* VRegInfo::Make(s32 aiIndex, s32 aiType, CFG* apContext)
{
    // Unchecked tail-dispatch through the register-type-info table: `aiType` is
    // the row index, and the row's factory receives Make's own arguments.
    return gaVRegTypeInfo[aiType].mpfnNewItem(aiIndex, aiType, apContext);
}

} // namespace XGRAPHICS

// ---------------------------------------------------------------------------
// STILL MISSING (link-time, invisible to `cl /c`): the definition of
//     const VRegTypeInfo gaVRegTypeInfo[KI_NUM_VREG_TYPES];
// Its 48 rows are fully MEASURED (X360 rodata @ 0x821B5100; the transcription
// is in scratchpad/waveM/VRegInfo.spec.md section 1) but the initializer cannot
// be compiled yet: 8 of the 17 factory classes (FixedValue, ExportValue,
// HosCoord, Physical, Interpolator, AutoIndexVtx, Resource, StandardIndex) have
// no header in b5-decomp/src at all, and the 9 that do declare `NewItem`
// returning their DERIVED type (`static TempValue* NewItem(...)`), which does
// not convert to VRegFactoryFn. The ready-to-merge table body is parked at
// scratchpad/waveM/parked/VRegInfo_01_gaVRegTypeInfo.cpp with the exact header
// lines that unblock it.
// ---------------------------------------------------------------------------
