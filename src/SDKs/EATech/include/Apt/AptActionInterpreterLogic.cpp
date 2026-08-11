// ===========================================================================
// EATech Apt -- AptActionInterpreter ActionScript logic opcodes.
//   DECOMPILED from the PS3 EXTERNAL ELF:
//     _FunctionAptActionNot @0x7F3BB8
//     _FunctionAptActionAnd @0x7FD4D4
//     _FunctionAptActionOr  @0x7FD29C
//
// Each pops its operand(s) off the interpreter's evaluation stack, coerces them,
// and pushes an AptBoolean. And/Or share one shape:
//   * peek the top two operands (top = a, top-1 = b) without consuming them;
//   * SWF v7 only: if either operand is undefined the result is `undefined`
//     (unless gpUndefinedValue is null), short-circuiting the compute;
//   * fast path -- both defined integers: compare the raw ints (And: both non-zero,
//     Or: either non-zero);
//   * otherwise coerce both with toFloat and test against zero;
//   * collapse: release the two operands, push the AptBoolean result (AddRef).
// Not is the unary form: push !toBool(top).
//
// The console selects AptBoolean::spBooleanTrue / spBooleanFalse directly; here
// that is AptBoolean::Create(b), which returns those same shared singletons. The
// stack collapse is written inline to mirror the asm exactly (the compiler inlined
// it rather than calling stackPopAndPush).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // c_integer()->GetInt()

// The `undefined` singleton (AptGlobals.cpp, built at AptInit) and the active
// movie's SWF version (AptGetSwfVersion, AptLinker.cpp).
extern AptValue*    gpUndefinedValue;
extern unsigned int AptGetSwfVersion();

// ---------------------------------------------------------------------------
// _FunctionAptActionNot @0x7F3BB8 -- push !toBool(top), replacing the operand.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionNot(AptActionInterpreter* pInterp, LocalContextT*)
{
    bool b = pInterp->mpStack[pInterp->mnStackTop - 1]->toBool();
    AptValue* pResult = AptBoolean::Create(!b);

    int top = pInterp->mnStackTop;
    if (top > 0)
    {
        pInterp->mpStack[top - 1]->Release();
        --top;
        pInterp->mnStackTop = top;
    }
    pInterp->mpStack[top] = pResult;
    pInterp->mnStackTop = top + 1;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionAnd @0x7FD4D4 -- (top-1) && (top) -> AptBoolean.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionAnd(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pA = pInterp->mpStack[pInterp->mnStackTop - 1];   // top
    AptValue* pB = pInterp->mpStack[pInterp->mnStackTop - 2];   // below top

    AptValue* pResult = gpUndefinedValue;
    bool bCompute = true;

    // SWF v7: an undefined operand makes the result undefined.
    if (AptGetSwfVersion() == 7 &&
        !(pA->getIsDefined() && pB->getIsDefined()) &&
        gpUndefinedValue)
    {
        bCompute = false;
    }

    if (bCompute)
    {
        if (pA->getVtblIndex() == AptVFT_Integer && pA->getIsDefined() &&
            pB->getVtblIndex() == AptVFT_Integer && pB->getIsDefined())
        {
            pResult = AptBoolean::Create(pA->c_integer()->GetInt() && pB->c_integer()->GetInt());
        }
        else
        {
            float fa = pA->toFloat();
            float fb = pB->toFloat();
            pResult = AptBoolean::Create(!(fa == 0.0f || fb == 0.0f));
        }
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
// _FunctionAptActionOr @0x7FD29C -- (top-1) || (top) -> AptBoolean.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionOr(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pA = pInterp->mpStack[pInterp->mnStackTop - 1];   // top
    AptValue* pB = pInterp->mpStack[pInterp->mnStackTop - 2];   // below top

    AptValue* pResult = gpUndefinedValue;
    bool bCompute = true;

    if (AptGetSwfVersion() == 7 &&
        !(pA->getIsDefined() && pB->getIsDefined()) &&
        gpUndefinedValue)
    {
        bCompute = false;
    }

    if (bCompute)
    {
        if (pA->getVtblIndex() == AptVFT_Integer && pA->getIsDefined() &&
            pB->getVtblIndex() == AptVFT_Integer && pB->getIsDefined())
        {
            pResult = AptBoolean::Create((pA->c_integer()->GetInt() | pB->c_integer()->GetInt()) != 0);
        }
        else
        {
            float fa = pA->toFloat();
            float fb = pB->toFloat();
            pResult = AptBoolean::Create(!(fa == 0.0f && fb == 0.0f));
        }
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
