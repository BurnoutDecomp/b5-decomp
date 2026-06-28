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

#include <cstdint>
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValue::AddRef/Release

class AptCIH;             // SDKs/EATech/include/Apt/AptCIH.h (the movie-clip scope)
class AptCharacterInst;   // SDKs/EATech/include/Apt/AptCharacterInst.h

class AptActionInterpreter
{
public:
    // ---- the per-execution context (the second handler argument) ----------
    // Reconstructed from runStream @0x81BD50 (the stack-local frame it builds and
    // passes to every sGlobalTable handler). Runtime-only (NOT serialised), so it
    // is reconstructed with x64-native widths + named members rather than the
    // console's 32-bit frame offsets. The data/branch/call handlers read the PC
    // (mpProgramCounter) to fetch inline operands; the expression handlers ignore
    // it.
    struct LocalContextT
    {
        const unsigned char* mpProgramCounter;     // the action-bytecode read pointer
        AptCIH*              mpCIH;                 // the current movie-clip scope
        AptValue*            mpPendingReleaseValue; // temp released when the PC reaches...
        const unsigned char* mpPendingReleasePC;   // ...this position
        AptValue*            mpScopeVariable;       // the run's scope ("this"), from getVariable
        bool                 mbStop;                // a stop/end op sets this -> end execution
        AptCharacterInst*    mpCharacterInst;       // the originating character instance
    };

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

    // Branch ops -- the first handlers that drive the PC. They read a 4-byte
    // (4-byte-aligned) signed offset inline from the bytecode, advance the PC past
    // it, then jump: BranchAlways unconditionally, BranchIf{True,False} after
    // popping a toBool condition.
    //   BranchAlways  @0x7F1C44   BranchIfTrue @0x7F349C   BranchIfFalse @0x7F35B4
    static void _FunctionAptActionBranchAlways (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionBranchIfTrue (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionBranchIfFalse(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Stack-manipulation + constant-push ops -- push a fresh/constant/duplicate
    // value, or reorder the top of the operand stack (no inline operands):
    //   PushDuplicate @0x7F3CC0 : re-push the top value (+AddRef)
    //   StackSwap @0x7EE634     : swap the top two operands
    //   Push0 @0x806528         : push AptInteger(0)
    //   PushTrue/PushFalse @0x7F3D38/0x7F3DA8 : push the AptBoolean singletons
    //   PushUndefined/PushNULL @0x7F3E88/0x7F3E18 : push gpUndefinedValue
    // (Pop @0x7F33D0 deferred -- it respects the run's stack base @+0x64; Push1 the
    //  same as Push0 with 1, deferred pending its confirmed address.)
    static void _FunctionAptActionPushDuplicate(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionStackSwap    (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPush0        (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushTrue     (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushFalse    (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushUndefined(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushNULL     (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // ---- partial state (see header note) ---------------------------------
    // Mapped so far: the operand stack. runStream/stackPushIndirect also revealed
    // (NOT yet added -- they would need a large unmapped gap fabricated, so they
    // are recorded here and added when runStream/the data handlers land):
    //   +0x24 mpCIHStack (a second stack: top@+0x24, array@+0x2C -- the target/CIH
    //         stack runStream pushes the running CIH onto)
    //   +0x44 mpRegisters (AptValue** -- the local register array, stackPushIndirect)
    //   +0x60 mnAbortFlag (checked after each handler in runStream)
    //   +0x64 mnStackBase  (the stack depth this run unwinds to)
    int        mnStackTop;   // +0x00
    int        field_04;     // +0x04 -- unmapped; reserved so mpStack lands at +0x08
    AptValue** mpStack;      // +0x08
};
