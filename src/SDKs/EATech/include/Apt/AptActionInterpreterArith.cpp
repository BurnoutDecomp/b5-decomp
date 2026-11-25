// ActionScript binary numeric opcodes.

// Retain the original out-of-line pop boundary and shared epilogue.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"

#include <cmath>

extern AptValue* gpUndefinedValue;
extern int gnAptSwfVersion;

void AptActionInterpreter::_FunctionAptActionAdd(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && (!pTop->getIsDefined() || !pUnder->getIsDefined()))
        pResult = gpUndefinedValue;

    if (!pResult)
    {
        if (pTop->isInteger() && pUnder->isInteger())
        {
            const int nTop = pTop->c_integer()->GetInt();
            const int nUnder = pUnder->c_integer()->GetInt();
            pResult = AptInteger::Create(nUnder + nTop);
        }
        else
        {
            const float fTop = pTop->toFloat();
            const float fUnder = pUnder->toFloat();
            pResult = AptFloat::Create(fUnder + fTop);
        }
    }
    pInterp->stackPop(2);
    pInterp->stackPush(pResult);
}

void AptActionInterpreter::_FunctionAptActionSubtract(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && (!pTop->getIsDefined() || !pUnder->getIsDefined()))
        pResult = gpUndefinedValue;

    if (!pResult)
    {
        if (pTop->isInteger() && pUnder->isInteger())
        {
            const int nTop = pTop->c_integer()->GetInt();
            const int nUnder = pUnder->c_integer()->GetInt();
            pResult = AptInteger::Create(nUnder - nTop);
        }
        else
        {
            const float fTop = pTop->toFloat();
            const float fUnder = pUnder->toFloat();
            pResult = AptFloat::Create(fUnder - fTop);
        }
    }
    pInterp->stackPop(2);
    pInterp->stackPush(pResult);
}

void AptActionInterpreter::_FunctionAptActionMultiply(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && (!pTop->getIsDefined() || !pUnder->getIsDefined()))
        pResult = gpUndefinedValue;

    if (!pResult)
    {
        if (pTop->isInteger() && pUnder->isInteger())
        {
            const int nTop = pTop->c_integer()->GetInt();
            const int nUnder = pUnder->c_integer()->GetInt();
            pResult = AptInteger::Create(nUnder * nTop);
        }
        else
        {
            const float fTop = pTop->toFloat();
            const float fUnder = pUnder->toFloat();
            pResult = AptFloat::Create(fUnder * fTop);
        }
    }
    pInterp->stackPop(2);
    pInterp->stackPush(pResult);
}

void AptActionInterpreter::_FunctionAptActionDivide(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && (!pTop->getIsDefined() || !pUnder->getIsDefined()))
        pResult = gpUndefinedValue;

    if (!pResult)
    {
        const float fTop = pTop->toFloat();
        const float fUnder = pUnder->toFloat();
        if (fTop == 0.0f)
            pResult = gpUndefinedValue;
        else
            pResult = AptFloat::Create(fUnder / fTop);
    }
    pInterp->stackPop(2);
    pInterp->stackPush(pResult);
}

void AptActionInterpreter::_FunctionAptActionModulo(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && (!pTop->getIsDefined() || !pUnder->getIsDefined()))
        pResult = gpUndefinedValue;

    if (!pResult)
    {
        const float fTop = pTop->toFloat();
        if (fTop == 0.0f)
            pResult = gpUndefinedValue;
        else
            pResult = AptFloat::Create(fmodf(pUnder->toFloat(), fTop));
    }
    pInterp->stackPop(2);
    pInterp->stackPush(pResult);
}
