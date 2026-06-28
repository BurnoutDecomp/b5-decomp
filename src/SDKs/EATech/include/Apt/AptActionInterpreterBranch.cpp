// ===========================================================================
// EATech Apt -- AptActionInterpreter ActionScript branch opcodes.
//   DECOMPILED from the PS3 EXTERNAL ELF:
//     _FunctionAptActionBranchAlways  @0x7F1C44
//     _FunctionAptActionBranchIfTrue  @0x7F349C
//     _FunctionAptActionBranchIfFalse @0x7F35B4
//
// The first handlers to drive the program counter (LocalContextT::mpProgramCounter).
// Each branch carries a 4-byte signed offset stored inline in the action bytecode
// at the next 4-byte-aligned position. The handler:
//   1. aligns the PC up to a 4-byte boundary,
//   2. reads the 32-bit offset there,
//   3. advances the PC past the offset (aligned + 4),
//   4. adds the offset to the PC when the branch is taken --
//        BranchAlways  : always;
//        BranchIfTrue  : when toBool(top) is true (then pops the condition);
//        BranchIfFalse : when toBool(top) is false (then pops the condition).
//
// x64 NOTE: the console aligns a 32-bit address (`(pc + 3) & 0xFFFFFFFC`); here the
// PC is a real 64-bit pointer into the loaded action stream, so the alignment is
// done on the full pointer width (`& ~(uintptr_t)3`). The offset is a byte delta
// within the stream, so PC + offset stays correct without a transcode.
//
// FLAG: the console then flushes the GC deferred-release vector (gpValuesToRelease)
// when the operand stack empties. That vector + its ReleaseValues (a layout
// distinct from AptValueVector) is not reconstructed yet, so the flush is deferred
// here -- it is GC housekeeping and does not affect branch control flow.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"

#include <cstdint>   // uintptr_t / int32_t

namespace
{
    // Read the inline 4-byte-aligned signed branch offset and advance the PC past
    // it; returns the offset (PC then points just after it).
    inline int32_t ReadBranchOffset(AptActionInterpreter::LocalContextT* pCtx)
    {
        const unsigned char* pAligned =
            reinterpret_cast<const unsigned char*>(
                (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
        int32_t offset = *reinterpret_cast<const int32_t*>(pAligned);
        pCtx->mpProgramCounter = pAligned + 4;
        return offset;
    }
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBranchAlways @0x7F1C44 -- unconditional jump.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionBranchAlways(AptActionInterpreter* /*pInterp*/, LocalContextT* pCtx)
{
    int32_t offset = ReadBranchOffset(pCtx);
    pCtx->mpProgramCounter += offset;
    // FLAG: console flushes gpValuesToRelease here when the operand stack is empty.
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBranchIfTrue @0x7F349C -- jump when the popped condition is true.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionBranchIfTrue(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    int32_t offset = ReadBranchOffset(pCtx);
    if (pInterp->mpStack[pInterp->mnStackTop - 1]->toBool())
        pCtx->mpProgramCounter += offset;
    pInterp->stackPop();   // release the condition
    // FLAG: console flushes gpValuesToRelease here when the operand stack is empty.
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBranchIfFalse @0x7F35B4 -- jump when the popped condition is false.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionBranchIfFalse(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    int32_t offset = ReadBranchOffset(pCtx);
    if (!pInterp->mpStack[pInterp->mnStackTop - 1]->toBool())
        pCtx->mpProgramCounter += offset;
    pInterp->stackPop();   // release the condition
    // FLAG: console flushes gpValuesToRelease here when the operand stack is empty.
}
