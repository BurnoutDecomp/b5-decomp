// ===========================================================================
// EATech Apt -- AptActionInterpreter ActionScript branch opcodes.
//   ARTIST handlers:
//     _FunctionAptActionBranchAlways  @0x82ADE590
//     _FunctionAptActionBranchIfTrue  @0x82ADE500
//     _FunctionAptActionBranchIfFalse @0x82ADE8A0
//
// The first handlers to drive the program counter (LocalContextT::mpProgramCounter).
// Each branch carries a 4-byte signed offset stored inline in the action bytecode
// at the next 4-byte-aligned position. The handler:
//   1. aligns the PC up to a 4-byte boundary,
//   2. advances the PC past the aligned displacement slot,
//   3. reads and adds the signed displacement when the branch is taken --
//        BranchAlways  : always;
//        BranchIfTrue  : when toBool(top) is true (then pops the condition);
//        BranchIfFalse : when toBool(top) is false (then pops the condition).
//
// ARTIST 0x82ADE51C, 0x82ADE598 and 0x82ADE8BC advance the program counter
// before reading the aligned operand. The conditional opcodes only fetch the
// signed displacement when the branch is taken.

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"

namespace
{
    inline const int32_t* AdvancePastBranchOffset(AptActionInterpreter::LocalContextT* pCtx)
    {
        pCtx->mpProgramCounter = reinterpret_cast<const unsigned char*>(
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 3u) & ~static_cast<uintptr_t>(3u));
        const int32_t* pOffset = reinterpret_cast<const int32_t*>(pCtx->mpProgramCounter);
        pCtx->mpProgramCounter += sizeof(*pOffset);
        return pOffset;
    }
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBranchAlways @0x82ADE590 -- unconditional jump.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionBranchAlways(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    const int32_t* pOffset = AdvancePastBranchOffset(pCtx);
    pCtx->mpProgramCounter = *pOffset + pCtx->mpProgramCounter;
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBranchIfTrue @0x82ADE500 -- jump when the popped condition is true.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionBranchIfTrue(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    const int32_t* pOffset = AdvancePastBranchOffset(pCtx);
    if (pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 1]->toBool() == true)
        pCtx->mpProgramCounter = *pOffset + pCtx->mpProgramCounter;
    pInterp->stackPop();   // release the condition
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBranchIfFalse @0x82ADE8A0 -- jump when the popped condition is false.
// ---------------------------------------------------------------------------
// ARTIST uses the record-form Boolean test at 0x82ADE8E0.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

void AptActionInterpreter::_FunctionAptActionBranchIfFalse(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    const int32_t* pOffset = AdvancePastBranchOffset(pCtx);
    if (!pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 1]->toBool())
        pCtx->mpProgramCounter = *pOffset + pCtx->mpProgramCounter;
    pInterp->stackPop();   // release the condition
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
}
