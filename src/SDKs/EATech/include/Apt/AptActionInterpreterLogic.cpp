// Native ActionScript logical opcodes.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"

extern AptValue* gpUndefinedValue;
extern int gnAptSwfVersion;

void AptActionInterpreter::_FunctionAptActionNot(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pValue = pInterp->stackAt(0);
    AptValue* pResult = AptBoolean::Create(!pValue->toBool());
    pInterp->stackPop();
    pInterp->stackPush(pResult);
}

void AptActionInterpreter::_FunctionAptActionAnd(AptActionInterpreter* pInterp, LocalContextT*)
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
            pResult = AptBoolean::Create(nTop && nUnder);
        }
        else
        {
            const float fTop = pTop->toFloat();
            const float fUnder = pUnder->toFloat();
            pResult = AptBoolean::Create(fTop != 0.0f && fUnder != 0.0f);
        }
    }
    pInterp->stackPop(2);
    pInterp->stackPush(pResult);
}

void AptActionInterpreter::_FunctionAptActionOr(AptActionInterpreter* pInterp, LocalContextT*)
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
            pResult = AptBoolean::Create(nTop || nUnder);
        }
        else
        {
            const float fTop = pTop->toFloat();
            const float fUnder = pUnder->toFloat();
            pResult = AptBoolean::Create(fTop != 0.0f || fUnder != 0.0f);
        }
    }
    pInterp->stackPop(2);
    pInterp->stackPush(pResult);
}
