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
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"     // AptFloat::Create (PushFloat)

#include <cstdint>
#include <cstring>   // memcpy (PushFloat bit reinterpret)

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

// ---------------------------------------------------------------------------
// _FunctionAptActionPush1 @0x8064B4 -- push AptInteger(1).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPush1(AptActionInterpreter* pInterp, LocalContextT*)
{
    pInterp->stackPush(AptInteger::Create(1));
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushByte @0x806430 -- read one signed byte inline, push it.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushByte(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    signed char v = static_cast<signed char>(*pCtx->mpProgramCounter);
    pCtx->mpProgramCounter += 1;
    pInterp->stackPush(AptInteger::Create(v));
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushWord @0x8063A0 -- read a big-endian int16 inline, push it
// (sign-extended). Assembled byte-by-byte so it is correct regardless of host
// endianness (the .apt bytecode is big-endian).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushWord(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    const unsigned char* p = pCtx->mpProgramCounter;
    int16_t v = static_cast<int16_t>((p[0] << 8) | p[1]);
    pCtx->mpProgramCounter += 2;
    pInterp->stackPush(AptInteger::Create(v));
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushDWord @0x8062FC -- read a big-endian int32 inline, push it.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushDWord(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    const unsigned char* p = pCtx->mpProgramCounter;
    int32_t v = (static_cast<int32_t>(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    pCtx->mpProgramCounter += 4;
    pInterp->stackPush(AptInteger::Create(v));
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushFloat @0x807F50 -- read a big-endian 32-bit float inline,
// push it. The console assembles the four bytes (big-endian) and reinterprets the
// word as a float; the byte-by-byte assembly keeps it x64-correct.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushFloat(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    const unsigned char* p = pCtx->mpProgramCounter;
    uint32_t u = (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
               | (static_cast<uint32_t>(p[2]) << 8) | p[3];
    pCtx->mpProgramCounter += 4;
    float f;
    std::memcpy(&f, &u, sizeof(f));
    pInterp->stackPush(AptFloat::Create(f));
}
