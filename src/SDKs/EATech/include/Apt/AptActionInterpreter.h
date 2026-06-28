#pragma once

// ===========================================================================
// EATech Apt -- AptActionInterpreter: the ActionScript bytecode virtual machine.
//
// This is the linchpin the rest of the Apt engine converges on (frame actions,
// AptCharacterAnimation::Fixup's resolveStream, value comparison/arithmetic, the
// native methods). It is reconstructed leaf-first; THIS header covers only the
// VM's operand stack -- the AptValue* evaluation stack every bytecode handler
// pushes/pops -- plus the interpreter fields the stack touches. The opcode
// dispatch (runStream), the per-action handlers (_FunctionAptAction*), the
// execution context (LocalContextT), the register/local machinery and
// PrepareForExecution/CleanupAfterExecution are the follow-on.
//
// LAYOUT recovered from the PS3 EXTERNAL ELF stack primitives (they read the
// interpreter as a flat record, no vtable):
//     +0x00  mnStackTop   int            -- count / next-free index into mpStack
//     +0x04  (unmapped)   int            -- a field exists here (mpStack is at +8,
//                                           not +4); its meaning is not yet
//                                           recovered. Reserved, named field_04.
//     +0x08  mpStack      AptValue**     -- the operand-stack array base
//     ...                                -- the large remainder (the register
//            +0x44  mpRegisters AptValue**  array at +0x44 is referenced by
//                                           stackPushIndirect; the rest of the VM
//                                           state) is mapped as later leaves land.
//
// The console indexes mpStack with byte math (4 * top + base); here it is typed
// element indexing (mpStack[top]), which is the same on x64 where AptValue* is
// 8 bytes -- the operand stack stays pointer-width-correct without a transcode.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValue::AddRef/Release

class AptActionInterpreter
{
public:
    // The per-execution context threaded through every opcode handler (the second
    // handler argument). Opaque here -- it is reconstructed alongside the dispatch
    // loop (runStream); the arithmetic/logic handlers below do not read it.
    struct LocalContextT;

    // ---- operand stack (PS3 EXTERNAL ELF) --------------------------------
    // Push: store + advance, AddRef the value (the stack owns a counted ref).
    void       stackPush(AptValue* pValue);          // @0x7F1790
    // Push without taking a reference (the caller's ref transfers to the stack).
    void       stackPushNoInc(AptValue* pValue);     // @0x7E990C

    // Pop the top value, Release the stack's ref, and return it (the returned
    // pointer has already been Release()'d -- callers use it before it can die,
    // matching the console). Returns null on an empty stack.
    AptValue*  stackPop();                            // @0x7F3248
    // Pop nCount values, releasing each (no-op unless mnStackTop >= nCount).
    void       stackPop(int nCount);                  // @0x7FDB68
    // As stackPop(nCount) but with the extra nCount>0 guard (bounds-safe form).
    void       stackSafePop(int nCount);              // @0x7DF7F8

    // Pop and return the top value WITHOUT releasing it (the caller takes over the
    // reference). No bounds check, matching the console.
    AptValue*  stackGetPop();                         // @0x7DF7B4
    // Lower the top slot without releasing the value (its ref is kept elsewhere).
    void       stackPopNoDec();                       // @0x7DF7DC

    // Replace the top nCount operands with a single value (AddRef the new value,
    // Release the nCount popped, store the new at the collapsed slot).
    void       stackPopAndPush(int nCount, AptValue* pValue);   // @0x7FB288

    // FOLLOW-ON: stackPushIndirect @0x7ECE34 resolves AptVFT_Lookup (via the
    // register array at +0x44) / AptVFT_Register (via AptScriptFunctionBase::
    // GetRegisterValue) values before pushing. Deferred until that register/local
    // machinery + AptScriptFunctionBase are reconstructed.

    // ---- ActionScript opcode handlers (static; (interpreter, context)) ----
    // The bytecode dispatch registers these by opcode. Each is a static function
    // taking the interpreter + the execution context; the dispatcher ignores the
    // return, so they are void. THIS leaf covers the logic ops -- they pop their
    // operand(s) off the stack, coerce, and push an AptBoolean result:
    //   Not @0x7F3BB8 : !toBool(top)
    //   And @0x7FD4D4 : top-1 && top   (int fast path, else toFloat non-zero AND)
    //   Or  @0x7FD29C : top-1 || top   (int fast path, else toFloat non-zero OR)
    // The arithmetic/comparison/branch/data handlers are the follow-on.
    static void _FunctionAptActionNot(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionAnd(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionOr (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Arithmetic ops -- pop the two operands (under OP top), push an AptInteger
    // (when both are defined ints, for +/-/*) or an AptFloat result:
    //   Add @0x808C90  Subtract @0x808A8C  Multiply @0x808888
    //   Divide @0x8086C4  Modulo @0x8084F8   (Divide/Modulo are float-only and
    //   yield `undefined` on a zero divisor).
    static void _FunctionAptActionAdd     (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionSubtract(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionMultiply(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionDivide  (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionModulo  (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Comparison ops -- compare the two operands (the result is `under OP top`) and
    // push an AptBoolean:
    //   Equals @0x7FD938  : under == top (exact int, else float within 0.001)
    //   LessThan @0x7FD714: under <  top
    //   Greater @0x7FCF30 : under >  top (type-aware: string strcmp / float / int)
    static void _FunctionAptActionEquals  (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionLessThan(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionGreater (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Bitwise ops -- coerce both operands to integers and push AptInteger(under OP
    // top); the shift ops take top as the shift amount. (Their stack collapse IS
    // stackPopAndPush(2, result) -- the compiler inlined that primitive.)
    //   BitAnd @0x806BCC  BitOr @0x806A40  BitXor @0x8068B4
    //   BitLShift @0x806728  BitRShift @0x80659C
    static void _FunctionAptActionBitAnd   (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionBitOr    (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionBitXor   (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionBitLShift(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionBitRShift(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Unary numeric ops -- pop one operand, coerce, push the result:
    //   Increment @0x80817C : x+1   (int when defined int, else AptFloat)
    //   Decrement @0x807FF4 : x-1
    //   ToInteger @0x806F04 : AptInteger(toInteger(x))
    // (ToNumber @0x808304 deferred -- it needs Get_ToString/toString + _isNaN +
    // UTF8_Find, the string-conversion layer.)
    static void _FunctionAptActionIncrement(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionDecrement(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionToInteger(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // ---- partial state (see header note) ---------------------------------
    int        mnStackTop;   // +0x00
    int        field_04;     // +0x04 -- unmapped; reserved so mpStack lands at +0x08
    AptValue** mpStack;      // +0x08
};
