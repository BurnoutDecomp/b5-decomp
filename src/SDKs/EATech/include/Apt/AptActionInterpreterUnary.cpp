// ActionScript unary numeric opcodes.

// Retain the original out-of-line pop boundary and shared epilogue.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"

extern AptValue* gpUndefinedValue;
extern int gnAptSwfVersion;

void AptActionInterpreter::_FunctionAptActionToInteger(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pValue = pInterp->stackAt(0);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && !pValue->getIsDefined())
        pResult = gpUndefinedValue;
    if (!pResult)
        pResult = AptInteger::Create(pValue->toInteger());
    pInterp->stackPop();
    pInterp->stackPush(pResult);
}

void AptActionInterpreter::_FunctionAptActionIncrement(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pValue = pInterp->stackAt(0);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && !pValue->getIsDefined())
        pResult = gpUndefinedValue;
    if (!pResult)
    {
        if (pValue->isInteger())
            pResult = AptInteger::Create(pValue->toInteger() + 1);
        else
            pResult = AptFloat::Create(pValue->toFloat() + 1.0f);
    }
    pInterp->stackPop();
    pInterp->stackPush(pResult);
}

void AptActionInterpreter::_FunctionAptActionDecrement(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pValue = pInterp->stackAt(0);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && !pValue->getIsDefined())
        pResult = gpUndefinedValue;
    if (!pResult)
    {
        if (pValue->isInteger())
            pResult = AptInteger::Create(pValue->toInteger() - 1);
        else
            pResult = AptFloat::Create(pValue->toFloat() - 1.0f);
    }
    pInterp->stackPop();
    pInterp->stackPush(pResult);
}
