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
// Host boundary: the trace text goes out through the host user-function table's
// printf-style pfnDebugPrint slot (installed by CgsGui::AptAux::ConstructApt).
// AptValue::Append_ToString (the value->string renderer) is homed in
// AptValue/AptValueConvert.cpp (@0x82AF9668).
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
#include "SDKs/EATech/include/Apt/Apt.h"                   // gAptFuncs (pfnDebugPrint)
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"

#include "types.hpp"

// ---------------------------------------------------------------------------
// Trace @0x82AE9B60 (0x26) -- AS trace(value): emit value's string form.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionTrace(AptActionInterpreter* pInterp,
                                                   LocalContextT* /*pContext*/)
{
    AptValue* pTop = pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 1];
    EAStringC scratch;
    const EAStringC* pStr = AptValue::Get_ToString(pTop, &scratch);
    gAptFuncs.pfnDebugPrint("AptTrace: %s\n", pStr->GetBuffer());
    pInterp->stackPop();
}

// ---------------------------------------------------------------------------
// Throw @0x82ADE930 (0x2A) -- AS throw value: record the thrown value in the
// interpreter abort slot (AddRef'd; the stack ref is dropped) so runStream stops.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionThrow(AptActionInterpreter* pInterp,
                                                   LocalContextT* /*pContext*/)
{
    AptValue* pValue = pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 1];
    pValue->AddRef();
    pInterp->mpAbortValue = pValue;   // runStream observes a non-null abort value and unwinds
    pInterp->stackPop();
}

// ---------------------------------------------------------------------------
// ToString @0x82AFACF8 (0x4B) -- coerce the top to a string. A value already of
// string type (1 / boxed 33) is left as-is; otherwise it is rendered into a fresh
// AptString that replaces the top.
// ---------------------------------------------------------------------------
// ARTIST records the string-defined test at 0x82AFAD38 before branching.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptActionInterpreter::_FunctionAptActionToString(AptActionInterpreter* pInterp,
                                                      LocalContextT* /*pContext*/)
{
    AptValue* pTop = pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 1];
    if (!pTop->isString())
    {
        AptString* pStr = AptString::Create();
        pTop->Append_ToString(pStr->GetInternalString());
        pInterp->stackPop();
        pInterp->stackPush(pStr);
    }
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

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
// The fused call opcodes reuse the normal CallFunction, CallMethod and
// SetVariable handlers. ARTIST 0x82B05454 starts the deferred release check
// with the vector count, then tests the operand-stack depth. The dictionary
// forms at 0x82B05AB0, 0x82B05B68 and 0x82B05C20 push directly from the
// constant pool before calling their handlers.
// ===========================================================================

namespace
{
    // Push the string-constant-dictionary value addressed by the next program-counter
    // dictionary byte onto the operand stack (the shared prologue of the three
    // Dict* fused ops). Console: r10 = byte<<2 (a 4-byte-pointer index into
    // mConstantPool.mppEntries @ +0x44); the value is stored at the top, the top advanced, and
    // vtbl[0] (AddRef) invoked -- i.e. stackPush of mConstantPool.mppEntries[dictByte].
    inline void PushDictRegister(AptActionInterpreter* pInterp,
                                 AptActionInterpreter::LocalContextT* pContext)
    {
        const uint8_t nDictByte = *pContext->mpProgramCounter;
        ++pContext->mpProgramCounter;                       // advance past the byte
        AptValue* pValue = pInterp->mConstantPool.mppEntries[nDictByte];
        pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop++] = pValue;   // inlined stackPush
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
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
}

// ---------------------------------------------------------------------------
// CallMethodPop @0x82B05490 -- CallMethod, then drop the result, then flush.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionCallMethodPop(AptActionInterpreter* pInterp,
                                                           LocalContextT* pContext)
{
    _FunctionAptActionCallMethod(pInterp, pContext);
    pInterp->stackPop();                  // console AptValue>::Pop -- drop the call result
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
}

// ---------------------------------------------------------------------------
// CallFuncAndPop (0x5B; ARTIST 0x82B053D0) -- CallFunction, drop the result, drain the
// deferred-release vector once the operand stack has fully unwound. CallMethodPop's
// function-call sibling.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionCallFuncAndPop(AptActionInterpreter* pInterp,
                                                            LocalContextT* pContext)
{
    _FunctionAptActionCallFunction(pInterp, pContext);
    pInterp->stackPop();                  // drop the call result (Release)
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
}

// ---------------------------------------------------------------------------
// DictCallFuncPop (0xB0; ARTIST @0x82B059F8) -- push the dictionary entry an inline BYTE
// indexes (the inlined stackPush + AddRef), CallFunction, drop the result, drain on
// stack-empty. The fused dict-push + CallFuncAndPop form.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDictCallFuncPop(AptActionInterpreter* pInterp,
                                                             LocalContextT* pContext)
{
    PushDictRegister(pInterp, pContext);
    _FunctionAptActionCallFunction(pInterp, pContext);
    pInterp->stackPop();
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
}

// The value->object resolver (homed in AptActionInterpreterInterpHelpers.cpp).
// AptActionInterpreter::valueToObject is now a member (declared in AptActionInterpreter.h).

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
            (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));   // four-byte ARTIST operand
    const unsigned char* pBlockEnd = *reinterpret_cast<const unsigned char* const*>(pAligned);
    pContext->mpProgramCounter = pAligned + 4;

    AptValue* pTarget = pInterp->mOperandStack.mppItems[pInterp->mOperandStack.mnTop - 1];
    if (pTarget->getIsDefined())
    {
        AptValue* pObject;
        pInterp->valueToObject(pContext->mpCIH, 0, pTarget, &pObject);
        pContext->mpPendingReleaseEnd = pBlockEnd;
        pContext->mpPendingReleaseValue = pObject;
        pObject->AddRef();
    }
    else
    {
        pContext->mpPendingReleaseValue = 0;        // ctx[2] = 0
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
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
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
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
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
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
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
    if (gpValuesToRelease->GetNumValues() != 0 && pInterp->mOperandStack.mnTop == 0)
        gpValuesToRelease->ReleaseValues();
}

// ===========================================================================
// Try @0x82B05CD8 -- AS try/catch/finally: run the try sub-stream, and on a thrown
// value (mpAbortValue set) run the catch sub-stream (binding the thrown value into
// the catch variable/register first), then always run the finally sub-stream
// (preserving + re-raising a pending throw across it). The three sub-streams are
// laid out inline after a 20-byte try record at the (4-byte-aligned) PC.
// ===========================================================================

namespace
{
    // ARTIST reads three 32-bit lengths, flags at +0x0C, the catch register
    // at +0x0F and the optional variable name at +0x10.
    struct AptTryRecordT
    {
        uint32_t       mnTryLen;
        uint32_t       mnCatchLen;
        uint32_t       mnFinallyLen;
        uint8_t        mnFlags;
        uint8_t        mauAlignment[2];
        uint8_t        mnRegister;
        const char*    mpCatchName;
    };
    CGS_STATIC_ASSERT(sizeof(AptTryRecordT) == 0x14, "ARTIST try record size");
    CGS_STATIC_ASSERT(offsetof(AptTryRecordT, mnRegister) == 0x0F, "ARTIST catch register offset");
    CGS_STATIC_ASSERT(offsetof(AptTryRecordT, mpCatchName) == 0x10, "ARTIST catch name offset");

    enum
    {
        KU_TRY_HAS_CATCH    = 0x1,
        KU_TRY_HAS_FINALLY  = 0x2,
        KU_TRY_CATCH_TO_REG = 0x4,
    };
}

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

void AptActionInterpreter::_FunctionAptActionTry(AptActionInterpreter* pInterp,
                                                 LocalContextT* pContext)
{
    const uint32_t nEntryStackTop = pInterp->mOperandStack.mnTop;
    pContext->mpProgramCounter = reinterpret_cast<const unsigned char*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
    const AptTryRecordT* pRecord = reinterpret_cast<const AptTryRecordT*>(pContext->mpProgramCounter);
    pContext->mpProgramCounter += sizeof(AptTryRecordT);
    pContext->mpProgramCounter += pRecord->mnTryLen;
    pContext->mpProgramCounter += pRecord->mnCatchLen;
    pContext->mpProgramCounter += pRecord->mnFinallyLen;

    pInterp->runStream(reinterpret_cast<const unsigned char*>(pRecord + 1), pContext->mpCIH,
                       pRecord->mnTryLen, pContext->mpCharacterInst);
    if (pInterp->doUnwindStack() && (pRecord->mnFlags & KU_TRY_HAS_CATCH))
    {
        AptValue* pThrown = pInterp->getThrownValue();
        if (pRecord->mnFlags & KU_TRY_CATCH_TO_REG)
        {
            AptScriptFunctionBase::SetRegisterValue(pRecord->mnRegister, pThrown);
        }
        else
        {
            EAStringC catchName(pRecord->mpCatchName);
            if (pInterp->mpCurrentFunction)
            {
                if (!AptScriptFunctionBase::GetActiveFrameStack())
                    pInterp->mpCurrentFunction->CreateFrameStack();
                AptScriptFunctionBase::GetActiveFrameStack()->Set(catchName, pThrown);
            }
            else
            {
                pInterp->setVariable(pContext->mpCIH, 0, &catchName, pThrown, 1, 1, 0);
            }
        }
        pInterp->clearThrownValue();
        pInterp->runStream(reinterpret_cast<const unsigned char*>(pRecord + 1) + pRecord->mnTryLen,
                           pContext->mpCIH, pRecord->mnCatchLen, pContext->mpCharacterInst);
    }

    if (pRecord->mnFlags & KU_TRY_HAS_FINALLY)
    {
        AptValue* pPending = pInterp->getThrownValue();
        if (pPending)
        {
            pPending->AddRef();
            pInterp->clearThrownValue();
        }
        pInterp->runStream(reinterpret_cast<const unsigned char*>(pRecord + 1) + (pRecord->mnTryLen + pRecord->mnCatchLen),
                           pContext->mpCIH, pRecord->mnFinallyLen, pContext->mpCharacterInst);
        if (pPending && !pInterp->doUnwindStack())
        {
            pInterp->throwValue(pPending);
            pPending->Release();
        }
    }

    const uint32_t nExitStackTop = pInterp->mOperandStack.mnTop;
    if (nExitStackTop > nEntryStackTop)
        pInterp->stackPop(nExitStackTop - nEntryStackTop);
}
