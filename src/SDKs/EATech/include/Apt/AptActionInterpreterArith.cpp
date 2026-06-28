// ===========================================================================
// EATech Apt -- AptActionInterpreter ActionScript arithmetic opcodes.
//   DECOMPILED from the PS3 EXTERNAL ELF:
//     _FunctionAptActionAdd      @0x808C90
//     _FunctionAptActionSubtract @0x808A8C
//     _FunctionAptActionMultiply @0x808888
//     _FunctionAptActionDivide   @0x8086C4
//     _FunctionAptActionModulo   @0x8084F8
//
// Binary ops over the top two operands -- top = the value pushed last, under = the
// value pushed first; the op is `under OP top` (so subtraction/division are
// under-minus-top / under-over-top). They share the And/Or shape:
//   * peek both operands;
//   * SWF v7 only: an undefined operand makes the result undefined (unless
//     gpUndefinedValue is null), short-circuiting the compute;
//   * Add/Subtract/Multiply take an integer fast path when BOTH operands are
//     defined ints (exact integer result), else coerce via toFloat;
//   * Divide/Modulo are float-only and return `undefined` when the divisor (top)
//     coerces to zero;
//   * collapse: release the two operands, push the result (AddRef).
//
// The stack collapse is inlined to mirror the asm (the compiler inlined it). The
// console's `4*top + base` byte addressing is element indexing here (x64-correct,
// AptValue* is 8 bytes).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // Create + c_integer()->GetInt()
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"     // Create

#include <cmath>   // fmodf

// FLAG (wired at AptInit; see AptValueConvert.cpp).
extern AptValue*    gpUndefinedValue;
extern unsigned int AptGetSwfVersion();

namespace
{
    // Both operands present, defined, and integer-typed -> the integer fast path.
    inline bool BothDefinedInts(const AptValue* pTop, const AptValue* pUnder)
    {
        return pTop->getVtblIndex()   == AptVFT_Integer && pTop->getIsDefined()
            && pUnder->getVtblIndex() == AptVFT_Integer && pUnder->getIsDefined();
    }

    // The shared SWF-v7 undefined rule: compute UNLESS (v7 && some operand
    // undefined && gpUndefinedValue is available).
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
// _FunctionAptActionAdd @0x808C90 -- under + top.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionAdd(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        if (BothDefinedInts(pTop, pUnder))
            pResult = AptInteger::Create(pTop->c_integer()->GetInt() + pUnder->c_integer()->GetInt());
        else
            pResult = AptFloat::Create(pTop->toFloat() + pUnder->toFloat());
    }

    int top = pInterp->mnStackTop;
    if (top >= 2)
    {
        pInterp->mpStack[top - 1]->Release();
        pInterp->mpStack[top - 2]->Release();
        top -= 2;
        pInterp->mnStackTop = top;
    }
    pInterp->mpStack[top] = pResult;
    pInterp->mnStackTop = top + 1;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionSubtract @0x808A8C -- under - top.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionSubtract(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        if (BothDefinedInts(pTop, pUnder))
            pResult = AptInteger::Create(pUnder->c_integer()->GetInt() - pTop->c_integer()->GetInt());
        else
            pResult = AptFloat::Create(pUnder->toFloat() - pTop->toFloat());
    }

    int top = pInterp->mnStackTop;
    if (top >= 2)
    {
        pInterp->mpStack[top - 1]->Release();
        pInterp->mpStack[top - 2]->Release();
        top -= 2;
        pInterp->mnStackTop = top;
    }
    pInterp->mpStack[top] = pResult;
    pInterp->mnStackTop = top + 1;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionMultiply @0x808888 -- under * top.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionMultiply(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        if (BothDefinedInts(pTop, pUnder))
            pResult = AptInteger::Create(pTop->c_integer()->GetInt() * pUnder->c_integer()->GetInt());
        else
            pResult = AptFloat::Create(pTop->toFloat() * pUnder->toFloat());
    }

    int top = pInterp->mnStackTop;
    if (top >= 2)
    {
        pInterp->mpStack[top - 1]->Release();
        pInterp->mpStack[top - 2]->Release();
        top -= 2;
        pInterp->mnStackTop = top;
    }
    pInterp->mpStack[top] = pResult;
    pInterp->mnStackTop = top + 1;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionDivide @0x8086C4 -- under / top (float only; top==0 -> undefined).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDivide(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        float fDivisor = pTop->toFloat();
        float fNum     = pUnder->toFloat();
        if (fDivisor != 0.0f)
            pResult = AptFloat::Create(fNum / fDivisor);
        // else: divide by zero -> the gpUndefinedValue default.
    }

    int top = pInterp->mnStackTop;
    if (top >= 2)
    {
        pInterp->mpStack[top - 1]->Release();
        pInterp->mpStack[top - 2]->Release();
        top -= 2;
        pInterp->mnStackTop = top;
    }
    pInterp->mpStack[top] = pResult;
    pInterp->mnStackTop = top + 1;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionModulo @0x8084F8 -- fmod(under, top) (float only; top==0 ->
// undefined). The divisor (top) is coerced first; only on a non-zero divisor is
// the dividend (under) coerced.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionModulo(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        float fDivisor = pTop->toFloat();
        if (fDivisor != 0.0f)
            pResult = AptFloat::Create(fmodf(pUnder->toFloat(), fDivisor));
        // else: modulo by zero -> the gpUndefinedValue default.
    }

    int top = pInterp->mnStackTop;
    if (top >= 2)
    {
        pInterp->mpStack[top - 1]->Release();
        pInterp->mpStack[top - 2]->Release();
        top -= 2;
        pInterp->mnStackTop = top;
    }
    pInterp->mpStack[top] = pResult;
    pInterp->mnStackTop = top + 1;
    pResult->AddRef();
}
