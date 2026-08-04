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
