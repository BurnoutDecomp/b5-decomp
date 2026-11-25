#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create

// The SWF version cache is owned by AptLinker.
extern AptValue*    gpUndefinedValue;
extern int gnAptSwfVersion;

void AptActionInterpreter::_FunctionAptActionBitAnd(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && (!pTop->getIsDefined() || !pUnder->getIsDefined()))
        pResult = gpUndefinedValue;
    if (!pResult)
    {
        const int nTop = pTop->toInteger();
        const int nUnder = pUnder->toInteger();
        pResult = AptInteger::Create(nUnder & nTop);
    }
    pInterp->mOperandStack.PopAndPush(2, pResult);
}

void AptActionInterpreter::_FunctionAptActionBitOr(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && (!pTop->getIsDefined() || !pUnder->getIsDefined()))
        pResult = gpUndefinedValue;
    if (!pResult)
    {
        const int nTop = pTop->toInteger();
        const int nUnder = pUnder->toInteger();
        pResult = AptInteger::Create(nUnder | nTop);
    }
    pInterp->mOperandStack.PopAndPush(2, pResult);
}

void AptActionInterpreter::_FunctionAptActionBitXor(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && (!pTop->getIsDefined() || !pUnder->getIsDefined()))
        pResult = gpUndefinedValue;
    if (!pResult)
    {
        const int nTop = pTop->toInteger();
        const int nUnder = pUnder->toInteger();
        pResult = AptInteger::Create(nUnder ^ nTop);
    }
    pInterp->mOperandStack.PopAndPush(2, pResult);
}

void AptActionInterpreter::_FunctionAptActionBitLShift(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && (!pUnder->getIsDefined() || !pTop->getIsDefined()))
        pResult = gpUndefinedValue;
    if (!pResult)
    {
        const int nTop = pTop->toInteger();
        const int nUnder = pUnder->toInteger();
        pResult = AptInteger::Create(nUnder << nTop);
    }
    pInterp->mOperandStack.PopAndPush(2, pResult);
}

void AptActionInterpreter::_FunctionAptActionBitRShift(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    if (gnAptSwfVersion == 7 && (!pUnder->getIsDefined() || !pTop->getIsDefined()))
        pResult = gpUndefinedValue;
    if (!pResult)
    {
        const int nTop = pTop->toInteger();
        const int nUnder = pUnder->toInteger();
        pResult = AptInteger::Create(nUnder >> nTop);
    }
    pInterp->mOperandStack.PopAndPush(2, pResult);
}
