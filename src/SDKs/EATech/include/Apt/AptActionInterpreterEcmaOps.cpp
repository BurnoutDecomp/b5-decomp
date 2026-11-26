// ===========================================================================
// EATech Apt -- the ECMA-style value opcodes StringEquals and Add2.
//   DECOMPILED from the X360 ARTIST:
//     StringEquals @0x82AE9898 (0x13) -- string-equality of the top two operands
//     Add2         @0x82AF9E88 (0x47) -- ECMA `+`: string concat OR numeric add
//
// StringEquals coerces both operands to strings and pushes AptBoolean(equal). Add2
// concatenates when EITHER operand is a string, otherwise adds numerically (an
// AptInteger when both are plain ints, else an AptFloat). Both honour the SWF-v7
// rule: with version 7 and an undefined operand, the result is `undefined`
// (StringEquals: a lone undefined -> undefined, two undefineds -> true).
//
// Dependencies (all homed): AptValue::Append_ToString -- the value->string
// renderer (Add2's concat) -- @0x82AF9668 in AptValueConvert.cpp; gpUndefinedValue
// is the AptGlobals.cpp singleton (built at AptInit); AptGetSwfVersion is
// AptLinker.cpp.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"    // AptString::Create
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"   // AptBoolean::Create
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"     // AptFloat::Create
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC::operator==

// gpUndefinedValue: AptGlobals.cpp (built at AptInit). AptGetSwfVersion: AptLinker.cpp.
extern AptValue*    gpUndefinedValue;
extern unsigned int AptGetSwfVersion();
extern int gnAptSwfVersion;


// ---------------------------------------------------------------------------
// StringEquals @0x82AE9898 (0x13) -- push AptBoolean(toString(top) == toString(under)).
// ---------------------------------------------------------------------------
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

void AptActionInterpreter::_FunctionAptActionStringEquals(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop = pInterp->stackAt(0);
    AptValue* pUnder = pInterp->stackAt(1);
    AptValue* pResult = 0;
    int nUndefined = 0;
    if (gnAptSwfVersion == 7)
    {
        if (!pTop->getIsDefined())
            ++nUndefined;
        if (!pUnder->getIsDefined())
            ++nUndefined;
        switch (nUndefined)
        {
        case 1:
            pResult = gpUndefinedValue;
            break;
        case 2:
            pResult = AptBoolean::Create(true);
            break;
        }
    }

    if (!pResult)
    {
        EAStringC lTop;
        const EAStringC* pTopString = AptValue::Get_ToString(pTop, &lTop);
        EAStringC lUnder;
        const EAStringC* pUnderString = AptValue::Get_ToString(pUnder, &lUnder);
        pResult = AptBoolean::Create(*pTopString == *pUnderString);
    }

    pInterp->stackPop(2);
    pInterp->stackPush(pResult);
}

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// ---------------------------------------------------------------------------
// Add2 @0x82AF9E88 (0x47) -- ECMA `+`: concat if either operand is a string, else
// numeric add (AptInteger when both are plain ints, otherwise AptFloat).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionAdd2(AptActionInterpreter* pInterp,
                                                  LocalContextT* /*pContext*/)
{
    AptValue* pTop   = pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 1];
    AptValue* pUnder = pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 2];

    AptValue* pResult;
    if (pTop->isString() || pUnder->isString())
    {
        // String concatenation: under ++ top.
        AptString* pStr = AptString::Create("");
        pUnder->Append_ToString(pStr->GetInternalString());   // Append_ToString @0x82AF9668 (AptValueConvert.cpp)
        pTop->Append_ToString(pStr->GetInternalString());
        pInterp->stackPop(2);
        pResult = pStr;
    }
    else
    {
        const AptVirtualFunctionTable_Indices eTop   = pTop->getVtblIndex();
        const AptVirtualFunctionTable_Indices eUnder = pUnder->getVtblIndex();
        const bool bTopInt    = (eTop == AptVFT_Integer) && pTop->getIsDefined();
        const bool bUnderInt  = (eUnder == AptVFT_Integer) && pUnder->getIsDefined();
        const bool bTopFloat  = (eTop == AptVFT_Float) && pTop->getIsDefined();
        const bool bUnderFloat= (eUnder == AptVFT_Float) && pUnder->getIsDefined();
        // Float unless both operands are plain ints.
        const bool bUseFloat  = (!bTopInt && !bUnderInt) || bTopFloat || bUnderFloat;
        // v7: an undefined operand yields `undefined` rather than computing.
        const bool bCompute   = (AptGetSwfVersion() != 7) || (pTop->getIsDefined() && pUnder->getIsDefined());

        if (!bCompute)
        {
            pInterp->stackPop(2);
            pResult = gpUndefinedValue;
        }
        else if (bUseFloat)
        {
            const float a = pTop->toFloat();
            const float b = pUnder->toFloat();
            pInterp->stackPop(2);
            pResult = AptFloat::Create(b + a);
        }
        else
        {
            const int a = pTop->toInteger();
            const int b = pUnder->toInteger();
            pInterp->stackPop(2);
            pResult = AptInteger::Create(b + a);
        }
    }

    pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop++] = pResult;
    pResult->AddRef();
}
