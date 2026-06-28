// ===========================================================================
// EATech Apt -- AptActionInterpreter ActionScript bitwise opcodes.
//   DECOMPILED from the PS3 EXTERNAL ELF:
//     _FunctionAptActionBitAnd    @0x806BCC
//     _FunctionAptActionBitOr     @0x806A40
//     _FunctionAptActionBitXor    @0x8068B4
//     _FunctionAptActionBitLShift @0x806728
//     _FunctionAptActionBitRShift @0x80659C
//
// All identical: apply the SWF-v7 undefined rule (an undefined operand yields
// `undefined` unless gpUndefinedValue is null), coerce both operands to integers
// (toInteger), compute `under OP top` (the shift ops use top as the shift count),
// and push AptInteger(result). Unlike the logic/arithmetic handlers, the stack
// collapse here is exactly stackPopAndPush(2, result) -- the compiler inlined that
// primitive -- so it is called directly.
//
// SHIFTS: the recovered expression is `under << top` / `under >> top` (RShift is
// arithmetic -- the dividend is signed). ActionScript shift counts are 0-31; the
// console PPC shift masks the count to 6 bits in hardware (count>=32 -> 0). The
// direct C++ shift is kept faithful to the decompiled pseudocode.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create

// FLAG (wired at AptInit; see AptValueConvert.cpp).
extern AptValue*    gpUndefinedValue;
extern unsigned int AptGetSwfVersion();

namespace
{
    // Compute UNLESS (v7 && some operand undefined && gpUndefinedValue available).
    inline bool ShouldCompute(const AptValue* pTop, const AptValue* pUnder)
    {
        if (AptGetSwfVersion() == 7 &&
            !(pTop->getIsDefined() && pUnder->getIsDefined()) &&
            gpUndefinedValue)
            return false;
        return true;
    }
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBitAnd @0x806BCC -- under & top.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionBitAnd(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        int top   = pTop->toInteger();
        int under = pUnder->toInteger();
        pResult = AptInteger::Create(under & top);
    }
    pInterp->stackPopAndPush(2, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBitOr @0x806A40 -- under | top.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionBitOr(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        int top   = pTop->toInteger();
        int under = pUnder->toInteger();
        pResult = AptInteger::Create(under | top);
    }
    pInterp->stackPopAndPush(2, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBitXor @0x8068B4 -- under ^ top.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionBitXor(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        int top   = pTop->toInteger();
        int under = pUnder->toInteger();
        pResult = AptInteger::Create(under ^ top);
    }
    pInterp->stackPopAndPush(2, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBitLShift @0x806728 -- under << top.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionBitLShift(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        int shift = pTop->toInteger();
        int under = pUnder->toInteger();
        pResult = AptInteger::Create(under << shift);
    }
    pInterp->stackPopAndPush(2, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionBitRShift @0x80659C -- under >> top (arithmetic).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionBitRShift(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        int shift = pTop->toInteger();
        int under = pUnder->toInteger();
        pResult = AptInteger::Create(under >> shift);
    }
    pInterp->stackPopAndPush(2, pResult);
}
