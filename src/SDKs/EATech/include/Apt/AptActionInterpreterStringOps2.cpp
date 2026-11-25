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
// Dependencies (all homed): _isNaN(AptValue*) -- the value NaN test -- lives in
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
extern int gnAptSwfVersion;
// The value NaN test (console isNaN; homed in AptActionInterpreterBuiltins.cpp).
extern bool         _isNaN(AptValue* pValue);

// ---------------------------------------------------------------------------
// SubString @0x82AF3AE0 (0x15) -- AS substring(str, start, len).
// Stack: [str, start, len] (len on top).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionSubString(AptActionInterpreter* pInterp,
                                                       LocalContextT* /*pContext*/)
{
    const int nLength = pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 1]->toInteger();
    AptValue* pStartV = pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 2];
    AptValue* pStrV   = pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 3];

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
    pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop++] = pResult;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// ToNumber @0x82AFABA8 (0x4A) -- coerce the top to a number in place.
// ---------------------------------------------------------------------------
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

void AptActionInterpreter::_FunctionAptActionToNumber(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pValue = pInterp->stackAt(0);
    if (!pValue->isFloat() && !pValue->isInteger())
    {
        AptValue* pResult = gpUndefinedValue;
        if (!_isNaN(pValue))
        {
            if (gnAptSwfVersion == 7 && !pValue->getIsDefined())
            {
                pInterp->stackPop();
                pInterp->stackPush(pResult);
                return;
            }

            EAStringC lBuffer;
            const EAStringC* pString = AptValue::Get_ToString(pValue, &lBuffer);
            const int nDot = pString->UTF8_Find(".", 0);
            if (nDot != -1 && nDot != static_cast<int>(pString->GetInternalSize()))
                pResult = AptFloat::Create(pValue->toFloat());
            else
                pResult = AptInteger::Create(pValue->toInteger());
        }
        pInterp->stackPop();
        pInterp->stackPush(pResult);
    }
}
