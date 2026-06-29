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

#include <cstdint>

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

// ===========================================================================
// Call / member / function-definition fused opcodes -- the "do a call, then a
// fused side-effect, then flush the deferred-release vector" family.
//   DECOMPILED from the X360 ARTIST:
//     CallFuncSetVar       @0x82B05428 -- CallFunction then SetVariable
//     CallMethodPop        @0x82B05490 -- CallMethod   then drop the top
//     CallMethodSetVar     @0x82B054E8 -- CallMethod   then SetVariable
//     DictCallFuncSetVar   @0x82B05AB0 -- push dict-register, CallFunction, SetVariable
//     DictCallMethodPop    @0x82B05B68 -- push dict-register, CallMethod,   drop the top
//     DictCallMethodSetVar @0x82B05C20 -- push dict-register, CallMethod,   SetVariable
//
// Each is a thin composition of the already-reconstructed leaf handlers
// (_FunctionAptActionCallFunction / _FunctionAptActionCallMethod /
// _FunctionAptActionSetVariable) plus, for the Dict* forms, an inline operand-
// stack push of a value taken from the per-call register window (mpRegisters,
// console +0x44) indexed by a single dictionary byte read from the program
// counter. After the side-effect each flushes the AptGC deferred-release value
// vector once the operand stack has drained (the console guards the flush by
// `off_8324E51C->count != 0 && mnStackTop == 0`; the count side of the guard is
// folded into AptApt_FlushDeferredReleases, exactly as the sibling Var/Member
// opcodes do).
//
// FLAG -- the AptGC deferred-release vector flush (console off_8324E51C /
// AptValueVector::ReleaseValues; the AptGC layer). Encapsulated as
// AptApt_FlushDeferredReleases (the same hook the Var/Member opcodes use); the
// stack-empty guard that triggers it is faithful + engine-side.
// ===========================================================================

// FLAG (AptGC layer -- AptValueVector::ReleaseValues over off_8324E51C): drain
// the deferred-release value vector once the operand stack empties. Shared with
// the Var/Member opcodes (declared there too); the host-side vector type/global
// are not reconstructed yet, so the flush is encapsulated.
extern void AptApt_FlushDeferredReleases();

namespace
{
    // Push the per-call register-window value addressed by the next program-counter
    // dictionary byte onto the operand stack (the shared prologue of the three
    // Dict* fused ops). Console: r10 = byte<<2 (a 4-byte-pointer index into
    // mpRegisters @ +0x44); the value is stored at the top, the top advanced, and
    // vtbl[0] (AddRef) invoked -- i.e. stackPush of mpRegisters[dictByte]. On x64
    // the window is an AptValue* array, so it indexes by element.
    inline void PushDictRegister(AptActionInterpreter* pInterp,
                                 AptActionInterpreter::LocalContextT* pContext)
    {
        const uint8_t nDictByte = *pContext->mpProgramCounter;
        ++pContext->mpProgramCounter;                       // advance past the byte
        AptValue* pValue = pInterp->mpRegisters[nDictByte];
        pInterp->mpStack[pInterp->mnStackTop++] = pValue;   // inlined stackPush
        pValue->AddRef();
    }
}

// ---------------------------------------------------------------------------
// CallFuncSetVar @0x82B05428 -- CallFunction, then SetVariable, then flush.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionCallFuncSetVar(AptActionInterpreter* pInterp,
                                                            LocalContextT* pContext)
{
    _FunctionAptActionCallFunction(pInterp, pContext);
    _FunctionAptActionSetVariable(pInterp, pContext);
    if (pInterp->mnStackTop == 0)
        AptApt_FlushDeferredReleases();   // FLAG: off_8324E51C / AptValueVector::ReleaseValues
}

// ---------------------------------------------------------------------------
// CallMethodPop @0x82B05490 -- CallMethod, then drop the result, then flush.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionCallMethodPop(AptActionInterpreter* pInterp,
                                                           LocalContextT* pContext)
{
    _FunctionAptActionCallMethod(pInterp, pContext);
    pInterp->stackPop();                  // console AptValue>::Pop -- drop the call result
    if (pInterp->mnStackTop == 0)
        AptApt_FlushDeferredReleases();   // FLAG: off_8324E51C / AptValueVector::ReleaseValues
}

// ---------------------------------------------------------------------------
// CallMethodSetVar @0x82B054E8 -- CallMethod, then SetVariable, then flush.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionCallMethodSetVar(AptActionInterpreter* pInterp,
                                                             LocalContextT* pContext)
{
    _FunctionAptActionCallMethod(pInterp, pContext);
    _FunctionAptActionSetVariable(pInterp, pContext);
    if (pInterp->mnStackTop == 0)
        AptApt_FlushDeferredReleases();   // FLAG: off_8324E51C / AptValueVector::ReleaseValues
}

// ---------------------------------------------------------------------------
// DictCallFuncSetVar @0x82B05AB0 -- push the dict-register operand, then
// CallFunction, then SetVariable, then flush.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDictCallFuncSetVar(AptActionInterpreter* pInterp,
                                                               LocalContextT* pContext)
{
    PushDictRegister(pInterp, pContext);
    _FunctionAptActionCallFunction(pInterp, pContext);
    _FunctionAptActionSetVariable(pInterp, pContext);
    if (pInterp->mnStackTop == 0)
        AptApt_FlushDeferredReleases();   // FLAG: off_8324E51C / AptValueVector::ReleaseValues
}

// ---------------------------------------------------------------------------
// DictCallMethodPop @0x82B05B68 -- push the dict-register operand, then
// CallMethod, then drop the result, then flush.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDictCallMethodPop(AptActionInterpreter* pInterp,
                                                              LocalContextT* pContext)
{
    PushDictRegister(pInterp, pContext);
    _FunctionAptActionCallMethod(pInterp, pContext);
    pInterp->stackPop();                  // console AptValue>::Pop -- drop the call result
    if (pInterp->mnStackTop == 0)
        AptApt_FlushDeferredReleases();   // FLAG: off_8324E51C / AptValueVector::ReleaseValues
}

// ---------------------------------------------------------------------------
// DictCallMethodSetVar @0x82B05C20 -- push the dict-register operand, then
// CallMethod, then SetVariable, then flush.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDictCallMethodSetVar(AptActionInterpreter* pInterp,
                                                                 LocalContextT* pContext)
{
    PushDictRegister(pInterp, pContext);
    _FunctionAptActionCallMethod(pInterp, pContext);
    _FunctionAptActionSetVariable(pInterp, pContext);
    if (pInterp->mnStackTop == 0)
        AptApt_FlushDeferredReleases();   // FLAG: off_8324E51C / AptValueVector::ReleaseValues
}
