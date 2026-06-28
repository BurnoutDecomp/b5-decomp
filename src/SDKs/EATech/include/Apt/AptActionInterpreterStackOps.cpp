// ===========================================================================
// EATech Apt -- AptActionInterpreter ActionScript stack/constant-push opcodes.
//   DECOMPILED from the PS3 EXTERNAL ELF:
//     _FunctionAptActionPushDuplicate @0x7F3CC0
//     _FunctionAptActionStackSwap     @0x7EE634
//     _FunctionAptActionPush0         @0x806528
//     _FunctionAptActionPushTrue      @0x7F3D38
//     _FunctionAptActionPushFalse     @0x7F3DA8
//     _FunctionAptActionPushUndefined @0x7F3E88
//     _FunctionAptActionPushNULL      @0x7F3E18
//
// The operand-stack primitives in opcode form -- no inline bytecode operands, so
// they do not touch the PC. PushDuplicate re-pushes the current top (taking a new
// reference); StackSwap exchanges the top two operands in place (ownership
// unchanged); the Push* constants push a fresh/shared value. Each constant push is
// the inlined stackPush (store, advance, AddRef), reproduced via stackPush() here.
//
// PushNULL pushes gpUndefinedValue, the same value as PushUndefined -- this build
// aliases the ActionScript `null` literal to `undefined` (faithful to the asm).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"   // AptBoolean::Create

// FLAG (wired at AptInit; see AptValueConvert.cpp).
extern AptValue* gpUndefinedValue;

// ---------------------------------------------------------------------------
// _FunctionAptActionPop @0x7F33D0 -- discard the top value, but only while the
// stack is above the run's reserved base (mnStackBase). Releases the popped value.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPop(AptActionInterpreter* pInterp, LocalContextT*)
{
    if (pInterp->mnStackBase < pInterp->mnStackTop && pInterp->mnStackTop > 0)
    {
        pInterp->mpStack[pInterp->mnStackTop - 1]->Release();
        --pInterp->mnStackTop;
    }
    // FLAG: console flushes gpValuesToRelease here when the stack had exactly one
    // element (the deferred-release GC vector -- reconstructed with the GC layer).
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushDuplicate @0x7F3CC0 -- re-push the top value (+AddRef).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushDuplicate(AptActionInterpreter* pInterp, LocalContextT*)
{
    pInterp->stackPush(pInterp->mpStack[pInterp->mnStackTop - 1]);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionStackSwap @0x7EE634 -- swap the top two operands.
// The asm expresses this as pop x2 / push x2 (with degenerate guards) whose net
// effect, for a valid >=2-deep stack, is exchanging the top two slots with the
// stack depth unchanged and no refcount change (ownership is preserved).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStackSwap(AptActionInterpreter* pInterp, LocalContextT*)
{
    int top = pInterp->mnStackTop;
    AptValue* a = pInterp->mpStack[top - 1];   // top
    AptValue* b = pInterp->mpStack[top - 2];   // under
    pInterp->mpStack[top - 2] = a;
    pInterp->mpStack[top - 1] = b;
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPush0 @0x806528 -- push AptInteger(0).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPush0(AptActionInterpreter* pInterp, LocalContextT*)
{
    pInterp->stackPush(AptInteger::Create(0));
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushTrue @0x7F3D38 -- push the shared `true` AptBoolean.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushTrue(AptActionInterpreter* pInterp, LocalContextT*)
{
    pInterp->stackPush(AptBoolean::Create(true));
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushFalse @0x7F3DA8 -- push the shared `false` AptBoolean.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushFalse(AptActionInterpreter* pInterp, LocalContextT*)
{
    pInterp->stackPush(AptBoolean::Create(false));
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushUndefined @0x7F3E88 -- push the `undefined` singleton.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushUndefined(AptActionInterpreter* pInterp, LocalContextT*)
{
    pInterp->stackPush(gpUndefinedValue);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushNULL @0x7F3E18 -- push `null` (aliased to undefined here).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushNULL(AptActionInterpreter* pInterp, LocalContextT*)
{
    pInterp->stackPush(gpUndefinedValue);
}
