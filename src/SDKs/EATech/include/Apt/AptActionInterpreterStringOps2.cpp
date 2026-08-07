// ===========================================================================
// EATech Apt -- the ActionScript string-conversion opcodes SubString and ToNumber.
//   DECOMPILED from the X360 ARTIST:
//     SubString @0x82AF3AE0 (0x15) -- AS substring(str, start, len)
//     ToNumber  @0x82AFABA8 (0x4A) -- coerce the top to a numeric value
//
// SubString: pop (str, start, len); start is 1-based (->0-based, clamped at 0);
// len>0 -> UTF8_Mid(start,len), len<0 -> UTF8_Mid(start) (to end), len==0 -> empty.
// ToNumber: a value already of number type (Float 6 / Integer 7) is left as-is;
// otherwise (and not NaN, honouring the SWF-v7 undefined rule) it is rendered to a
// string and parsed -- an AptInteger when there is no '.' , else an AptFloat;
// non-numeric -> undefined.
//
// Dependencies (all homed): isNaN(AptValue*) -- the value NaN test -- lives in
// AptActionInterpreterBuiltins.cpp; EAStringC::UTF8_Mid / UTF8_Find are the
// EAString.cpp text suite; gpUndefinedValue is the AptGlobals.cpp singleton
// (built at AptInit); AptGetSwfVersion is AptLinker.cpp.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"    // AptString::Create / GetInternalString
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"     // AptFloat::Create
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC::UTF8_Mid / UTF8_Find

// gpUndefinedValue: AptGlobals.cpp (built at AptInit). AptGetSwfVersion: AptLinker.cpp.
extern AptValue*    gpUndefinedValue;
extern unsigned int AptGetSwfVersion();
// The value NaN test (console isNaN; homed in AptActionInterpreterBuiltins.cpp).
extern bool         isNaN(AptValue* pValue);

// ---------------------------------------------------------------------------
// SubString @0x82AF3AE0 (0x15) -- AS substring(str, start, len).
// Stack: [str, start, len] (len on top).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionSubString(AptActionInterpreter* pInterp,
                                                       LocalContextT* /*pContext*/)
{
    const int nLength = pInterp->mpStack[pInterp->mnStackTop - 1]->toInteger();
    AptValue* pStartV = pInterp->mpStack[pInterp->mnStackTop - 2];
    AptValue* pStrV   = pInterp->mpStack[pInterp->mnStackTop - 3];

    int nStart = pStartV->toInteger() - 1;   // AS substring is 1-based
    if (nStart < 0)
        nStart = 0;

    EAStringC scratch;
    const EAStringC* pSrc = AptValue::Get_ToString(pStrV, &scratch);
    AptString* pResult = AptString::Create("");
    if (nLength > 0)
        *pResult->GetInternalString() = pSrc->UTF8_Mid(nStart, nLength);
    else if (nLength < 0)
        *pResult->GetInternalString() = pSrc->UTF8_Mid(nStart);   // to the end
    // nLength == 0 -> the empty string from Create("")

    pInterp->stackPop(3);
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// ToNumber @0x82AFABA8 (0x4A) -- coerce the top to a number in place.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionToNumber(AptActionInterpreter* pInterp,
                                                      LocalContextT* /*pContext*/)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];
    const AptVirtualFunctionTable_Indices eType = pTop->getVtblIndex();
    if ((eType == AptVFT_Float && pTop->getIsDefined())
        || (eType == AptVFT_Integer && pTop->getIsDefined()))
        return;   // already a number -> nothing to do

    AptValue* pResult = gpUndefinedValue;
    if (!isNaN(pTop) && (AptGetSwfVersion() != 7 || pTop->getIsDefined()))   // isNaN (AptActionInterpreterBuiltins.cpp)
    {
        EAStringC scratch;
        const EAStringC* pStr = AptValue::Get_ToString(pTop, &scratch);
        const int nDot = pStr->UTF8_Find(".", 0);
        if (nDot == -1 || nDot == static_cast<int>(pStr->GetLength()))
            pResult = AptInteger::Create(pTop->toInteger());
        else
            pResult = AptFloat::Create(pTop->toFloat());
    }

    pInterp->stackPop();
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;
    pResult->AddRef();
}
