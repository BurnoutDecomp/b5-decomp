#pragma once

// ===========================================================================
// EATech Apt -- AptActionInterpreter: the ActionScript bytecode virtual machine.
//
// This is the linchpin the rest of the Apt engine converges on (frame actions,
// AptCharacterAnimation::Fixup's resolveStream, value comparison/arithmetic, the
// native methods). Reconstructed leaf-first. This header now covers: the full
// interpreter LAYOUT (the five {count,capacity,array} stacks + per-run state, from
// initialize() @0x7F29D4), the operand-stack primitives, the per-execution context
// (LocalContextT, from runStream @0x81BD50), and the bytecode opcode handlers.
// STATUS (corrected 2026-07-01): runStream's dispatch loop IS homed
// (AptActionRun.cpp: reads each opcode + calls sGlobalTable[opcode](this,&ctx)); the
// dispatch table is extracted + filled by InitDispatchTable (AptActionDispatch.cpp), so
// the earlier "blocked on binary DATA" note was stale. getVariable/setVariable + the
// bulk of the opcode handlers are homed. Genuine follow-on: the remaining unbuilt
// sGlobalTable slots (still the no-op _FunctionAptActionStub) and the .apt-format
// resolve re-parse (_parseStream). See the Apt audit for the VM-reconnection worklist.
//
// The console indexes the stacks with byte math (4 * top + base); here it is typed
// element indexing (mpStack[top]), which is the same on x64 where the pointers are
// 8 bytes. Because the interpreter is a runtime object (not serialised), the struct
// is reconstructed x64-native (8-byte pointers) -- the console's 32-bit field
// offsets (noted [c:0xNN]) therefore differ from this struct's; access is by name.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValue::AddRef/Release
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"  // mpCurrentFunction + SavedExecutionState

struct AptCIH;             // SDKs/EATech/include/Apt/AptCIH.h (the movie-clip scope)
struct AptCharacterInst;   // SDKs/EATech/include/Apt/AptCharacterInst.h
class EAStringC;          // SDKs/EATech/include/Apt/AptString/EAString.h (path/name buffers)

// FLAG (runtime-only AptActionInterpreter init parameters -- the block initialize()
// reads; not serialised, so it is modelled by its console field offsets). DEFINED
// here (was a TU-local struct in AptActionInterpreterStackOps.cpp; promoted to the
// header so the host bring-up -- BrnAptRuntimeBringUp.cpp -- can construct one to call
// AptActionInterpreter::initialize, the X360 AptUpdateInitialize's job). Layout is
// byte-identical to the prior local definition (console offsets 0x20/0x24/0x40).
struct AptInitParmsT
{
    uint8_t  mPad00[0x20];
    int32_t  iStackSize;        // [c:0x20] the operand-stack capacity
    int32_t  iCallStackDepth;   // [c:0x24] the four call-depth stacks' capacity
    uint8_t  mPad28[0x30 - 0x28];
    int32_t  iRegisterCount;    // [c:0x30] the AS register-block slot capacity (the
                                //   X360 InitializeStaticData @0x82AE26C0 reads
                                //   *(parms+48); default config word[12] == 128)
    uint8_t  mPad34[0x40 - 0x34];
    uint8_t  mbSkipTraceBytecodes; // [c:0x40] -> mbSkipTraceBytecodes
};

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

    // ---- the opcode dispatch table ---------------------------------------
    // runStream reads each bytecode byte and calls sGlobalTable[opcode]. The
    // opcode->handler MAP was extracted from the X360 ARTIST decrypted XEX (the
    // static table at vaddr 0x82F73068; see AptActionDispatch.cpp) -- valid for
    // opcodes 0x00..0xB8. Each handler is a static (interpreter, context) function.
    typedef void (*AptActionHandler)(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static AptActionHandler sGlobalTable[256];

    // Populate sGlobalTable from the extracted map (the console ships it as static
    // data; here it is filled at startup -- equivalent result). Unbuilt opcodes get
    // the no-op stub until their handlers are reconstructed.
    static void InitDispatchTable();
    static void _FunctionAptActionStub(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // ---- the execution loop (runStream @0x81BD50) ------------------------
    // Run an action bytecode stream against a CIH scope: set up the LocalContextT,
    // reserve a fresh operand-stack base, then dispatch opcode-by-opcode through
    // sGlobalTable until a stop op / the stream end / an abort. nLength == -1 is a
    // top-level run (pushes/pops the CIH on the target stack); nLength >= 0 is a
    // bounded sub-stream (e.g. a function body, which leaves its result on the
    // stack). Returns the final program counter.
    const unsigned char* runStream(const unsigned char* pStream, AptCIH* pCIH,
                                   int nLength, AptCharacterInst* pCharInst);

    // getVariable @0x82B03430 (X360) / 0x819814 (PS3) -- resolve a variable/property
    // by name against a scope: dynamic "$" vars, the path context (getContext +
    // findChild), the function scope chain, the object member lookup, and the global
    // fallback, else `undefined`. runStream binds the run's "this" through it.
    AptValue* getVariable(AptValue* pScope, AptValue* pTarget, const EAStringC* pName,
                          int nAllowSelf, int nSearchScopeChain, int nDirect);

    // getContext @0x8194CC -- parse a variable PATH (slash/dot syntax) into its
    // (context object, leaf name) and return the context kind. FLAG: the 115-line
    // path parser is its own follow-on TU; declared so getVariable can call it.
    int getContext(AptValue* pScope, AptValue* pTarget, const EAStringC* pPath,
                   AptValue** ppOutContext, EAStringC* pOutName);

    // setVariable @0x82B03048 -- getVariable's write side: resolve the path
    // (getContext), then the object member set, the scope chain, or the context's
    // native hash. Returns 1 if stored.
    int setVariable(AptValue* pScope, AptValue* pTarget, const EAStringC* pName,
                    AptValue* pValue, int nAllowScopeChain, int nSearchScopeChain, int nDirect);

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

    // stackAt @0x82ADC060 -- the operand nDepth slots below the top (0 = top).
    AptValue*  stackAt(int nDepth) const { return mpStack[mnStackTop - nDepth - 1]; }

    // ---- abort / thrown value (the AS Throw/try-catch abort latch) --------
    // The interpreter aborts the current run when mpAbortValue is set (Throw 0x2A
    // records the top operand here; runStream unwinds to mnStackBase). All four
    // are the leak's trivial accessors over mpAbortValue.
    // getThrownValue @0x82AD52B0 / doUnwindStack @0x82AD5298 / throwValue
    // @0x82AD52B8 / clearThrownValue @0x82AD5308.
    AptValue*  getThrownValue() const { return mpAbortValue; }
    bool       doUnwindStack() const  { return mpAbortValue != 0; }
    void       throwValue(AptValue* pValue) { pValue->AddRef(); mpAbortValue = pValue; }
    void       clearThrownValue() { mpAbortValue->Release(); mpAbortValue = 0; }

    // stackPushIndirect @0x7ECE34 -- push a value, first resolving an indirection:
    // an AptVFT_Lookup value (tag 8) indexes the interpreter's per-run register
    // window (mpRegisters); an AptVFT_Register value (tag 4) indexes the AS function
    // register file (AptScriptFunctionBase::GetRegisterValue). Anything else pushes
    // as-is. The push itself is the inlined stackPush (store/advance/AddRef).
    void       stackPushIndirect(AptValue* pValue);

    // ---- lifecycle (initialize/destroy the five stacks) ------------------
    // initialize @0x82AE39D8 -- allocate the five {count,capacity,array} stacks from
    // the operand-stack pool (operand stack sized by iStackSize, the four call-depth
    // stacks by iCallStackDepth), reset the per-run bookkeeping, and bring up the AS
    // register window / frame machinery (InitializeStaticData). Body in
    // AptActionInterpreterStackOps.cpp.
    void initialize(const AptInitParmsT* pParms);

    // ~AptActionInterpreter @0x82AE3918 -- free each of the five {count,capacity,
    // array} stacks back to the operand-stack pool (Deallocate(array, count*sizeof)).
    ~AptActionInterpreter();

    // ---- run scaffolding / native call dispatch --------------------------
    // CleanupAfterExecution @0x82AE9388 -- end-of-run cleanup: drop the pending
    // thrown value (mpAbortValue: render its string form then Release + clear) and
    // pop the register-block window back to pSavedBase (PopStaticData). The X360
    // form is (this, a2): a2 is the register-window base the caller saved before the
    // run (PushStaticData's return), so the arg is the void* base -- NOT a
    // SavedExecutionState (that mis-typing was an earlier reconstruction guess).
    void CleanupAfterExecution(void* pSavedBase);

    // callFunction @0x82AE3C08 -- invoke an AS callable value pFunction with nArgs
    // operands already on the stack (the function body run / native callback / new
    // dispatch). pScope is the "this"/path scope; pNewTarget/pConstructTarget feed
    // the SetupBeforeExecution preloads. Collapses the args + result on the stack.
    AptValue* callFunction(AptValue* pScope, AptValue* pFunction, int nArgs,
                           AptValue* pNewTarget, AptValue* pConstructTarget);

    // _doEnumerate @0x82B036D8 -- AS `for..in`: resolve the enumeration target,
    // replace the stack top with a null marker, then push each enumerable member
    // name of the target's native hash (skipping the two reserved keys).
    void _doEnumerate(AptValue* pScope, AptValue* pTarget);

    // _parseStream @0x82AF3440 -- relocate/transcode an action bytecode stream's
    // inline pointer/constant operands between the on-disk (.apt) form and the live
    // form (the resolve / unresolve pass). a2 is the relocation base; a4 the live/
    // disk direction (0 = resolve). FLAG: faithful transcode is a follow-on (see body).
    const unsigned char* _parseStream(const unsigned char* pStream, int nRelocBase,
                                      AptValue* pResolveCtx, int nDirection);

    // getObject @0x82B07F18 -- resolve a path name (pName) under (pScope, pTarget)
    // to the AptValue object it designates (findChild on the parsed context), or
    // null. Used by the SetTarget / valueToObject paths.
    AptValue* getObject(AptValue* pScope, AptValue* pTarget, const EAStringC* pName);

    // getName @0x82AF75C8 / getName2 @0x82AF7540 -- build the slash/dot path name of
    // a value into pOut (getName2 also appends a trailing "/" for an empty path).
    // FLAG: the core path walk (sub_82AF7400) is the path-builder follow-on.
    void getName (AptValue* pValue, EAStringC* pOut);
    void getName2(AptValue* pValue, EAStringC* pOut);

    // isObjectOfType @0x82AEA5B8 -- AS `instanceof`: walk pObject's prototype /
    // interface chain looking for pClass's prototype; type-aware fallback for the
    // non-object cases. Returns 1 on a match.
    int isObjectOfType(AptValue* pObject, AptValue* pClass);

    // _createObject @0x82B08088 -- the AS `new ClassName(args...)` value-materialiser.
    // Resolves the class value pClassName under (pScope, pTarget) via getVariable; if
    // it is a constructible script/native class, builds the matching built-in instance
    // (Array / String / Date / TextFormat / Color / MovieClip / XML / Error, else a
    // generic Object) from the nArrayLenHint operands on the stack, links it to the
    // class prototype (creating one when missing), copies the class's implemented
    // interfaces, and -- when bConstruct -- runs the constructor body (callFunction).
    // nArrayLenHint doubles as the constructor argument count. Returns the new value
    // (caller owns the construction ref) or null when the class cannot be constructed.
    AptValue* _createObject(AptValue* pScope, AptValue* pTarget, const EAStringC* pClassName,
                            int nArrayLenHint, char bConstruct);

    // isFSCommand @0x82AD9148 / doFSCommand @0x82AD91A0 -- the AS getURL "FSCommand:"
    // host-callback path: detect the "FSCommand:" url prefix and dispatch the
    // command (the part after the prefix) + its argument to the host hook.
    bool isFSCommand(const char* pUrl);
    int  doFSCommand(const char* pUrl, const char* pArgs);

    // urlDecode @0x82AEE208 -- parse one "key=value[&...]" pair out of pStream into
    // (pOutKey, pOutValue) (un-escaping each), returning the read position past the
    // pair (or null when there is no pair). Used by loadVariables.
    const char* urlDecode(const char* pStream, EAStringC* pOutKey, EAStringC* pOutValue);

    // ---- AS global builtins (cbCallMethod_*) that this TU owns ------------
    // cbCallMethod_ASSetPropFlags @0x82AD8448 -- ASSetPropFlags() stub (returns
    // `undefined`; the AS member-visibility flags are a no-op in this build).
    static AptValue* cbCallMethod_ASSetPropFlags(AptValue* pThis, int nArgCount);
    // cbCallMethod_setInterval @0x82B019D8 -- setInterval(fn|obj,method,ms,...args):
    // allocate a free AptIntervalTimer slot, bind the callback + period + extra args,
    // and return its integer id (or `undefined` when the table is full).
    static AptValue* cbCallMethod_setInterval(AptValue* pThis, int nArgCount);
    // cbCallMethod_clearInterval @0x82AE3AE0 -- clearInterval(id): find + tear down
    // the timer slot whose id matches the (integer) argument.
    static AptValue* cbCallMethod_clearInterval(AptValue* pThis, int nArgCount);

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

    // AsciiToChar @0x82AF3C18 (the AS chr() opcode) -- replace the top operand with
    // a one-byte string of its integer char code. Body in AptActionInterpreterBuiltins.cpp.
    static void _FunctionAptActionAsciiToChar(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // ---- ActionScript global builtins (cbCallMethod_*) --------------------
    // AS global functions the CallMethod dispatch invokes as f(thisValue, argCount),
    // reading their args off the global native-arg stack. Bodies in
    // AptActionInterpreterBuiltins.cpp.
    static AptValue* cbCallMethod_isNaN(AptValue* pThis, int nArgCount);     // @0x82AF99E8
    static AptValue* cbCallMethod_boolean(AptValue* pThis, int nArgCount);   // @0x82AF9BD8
    static AptValue* cbCallMethod_escape(AptValue* pThis, int nArgCount);    // @0x82AF9B08
    static AptValue* cbCallMethod_unescape(AptValue* pThis, int nArgCount);  // @0x82AF9A50

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
    //   Pop @0x7F33D0           : discard the top (only above the run's stack base)
    // (Push1 the same as Push0 with 1, deferred pending its confirmed address.)
    static void _FunctionAptActionPop          (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushDuplicate(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionStackSwap    (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPush0        (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushTrue     (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushFalse    (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushUndefined(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushNULL     (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Immediate pushes -- read an inline operand from the bytecode (via the PC) and
    // push it. The .apt stores them big-endian, so they are assembled byte-by-byte
    // (host-endian-independent -> x64-correct):
    //   Push1 @0x8064B4    : AptInteger(1)
    //   PushByte @0x806430 : signed byte
    //   PushWord @0x8063A0 : big-endian int16 (sign-extended)
    //   PushDWord @0x8062FC: big-endian int32
    //   PushFloat @0x807F50: big-endian float bits
    static void _FunctionAptActionPush1    (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushByte (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushWord (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushDWord(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushFloat(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Variable access opcodes -- coerce the stack-top name (Get_ToString) and
    // delegate to the resolver, using the run scope (ctx.mpCIH) + target slot
    // (ctx.mpPendingReleaseValue):
    //   GetVariable @0x82B038A0 (0x1C): getVariable -> replace the name with the result
    //   SetVariable @0x82B03970 (0x1D): setVariable name = value, then pop both
    static void _FunctionAptActionGetVariable(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionSetVariable(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Member access opcodes -- dispatch on the object's value type (array index /
    // named member / extern), coercing the key via Get_ToString:
    //   GetMember @0x82B04200 (0x4E): object[key] -> push the member (else undefined)
    //   SetMember @0x82B043C8 (0x4F): object[key] = value, then pop all three
    static void _FunctionAptActionGetMember(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionSetMember(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Special-value / register / control opcodes (small standalone leaves):
    //   PushThis 0x56 / PushGlobal 0x58 : push the name string "this" / "_global"
    //   PushThisVariable 0x70 : resolve `this` -> push;  PushGlobalVariable 0x71 : push _global
    //   GetTimer 0x34 : push AptInteger(elapsed ms);  StoreRegister 0x87 : top -> register N
    //   Return 0x3E : end the run (ctx.mbStop)
    static void _FunctionAptActionPushThis          (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushGlobal        (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushThisVariable  (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushGlobalVariable(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionGetTimer          (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionStoreRegister     (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionReturn            (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Value-output / control opcodes:
    //   Trace 0x26 : emit the top's string form via the host debug sink
    //   Throw 0x2A : record the top as the thrown value (mpAbortValue) -> runStream stops
    //   ToString 0x4B : coerce the top to a string in place
    static void _FunctionAptActionTrace   (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionThrow   (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionToString(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Delete2 0x3B : `delete name` -- clear the variable (setVariable null), push true
    static void _FunctionAptActionDelete2 (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Self-contained value / string / array opcodes:
    //   PushZeroSetVar 0x72 : push 0, then SetVariable;  StringLength 0x14 : top -> its length
    //   StringAdd 0x21 : concat top two as strings;  Random 0x30 : AptInteger(rand % top)
    //   InitArray 0x42 : build an AptArray from N stack values
    static void _FunctionAptActionPushZeroSetVar(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionStringLength  (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionStringAdd     (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionRandom        (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionInitArray     (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // ECMA value opcodes:
    //   StringEquals 0x13 : AptBoolean(toString(top) == toString(under))
    //   Add2 0x47 : ECMA `+` -- string concat OR numeric (int/float) add
    static void _FunctionAptActionStringEquals(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionAdd2        (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Class opcodes (prototype chain):
    //   Extends 0x69 : chain the subclass prototype to the superclass
    //   ImplementsOp 0x2C : record the implemented interfaces' prototypes
    static void _FunctionAptActionExtends     (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionImplementsOp(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // String-conversion opcodes:
    //   SubString 0x15 : substring(str, start, len);  ToNumber 0x4A : coerce top to a number
    static void _FunctionAptActionSubString(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionToNumber (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Local-variable definition (bind in the current function's frame via the
    // AptScriptFunctionBase keystone):
    //   DefineLocal 0x3C : `var name = value`;  DefineLocal2 0x41 : `var name`
    static void _FunctionAptActionDefineLocal (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionDefineLocal2(AptActionInterpreter* pInterp, LocalContextT* pContext);

    // ---- call / member / function-definition opcodes ----------------------
    // The function-call family (CallFunction/CallMethod/...) drives callFunction;
    // the New* ops construct objects; the Define* ops build script-function /
    // dictionary values from the inline bytecode. Bodies in sibling call-op TUs.
    static void _FunctionAptActionCallFrame            (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionCallFuncSetVar       (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionCallFunction         (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionCallMethod           (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionCallMethodPop        (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionCallMethodSetVar     (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionDictCallFuncSetVar   (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionDictCallMethodPop    (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionDictCallMethodSetVar (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionNewMethod            (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionTry                  (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Dictionary-aware constant/variable push opcodes (the .apt string-dictionary
    // shortcut forms of Push + the Get/Set fused ops):
    static void _FunctionAptActionPush                 (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushStringDictByte   (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushStringGetMember  (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushStringGetVar     (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushStringSetMember  (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPushStringSetVar     (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionStringDictByteGetMember(AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionStringDictByteGetVar (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Timeline / sprite control opcodes (gotoFrame / play / stop / clone / drag /
    // setTarget) -- they drive the bound character instance's animation timeline.
    static void _FunctionAptActionGotoFrame            (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionGotoFrame2           (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionGotoLabel            (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionNextFrame            (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionPlay                 (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionStop                 (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionCloneSprite          (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionRemoveSprite         (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionStartDragMovie       (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionSetTarget            (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionSetTarget2           (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionTargetPath           (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // Object construction / property / type opcodes:
    static void _FunctionAptActionNewObject            (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionInitObject           (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionDefineFunction       (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionDefineFunction2      (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionDefineDictionary     (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionGetProperty          (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionSetProperty          (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionEnumerate            (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionCastOp               (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionTypeOf               (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // URL / trace / equality opcodes:
    static void _FunctionAptActionGetUrl               (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionGetUrl2              (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionTraceStart           (AptActionInterpreter* pInterp, LocalContextT* pContext);
    static void _FunctionAptActionEquals2              (AptActionInterpreter* pInterp, LocalContextT* pContext);

    // ---- state ------------------------------------------------------------
    // Full layout mapped from initialize() @0x7F29D4: the interpreter owns five
    // parallel {count, capacity, array} stacks (the operand stack sized by
    // AptInitParmsT::iStackSize, the other four by iCallStackDepth) plus per-run
    // bookkeeping. Console offsets (32-bit) are noted in comments; this struct is
    // reconstructed x64-native (8-byte pointers) so the offsets differ -- access is
    // by member name, which is what the handlers/runStream do.
    //
    // The operand stack (#1) and the CIH/target stack (#4, which runStream pushes
    // the running CIH onto) are confirmed; the other three call-depth-sized stacks
    // (#2/#3/#5 -- the AVM1 call/return, with-scope and try stacks, in some order)
    // are named generically pending confirmation.

    int        mnStackTop;        // [c:0x00] operand-stack count / next-free index
    int        mnStackCapacity;   // [c:0x04] iStackSize
    AptValue** mpStack;           // [c:0x08] operand-stack array

    int        mnCallStackB_Count;    // [c:0x0C] call-depth stack #2 (role TBD)
    int        mnCallStackB_Capacity; // [c:0x10] iCallStackDepth
    void**     mpCallStackB;          // [c:0x14]

    int        mnCallStackC_Count;    // [c:0x18] call-depth stack #3 (role TBD)
    int        mnCallStackC_Capacity; // [c:0x1C]
    void**     mpCallStackC;          // [c:0x20]

    int        mnCIHStackTop;         // [c:0x24] CIH/target stack count
    int        mnCIHStackCapacity;    // [c:0x28]
    AptCIH**   mpCIHStack;            // [c:0x2C] CIH/target stack array

    int        mnCallStackE_Count;    // [c:0x30] call-depth stack #5 (role TBD)
    int        mnCallStackE_Capacity; // [c:0x34]
    void**     mpCallStackE;          // [c:0x38]

    AptScriptFunctionBase* mpCurrentFunction;  // [c:0x3C] the script function currently executing
                                  // (callFunction sets it; DefineLocal + the scope chain read it)
    uint32_t   field_40;          // [c:0x40] unmapped (callFunction's saved register-window slot)
    AptValue** mpRegisters;       // [c:0x44] the per-call register window (stackPushIndirect)
    uint32_t   field_48[6];       // [c:0x48..0x5C] unmapped
    AptValue*  mpAbortValue;      // [c:0x60] the thrown value (null = no abort) -- Throw sets it,
                                  // runStream checks it after each op and unwinds. x64: a pointer
                                  // (the console int slot held the 32-bit AptValue*), not an int.
    int        mnStackBase;       // [c:0x64] operand-stack base this run unwinds to (Pop won't go below it)
    uint8_t    mbSkipTraceBytecodes; // [c:0x68]
    uint8_t    field_69;          // [c:0x69]
};
