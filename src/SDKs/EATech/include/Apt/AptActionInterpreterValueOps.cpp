// ===========================================================================
// EATech Apt -- a batch of self-contained ActionScript value/string/array opcodes.
//   DECOMPILED from the X360 ARTIST:
//     PushZeroSetVar @0x82B055C8 (0x72) -- push AptInteger(0), then SetVariable
//     StringLength   @0x82AE99D0 (0x14) -- replace top with its string length
//     StringAdd      @0x82AF9DE8 (0x21) -- concatenate the top two as strings
//     Random         @0x82AE9BE8 (0x30) -- push AptInteger(rand() % top)
//     InitArray      @0x82AF3CF8 (0x42) -- build an AptArray from N stack values
//
// All reuse already-reconstructed pieces (Get_ToString, setVariable, the stack
// primitives, AptInteger/AptString::Create, AptArray) -- no new subsystem coupling.
//
// Dependencies (all homed): AptRand @0x82AE04F0 (AptRandom.cpp -- the X360
// MT19937); AptValue::Append_ToString @0x82AF9668 (AptValueConvert.cpp;
// StringAdd's concatenation). Random: the console traps (twllei) when the
// modulus is <= 0; PC guards instead (platform-behaviour note, not debt).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"    // AptString::Create
#include "SDKs/EATech/include/Apt/AptArray.h"              // AptArray (InitArray)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC::GetLength

#include <cstdint>

// The random source Random draws from (AptRand @0x82AE04F0, homed in AptRandom.cpp).
extern unsigned int AptRand();

// ---------------------------------------------------------------------------
// PushZeroSetVar @0x82B055C8 (0x72) -- push 0 as the value, then SetVariable
// (the name is already on the stack): `name = 0`.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushZeroSetVar(AptActionInterpreter* pInterp,
                                                            LocalContextT* pContext)
{
    AptInteger* pZero = AptInteger::Create(0);
    pInterp->mpStack[pInterp->mnStackTop++] = pZero;   // inlined stackPush
    pZero->AddRef();
    _FunctionAptActionSetVariable(pInterp, pContext);
}

// ---------------------------------------------------------------------------
// StringLength @0x82AE99D0 (0x14) -- replace the top with its string length.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStringLength(AptActionInterpreter* pInterp,
                                                          LocalContextT* /*pContext*/)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];
    EAStringC scratch;
    const EAStringC* pStr = AptValue::Get_ToString(pTop, &scratch);
    AptInteger* pLen = AptInteger::Create(static_cast<int>(pStr->GetLength()));
    pInterp->stackPop();                                // pop the string
    pInterp->mpStack[pInterp->mnStackTop++] = pLen;
    pLen->AddRef();
}

// ---------------------------------------------------------------------------
// StringAdd @0x82AF9DE8 (0x21) -- concatenate the top two operands as strings
// (under + top), replacing both with the new string.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStringAdd(AptActionInterpreter* pInterp,
                                                       LocalContextT* /*pContext*/)
{
    AptValue* pSecond = pInterp->mpStack[pInterp->mnStackTop - 1];   // top
    AptValue* pFirst  = pInterp->mpStack[pInterp->mnStackTop - 2];   // under
    AptString* pResult = AptString::Create("");
    pFirst->Append_ToString(pResult->GetInternalString());    // Append_ToString @0x82AF9668 (AptValueConvert.cpp)
    pSecond->Append_ToString(pResult->GetInternalString());
    pInterp->stackPop(2);
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// Random @0x82AE9BE8 (0x30) -- push AptInteger(AptRand() % top); 0 if the top is
// undefined. (Console traps when the modulus <= 0; PC guards.)
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionRandom(AptActionInterpreter* pInterp,
                                                    LocalContextT* /*pContext*/)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];
    int nResult = 0;
    if (pTop->getIsDefined())
    {
        const unsigned int nRange = static_cast<unsigned int>(pTop->toInteger());
        nResult = (nRange > 0) ? static_cast<int>(AptRand() % nRange) : 0;   // console twllei-traps when <=0; PC guards
    }
    pInterp->stackPop();
    AptInteger* pResult = AptInteger::Create(nResult);
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// InitArray @0x82AF3CF8 (0x42) -- pop the element count, build an AptArray from
// that many stack values (top-down), pop them, push the array.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionInitArray(AptActionInterpreter* pInterp,
                                                       LocalContextT* /*pContext*/)
{
    const int nCount = pInterp->mpStack[pInterp->mnStackTop - 1]->toInteger();
    pInterp->stackPop();                               // pop the count

    AptArray* pArray = new AptArray();
    pArray->AddRef();
    for (int i = 0; i < nCount; ++i)
        pArray->set(i, pInterp->mpStack[pInterp->mnStackTop - i - 1]);

    pInterp->stackSafePop(nCount);                     // pop the consumed elements
    pInterp->mpStack[pInterp->mnStackTop++] = pArray;  // push (creation AddRef carries)
}
