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

namespace
{
    // A defined string-typed value (meValueType 1 = AptVFT_StringValue, or the boxed
    // tag 33). Both opcodes branch on this.
    inline bool IsDefinedString(const AptValue* p)
    {
        const AptVirtualFunctionTable_Indices e = p->getVtblIndex();
        return (e == AptVFT_StringValue || e == static_cast<AptVirtualFunctionTable_Indices>(33))
            && p->getIsDefined();
    }
}

// ---------------------------------------------------------------------------
// StringEquals @0x82AE9898 (0x13) -- push AptBoolean(toString(top) == toString(under)).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStringEquals(AptActionInterpreter* pInterp,
                                                          LocalContextT* /*pContext*/)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = 0;
    if (AptGetSwfVersion() == 7)
    {
        // v7: count undefined operands -- one undefined => undefined, two => equal.
        int nUndefined = (pTop->getIsDefined() ? 0 : 1) + (pUnder->getIsDefined() ? 0 : 1);
        if (nUndefined == 1)
            pResult = gpUndefinedValue;
        else if (nUndefined == 2)
            pResult = AptBoolean::Create(true);
    }

    if (!pResult)
    {
        EAStringC topScratch, underScratch;
        const EAStringC* pTopStr   = AptValue::Get_ToString(pTop, &topScratch);
        const EAStringC* pUnderStr = AptValue::Get_ToString(pUnder, &underScratch);
        pResult = AptBoolean::Create(*pTopStr == *pUnderStr);
    }

    pInterp->stackPop(2);
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// Add2 @0x82AF9E88 (0x47) -- ECMA `+`: concat if either operand is a string, else
// numeric add (AptInteger when both are plain ints, otherwise AptFloat).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionAdd2(AptActionInterpreter* pInterp,
                                                  LocalContextT* /*pContext*/)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult;
    if (IsDefinedString(pTop) || IsDefinedString(pUnder))
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

    pInterp->mpStack[pInterp->mnStackTop++] = pResult;
    pResult->AddRef();
}
