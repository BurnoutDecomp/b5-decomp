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
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h" // SetRegisterValue / CreateFrameStack
#include "SDKs/EATech/include/Apt/AptFrameStack.h"         // the active frame-stack value + mHash
#include "SDKs/EATech/include/Apt/AptNativeHash.h"         // AptNativeHash::Set (catch binding)
#include "SDKs/EATech/include/Apt/AptCIH.h"                // ctx->mpCIH -> AptValue* upcast

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
// CallFuncAndPop (0x5B; PS3 @0x823970) -- CallFunction, drop the result, drain the
// deferred-release vector once the operand stack has fully unwound. CallMethodPop's
// function-call sibling.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionCallFuncAndPop(AptActionInterpreter* pInterp,
                                                            LocalContextT* pContext)
{
    _FunctionAptActionCallFunction(pInterp, pContext);
    pInterp->stackPop();                  // drop the call result (Release)
    if (pInterp->mnStackTop == 0)
        AptApt_FlushDeferredReleases();   // console: vector-count && top==0 -> ReleaseValues
}

// ---------------------------------------------------------------------------
// DictCallFuncPop (0xB0; PS3 @0x8237BC) -- push the dictionary entry an inline BYTE
// indexes (the inlined stackPush + AddRef), CallFunction, drop the result, drain on
// stack-empty. The fused dict-push + CallFuncAndPop form.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDictCallFuncPop(AptActionInterpreter* pInterp,
                                                             LocalContextT* pContext)
{
    const unsigned int nIndex = *pContext->mpProgramCounter;   // inline byte dict index
    pContext->mpProgramCounter += 1;

    AptValue* pEntry = pInterp->mpRegisters[nIndex];           // console: *(4*idx + a1[17])
    pInterp->mpStack[pInterp->mnStackTop++] = pEntry;          // inlined stackPush
    pEntry->AddRef();

    _FunctionAptActionCallFunction(pInterp, pContext);
    pInterp->stackPop();                                       // drop the call result (Release)
    if (pInterp->mnStackTop == 0)
        AptApt_FlushDeferredReleases();
}

// The value->object resolver (homed in AptActionInterpreterInterpHelpers.cpp).
extern void AptActionInterpreter_valueToObject(AptValue* pScope, AptValue* pTarget,
                                               AptValue* pValue, AptValue** ppOut);

// ---------------------------------------------------------------------------
// With (0x94; PS3 @0x82016C) -- AS `with (obj) { ... }`. The aligned inline operand
// is the with-block END position (console 4-byte slot; read pointer-wide per the
// sibling inline-operand convention pending the _parseStream transcode). A defined
// with-target resolves to its object (valueToObject) and installs the WITH-SCOPE
// through the context's pending-release pair -- {scope object, end-of-block PC} --
// which runStream releases exactly when the PC reaches the block end (the with
// scope's lifetime). An undefined target skips the whole block (PC = end). Either
// way the with-target operand is popped (Release).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionWith(AptActionInterpreter* pInterp,
                                                  LocalContextT* pContext)
{
    const unsigned char* pAligned =
        reinterpret_cast<const unsigned char*>(
            (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));   // 8-aligned (GUIAPT64)
    const unsigned char* pBlockEnd = *reinterpret_cast<const unsigned char* const*>(pAligned);

    AptValue* pTarget = pInterp->mpStack[pInterp->mnStackTop - 1];
    if (pTarget->getIsDefined())
    {
        pContext->mpProgramCounter = pAligned + 8;   // consume the qword operand (GUIAPT64)

        AptValue* pObject = nullptr;   // FLAG: the console leaves the out slot uninitialised
                                       // on a no-object value and AddRefs it regardless; the
                                       // null init + guard below keep that hazard out.
        AptActionInterpreter_valueToObject(pContext->mpCIH, nullptr, pTarget, &pObject);
        if (pObject)
        {
            pContext->mpPendingReleaseValue = pObject;    // ctx[2] -- the with-scope
            pContext->mpPendingReleasePC    = pBlockEnd;  // ctx[3] -- released at block end
            pObject->AddRef();                            // console (**v13)(v13)
        }
    }
    else
    {
        pContext->mpPendingReleaseValue = nullptr;        // ctx[2] = 0
        pContext->mpProgramCounter      = pBlockEnd;      // skip the whole with-block
    }

    pInterp->stackPop();                                  // pop the with-target (Release)
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

// ===========================================================================
// Try @0x82B05CD8 -- AS try/catch/finally: run the try sub-stream, and on a thrown
// value (mpAbortValue set) run the catch sub-stream (binding the thrown value into
// the catch variable/register first), then always run the finally sub-stream
// (preserving + re-raising a pending throw across it). The three sub-streams are
// laid out inline after a 20-byte try record at the (4-byte-aligned) PC.
// ===========================================================================

// FLAG (console Burnout_X360_Artist_01e3_0 -- the inlined stack-collapse primitive):
// pop nCount operands off the operand stack, Releasing each. Shared with the StackOps
// call opcodes (declared there too); used here to unwind any operands the sub-streams
// left above the entry top.
// AptApt_PopValues -> AptActionInterpreter::stackPop(int) member (@0x7FDB68; the {} shim was a no-op).

// FLAG (the active AS local-variable frame stack -- console off_8324E3DC /
// AptScriptFunctionBase::spFrameStack, a protected static). The catch-variable bind
// reads it (and lazily creates it via CreateFrameStack) to store the caught value in
// the current activation's locals; reached through an accessor so the protected
// static stays encapsulated.
extern AptFrameStack* AptScriptFunctionBase_GetActiveFrameStack();

namespace
{
    // The inline try record (console v5, at the 4-byte-aligned PC). The three code
    // sub-streams follow the 20-byte header: try @ +20, catch @ +20+mnTryLen, finally
    // @ +20+mnTryLen+mnCatchLen. The flags byte gates the catch / finally / register
    // forms; the catch payload is either a register index (mFlags bit2) or a variable
    // name pointer (console +0x10).
    struct AptTryRecordT
    {
        int32_t        mnTryLen;        // [+0x00] try block byte length
        int32_t        mnCatchLen;      // [+0x04] catch block byte length
        int32_t        mnFinallyLen;    // [+0x08] finally block byte length
        uint8_t        mFlags;          // [+0x0C] bit0=has catch, bit1=has finally, bit2=catch-to-register
        uint8_t        mPad0D[2];       // [+0x0D]
        uint8_t        mnCatchRegister; // [+0x0F] catch register index (mFlags bit2)
        const char*    mpCatchName;     // [+0x10] catch variable name (mFlags bit2 == 0)
                                        // [+0x14] -> the try sub-stream begins here.
    };

    enum
    {
        KU_TRY_HAS_CATCH    = 0x1,
        KU_TRY_HAS_FINALLY  = 0x2,
        KU_TRY_CATCH_TO_REG = 0x4,
    };
}

void AptActionInterpreter::_FunctionAptActionTry(AptActionInterpreter* pInterp,
                                                 LocalContextT* pContext)
{
    const int nEntryStackTop = pInterp->mnStackTop;   // console v4 = *a1 (unwind target)

    // The try record sits at the 8-aligned PC (GUIAPT64); the three sub-streams follow
    // its 24-byte header (the console's 20-byte form had a 4-byte name slot). The PC is
    // advanced past the whole try/catch/finally body.
    AptTryRecordT* const pRecord = reinterpret_cast<AptTryRecordT*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));

    const unsigned char* const pTryStream     = reinterpret_cast<const unsigned char*>(pRecord) + 24;
    const unsigned char* const pCatchStream   = pTryStream + pRecord->mnTryLen;
    const unsigned char* const pFinallyStream = pCatchStream + pRecord->mnCatchLen;
    pContext->mpProgramCounter = pFinallyStream + pRecord->mnFinallyLen;

    // Run the try block (a bounded sub-stream under the same scope / character inst).
    pInterp->runStream(pTryStream, pContext->mpCIH, pRecord->mnTryLen, pContext->mpCharacterInst);

    // --- catch: a pending throw + a catch clause binds the thrown value + runs it ---
    AptValue* const pThrown = pInterp->mpAbortValue;   // console v9 = a1[24]
    if (pThrown && (pRecord->mFlags & KU_TRY_HAS_CATCH))
    {
        if (pRecord->mFlags & KU_TRY_CATCH_TO_REG)
        {
            // catch into a register slot.
            AptScriptFunctionBase::SetRegisterValue(pRecord->mnCatchRegister, pThrown);
        }
        else
        {
            // catch into a named variable: the current activation's frame-stack locals
            // when one is live, else the run-scope variable table.
            EAStringC catchName(pRecord->mpCatchName);   // console InitFromBuffer(v13, *(v5+16))
            if (pInterp->mpCurrentFunction)              // console a1[15] (mpCurrentFunction) set
            {
                AptFrameStack* pFrame = AptScriptFunctionBase_GetActiveFrameStack();  // FLAG: off_8324E3DC
                if (!pFrame)
                {
                    pInterp->mpCurrentFunction->CreateFrameStack();
                    pFrame = AptScriptFunctionBase_GetActiveFrameStack();
                }
                pFrame->GetNativeHashVirtual()->Set(catchName, pThrown);  // console AptNativeHash::Set(frame+8, ...)
            }
            else
            {
                pInterp->setVariable(static_cast<AptValue*>(pContext->mpCIH), nullptr,
                                     &catchName, pThrown, 1, 1, 0);
            }
        }

        pInterp->mpAbortValue->Release();   // console (*(*a1[24]+4))(a1[24])
        pInterp->mpAbortValue = nullptr;    // clear the pending throw before the catch runs
        pInterp->runStream(pCatchStream, pContext->mpCIH, pRecord->mnCatchLen,
                           pContext->mpCharacterInst);
    }

    // --- finally: always runs; preserve + re-raise a pending throw across it ---
    if (pRecord->mFlags & KU_TRY_HAS_FINALLY)
    {
        AptValue* const pPending = pInterp->mpAbortValue;   // console v12
        if (pPending)
        {
            pPending->AddRef();                 // console (**v12)(v12)
            pInterp->mpAbortValue->Release();   // console (*(*a1[24]+4))(a1[24])
            pInterp->mpAbortValue = nullptr;
        }

        pInterp->runStream(pFinallyStream, pContext->mpCIH, pRecord->mnFinallyLen,
                           pContext->mpCharacterInst);

        // re-raise the saved throw if the finally block did not itself throw.
        if (pPending && !pInterp->mpAbortValue)
        {
            pPending->AddRef();
            pInterp->mpAbortValue = pPending;
            pPending->Release();
        }
    }

    // unwind any operands the sub-streams left above the try's entry top.
    if (pInterp->mnStackTop > nEntryStackTop)
        pInterp->stackPop(pInterp->mnStackTop - nEntryStackTop);   // stack collapse to entry level (member; retired the AptApt_PopValues no-op shim)
}
