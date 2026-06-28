// ===========================================================================
// EATech Apt -- the value-output / control ActionScript opcodes: Trace, Throw,
// ToString.   DECOMPILED from the X360 ARTIST:
//     Trace    @0x82AE9B60  (0x26)  -- AS trace(): coerce the top to a string and
//                                      emit it through the host debug sink
//     Throw    @0x82ADE930  (0x2A)  -- AS throw: take the top as the thrown value,
//                                      stash it in the interpreter abort slot
//     ToString @0x82AFACF8  (0x4B)  -- coerce the top to a string in place (a
//                                      non-string value is rendered to a new AptString)
//
// Trace/ToString reuse Get_ToString / the value->string renderer; ToString leaves
// an already-string value untouched. Throw records the thrown value where runStream
// observes it (the abort slot, mpAbortValue) and unwinds the run.
//
// FLAG -- deferred dependencies:
//   * AptHook_Trace -- the host debug-output sink (console dword_8324E82C, a
//     printf-style hook installed by the host); the trace TEXT is faithful, the
//     sink is the host boundary.
//   * AptValue::Append_ToString -- the value->string renderer (the StringPool /
//     Append path, a toString sibling); declared, body is the value-layer follow-on.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"   // AptString::Create / GetInternalString
#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC::GetBuffer

// FLAG (host debug sink -- installed by the host; console dword_8324E82C): the
// trace output function. printf-style; the one call site passes (fmt, message).
extern void AptHook_Trace(const char* szFormat, const char* szMessage);

// ---------------------------------------------------------------------------
// Trace @0x82AE9B60 (0x26) -- AS trace(value): emit value's string form.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionTrace(AptActionInterpreter* pInterp,
                                                   LocalContextT* /*pContext*/)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];
    EAStringC scratch;
    const EAStringC* pStr = AptValue::Get_ToString(pTop, &scratch);
    AptHook_Trace("AptTrace: %s\n", pStr->GetBuffer());   // FLAG: host debug sink
    pInterp->stackPop();
}

// ---------------------------------------------------------------------------
// Throw @0x82ADE930 (0x2A) -- AS throw value: record the thrown value in the
// interpreter abort slot (AddRef'd; the stack ref is dropped) so runStream stops.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionThrow(AptActionInterpreter* pInterp,
                                                   LocalContextT* /*pContext*/)
{
    AptValue* pValue = pInterp->mpStack[pInterp->mnStackTop - 1];
    pValue->AddRef();
    pInterp->mpAbortValue = pValue;   // runStream observes a non-null abort value and unwinds
    pInterp->stackPop();
}

// ---------------------------------------------------------------------------
// ToString @0x82AFACF8 (0x4B) -- coerce the top to a string. A value already of
// string type (1 / boxed 33) is left as-is; otherwise it is rendered into a fresh
// AptString that replaces the top.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionToString(AptActionInterpreter* pInterp,
                                                      LocalContextT* /*pContext*/)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];
    const AptVirtualFunctionTable_Indices eType = pTop->getVtblIndex();
    if ((eType == AptVFT_StringValue || eType == static_cast<AptVirtualFunctionTable_Indices>(33))
        && pTop->getIsDefined())
        return;   // already a string -> nothing to do

    AptString* pStr = AptString::Create("");                 // FLAG: seed const, filled below
    pTop->Append_ToString(pStr->GetInternalString());        // FLAG: value->string renderer (deferred)
    pInterp->stackPop();
    pInterp->mpStack[pInterp->mnStackTop++] = pStr;           // inlined stackPush
    pStr->AddRef();
}
