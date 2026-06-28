// ===========================================================================
// EATech Apt -- small standalone ActionScript opcodes: the "push a special value"
// ops (this / _global / the timer), the register store, and the function return.
//   DECOMPILED from the X360 ARTIST:
//     PushThis           @0x82AF4018  (0x56)  -- push the string "this"
//     PushGlobal         @0x82AF40A0  (0x58)  -- push the string "_global"
//     PushThisVariable   @0x82B05550  (0x70)  -- push the resolved `this` value
//     PushGlobalVariable @0x82ADE798  (0x71)  -- push the _global object
//     GetTimer           @0x82AE9CA0  (0x34)  -- push AptInteger(elapsed ms)
//     StoreRegister      @0x82ADE468  (0x87)  -- store the stack top into a register
//     Return             @0x82AD9208  (0x3E)  -- end the run (set ctx.mbStop)
//
// PushThis/PushGlobal build an AptString holding the pooled key ("this"/"_global")
// -- the NAME, to be resolved by a following GetVariable. PushThisVariable resolves
// `this` directly through getVariable (run scope mpCIH + target slot). The standard
// SWF opcodes match exactly (GetTime 0x34, Return 0x3E, StoreRegister 0x87); the
// 0x56/0x58/0x70/0x71 are EA's extended set.
//
// FLAG -- string-pool constants + the host clock (wired at AptInit):
//   gAptKeyThis (unk_8324E6C0 "this"), gAptKeyGlobal (unk_8324E59C "_global") are
//   pooled EAStringC keys; gAptTimerMs (dword_8324D820) is the host millisecond
//   clock the runtime advances each frame. The AptString::Create seed (unk_820046A7)
//   is the console's seed-prefix const -- irrelevant here (the str is overwritten).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"     // AptString::Create / GetInternalString
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"    // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptString/EAString.h"     // EAStringC::operator=
#include "SDKs/EATech/include/Apt/AptObject.h"              // AptValueWithHash (gpAptGlobalFallback upcast)
#include "SDKs/EATech/include/Apt/AptGlobal.h"              // gpAptGlobalFallback
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"  // AptScriptFunctionBase::SetRegisterValue
#include "SDKs/EATech/include/Apt/AptCIH.h"                 // AptCIH : AptValueGC (mpCIH -> AptValue* upcast)

#include <cstdint>

// FLAG (string-pool constants -- wired at AptInit; gAptKeyThis already used by
// AptScriptFunction2): the pooled "this"/"_global" keys.
extern const EAStringC gAptKeyThis;     // unk_8324E6C0
extern const EAStringC gAptKeyGlobal;   // unk_8324E59C

// FLAG (host clock -- advanced by the runtime each frame; wired at AptInit): the
// elapsed-milliseconds counter GetTimer reads (console dword_8324D820).
extern int32_t gAptTimerMs;

// ---------------------------------------------------------------------------
// PushThis @0x82AF4018 (0x56) -- push the string "this".
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushThis(AptActionInterpreter* pInterp,
                                                      LocalContextT* /*pContext*/)
{
    AptString* pStr = AptString::Create("");          // FLAG: seed const, overwritten below
    *pStr->GetInternalString() = gAptKeyThis;
    pInterp->mpStack[pInterp->mnStackTop++] = pStr;    // inlined stackPush
    pStr->AddRef();
}

// ---------------------------------------------------------------------------
// PushGlobal @0x82AF40A0 (0x58) -- push the string "_global".
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushGlobal(AptActionInterpreter* pInterp,
                                                        LocalContextT* /*pContext*/)
{
    AptString* pStr = AptString::Create("");          // FLAG: seed const, overwritten below
    *pStr->GetInternalString() = gAptKeyGlobal;
    pInterp->mpStack[pInterp->mnStackTop++] = pStr;
    pStr->AddRef();
}

// ---------------------------------------------------------------------------
// PushThisVariable @0x82B05550 (0x70) -- resolve `this` and push it.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushThisVariable(AptActionInterpreter* pInterp,
                                                              LocalContextT* pContext)
{
    AptValue* pResult = pInterp->getVariable(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                             &gAptKeyThis, 1, 1, 0);
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// PushGlobalVariable @0x82ADE798 (0x71) -- push the _global object directly.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushGlobalVariable(AptActionInterpreter* pInterp,
                                                                LocalContextT* /*pContext*/)
{
    pInterp->mpStack[pInterp->mnStackTop++] = gpAptGlobalFallback;
    gpAptGlobalFallback->AddRef();
}

// ---------------------------------------------------------------------------
// GetTimer @0x82AE9CA0 (0x34) -- push the elapsed-milliseconds count.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGetTimer(AptActionInterpreter* pInterp,
                                                      LocalContextT* /*pContext*/)
{
    AptInteger* pVal = AptInteger::Create(gAptTimerMs);
    pInterp->mpStack[pInterp->mnStackTop++] = pVal;
    pVal->AddRef();
}

// ---------------------------------------------------------------------------
// StoreRegister @0x82ADE468 (0x87) -- store the stack top into register N (an
// inline 4-byte-aligned register index; the value stays on the stack).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStoreRegister(AptActionInterpreter* pInterp,
                                                           LocalContextT* pContext)
{
    const int32_t* pRegIndex = reinterpret_cast<const int32_t*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
    pContext->mpProgramCounter = reinterpret_cast<const unsigned char*>(pRegIndex + 1);
    AptScriptFunctionBase::SetRegisterValue(*pRegIndex, pInterp->mpStack[pInterp->mnStackTop - 1]);
}

// ---------------------------------------------------------------------------
// Return @0x82AD9208 (0x3E) -- end the action run (the bounded sub-stream leaves
// the return value on the operand stack; runStream observes mbStop and unwinds).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionReturn(AptActionInterpreter* /*pInterp*/,
                                                    LocalContextT* pContext)
{
    pContext->mbStop = true;
}
