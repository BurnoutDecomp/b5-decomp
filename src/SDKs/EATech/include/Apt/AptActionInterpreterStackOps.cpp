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
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"    // AptString::Create / GetInternalString
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"  // gpAptOperandStackPool (off_8324D808)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC (dictionary-string temp)
#include "SDKs/EATech/include/Apt/AptCIH.h"                // AptCIH : AptValueGC (mpCIH -> AptValue* upcast)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"          // GetCharacterInst
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h" // mnClipActionFlags
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h" // SetRegisterValue / InitializeStaticData

#include <cstdint>
#include <cstring>   // memcpy (PushFloat bit reinterpret)

// FLAG (wired at AptInit; see AptValueConvert.cpp).
extern AptValue* gpUndefinedValue;

// FLAG (member follow-on -- stackPushIndirect @0x7ECE34 is declared in
// AptActionInterpreter.h only as a deferred FOLLOW-ON, not as a callable member:
// it resolves AptVFT_Lookup (via the register array at +0x44) / AptVFT_Register
// (via AptScriptFunctionBase::GetRegisterValue) values before pushing. _Push
// below needs it; it is forwarded through this shim until that register/local
// machinery is reconstructed and the member is declared. The console call is the
// member AptActionInterpreter::stackPushIndirect(pInterp, pValue).
extern void AptActionInterpreter_stackPushIndirect(AptActionInterpreter* pInterp, AptValue* pValue);

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

// ===========================================================================
// Dictionary-aware constant / variable push opcodes (the .apt string-dictionary
// shortcut forms of Push + the fused Push+Get/Set ops).
//   DECOMPILED from the X360 ARTIST:
//     _FunctionAptActionPush                 @0x82ADE498
//     _FunctionAptActionPushStringDictByte   @0x82ADE7D0
//     _FunctionAptActionPushStringGetVar     @0x82B05640
//     _FunctionAptActionPushStringGetMember  @0x82B056E8
//     _FunctionAptActionPushStringSetVar     @0x82B05788
//     _FunctionAptActionPushStringSetMember  @0x82B05828
//     _FunctionAptActionStringDictByteGetVar @0x82B058C8
//     _FunctionAptActionStringDictByteGetMember @0x82B05970
//
// The "StringDictByte"/"Push*String*" families take a single inline byte that
// indexes the string-constant dictionary the console keeps at interpreter offset
// +0x44 (the same slot the header names mpRegisters; the dict opcodes reuse it as
// the AptValue* string-pool array). Each pushes the dictionary entry onto the
// operand stack -- the inlined store/advance/AddRef sequence the asm emits is
// exactly stackPush(), reproduced here -- and the fused forms then delegate to the
// matching variable/member opcode handler. The console indexes the array with byte
// math (4 * byteIndex); on x64 it is typed element indexing (mpRegisters[byte]),
// the same entry where the pointers are 8 bytes.
//
// The "Push*String*Get/SetVar" forms also pass the entry's embedded EAStringC name
// straight into getVariable; the asm inlines AptValue::Get_ToString (a raw type-1
// StringValue exposes its own string at +8, a boxed StringObject tag 33 indirects
// through +0x20 first). That dispatch is reused via Get_ToString here.
// ===========================================================================

// ---------------------------------------------------------------------------
// _FunctionAptActionPush @0x82ADE498 -- push a run of dictionary/constant values
// described by an inline operand block. The console aligns the PC up to 4, reads
// {int count; AptValue** array}, advances the PC past both, then stackPushIndirect's
// each array[i] (resolving Lookup/Register entries as it pushes).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPush(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    // Align the read pointer up to the next 4-byte boundary, then consume the
    // {count, array} header. FLAG: the inline operand block stores a 4-byte
    // serialized pointer (console widths); its on-x64 resolved width is settled by
    // _parseStream (the transcode, a deferred follow-on), so the count+array header
    // is read in the console's serialized form here.
    const unsigned char* pAligned =
        reinterpret_cast<const unsigned char*>(
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
    const int32_t   nCount = *reinterpret_cast<const int32_t*>(pAligned);
    AptValue* const* pArray = *reinterpret_cast<AptValue* const* const*>(pAligned + 4);
    pCtx->mpProgramCounter = pAligned + 8;

    for (int32_t i = 0; i < nCount; ++i)
        AptActionInterpreter_stackPushIndirect(pInterp, pArray[i]);   // FLAG: deferred member shim
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushStringDictByte @0x82ADE7D0 -- push the dictionary string
// the inline byte indexes (the .apt string-constant table at +0x44). The console
// emits the inlined stackPush (store at the top, advance, AddRef via vtbl[0]).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushStringDictByte(AptActionInterpreter* pInterp,
                                                                LocalContextT* pCtx)
{
    const unsigned int nIndex = *pCtx->mpProgramCounter;   // byte dictionary index
    pCtx->mpProgramCounter += 1;

    AptValue* pEntry = pInterp->mpRegisters[nIndex];        // console: *(4*idx + a1[17])
    pInterp->mpStack[pInterp->mnStackTop++] = pEntry;       // inlined stackPush (store + advance)
    pEntry->AddRef();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushStringGetMember @0x82B056E8 -- build an AptString from the
// inline dictionary string, push it, then run the GetMember opcode (object[name]).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushStringGetMember(AptActionInterpreter* pInterp,
                                                                 LocalContextT* pCtx)
{
    // Align the PC up to 4 and read the inline string pointer (advances the PC by 4).
    const unsigned char* pAligned =
        reinterpret_cast<const unsigned char*>(
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
    const char* szName = *reinterpret_cast<const char* const*>(pAligned);
    pCtx->mpProgramCounter = pAligned + 4;

    // AptString::Create("") + InitFromBuffer/operator=/Decrease idiom (the X360's
    // empty-seed-then-assign); the temporary EAStringC's ctor/dtor are the asm's
    // InitFromBuffer / DecreaseInternalRefCount pair.
    AptString* pStr = AptString::Create("");                // FLAG: seed const @0x820046A7 ("")
    *pStr->GetInternalString() = EAStringC(szName);

    pInterp->mpStack[pInterp->mnStackTop++] = pStr;         // inlined stackPush (store + advance)
    pStr->AddRef();

    _FunctionAptActionGetMember(pInterp, pCtx);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushStringGetVar @0x82B05640 -- resolve the inline dictionary
// string as a variable (getVariable) and push the result. The console builds a
// scratch EAStringC from the inline string into the global name slot (off_82F733B0)
// and reads the run scope/target from the context (ctx+4 / ctx+8).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushStringGetVar(AptActionInterpreter* pInterp,
                                                              LocalContextT* pCtx)
{
    const unsigned char* pAligned =
        reinterpret_cast<const unsigned char*>(
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
    const char* szName = *reinterpret_cast<const char* const*>(pAligned);
    pCtx->mpProgramCounter = pAligned + 4;

    // FLAG: the console assembles the name into the shared scratch EAStringC
    // off_82F733B0 (InitFromBuffer + operator= + DecreaseInternalRefCount) and
    // passes &off_82F733B0 as the name. The scratch global is not reconstructed; a
    // local EAStringC name is behaviourally faithful (same value passed to
    // getVariable).
    EAStringC name(szName);
    AptValue* pResult = pInterp->getVariable(pCtx->mpCIH, pCtx->mpPendingReleaseValue,
                                             &name, 1, 1, 0);

    pInterp->mpStack[pInterp->mnStackTop++] = pResult;      // inlined stackPush (store + advance)
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushStringSetMember @0x82B05828 -- build an AptString from the
// inline dictionary string, push it, then run the SetMember opcode (object[name] = v).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushStringSetMember(AptActionInterpreter* pInterp,
                                                                 LocalContextT* pCtx)
{
    const unsigned char* pAligned =
        reinterpret_cast<const unsigned char*>(
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
    const char* szName = *reinterpret_cast<const char* const*>(pAligned);
    pCtx->mpProgramCounter = pAligned + 4;

    AptString* pStr = AptString::Create("");                // FLAG: seed const @0x820046A7 ("")
    *pStr->GetInternalString() = EAStringC(szName);

    pInterp->mpStack[pInterp->mnStackTop++] = pStr;         // inlined stackPush (store + advance)
    pStr->AddRef();

    _FunctionAptActionSetMember(pInterp, pCtx);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushStringSetVar @0x82B05788 -- build an AptString from the
// inline dictionary string, push it, then run the SetVariable opcode (name = value).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushStringSetVar(AptActionInterpreter* pInterp,
                                                              LocalContextT* pCtx)
{
    const unsigned char* pAligned =
        reinterpret_cast<const unsigned char*>(
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
    const char* szName = *reinterpret_cast<const char* const*>(pAligned);
    pCtx->mpProgramCounter = pAligned + 4;

    AptString* pStr = AptString::Create("");                // FLAG: seed const @0x820046A7 ("")
    *pStr->GetInternalString() = EAStringC(szName);

    pInterp->mpStack[pInterp->mnStackTop++] = pStr;         // inlined stackPush (store + advance)
    pStr->AddRef();

    _FunctionAptActionSetVariable(pInterp, pCtx);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionStringDictByteGetMember @0x82B05970 -- push the dictionary
// string the inline byte indexes (the +0x44 string table), then run GetMember.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStringDictByteGetMember(AptActionInterpreter* pInterp,
                                                                     LocalContextT* pCtx)
{
    const unsigned int nIndex = *pCtx->mpProgramCounter;    // byte dictionary index
    pCtx->mpProgramCounter += 1;

    AptValue* pEntry = pInterp->mpRegisters[nIndex];        // console: *(4*idx + a1[17])
    pInterp->mpStack[pInterp->mnStackTop++] = pEntry;       // inlined stackPush (store + advance)
    pEntry->AddRef();

    _FunctionAptActionGetMember(pInterp, pCtx);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionStringDictByteGetVar @0x82B058C8 -- resolve the dictionary
// string the inline byte indexes as a variable (getVariable) and push the result.
// The console takes the dictionary entry's embedded EAStringC name directly: a raw
// StringValue (type 1) exposes its own string at +8; any other (boxed StringObject
// tag 33) indirects through +0x20 first -- i.e. AptValue::Get_ToString.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStringDictByteGetVar(AptActionInterpreter* pInterp,
                                                                  LocalContextT* pCtx)
{
    const unsigned int nIndex = *pCtx->mpProgramCounter;    // byte dictionary index
    pCtx->mpProgramCounter += 1;

    AptValue* pEntry = pInterp->mpRegisters[nIndex];        // console: *(4*idx + a1[17])

    // The asm inlines Get_ToString: type-1 StringValue -> its own EAStringC (+8);
    // else (boxed tag 33) -> the AptString at +0x20, then its EAStringC. Reuse the
    // recovered Get_ToString (a string-typed value never touches the scratch).
    EAStringC scratch;
    const EAStringC* pName = AptValue::Get_ToString(pEntry, &scratch);

    AptValue* pResult = pInterp->getVariable(pCtx->mpCIH, pCtx->mpPendingReleaseValue,
                                             pName, 1, 1, 0);

    pInterp->mpStack[pInterp->mnStackTop++] = pResult;      // inlined stackPush (store + advance)
    pResult->AddRef();
}

// ===========================================================================
// Lifecycle + call / timeline opcodes that this class home owns (the leaves of
// the ActionScript function-call and timeline-control families).
//   DECOMPILED from the X360 ARTIST:
//     initialize     @0x82AE39D8 -- allocate the five operand/call stacks
//     CallFunction   @0x82B03F78 (0x3D) -- f(args)
//     NewMethod      @0x82B091B8 (0x53) -- new obj.method(args)
//     CallFrame      @0x82B052A0 (0x9E) -- call the frame named by the top operand
//     GotoFrame2     @0x82B0C688 (0x9F) -- jump to a frame/label off the stack
//     GotoLabel      @0x82B0C598 (0x8C) -- jump to the inline frame label
//
// The console addresses the operand stack by 32-bit byte math (4*top+base); here
// it is typed element indexing (mpStack[mnStackTop-1]), identical on x64 where
// AptValue* is 8 bytes. mpStack[mnStackTop-N] is the console's *(4*(top-N)+base).
//
// FLAG -- deferred-subsystem shims (matching the sibling SpecialOps/Variable TUs;
// the deep node->timeline/native-hash chains are not yet reconstructed as named
// members, so they are encapsulated as flagged externs rather than raw console
// offset arithmetic):
//   * AptInterp_ResolveTargetContext (console sub_82B02F80) -- parse the path
//     context out of a name into (out context-value, out leaf-name) under
//     (scope, target). The same path-context resolver getObject uses.
//   * AptInterp_LabelToFrame (console: AptNativeHash::Lookup of the node's frame-
//     label hash, then AptValue::toInteger) -- the frame index for a label, or -1.
//   * AptCIH_jumpToFrame / AptCIH_SetDirtyState -- the play-head seek + dirty
//     latch (un-homed AptCIH play-head subsystem; shared with SpecialOps).
//   * AptMovie_runFrameActions -- run a frame's queued ActionScript (the timeline
//     VM driver; the AptMovie follow-on).
//   * AptApt_PopValues (console Burnout_X360_Artist_01e3_0) -- pop N operands,
//     releasing each (the stack-collapse primitive the compiler inlined).
//   * AptInitParmsT -- the runtime init-parameters block initialize() reads
//     (iStackSize @0x20, iCallStackDepth @0x24, byte @0x40); runtime-only, not
//     serialised, so it is modelled as a flagged local struct.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

// FLAG (console sub_82B02F80 -- path-context resolver; the same one getObject's
// AptActionInterpreter_ParsePathContext wraps, here in its raw asm arg shape):
// resolve pName into (*ppOutContext, *pOutLeaf) under (pScope, pTarget).
extern void AptInterp_ResolveTargetContext(AptValue* pScope, AptValue* pTarget,
                                           const EAStringC* pName,
                                           AptValue** ppOutContext, EAStringC* pOutLeaf);

// FLAG (console: AptNativeHash::Lookup(node's frame-label hash, label) then
// AptValue::toInteger): the frame index a label resolves to under pNode's timeline,
// or -1 when absent. The node->char-inst->AptMovie->labelHash chain is not yet a
// named-member path, so the lookup is encapsulated (AptMovie::labelToFrame is its
// VM-free sibling).
extern int AptInterp_LabelToFrame(AptCIH* pNode, const EAStringC* pLabel);

// FLAG (un-homed AptCIH play-head subsystem -- shared shims, matching SpecialOps):
extern void AptCIH_jumpToFrame(AptCIH* pNode, int nFrame);              // @0x82B0C... seek
extern void AptCIH_SetDirtyState(AptCIH* pNode, bool bDirty, bool bProp); // @0x82AD76B8

// FLAG (the AptMovie timeline VM driver -- run a frame's queued ActionScript;
// AptMovie::runFrameActions follow-on): drive the bound clip's frame actions.
extern void AptMovie_runFrameActions(void* pFrameActionList);

// FLAG (console Burnout_X360_Artist_01e3_0 -- the inlined stack-collapse primitive):
// pop nCount operands off the operand stack, Releasing each.
extern void AptApt_PopValues(AptActionInterpreter* pInterp, int nCount);

// FLAG (runtime-only AptActionInterpreter init parameters -- the block initialize()
// reads; not serialised, so it is modelled by its console field offsets):
struct AptInitParmsT
{
    uint8_t  mPad00[0x20];
    int32_t  iStackSize;        // [c:0x20] the operand-stack capacity
    int32_t  iCallStackDepth;   // [c:0x24] the four call-depth stacks' capacity
    uint8_t  mPad28[0x40 - 0x28];
    uint8_t  mbSkipTraceBytecodes; // [c:0x40] -> mbSkipTraceBytecodes
};

// FLAG (AptScriptFunctionBase::InitializeStaticData @0x82AE26C0 -- the console
// passes the whole AptInitParmsT; the in-header member declaration takes the
// resolved register count, so the parms-shaped entry point is reached through a
// flagged extern to keep initialize() faithful without touching that header).
extern void AptScriptFunctionBase_InitializeStaticData(const AptInitParmsT* pParms);

// FLAG (console: a FrameStack-typed (tag 14) function name owning >=1 local resolves
// to its first slot's captured value -- *Variable[8] when Variable[10] (local count)
// > 0; the AptFrameStack slot array is not yet a named member, so it is encapsulated).
extern AptValue* AptInterp_FrameStackFirstLocal(AptValue* pFrameStack);

// FLAG (AptActionInterpreter::_createObject @0x82B08088 -- the value-materialiser;
// homed in a sibling TU, reached through the same free shim the ProtoOps handlers
// use). Builds the AS object/array/class value under (pScope, pTarget); the trailing
// int/char are the array-length hint + the "construct" flag.
extern AptValue* AptActionInterpreter_createObject(AptActionInterpreter* pInterp,
                                                   AptValue* pScope, AptValue* pTarget,
                                                   const EAStringC* pClassName,
                                                   int nArrayLenHint, char bConstruct);

// ---------------------------------------------------------------------------
// initialize @0x82AE39D8 -- allocate the interpreter's five {count,capacity,array}
// stacks from the operand-stack pool: the operand stack (#1) sized by iStackSize,
// the four call-depth stacks (#2..#5) by iCallStackDepth. The per-run bookkeeping
// (abort slot, stack base, trace-skip flag) is reset, then the AS register window /
// frame machinery is brought up (InitializeStaticData). The console allocates
// `4 * capacity` bytes; x64-native that is `sizeof(AptValue*) * capacity`.
// ---------------------------------------------------------------------------
void AptActionInterpreter::initialize(const AptInitParmsT* pParms)
{
    mnStackCapacity = pParms->iStackSize;
    mpStack = static_cast<AptValue**>(
        gpAptOperandStackPool->Allocate(sizeof(AptValue*) * pParms->iStackSize));

    mnCallStackB_Capacity = pParms->iCallStackDepth;
    mpCallStackB = static_cast<void**>(
        gpAptOperandStackPool->Allocate(sizeof(AptValue*) * pParms->iCallStackDepth));

    mnCallStackC_Capacity = pParms->iCallStackDepth;
    mpCallStackC = static_cast<void**>(
        gpAptOperandStackPool->Allocate(sizeof(AptValue*) * pParms->iCallStackDepth));

    mnCIHStackCapacity = pParms->iCallStackDepth;
    mpCIHStack = static_cast<AptCIH**>(
        gpAptOperandStackPool->Allocate(sizeof(AptValue*) * pParms->iCallStackDepth));

    mnCallStackE_Capacity = pParms->iCallStackDepth;
    mpCallStackE = static_cast<void**>(
        gpAptOperandStackPool->Allocate(sizeof(AptValue*) * pParms->iCallStackDepth));

    field_69 = 0;                                   // console *(a1+105) = 0
    mnStackBase = 0;                                // console *(a1+100) = 0
    mbSkipTraceBytecodes = pParms->mbSkipTraceBytecodes;  // console *(a1+104) = *(a2+64)

    AptScriptFunctionBase_InitializeStaticData(pParms);   // FLAG: parms-shaped entry
}

// ---------------------------------------------------------------------------
// CallFunction @0x82B03F78 (0x3D) -- AS f(args): the top operand is the function
// name/value, the operand under it (top-2) the argument count. When the name is a
// string value the path context is resolved (ResolveTargetContext -> getVariable)
// to the live function; otherwise the name value is used directly. The name +
// count operands are popped (Burnout_X360_Artist_01e3_0(this, 2)) and callFunction
// runs the body with the resolved scope (the resolved context, else the run scope).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionCallFunction(AptActionInterpreter* pInterp,
                                                          LocalContextT* pContext)
{
    AptValue* const pCountValue = pInterp->mpStack[pInterp->mnStackTop - 2];
    AptValue*       pFunction   = pInterp->mpStack[pInterp->mnStackTop - 1];

    EAStringC scratch;                                      // console v19 (&unk_82F72FF8 seed)
    const int nArgs = pCountValue->toInteger();
    AptValue* pResolvedScope = nullptr;                     // console v10

    // A FrameStack-typed name (tag 14) that owns at least one local resolves to its
    // first slot's value (the captured function); else `undefined`.
    if (pFunction->getVtblIndex() == AptVFT_Array && pFunction->getIsDefined())
    {
        // console: Variable[10] (local count) > 0 && *Variable[8] (first slot) != 0
        AptValue* const pFirst = AptInterp_FrameStackFirstLocal(pFunction);  // FLAG: frame-stack slot
        pFunction = pFirst ? pFirst : gpUndefinedValue;
    }

    // A string name (raw type 1 / boxed 33) is resolved through the path context.
    // Get_ToString inlines the console's "raw type-1 -> +8, boxed 33 -> +0x20 then
    // +8" name extraction (the same idiom the sibling dict opcodes use).
    const AptVirtualFunctionTable_Indices eType = pFunction->getVtblIndex();
    if ((eType == AptVFT_StringValue || eType == AptVFT_StringObject) && pFunction->getIsDefined())
    {
        const EAStringC* const pName = AptValue::Get_ToString(pFunction, &scratch);

        AptInterp_ResolveTargetContext(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                       pName, &pResolvedScope, &scratch);
        // console getVariable(this, resolvedContext, target, &leafName, 1, 1, 0).
        pFunction = pInterp->getVariable(pResolvedScope, pContext->mpPendingReleaseValue,
                                         &scratch, 1, 1, 0);
    }

    pFunction->AddRef();                                    // console (**Variable)(Variable)
    AptApt_PopValues(pInterp, 2);                           // pop the name + count operands

    AptValue* const pCallScope = pResolvedScope ? pResolvedScope : pContext->mpCIH;
    pInterp->callFunction(pCallScope, pFunction, nArgs, nullptr, nullptr);

    pFunction->Release();                                   // console (*(*Variable+4))(Variable)
}

// ---------------------------------------------------------------------------
// NewMethod @0x82B091B8 (0x53) -- AS new <object>.<method>(args): the top operand
// is the method name (coerced to a string), top-2 the constructor object, top-3 the
// argument count. The name + count are popped, _createObject builds the instance
// (with the construct flag), and the new object (or `undefined`) is pushed.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionNewMethod(AptActionInterpreter* pInterp,
                                                       LocalContextT* pContext)
{
    AptValue* const pNameValue  = pInterp->mpStack[pInterp->mnStackTop - 1];  // console v7
    AptValue* const pCtorObject = pInterp->mpStack[pInterp->mnStackTop - 2];  // console v8
    AptValue* const pCountValue = pInterp->mpStack[pInterp->mnStackTop - 3];  // console v9

    EAStringC scratch;
    const EAStringC* const pName = AptValue::Get_ToString(pNameValue, &scratch);  // console v10
    const int nArgs = pCountValue->toInteger();                                   // console v11

    pInterp->stackPop();                                   // console Pop (name)
    if (pInterp->mnStackTop > 0)
        --pInterp->mnStackTop;                             // console: drop the count slot (no release)
    pInterp->stackPop();                                   // console Pop (the object slot's ref)

    AptValue* const pObject = AptActionInterpreter_createObject(   // FLAG: _createObject (un-homed)
        pInterp, pCtorObject, pContext->mpPendingReleaseValue, pName, nArgs, 1);
    pCtorObject->Release();                                // console (*(*v8+4))(v8)

    if (pObject)
    {
        pInterp->mpStack[pInterp->mnStackTop++] = pObject; // inlined stackPush
        pObject->AddRef();
        pObject->Release();   // console: (**Object)(Object) then (*(*Object+4))(Object) -- net no ref
    }
    else
    {
        pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;
        gpUndefinedValue->AddRef();
    }
}

// ---------------------------------------------------------------------------
// CallFrame @0x82B052A0 (0x9E) -- AS callFrame: the top operand names a frame
// (a string label, resolved through the path context + the node's label hash) or
// gives the frame index directly (an integer). The top operand is popped; on a
// valid frame index the run scope's frame actions for that frame are run.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionCallFrame(AptActionInterpreter* pInterp,
                                                       LocalContextT* pContext)
{
    AptValue* const pTop = pInterp->mpStack[pInterp->mnStackTop - 1];
    int nFrame = -1;                                       // console v4

    const AptVirtualFunctionTable_Indices eType = pTop->getVtblIndex();
    if ((eType == AptVFT_StringValue || eType == AptVFT_StringObject) && pTop->getIsDefined())
    {
        // a string label -> resolve the path context, then the node's label hash.
        // Get_ToString inlines the console's raw/boxed name extraction (v9+8 / *(v5+32)+8).
        EAStringC scratch;                                 // console v14 (&unk_82F72FF8 seed)
        const EAStringC* const pName = AptValue::Get_ToString(pTop, &scratch);

        AptValue* pResolved = nullptr;                     // console v15
        AptInterp_ResolveTargetContext(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                       pName, &pResolved, &scratch);
        // console: AptNativeHash::Lookup(resolved's frame-label hash, &scratch) -> toInteger
        if (pResolved)
            nFrame = AptInterp_LabelToFrame(static_cast<AptCIH*>(pResolved), &scratch);
    }
    else if (eType == AptVFT_Integer && pTop->getIsDefined())
    {
        nFrame = pTop->toInteger();
    }

    pInterp->stackPop();                                   // console AptValue>::Pop

    if (nFrame != -1)
        AptMovie_runFrameActions(pContext->mpCIH->GetCharacterInst());  // FLAG: AptMovie follow-on
}

// ---------------------------------------------------------------------------
// GotoLabel @0x82B0C598 (0x8C) -- jump the bound clip to the frame named by the
// inline string label. The target node is the run's current target slot when it is
// a movie clip / CIHNone, else the run scope. The label is resolved against the
// node's frame-label hash; a valid index seeks the play-head and clears the
// "playing" bit. The inline string is a 4-byte (4-byte-aligned) operand.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGotoLabel(AptActionInterpreter* /*pInterp*/,
                                                       LocalContextT* pContext)
{
    // Align the PC up to 4, read the inline string pointer, advance the PC by 4.
    const unsigned char* const pAligned = reinterpret_cast<const unsigned char*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
    const char* const szLabel = *reinterpret_cast<const char* const*>(pAligned);
    pContext->mpProgramCounter = pAligned + 4;

    EAStringC label(szLabel);   // console EAStringC::InitFromBuffer scratch (RAII Decrease)

    // Pick the target node: the current-target slot when it is a clip / CIHNone,
    // otherwise the run scope.
    AptCIH* pNode = pContext->mpCIH;
    if (pContext->mpPendingReleaseValue)
    {
        const AptVirtualFunctionTable_Indices eTgt =
            pContext->mpPendingReleaseValue->getVtblIndex();
        if ((eTgt == AptVFT_CharacterInstHandle && pContext->mpPendingReleaseValue->getIsDefined())
            || eTgt == AptVFT_CIHNone)
            pNode = static_cast<AptCIH*>(pContext->mpPendingReleaseValue);
    }

    const int nFrame = AptInterp_LabelToFrame(pNode, &label);   // FLAG: node frame-label hash
    if (nFrame >= 0)
    {
        AptCIH_jumpToFrame(pNode, nFrame);                      // FLAG: play-head seek
        // clear the "playing" bit on the node's sprite instance (console *(node+0x20)+0x14 &= ~0x40).
        static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst())->mnClipActionFlags
            &= ~0x40u;
    }
}

// ---------------------------------------------------------------------------
// GotoFrame2 @0x82B0C688 (0x9F) -- the dynamic goto: the top operand gives the
// destination as a string label (resolved through the path context + label hash) or
// an integer frame index. The target node is the run's current target slot (else
// the second context slot, else the run scope) when it is a clip / CIHNone. A valid
// index seeks the play-head; the "playing" bit is then set/cleared from the inline
// play flag (the 4-byte-aligned operand), and a play -> dirty the node.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGotoFrame2(AptActionInterpreter* pInterp,
                                                        LocalContextT* pContext)
{
    // Align the PC up to 4; the inline 4-byte word is the "start playing" flag.
    const uint32_t* const pPlayFlag = reinterpret_cast<const uint32_t*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
    pContext->mpProgramCounter = reinterpret_cast<const unsigned char*>(pPlayFlag + 1);

    AptValue* const pTop = pInterp->mpStack[pInterp->mnStackTop - 1];   // console v8

    // Pick the target node (console v4): the current-target slot (a2[2]=v5) first,
    // else the run scope (a2[1]=mpCIH), each only when it is a clip / CIHNone.
    AptCIH* pNode = nullptr;                               // console v4
    AptValue* const pTarget = pContext->mpPendingReleaseValue;   // console v5 = a2[2]
    if (pTarget)
    {
        const AptVirtualFunctionTable_Indices eTgt = pTarget->getVtblIndex();
        if ((eTgt == AptVFT_CharacterInstHandle && pTarget->getIsDefined()) || eTgt == AptVFT_CIHNone)
            pNode = static_cast<AptCIH*>(pTarget);
    }
    if (!pNode)
    {
        AptCIH* const pScope = pContext->mpCIH;            // console a2[1]
        const AptVirtualFunctionTable_Indices eScope = pScope->getVtblIndex();
        if ((eScope == AptVFT_CharacterInstHandle && pScope->getIsDefined())
            || eScope == AptVFT_CIHNone)
            pNode = pScope;
    }

    // Resolve the destination off the top operand.
    int nFrame = -1;                                       // console v16
    const AptVirtualFunctionTable_Indices eType = pTop->getVtblIndex();
    if ((eType == AptVFT_StringValue || eType == AptVFT_StringObject) && pTop->getIsDefined())
    {
        // Get_ToString inlines the console's raw/boxed name extraction (v8+8 / *(v8+32)+8).
        EAStringC scratch;                                 // console v30 (&unk_82F72FF8 seed)
        const EAStringC* const pName = AptValue::Get_ToString(pTop, &scratch);

        AptValue* pResolved = nullptr;                     // console v31
        AptInterp_ResolveTargetContext(pContext->mpCIH, pTarget, pName,
                                       &pResolved, &scratch);
        if (pResolved)
        {
            const AptVirtualFunctionTable_Indices eRes = pResolved->getVtblIndex();
            // console: a clip / CIHNone resolved node -> look up the label in its hash
            if ((eRes == AptVFT_CharacterInstHandle && pResolved->getIsDefined())
                || eRes == AptVFT_CIHNone)
                nFrame = AptInterp_LabelToFrame(static_cast<AptCIH*>(pResolved), &scratch);
        }
    }
    else if (eType == AptVFT_Integer && pTop->getIsDefined())
    {
        nFrame = pTop->toInteger();
    }

    if (nFrame != -1 && pNode)
    {
        AptCIH_jumpToFrame(pNode, nFrame);                 // FLAG: play-head seek
        const bool bPlay = (*pPlayFlag != 0);              // console: cntlzw test of the inline word
        AptCharacterSpriteInstBase* const pSprite =
            static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst());
        // set/clear bit 6 (0x40, "playing") from the play flag.
        pSprite->mnClipActionFlags = (pSprite->mnClipActionFlags & ~0x40u) | (bPlay ? 0x40u : 0u);
        if (bPlay)
            AptCIH_SetDirtyState(pNode, true, true);       // FLAG: dirty latch
    }

    pInterp->stackPop();   // console AptValue>::Pop(a1) -- drop the destination operand
}
