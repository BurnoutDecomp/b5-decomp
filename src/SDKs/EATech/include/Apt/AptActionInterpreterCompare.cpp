// ===========================================================================
// EATech Apt -- AptActionInterpreter ActionScript comparison opcodes.
//   DECOMPILED from the PS3 EXTERNAL ELF:
//     _FunctionAptActionEquals   @0x7FD938
//     _FunctionAptActionLessThan @0x7FD714
//     _FunctionAptActionGreater  @0x7FCF30
//
// Each compares the top two operands and pushes an AptBoolean; the result is
// `under OP top` (under = pushed first, top = pushed last), matching the stack
// convention. Same frame as the logic/arithmetic ops: SWF-v7 undefined rule
// (an undefined operand yields `undefined` unless gpUndefinedValue is null), an
// integer/numeric fast path, then the inline two-operand collapse.
//
//   Equals   : both defined ints -> exact ==, else |toFloat(under)-toFloat(top)| < 0.001.
//   LessThan : both defined ints -> under < top (asm: top > under), else toFloat.
//   Greater  : type-aware -- both defined strings -> strcmp(top,under) < 0 (i.e.
//              under > top lexically); else if either operand is a defined float ->
//              toFloat(under) > toFloat(top); else integer -> toInteger(top) <
//              toInteger(under). (Greater's v7-undefined-fail path pushes
//              gpUndefinedValue rather than skipping the collapse.)
//
// The console selects AptBoolean::spBooleanTrue/spBooleanFalse directly; here that
// is AptBoolean::Create(b) (same singletons). The stack collapse is inlined to
// mirror the asm.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // c_integer()->GetInt()
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"    // c_string()->GetInternalString()->GetBuffer()

#include <cmath>     // fabsf
#include <cstring>   // strcmp

// FLAG (wired at AptInit; see AptValueConvert.cpp).
extern AptValue*    gpUndefinedValue;
extern unsigned int AptGetSwfVersion();

namespace
{
    inline bool BothDefinedInts(const AptValue* pTop, const AptValue* pUnder)
    {
        return pTop->getVtblIndex()   == AptVFT_Integer && pTop->getIsDefined()
            && pUnder->getVtblIndex() == AptVFT_Integer && pUnder->getIsDefined();
    }

    inline bool ShouldCompute(const AptValue* pTop, const AptValue* pUnder)
    {
        if (AptGetSwfVersion() == 7 &&
            !(pTop->getIsDefined() && pUnder->getIsDefined()) &&
            gpUndefinedValue)
            return false;
        return true;
    }

    inline bool IsDefinedString(const AptValue* pV)
    {
        AptVirtualFunctionTable_Indices t = pV->getVtblIndex();
        return (t == AptVFT_StringValue || t == AptVFT_StringObject) && pV->getIsDefined();
    }

    // The shared two-operand collapse: release the operands, push the result (AddRef).
    inline void CollapseTwo(AptActionInterpreter* pInterp, AptValue* pResult)
    {
        int top = pInterp->mnStackTop;
        if (top >= 2)
        {
            pInterp->mpStack[top - 1]->Release();
            pInterp->mpStack[top - 2]->Release();
            top -= 2;
            pInterp->mnStackTop = top;
        }
        pInterp->mpStack[top] = pResult;
        pInterp->mnStackTop = top + 1;
        pResult->AddRef();
    }
}

// ---------------------------------------------------------------------------
// _FunctionAptActionEquals @0x7FD938 -- under == top.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionEquals(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        if (BothDefinedInts(pTop, pUnder))
        {
            pResult = AptBoolean::Create(pTop->c_integer()->GetInt() == pUnder->c_integer()->GetInt());
        }
        else
        {
            float ftop   = pTop->toFloat();
            float funder = pUnder->toFloat();
            pResult = AptBoolean::Create(fabsf(ftop - funder) < 0.001f);
        }
    }

    CollapseTwo(pInterp, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionLessThan @0x7FD714 -- under < top (asm computes top > under).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionLessThan(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        if (BothDefinedInts(pTop, pUnder))
        {
            pResult = AptBoolean::Create(pTop->c_integer()->GetInt() > pUnder->c_integer()->GetInt());
        }
        else
        {
            float ftop   = pTop->toFloat();
            float funder = pUnder->toFloat();
            pResult = AptBoolean::Create(ftop > funder);
        }
    }

    CollapseTwo(pInterp, pResult);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionGreater @0x7FCF30 -- under > top (type-aware).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGreater(AptActionInterpreter* pInterp, LocalContextT*)
{
    AptValue* pTop   = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];

    AptValue* pResult = gpUndefinedValue;
    if (ShouldCompute(pTop, pUnder))
    {
        if (IsDefinedString(pTop) && IsDefinedString(pUnder))
        {
            // Lexical: strcmp(top, under) < 0  <=>  under > top.
            const char* sTop   = pTop->c_string()->GetInternalString()->GetBuffer();
            const char* sUnder = pUnder->c_string()->GetInternalString()->GetBuffer();
            pResult = AptBoolean::Create(strcmp(sTop, sUnder) < 0);
        }
        else if ((pTop->getVtblIndex()   == AptVFT_Float && pTop->getIsDefined()) ||
                 (pUnder->getVtblIndex() == AptVFT_Float && pUnder->getIsDefined()))
        {
            pResult = AptBoolean::Create(pUnder->toFloat() > pTop->toFloat());
        }
        else
        {
            pResult = AptBoolean::Create(pTop->toInteger() < pUnder->toInteger());
        }
    }

    CollapseTwo(pInterp, pResult);
}
