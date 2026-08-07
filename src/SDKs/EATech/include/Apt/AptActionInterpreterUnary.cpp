// ===========================================================================
// EATech Apt -- AptActionInterpreter ActionScript unary numeric opcodes.
//   DECOMPILED from the PS3 EXTERNAL ELF:
//     _FunctionAptActionIncrement @0x80817C
//     _FunctionAptActionDecrement @0x807FF4
//     _FunctionAptActionToInteger @0x806F04
//
// Each pops one operand, coerces it, and pushes the result (the unary CollapseOne:
// release the operand, push the result with AddRef). Same SWF-v7 undefined rule as
// the binary ops, but on the single operand. Increment/Decrement keep an integer
// result when the operand is a defined int (else AptFloat +/- 1.0); ToInteger
// always produces AptInteger(toInteger(x)).
//
// ToNumber @0x808304 is deferred -- it Get_ToStrings the value and inspects it for
// a decimal point (UTF8_Find ".") + uses _isNaN, pulling in the toString/string
// layer that is not yet reconstructed.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"     // AptFloat::Create

// gpUndefinedValue: AptGlobals.cpp (built at AptInit). AptGetSwfVersion: AptLinker.cpp.
extern AptValue*    gpUndefinedValue;
extern unsigned int AptGetSwfVersion();

namespace
{
    // Unary v7 rule: compute UNLESS (v7 && operand undefined && gpUndefinedValue).
    inline bool ShouldComputeUnary(const AptValue* pTop)
    {
        if (AptGetSwfVersion() == 7 && !pTop->getIsDefined() && gpUndefinedValue)
            return false;
        return true;
    }

    // Release the single operand, push the result (AddRef) -- the unary collapse.
    inline void CollapseOne(AptActionInterpreter* pInterp, AptValue* pResult)
    {
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
}

// ---------------------------------------------------------------------------
// _FunctionAptActionIncrement @0x80817C -- x + 1.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionIncrement(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldComputeUnary(pTop))
    {
        if (pTop->getVtblIndex() == AptVFT_Integer && pTop->getIsDefined())
            pResult = AptInteger::Create(pTop->toInteger() + 1);
        else
            pResult = AptFloat::Create(pTop->toFloat() + 1.0f);
    }
    CollapseOne(pInterp, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionDecrement @0x807FF4 -- x - 1.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDecrement(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldComputeUnary(pTop))
    {
        if (pTop->getVtblIndex() == AptVFT_Integer && pTop->getIsDefined())
            pResult = AptInteger::Create(pTop->toInteger() - 1);
        else
            pResult = AptFloat::Create(pTop->toFloat() - 1.0f);
    }
    CollapseOne(pInterp, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionToInteger @0x806F04 -- AptInteger(toInteger(x)).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionToInteger(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldComputeUnary(pTop))
        pResult = AptInteger::Create(pTop->toInteger());

    CollapseOne(pInterp, pResult);
}
