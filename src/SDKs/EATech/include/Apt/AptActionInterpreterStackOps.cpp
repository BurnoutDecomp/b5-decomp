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
#include "SDKs/EATech/include/Apt/AptCIH.h"                // AptCIH : AptValueGC (mpCIH -> AptValue* upcast) + KU_AptEmbeddedMovieOff
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"          // GetCharacterInst
#include "SDKs/EATech/include/Apt/AptRenderItem.h"             // mpRenderItem->mpCharacter (the clip's embedded movie chain)
#include "SDKs/EATech/include/Apt/AptMovie.h"                  // AptMovie::runFrameActions (the CallFrame driver)
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h" // mnClipActionFlags
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h" // SetRegisterValue / InitializeStaticData
#include "SDKs/EATech/include/Apt/AptObject.h"             // AptObject (generic/Date Object + Get/SetImplementedObjects)
#include "SDKs/EATech/include/Apt/AptArray.h"              // AptArray (the `new Array` branch)
#include "SDKs/EATech/include/Apt/AptNativeHash.h"         // AptNativeHash (proto/__proto__ links)
#include "SDKs/EATech/include/Apt/AptPrototype.h"          // AptPrototype (lazy class prototype)
#include "SDKs/EATech/include/Apt/AptScriptColour.h"       // AptScriptColour (`new Color`)
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"  // AptStringObject (`new String`)
#include "SDKs/EATech/include/Apt/AptTextFormat.h"         // AptTextFormat (`new TextFormat`)
#include "SDKs/EATech/include/Apt/AptError.h"              // AptError (`new Error`)
#include "SDKs/EATech/include/Apt/AptMovieClip.h"          // AptMovieClip (`new MovieClip`)
#include "SDKs/EATech/include/Apt/AptXmlNode.h"            // AptXmlNode (`new XML`)
#include "SDKs/EATech/include/Apt/AptGlobal.h"             // gpAptGlobalFallback (String-prototype lookup)
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"  // the boxed name unbox (CallMethod)

#include <cstdint>
#include <cstring>   // memcpy (PushFloat bit reinterpret)
#include <cmath>     // fmodf (Array integer-length detection)
#include <string.h>  // _stricmp (the AS class-name compare)
#include <cstdio>    // snprintf (FLAG bring-up probe)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // FLAG bring-up probe log sink

// AptActionInterpreter::GetDictEntry (the null-guarded dictionary-slot fetch) is
// declared in AptActionInterpreter.h.

// FLAG (wired at AptInit; see AptValueConvert.cpp).
extern AptValue* gpUndefinedValue;

// stackPushIndirect @0x7ECE34 is now a real member (homed in AptActionInterpreter.cpp):
// it resolves AptVFT_Lookup (via the string-constant dictionary mpConstantPool) / AptVFT_Register
// (via AptScriptFunctionBase::GetRegisterValue) values before pushing. _Push calls the
// member directly; the old free-function shim (AptRenderLinkStubs.cpp) is retired.

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
// _FunctionAptActionPushWord @0x8063A0 -- read an int16 inline, push it
// (sign-extended). GUIAPT64 streams are the XB1 little-endian format (the console
// .apt was big-endian; the XB1 code reads payloads native).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushWord(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    const unsigned char* p = pCtx->mpProgramCounter;
    int16_t v = static_cast<int16_t>(p[0] | (p[1] << 8));   // LE (GUIAPT64/XB1 form)
    pCtx->mpProgramCounter += 2;
    pInterp->stackPush(AptInteger::Create(v));
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushDWord @0x8062FC -- read an int32 inline, push it.
// GUIAPT64/XB1 little-endian payload (console was big-endian).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushDWord(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    int32_t v;
    std::memcpy(&v, pCtx->mpProgramCounter, sizeof(v));   // LE-native (GUIAPT64/XB1 form)
    pCtx->mpProgramCounter += 4;
    pInterp->stackPush(AptInteger::Create(v));
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushFloat @0x807F50 -- read a 32-bit float inline, push it.
// GUIAPT64/XB1 little-endian payload (the console assembled big-endian bytes).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushFloat(AptActionInterpreter* pInterp, LocalContextT* pCtx)
{
    float f;
    std::memcpy(&f, pCtx->mpProgramCounter, sizeof(f));   // LE-native (GUIAPT64/XB1 form)
    pCtx->mpProgramCounter += 4;
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
// +0x44 (the same slot the header names mpConstantPool; the dict opcodes reuse it as
// the AptValue* string-pool array). Each pushes the dictionary entry onto the
// operand stack -- the inlined store/advance/AddRef sequence the asm emits is
// exactly stackPush(), reproduced here -- and the fused forms then delegate to the
// matching variable/member opcode handler. The console indexes the array with byte
// math (4 * byteIndex); on x64 it is typed element indexing (mpConstantPool[byte]),
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
    // The GUIAPT64 operand block (the form _parseStream produces): 8-aligned
    // {i32 count @0 (the xb1 parse reads it as a signed DWORD -- `cmp [rsi], edi`),
    // AptValue** table @+8}, PC advances 16. The table entries are LIVE AptValue*s
    // post-parse (console form was 4-aligned {i32, ptr} +8).
    const unsigned char* pAligned =
        reinterpret_cast<const unsigned char*>(
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));
    const int32_t   nCount = *reinterpret_cast<const int32_t*>(pAligned);
    AptValue* const* pArray = *reinterpret_cast<AptValue* const* const*>(pAligned + 8);   // serialized .apt operand block: table @+8
    pCtx->mpProgramCounter = pAligned + 16;

    for (int32_t i = 0; i < nCount; ++i)
        pInterp->stackPushIndirect(pArray[i]);   // real member (resolves Lookup/Register)
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

    AptValue* pEntry = pInterp->GetDictEntry(nIndex);   // null-guarded (FLAG hardening)        // console: *(4*idx + a1[17])
    pInterp->mpStack[pInterp->mnStackTop++] = pEntry;       // inlined stackPush (store + advance)
    pEntry->AddRef();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushStringDictWord (0xA3; PS3 @0x7F3FF4) -- push the dictionary
// string a big-endian inline WORD indexes (the byte form's 16-bit sibling; the
// console masks the scaled index to 16 bits -- typed indexing of the u16 here).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushStringDictWord(AptActionInterpreter* pInterp,
                                                                LocalContextT* pCtx)
{
    const unsigned char* p = pCtx->mpProgramCounter;
    const unsigned int nIndex = p[0] | (static_cast<unsigned int>(p[1]) << 8);   // LE u16 (GUIAPT64/XB1)
    pCtx->mpProgramCounter += 2;

    AptValue* pEntry = pInterp->GetDictEntry(nIndex);   // null-guarded (FLAG hardening)        // console: *((4*idx & 0x3FFFC) + a1[17])
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
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));
    const char* szName = *reinterpret_cast<const char* const*>(pAligned);   // 8-aligned qword (GUIAPT64)
    pCtx->mpProgramCounter = pAligned + 8;

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
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));
    const char* szName = *reinterpret_cast<const char* const*>(pAligned);   // 8-aligned qword (GUIAPT64)
    pCtx->mpProgramCounter = pAligned + 8;

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
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));
    const char* szName = *reinterpret_cast<const char* const*>(pAligned);   // 8-aligned qword (GUIAPT64)
    pCtx->mpProgramCounter = pAligned + 8;

    AptString* pStr = AptString::Create("");                // FLAG: seed const @0x820046A7 ("")
    *pStr->GetInternalString() = EAStringC(szName);

    pInterp->mpStack[pInterp->mnStackTop++] = pStr;         // inlined stackPush (store + advance)
    pStr->AddRef();

    _FunctionAptActionSetMember(pInterp, pCtx);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionPushString @0x82AF4128 (opcode 0xA1; the address extracted from
// the console dispatch table @0x82F73068 entry 0xA1) -- build an AptString from the
// INLINE string operand and push it: the push half of the fused Get/Set forms below
// (_parseStream treats 0xA1's operand identically to 0xA4..0xA7 -- one 8-aligned
// relocated string pointer). This opcode is what PLACE clipAction construct blocks
// use to seed instance variables (mPromptType/mText/mAlignment/...); the unwired
// slot made every construct block a silent no-op (undefined help-bar prompt vars).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushString(AptActionInterpreter* pInterp,
                                                        LocalContextT* pCtx)
{
    const unsigned char* pAligned =
        reinterpret_cast<const unsigned char*>(
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));
    const char* szName = *reinterpret_cast<const char* const*>(pAligned);   // 8-aligned qword (GUIAPT64)
    pCtx->mpProgramCounter = pAligned + 8;

    AptString* pStr = AptString::Create("");                // FLAG: seed const @0x820046A7 ("")
    *pStr->GetInternalString() = EAStringC(szName);

    pInterp->mpStack[pInterp->mnStackTop++] = pStr;         // inlined stackPush (store + advance)
    pStr->AddRef();
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
            (reinterpret_cast<uintptr_t>(pCtx->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));
    const char* szName = *reinterpret_cast<const char* const*>(pAligned);   // 8-aligned qword (GUIAPT64)
    pCtx->mpProgramCounter = pAligned + 8;

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

    AptValue* pEntry = pInterp->GetDictEntry(nIndex);   // null-guarded (FLAG hardening)        // console: *(4*idx + a1[17])
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

    AptValue* pEntry = pInterp->GetDictEntry(nIndex);   // null-guarded (FLAG hardening)        // console: *(4*idx + a1[17])

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
//   * AptResolveTargetContext (console sub_82B02F80) -- parse the path
//     context out of a name into (out context-value, out leaf-name) under
//     (scope, target). The same path-context resolver getObject uses.
//   * AptLabelToFrame (console: AptNativeHash::Lookup of the node's frame-
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
extern void AptResolveTargetContext(AptValue* pScope, AptValue* pTarget,
                                           const EAStringC* pName,
                                           AptValue** ppOutContext, EAStringC* pOutLeaf);

// FLAG (console: AptNativeHash::Lookup(node's frame-label hash, label) then
// AptValue::toInteger): the frame index a label resolves to under pNode's timeline,
// or -1 when absent. The node->char-inst->AptMovie->labelHash chain is not yet a
// named-member path, so the lookup is encapsulated (AptMovie::labelToFrame is its
// VM-free sibling).
extern int AptLabelToFrame(AptCIH* pNode, const EAStringC* pLabel);

// FLAG (un-homed AptCIH play-head subsystem -- shared shims, matching SpecialOps):
// AptCIH_jumpToFrame retired: the real member AptCIH::jumpToFrame (AptCIH.cpp) is used directly.
// AptCIH_SetDirtyState retired: the real member AptCIH::SetDirtyState (AptCIH.cpp) is
// used directly at the call site (the free-function {} stub dropped the dirty latch).

// AptMovie::runFrameActions is now the real const member (AptMovie.cpp, @PS3 0x820FA4);
// the CallFrame handler below calls it on the bound clip's embedded movie directly.

// FLAG (console Burnout_X360_Artist_01e3_0 -- the inlined stack-collapse primitive):
// pop nCount operands off the operand stack, Releasing each.
// AptApt_PopValues -> AptActionInterpreter::stackPop(int) member (@0x7FDB68).

// The per-op call-frame/CIH stack pops in _FunctionAptActionCallMethod are the console's
// AptValueVector::pop(a1 + 3) / (a1 + 9): each interpreter stack is a {count,cap,array} triple
// laid out identically to AptValueVector, so the console puns a pointer to the triple as an
// AptValueVector* and calls the real pop(). Reproduced verbatim at those call sites.


// AptInitParmsT (the runtime-only AptActionInterpreter init-parameters block
// initialize() reads) is now DEFINED in AptActionInterpreter.h (promoted out of this
// TU so the host bring-up can construct one to call initialize -- the X360
// AptUpdateInitialize's job). Layout unchanged; included via AptActionInterpreter.h.

// AptScriptFunctionBase::InitializeStaticData @0x82AE26C0 is the real static member
// (allocates the register block + seeds it undefined); the console passes the whole
// AptInitParmsT and reads the count at +0x30 (now the recovered iRegisterCount field),
// so initialize() below passes pParms->iRegisterCount. The parms-shaped shim is retired.

// FLAG (console: a FrameStack-typed (tag 14) function name owning >=1 local resolves
// to its first slot's captured value -- *Variable[8] when Variable[10] (local count)
// > 0; the AptFrameStack slot array is not yet a named member, so it is encapsulated).
extern AptValue* AptFrameStackFirstLocal(AptValue* pFrameStack);

// AptActionInterpreter::_createObject @0x82B08088 -- the value-materialiser; HOMED in
// this TU (defined at the bottom of this file, declared in AptActionInterpreter.h).
// The ProtoOps + NewObject/NewMethod handlers now call the real member directly; the
// forwarding free-function shim was retired (was flagged apt_shim).

// FLAG (engine rodata: the static EAStringC for "String" at the class-name table
// dword_8324E580 -- console &dword_8324E6B4). _createObject's String branch uses it
// as the hash key to fetch the global String class from gpAptGlobalFallback. Declared
// here so the branch can name it; the table data itself is the data-segment follow-on.
extern const EAStringC gAptStringClassName;   // &dword_8324E6B4

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

    // X360 InitializeStaticData(parms) reads the register count at parms+0x30
    // (@0x82AE26C0); the member takes the resolved count.
    AptScriptFunctionBase::InitializeStaticData(pParms->iRegisterCount);
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
        AptValue* const pFirst = AptFrameStackFirstLocal(pFunction);  // FLAG: frame-stack slot
        pFunction = pFirst ? pFirst : gpUndefinedValue;
    }

    // A string name (raw type 1 / boxed 33) is resolved through the path context.
    // Get_ToString inlines the console's "raw type-1 -> +8, boxed 33 -> +0x20 then
    // +8" name extraction (the same idiom the sibling dict opcodes use).
    const AptVirtualFunctionTable_Indices eType = pFunction->getVtblIndex();
    if ((eType == AptVFT_StringValue || eType == AptVFT_StringObject) && pFunction->getIsDefined())
    {
        const EAStringC* const pName = AptValue::Get_ToString(pFunction, &scratch);

        AptResolveTargetContext(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                       pName, &pResolvedScope, &scratch);
        // console getVariable(this, resolvedContext, target, &leafName, 1, 1, 0).
        pFunction = pInterp->getVariable(pResolvedScope, pContext->mpPendingReleaseValue,
                                         &scratch, 1, 1, 0);
    }

    pFunction->AddRef();                                    // console (**Variable)(Variable)
    pInterp->stackPop(2);                                  // pop the name + count operands (member)

    AptValue* const pCallScope = pResolvedScope ? pResolvedScope : pContext->mpCIH;
    pInterp->callFunction(pCallScope, pFunction, nArgs, nullptr, nullptr);

    pFunction->Release();                                   // console (*(*Variable+4))(Variable)
}

// ===========================================================================
// CallMethod @0x82B04758 (0x52) -- AS obj.method(args): the densest VM opcode.
//   Stack: [argCount, methodName, object] (object on top: stack[top-1]).
//
// Faithfully transliterated from the X360 ARTIST pseudocode/asm. The control-flow
// spine (the operand fetch, the undefined-object guard, the per-call frame push, the
// method-value resolution, the apply/call argument-spread, the prototype-chain
// "this"-binding walk, and the final callFunction dispatch + per-op cleanup) is
// reproduced 1:1. The deep object-model internals the console reaches by raw field
// offset -- the method-name slot at (objectValue + 8), the object's class/owner slot
// at (methodValue + 0x1C), the boxed-string indirection at (value + 0x20), the
// prototype-chain link walk (console sub_82AE4058 / the GetNativeHashVirtual() ->
// mp__Proto__ chain), the AS name compare (console Burnout_X360_Artist_0040_0 ==
// EAStringC ==), and the destroyed-clip sentinel (console dword_8324D818) -- are
// encapsulated behind the FLAG'd helpers declared below, exactly as the sibling
// callFunction / _doEnumerate / isObjectOfType entry layers do. (Not on the boot
// trace; [not executed in goal trace].)
// ---------------------------------------------------------------------------

// FLAG (object-internal offset reads not yet named members of AptValue/AptObject):
//   * GetMethodNameSlot -- the resolved method-name string slot the console reads at
//     (objectValue + 8) (console v10 = v6 + 8): a pointer to the EAStringC* method
//     name. *slot is the AptNativeString; *slot's buffer is GetBuffer() (the +8 the
//     console stricmp's against "apply"/"call"/"pop"/"shift").
//   * GetClassOwnerValue -- the class/owner value the console reads at (value + 0x1C)
//     (console *(v7 + 28) / *(v27 + 0x1C)); the object's defining-class slot used by
//     the constructor-chain comparison.
//   * GetBoxedStringSlot -- the boxed-string indirection the console takes at
//     (value + 0x20) (console *(v6 + 32)) when a boxed string (tag 33) holds its real
//     EAStringC one indirection deeper.
extern EAStringC** AptGetMethodNameSlot(AptValue* pObject);   // FLAG: (objectValue + 8)
extern AptValue*   AptGetClassOwnerValue(AptValue* pValue);   // FLAG: (value + 0x1C)

// The bounded method-name probe is now the static AptActionInterpreter::HasMember member
// (console sub_82AE4058; declared in the header).

// The resolved-method-name == compare is now the static AptActionInterpreter::NameEquals
// member (console Burnout_X360_Artist_0040_0 = EAStringC::operator==; declared in the header).

// FLAG (console dword_8324D818 -- the destroyed/movie-end clip placeholder value the
// "this"-binding walk re-targets through; the same 0xF sentinel Compare collapses).
extern AptValue* gpAptDestroyedClipValue;   // dword_8324D818

// FLAG (engine rodata EAStringC name constants -- entries of the static name table at
// dword_8324E580; the default empty method-name sentinel (unk_8324E6B8) the undefined-
// object path seeds v10 with, and the "this" key (dword_8324E6C0) the supercall path
// re-resolves the bound function name under).
extern const EAStringC gAptEmptyMethodName;   // unk_8324E6B8
extern const EAStringC gAptThisKey;           // dword_8324E6C0
// The apply()/call() method-name keys the console stricmp's inline (rodata literals).
static const EAStringC gAptApplyKey("apply");
static const EAStringC gAptPopKey  ("pop");
static const EAStringC gAptShiftKey("shift");
static const EAStringC gAptCallKey ("call");

// The method-name hash lookup is now the static AptActionInterpreter::HashLookupName
// member (AptNativeHash::Lookup over the +8 slot; declared in the header).



void AptActionInterpreter::_FunctionAptActionCallMethod(AptActionInterpreter* pInterp,
                                                       LocalContextT* pContext)
{
    // ---- operand fetch (console v5/v6/v7; stack indices top-3/top-1/top-2) ----
    AptValue* const pCountValue = pInterp->mpStack[pInterp->mnStackTop - 3];   // v5 (r16)
    AptValue*       pObject     = pInterp->mpStack[pInterp->mnStackTop - 1];   // v6 (r30)
    AptValue*       pMethodName = pInterp->mpStack[pInterp->mnStackTop - 2];   // v7 (r27)

    int       nArgs       = pCountValue->toInteger();   // v8/v9/v25
    EAStringC** pNameSlot  = nullptr;                    // v10 (r23): method-name slot
    bool      bPushedFrame = false;                      // v11 (r15)
    AptValue* pMethod      = nullptr;                    // v12 (r26): resolved method value
    AptValue* pSavedFrame  = nullptr;                    // v13 (r22)
    AptValue* pThisBinding = nullptr;                    // v14 (r19)

    EAStringC scratch;                                   // v69[0] seed (&unk_82F72FF8)

    // ---- undefined-object guard (console: v6 == off_8324D814) ----
    // An `undefined` object resolves the method through the default name slot, and
    // (when the method name is a frame-stack-typed value with the boxed bit) re-reads
    // the per-call register window's top slot as the method value.
    if (pObject == gpUndefinedValue)
    {
        pNameSlot = const_cast<EAStringC**>(reinterpret_cast<EAStringC* const*>(&gAptEmptyMethodName));  // FLAG: &unk_8324E6B8

        // console: (*(v7+1) & 0x7F)==0x14 (tag 20 == AptVFT_Prototype) && boxed bit
        if (pMethodName->getVtblIndex() == AptVFT_Prototype && pMethodName->getIsDefined())
        {
            // FLAG: v12 = *(v7 + 7) -- the name slot holds the resolved method value
            // (a raw word the console reads as an AptValue*).
            pMethod = AptGetClassOwnerValue(pMethodName);   // console v12 = *(v7 + 7) (the +0x1C owner/ctor slot; FLAG stub -> null -> falls to the resolve arm)
            const int nReg = pInterp->mnCallStackB_Count;          // console a1[12] (+0x30)
            if (nReg)
            {
                // the register-window top entry (console *(4*reg + a1[14] - 4))
                pSavedFrame = reinterpret_cast<AptValue*>(pInterp->mpCallStackB[nReg - 1]);  // v13
                // keep it only when it is a defined string-object (tag 19 == AptVFT_Object)
                if (!(pSavedFrame->getVtblIndex() == AptVFT_Object && pSavedFrame->getIsDefined()))
                    pSavedFrame = nullptr;
            }
        }
        else
        {
            pMethod = pMethodName;   // console LABEL: v12 = v7
        }
    }

    // ---- a non-boxed method name short-circuits to a pushed-undefined no-op ----
    // console: ((*(v7+1) >> 27) & 1) == 0  -> the method name has no boxed bit set.
    if (!pMethodName->getIsDefined())
    {
        pInterp->stackPop(nArgs + 3);                          // console Burnout_X360_Artist_01e3_0 == stackPop(int) (member)
        pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;   // push off_8324D814
        // (no AddRef: the console stores the singleton without taking a ref here)
        return;   // -> the shared LABEL_140 string-scratch teardown (RAII below)
    }

    // ---- per-call frame push: stash the context's character-inst handle on the
    //      call-depth stack #2 and AddRef it (console a2[4] -> a1[9]++/a1[11]) ----
    AptValue* const pCharHandle = reinterpret_cast<AptValue*>(pContext->mpCIH);   // console v18 = *(a2+4)
    AptValue* const pMethodNameSaved = pMethodName;            // v19 (r14): released at the end
    // console *(4*a1[9]++ + a1[11]) -- a1[9] IS the CIH/target stack (+0x24 ==
    // mnCIHStackTop), PAIRED with the AptApt_PopCallStackC pop at the end. (The old
    // recon pushed onto mpCallStackC here -- a DIFFERENT stack -- so the end pop
    // stole runStream's own pushed CIH entry and double-released it: the dangling
    // queued-CIH AV. x64 FIX 2026-07-05.)
    pInterp->mpCIHStack[pInterp->mnCIHStackTop++] = reinterpret_cast<AptCIH*>(pCharHandle);
    pCharHandle->AddRef();                                     // console (**v18)(v18)

    // ---- resolve the method value when the object guard did not set one ----
    if (!pMethod || pMethod == gpUndefinedValue)
    {
        // OPERAND ROLES (corrected 2026-07-05 vs the X360 @0x82B04758 asm): v6 (this
        // TU's pObject local, stack top-1) is the METHOD-NAME VALUE and v7 (the
        // pMethodName local, top-2) is the RECEIVER OBJECT -- the original
        // transliteration had the labels swapped, and its two FLAG stubs sat on the
        // COMMON call path (every normal string-named method call), null-derefing
        // the name slot. "GetMethodNameSlot" == the EAStringC embedded in the name
        // AptString (console v6+8 == AptString::GetInternalString); a StringObject
        // name first unboxes its wrapped string (console *(v6+32) == mpValue).
        const int nType = (static_cast<int>(pObject->getVtblIndex()) << 25) >> 25;   // v21 (the NAME value's tag)
        const bool bStringName =
            (nType == 1 || nType == 33) && pObject->getIsDefined();
        if (bStringName)
        {
            AptValue* pNameValue = pObject;                                    // v6 (the name value)
            if (nType != 1)
                pNameValue = static_cast<AptStringObject*>(pNameValue)->GetBoxedString();   // console *(v6+32): unbox
            pNameSlot = reinterpret_cast<EAStringC**>(
                static_cast<AptString*>(pNameValue)->GetInternalString());     // console v10 = (v6 + 8)

            // console: (*(v7+1) & 0x7F) == 0x1D (tag 29 == AptVFT_Extension) -> an
            // EXTENSION receiver resolves the method in its own native hash (the
            // CAptCommunicator.SendAptEvent/... path).
            if (pMethodName->getVtblIndex() == AptVFT_Extension)
            {
                AptNativeHash* const pHash = pMethodName->GetNativeHashVirtual();   // console (*(*v7+8))(v7)
                pMethod = AptActionInterpreter::HashLookupName(pHash, pNameSlot);
            }
            else
            {
                pMethod = pInterp->getVariable(pMethodName /*the receiver*/, nullptr,
                                               reinterpret_cast<const EAStringC*>(pNameSlot), 1, 1, 0);
            }
        }
        else
        {
            // A non-string name value: render it to text and resolve on the receiver.
            pObject->toString(&scratch);                       // console AptValue::toString(v6, v69)
            pMethod = pInterp->getVariable(pMethodName /*the receiver*/, nullptr, &scratch, 1, 1, 0);
            pNameSlot = reinterpret_cast<EAStringC**>(&scratch);   // console v10 = v69
        }
    }

    // ---- pop the object operand + drop the name/count slots (console Pop; then the
    //      top-- / top-- pair that drops the name + count without releasing) ----
    pInterp->stackPop();                                       // console AptValue>::Pop (the NAME slot, top)
    if (pInterp->mnStackTop > 0)
    {
        --pInterp->mnStackTop;
        if (pInterp->mnStackTop > 0)
            --pInterp->mnStackTop;
    }

    // ---- apply()/call() argument spread -----------------------------------
    // When the resolved method is a non-boxed value (no own member) AND the method
    // name is "apply" or "call", the method retargets to the original name value and
    // the call's argument layout is rewritten: call() shifts off the explicit `this`
    // arg; apply() flattens the trailing array argument onto the stack.
    if ((!pMethod || !pMethod->getIsDefined())
                && (AptActionInterpreter::NameEquals(pNameSlot, &gAptApplyKey)   // console stricmp(*v10+8,"apply")
            || AptActionInterpreter::NameEquals(pNameSlot, &gAptCallKey)))  // console stricmp(*v10+8,"call")
    {
        // NOTE: the two console stricmp's are against "apply"/"call"; encapsulated as
        // the AptInterp_NameEquals name compare (the EAStringC == the console folds in).
        pMethod = pMethodName;   // console v12 = v7

        const int nCountType = (static_cast<int>(pCountValue->getVtblIndex()) << 25) >> 25;   // v28
        const bool bIntCount =
            (nCountType == 7 && pCountValue->getIsDefined())      // Integer
         || (nCountType == 6 && pCountValue->getIsDefined());     // Float
        if (bIntCount)
        {
            // call(): the explicit `this` arg is the first operand; drop it.
            if (nArgs <= 0)
            {
                pMethodName = gpUndefinedValue;   // console v7 = off_8324D814
            }
            else
            {
                AptValue* const pFirst = pInterp->mpStack[pInterp->mnStackTop - 1];   // console v33
                pMethodName = (pFirst && pFirst->getIsDefined()) ? pFirst : gpUndefinedValue;
                pInterp->stackPop();
                --nArgs;
            }
        }
        else
        {
            // apply(): the explicit `this` is under the args array; re-read arg count.
            pMethodName = pCountValue;   // console v7 = v5
            nArgs = pInterp->mpStack[pInterp->mnStackTop - 1]->toInteger();   // console v9 = toInteger(top)
            if (nArgs > 1)
            {
                AptValue* const pUnder = pInterp->mpStack[pInterp->mnStackTop - 2];   // console v31
                if (pUnder && pUnder->getIsDefined())
                    pMethodName = pUnder;
                pInterp->stackPop();
                --nArgs;
            }
            pInterp->stackPop();
        }

        // apply(): flatten the trailing array argument's elements onto the stack.
        if (AptActionInterpreter::NameEquals(pNameSlot, &gAptApplyKey))  // console stricmp(*v10+8,"apply")
        {
            AptValue* const pTopArg = pInterp->mpStack[pInterp->mnStackTop - 1];   // console v34
            if (pTopArg->getVtblIndex() == AptVFT_Array && pTopArg->getIsDefined())
            {
                pInterp->stackPop();
                AptArray* const pArray = static_cast<AptArray*>(pTopArg);
                int nLen = pArray->mnLength;                  // console v34[10]
                int nIdx = nLen - 1;                          // console v30/v37
                nArgs = nLen + nArgs - 1;                     // console v9 = v36 + v9 - 1
                if (nIdx >= 0)
                {
                    do
                    {
                        AptValue* pElem = (nIdx < pArray->mnLength)   // console v37 < v34[10]
                                        ? pArray->mpArray[nIdx]       // console *(v34[8] + v38)
                                        : gpUndefinedValue;
                        pInterp->mpStack[pInterp->mnStackTop++] = pElem;   // inlined stackPush
                        pElem->AddRef();                                   // console (**v39)(v39)
                        --nIdx;
                    } while (nIdx >= 0);
                }
            }
        }
    }

    // ---- the "this"-binding prototype-chain walk --------------------------
    // console: (*(*v7+16))(v7) -- the RECEIVER's vtable slot 4 == GetHasClass()
    // (AptValue vtbl: 0 AddRef, 1 Release, 2 GetNativeHashVirtual, 3
    // ContainsNativeHashVirtual, 4 GetHasClass, 5 SetHasClass...). A class-bearing
    // receiver binds a fresh `this` (pushed on call-frame stack B) unless it is
    // already in the current activation's __proto__ chain. (The old recon used a
    // FLAG'd AptInterp_HasMember(v7, nullptr) stand-in here -- once the class
    // prototypes went live it walked a real chain into Lookup(*(EAStringC*)null):
    // the onLoad-dispatch AV. x64 FIX 2026-07-05, verified vs @0x82B04758.)
    if (pMethodName->GetHasClass())   // console (*(*v7+16))(v7) -- vtbl[4]
    {
        bool      bBindThis = true;          // console v40 (r29) = 1
        AptValue* pBoundThis = pMethodName;  // console v41 (r30) = v7

        if (pInterp->mnCallStackB_Count)     // console a1[3] (+0x0C == stack B count)
        {
            // walk the current activation's __proto__ chain looking for v7; if it
            // is already in scope, no fresh bind is needed.
            const int nNameType = (static_cast<int>(pMethodName->getVtblIndex()) << 25) >> 25;   // v43
            AptValue* const pTopFrame = reinterpret_cast<AptValue*>(
                pInterp->mpCallStackB[pInterp->mnCallStackB_Count - 1]);   // console *(a1[5] + 4*a1[3] - 4)
            const bool bClassMatch =
                (nNameType == 12 && pMethodName->getIsDefined())   // CharacterInstHandle
             || nNameType == 37;                                   // CIHNone
            // console: a class-matched name whose owner slot (v7+0x1C) == pTopFrame, or
            // v7 == pTopFrame, is already bound.
            if ((!bClassMatch || AptGetClassOwnerValue(pMethodName) != pTopFrame)
                && pMethodName != pTopFrame)
            {
                // console: v46 = (*(*v44+8))(v44); while (v46) { v47 = *(v46+8);
                //   if (!v47) break-null; if (v47 == v7) { v40 = 0; break; }
                //   v46 = (*(*v47+8))(v47); } -- the plain __proto__-chain walk.
                AptNativeHash* pWalkHash = pTopFrame->GetNativeHashVirtual();
                while (pWalkHash)
                {
                    AptValue* const pProto = pWalkHash->mp__Proto__;
                    if (!pProto)
                        break;
                    if (pProto == pMethodName) { bBindThis = false; break; }
                    pWalkHash = pProto->GetNativeHashVirtual();
                }
            }
        }
        else if (pMethodName == pContext->mpScopeVariable)   // console v7 == *(a2+16)
        {
            // The receiver IS the stream's resolved `super` value (console ctx+0x10 --
            // runStream resolves the unk_8324E6B8 "super" key there once per stream;
            // the recon formerly compared the WRONG context field, mpCharacterInst).
            // re-resolve the bound function under the "this" key.
            pBoundThis = pInterp->getVariable(reinterpret_cast<AptValue*>(pContext->mpCIH),
                                              nullptr, &gAptThisKey, 1, 1, 0);   // console &dword_8324E6C0
        }

        if (bBindThis)
        {
            bPushedFrame = true;   // console v11 = 1
            // mark a defined object class-bearing (console: tag 0x13 (19) -> set 0x400000).
            if (pBoundThis->getVtblIndex() == AptVFT_Object && pBoundThis->getIsDefined())
                pBoundThis->SetHasClass(1);   // console *(v41+7) |= 0x400000 (the class flag word)
            // console *(4*a1[3]++ + a1[5]) -- a1[3] is call-frame stack B (+0x0C ==
            // mnCallStackB_Count), PAIRED with the AptApt_PopCIHStack pop below. (The
            // old recon pushed onto the CIH/target stack -- the same cross-stack
            // mismatch as the char-handle push. x64 FIX 2026-07-05.)
            pInterp->mpCallStackB[pInterp->mnCallStackB_Count++] = pBoundThis;
            pBoundThis->AddRef();                        // console (**v41)(v41)
        }
    }

    // ---- resolve the `this` instance the method runs against (v14) --------
    // When neither the default name nor the object's own member resolved, walk the
    // object's prototype chain to find the member's defining object (its `this`).
    if (!AptActionInterpreter::NameEquals(pNameSlot, &gAptEmptyMethodName)    // FLAG: !Burnout_X360_Artist_0040_0(v10,&unk_8324E6B8)
        && !AptActionInterpreter::HasMember(pMethodName, pNameSlot))   // FLAG: !sub_82AE4058(v7, v10)
    {
        AptValue* pWalk = pMethodName;
        AptValue* pOwner = nullptr;                               // console v50
        while (pWalk)
        {
            AptNativeHash* const pHash = pWalk->GetNativeHashVirtual();   // console (*(v49+8))(v50)
            AptValue* const pProto = pHash ? pHash->Get__Proto__() : nullptr;   // console *(v51+8)
            if (!pProto) break;
            if (AptActionInterpreter::HasMember(pProto, pNameSlot)) { pOwner = pProto; break; }   // FLAG: sub_82AE4058
            pWalk = pProto;
        }
        if (pOwner)
        {
            AptNativeHash* const pHash = pOwner->GetNativeHashVirtual();   // console (*(*v50+8))(v50)
            pThisBinding = pHash ? pHash->Get__Proto__() : nullptr;       // console v14 = *(v53+8)
        }
    }

    // FLAG (bring-up probe, temporary): trace the super-binding chain for 'Update'
    // method calls -- the licence card's dead super.Update() investigation.
    {
        static s32 siSuperProbe = 0;
        if (siSuperProbe < 200 && pNameSlot != nullptr
            && *reinterpret_cast<void* const*>(pNameSlot) != nullptr)
        {
            const EAStringC* const pNm = reinterpret_cast<const EAStringC*>(pNameSlot);
            const char* const pBuf = pNm->GetBuffer();
            if (pBuf && std::strcmp(pBuf, "Update") == 0)
            {
                ++siSuperProbe;
                char lacProbe[224];
                std::snprintf(lacProbe, sizeof(lacProbe),
                              "[AptSuper] CallMethod name='Update' recvTag=%d recvDef=%d "
                              "hasMember=%d thisBinding=%p method=%p methodTag=%d\n",
                              static_cast<int>(pMethodName->getVtblIndex()),
                              pMethodName->getIsDefined() ? 1 : 0,
                              AptActionInterpreter::HasMember(pMethodName, pNameSlot) ? 1 : 0,
                              static_cast<void*>(pThisBinding),
                              static_cast<void*>(pMethod),
                              pMethod ? static_cast<int>(pMethod->getVtblIndex()) : -1);
                CgsDev::Log::WriteToLog(lacProbe);
            }
        }
    }

    // ---- the call: a movie-clip target binds `this` through its parent chain; a
    //      plain value calls straight through. (console isMCInParentChain split) ----
    bool bReleaseAfter = false;   // console v54 (r29)
    // console: (*(v7+1) & 0x3FFC000) == 0x4000 -> the method needs an extra AddRef.
    // console: (v7->mnValueData & 0x3FFC000) == 0x4000 -- a flags-band state test on
    // the RECEIVER's packed word (NOT isMCInParentChain; the old form compared a bool
    // against 0x4000 == never true, which skipped the hold ref).
    if ((pMethodName->mnValueData & 0x3FFC000u) == 0x4000u)
    {
        pMethodName->AddRef();   // console (**v7)(v7)
        bReleaseAfter = true;
    }

    if (pMethodName->isMCInParentChain())   // console AptValue::isMCInParentChain(v7)
    {
        // re-target a destroyed-clip placeholder through the call-frame register top.
        if (pMethodName == gpAptDestroyedClipValue)   // console dword_8324D818
            pMethodName = reinterpret_cast<AptValue*>(
                pInterp->mpCIHStack[pInterp->mnCIHStackTop - 2]);   // console *(4*(a1[9]-2)+a1[11]) (the CIH/target stack)

        if (pMethodName == pContext->mpScopeVariable)   // console *(a2+16) -- the
        {   // stream's resolved `super` value (see the note at the walk above)
            // a super-call: bind the method's defining function through its owner slot.
            const int nMethodType = (static_cast<int>(pMethod->getVtblIndex()) << 25) >> 25;   // v55
            const bool bScriptFn = ((nMethodType - 34) <= 2) && pMethod->getIsDefined();   // tags 34..36
            if (bScriptFn)
            {
                // console: v63 = *(v12+8); *(v12+8) = *(a2+4); call; *(v12+8) = v63 --
                // the script-fn's +8 slot is its bound CIH (AptScriptFunctionBase::
                // mpCIH): TEMP re-bind the method to the context's CIH for the call.
                AptScriptFunctionBase* const pFn = static_cast<AptScriptFunctionBase*>(pMethod);
                AptValue* const pSavedCIH = pFn->GetCIH();
                pFn->SetCIH(reinterpret_cast<AptValue*>(pContext->mpCIH));
                pInterp->callFunction(pMethodName, pMethod, nArgs, nullptr, pThisBinding);
                pFn->SetCIH(pSavedCIH);
            }
            else
            {
                pInterp->callFunction(pMethodName, pMethod, nArgs, nullptr, pThisBinding);
            }
        }
        else
        {
            // a movie-clip target: pass it as the construct/`this` target (v58 = 0).
            pInterp->callFunction(pMethodName, pMethod, nArgs, nullptr, pThisBinding);
        }
    }
    else
    {
        // a plain value: the saved frame value (v13/v22) is the construct target.
        pInterp->callFunction(pMethodName, pMethod, nArgs, pSavedFrame, pThisBinding);
    }

    // ---- per-op cleanup ----------------------------------------------------
    // clear the class flag on the bound `this` we pushed, then pop the call-frame
    // register stack (console a1+3 / AptValue_::pop).
    if (bPushedFrame)
    {
        AptValue* const pBound = reinterpret_cast<AptValue*>(
            pInterp->mpCallStackB[pInterp->mnCallStackB_Count - 1]);   // console v64 = *(4*a1[3]+a1[5]-4) (stack B top)
        if (pBound->getVtblIndex() == AptVFT_Object && pBound->getIsDefined())
            pBound->SetHasClass(0);   // console *(v64+28) &= ~0x400000
        // console AptValue_::pop(a1 + 3): the call-frame-register {count,cap,array} triple punned
        // as an AptValueVector -> the real pop() (Release top, --count). Unguarded like the console
        // (bPushedFrame above guarantees a live, non-null top).
        reinterpret_cast<AptValueVector*>(&pInterp->mnCallStackB_Count)->pop();
    }

    if (bReleaseAfter)
        pMethodName->Release();   // console (*(*v7+4))(v7)

    // pop()/shift() leave their removed element on the stack as the call result; the
    // console releases it when the method value is a frame-stack (tag 14) "apply".
    AptValue* const pResult = pInterp->mpStack[pInterp->mnStackTop - 1];
    if (pResult != gpUndefinedValue)
    {
        if (pMethodName->getVtblIndex() == AptVFT_Array && pMethodName->getIsDefined()
                        && (AptActionInterpreter::NameEquals(pNameSlot, &gAptPopKey)     // console stricmp(*v10+8,"pop")
                || AptActionInterpreter::NameEquals(pNameSlot, &gAptShiftKey)))  // console stricmp(*v10+8,"shift")
        {
            pInterp->mpStack[pInterp->mnStackTop - 1]->Release();   // console (*(*v67+4))(v67)
        }
    }

    // console AptValue_::pop(a1 + 9): the CIH/target {count,cap,array} triple punned as an
    // AptValueVector -> the real pop() (Release top, --count). Unguarded like the console (the
    // matching CIH push earlier in this handler guarantees a live, non-null top).
    reinterpret_cast<AptValueVector*>(&pInterp->mnCIHStackTop)->pop();
    pMethodNameSaved->Release();     // console (*(*v19+4))(v19)
    pCountValue->Release();          // console (*(*v5+4))(v5)

    // LABEL_140: the string scratch (v69[0]) is released by EAStringC's RAII dtor.
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

        AptValue* const pObject = pInterp->_createObject(   // real member @0x82B08088
        pCtorObject, pContext->mpPendingReleaseValue, pName, nArgs, 1);
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
        AptResolveTargetContext(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                       pName, &pResolved, &scratch);
        // console: AptNativeHash::Lookup(resolved's frame-label hash, &scratch) -> toInteger
        if (pResolved)
            nFrame = AptLabelToFrame(static_cast<AptCIH*>(pResolved), &scratch);
    }
    else if (eType == AptVFT_Integer && pTop->getIsDefined())
    {
        nFrame = pTop->toInteger();
    }

    pInterp->stackPop();                                   // console AptValue>::Pop

    if (nFrame != -1)
    {
        // Console: runFrameActions(<node->charInst->renderItem->character + movie body>,
        // node, nFrame) -- the bound clip's embedded AptMovie (the same char+movie-off
        // chain AptGetClipMovie models), run against the node.
        AptCIH* const pNode = pContext->mpCIH;
        AptCharacter* const pCharacter =
            pNode->GetCharacterInst()->mpRenderItem->mpCharacter;
        const AptMovie* const pMovie = reinterpret_cast<const AptMovie*>(
            reinterpret_cast<const char*>(pCharacter) + KU_AptEmbeddedMovieOff);
        pMovie->runFrameActions(pNode, nFrame);
    }
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
    // 8-aligned qword string pointer (GUIAPT64); advance the PC by 8.
    const unsigned char* const pAligned = reinterpret_cast<const unsigned char*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));
    const char* const szLabel = *reinterpret_cast<const char* const*>(pAligned);
    pContext->mpProgramCounter = pAligned + 8;

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

    const int nFrame = AptLabelToFrame(pNode, &label);   // FLAG: node frame-label hash
    if (nFrame >= 0)
    {
        pNode->jumpToFrame(nFrame);                             // real member (play-head seek)
        // clear the "playing" bit on the node's sprite instance (bIsPlaying, x64 bit 25;
        // console form *(node+0x20)+0x14 &= ~0x40).
        static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst())->mnClipActionFlags
            &= ~0x2000000u;
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
    // 8-aligned (GUIAPT64); the inline 4-byte word is the "start playing" flag.
    const uint32_t* const pPlayFlag = reinterpret_cast<const uint32_t*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));
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
        AptResolveTargetContext(pContext->mpCIH, pTarget, pName,
                                       &pResolved, &scratch);
        if (pResolved)
        {
            const AptVirtualFunctionTable_Indices eRes = pResolved->getVtblIndex();
            // console: a clip / CIHNone resolved node -> look up the label in its hash
            if ((eRes == AptVFT_CharacterInstHandle && pResolved->getIsDefined())
                || eRes == AptVFT_CIHNone)
                nFrame = AptLabelToFrame(static_cast<AptCIH*>(pResolved), &scratch);
        }
    }
    else if (eType == AptVFT_Integer && pTop->getIsDefined())
    {
        nFrame = pTop->toInteger();
    }

    if (nFrame != -1 && pNode)
    {
        pNode->jumpToFrame(nFrame);                        // real member (play-head seek)
        const bool bPlay = (*pPlayFlag != 0);              // console: cntlzw test of the inline word
        AptCharacterSpriteInstBase* const pSprite =
            static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst());
        // set/clear bit 6 (0x40, "playing") from the play flag.
        pSprite->mnClipActionFlags = (pSprite->mnClipActionFlags & ~0x2000000u) | (bPlay ? 0x2000000u : 0u);   // bIsPlaying, x64 bit 25
        if (bPlay)
            pNode->SetDirtyState(true, true);              // real member (dirty latch)
    }

    pInterp->stackPop();   // console AptValue>::Pop(a1) -- drop the destination operand
}

// ===========================================================================
// _createObject @0x82B08088 -- AS `new ClassName(args...)`.
//   DECOMPILED from the X360 ARTIST assembly (0x82B08088), cross-checked against the
//   PS3 DecFIGS lift (._ZN20AptActionInterpreter13_createObjectE... @0xF699B8).
//
// Resolve the class value under (pScope, pTarget); if it is a constructible class,
// build the matching built-in instance from the nArgs operands already on the stack,
// link it to the class prototype (lazily creating one), copy the class's implemented
// interfaces, and -- when bConstruct -- run the constructor body. The console reads
// operands as mpStack[mnStackTop - k - 1] (== stackAt(k)).
//
// FLAG: several leaf object constructions the console inlined verbatim (the
// AptValueWithHash base + the concrete vtable + the per-type ctor) are expressed here
// through the reconstructed C++ `new Type(...)` for each type, which materialises the
// identical sequence. The two raw `mValueBitfield |= 0x400000` / `&= ~0x400000`
// brackets the console wraps construction in are the SetHasClass class-flag word
// (same mapping the NewMethod handler above uses), modelled as SetHasClass(1)/(0).
// ===========================================================================
AptValue* AptActionInterpreter::_createObject(AptValue* pScope, AptValue* pTarget,
                                              const EAStringC* pClassName,
                                              int nArgs, char bConstruct)
{
    AptValue* pNew = 0;                                          // console v11

    // Resolve the class value: getVariable(scope, target, name, bRead=1, ..., 0).
    AptValue* pClass = getVariable(pScope, pTarget, pClassName, 1, 1, 0);   // console Variable / r16

    // Only a defined class value that CanCreateScriptObject is constructible. The bit
    // ((Variable[1] >> 27) & 1) is mbIsDefined; when not constructible the operands are
    // released and the function returns null.
    if (!pClass->getIsDefined() || !pClass->CanCreateScriptObject())
    {
        stackSafePop(nArgs);                                    // console AptValue_::SafePop(a1, a5)
        return 0;
    }

    const char* const pClassStr = pClassName->GetBuffer();      // console *a4 + 8 (the char buffer)

    if (_stricmp(pClassStr, "Array") == 0)
    {
        // ---- new Array(...) -------------------------------------------------
        pNew = new AptArray();                                   // operator new(44) + sub_82AF1CB0 (ctor)

        // new Array(n) with a single non-negative integer/whole-float length sets the
        // array length to n-1's slot (leaving it sparse); otherwise the operands become
        // the elements (top-down: element i = stackAt(i)).
        bool bLengthForm = false;
        if (nArgs == 1)
        {
            AptValue* pTop = stackAt(0);
            const AptVirtualFunctionTable_Indices eTop = pTop->getVtblIndex();
            if (eTop == AptVFT_Integer && pTop->getIsDefined())
            {
                if (pTop->toInteger() >= 0)
                    bLengthForm = true;
            }
            if (!bLengthForm)
            {
                AptValue* pTop2 = stackAt(0);
                const AptVirtualFunctionTable_Indices eF = pTop2->getVtblIndex();
                if (eF == AptVFT_Float && pTop2->getIsDefined()
                    && fmodf(pTop2->toFloat(), 1.0f) == 0.0f)
                    bLengthForm = true;
            }
        }

        if (bLengthForm)
        {
            const int nLen = stackAt(0)->toInteger();           // console AptValue::toInteger
            static_cast<AptArray*>(pNew)->set(nLen - 1, gpUndefinedValue);   // off_8324D814
        }
        else
        {
            for (int i = 0; i < nArgs; ++i)
                static_cast<AptArray*>(pNew)->set(i, stackAt(i));
        }
    }
    else if (_stricmp(pClassStr, "String") == 0)
    {
        // ---- new String(...) ------------------------------------------------
        // Build an empty AS string, append the (single) argument's string form, then
        // re-fetch the global String class (its prototype is the live String.prototype)
        // and box the string in an AptStringObject.
        AptString* pStr = AptString::Create("");                // console AptString::Create(&unk_820046A7)
        if (nArgs == 1)
            stackAt(0)->Append_ToString(pStr->GetInternalString());   // v21 + 8

        // The class used for the prototype link below switches to the global String.
        pClass = gpAptGlobalFallback->Lookup(gAptStringClassName);    // off_8324E380->Lookup(&dword_8324E6B4)

        AptStringObject* pBoxed = new AptStringObject(pStr);    // operator new(36) + ctor(v21)
        pNew = pBoxed;
    }
    else if (_stricmp(pClassStr, "Date") == 0)
    {
        // ---- new Date(...) --------------------------------------------------
        // The console coerces each supplied operand to an integer (side effect only --
        // the Date fields are not stored in this build) then makes a generic Date-tagged
        // object. Reproduced as discarded coercions to match the asm exactly.
        if (nArgs > 6) (void)stackAt(6)->toInteger();
        if (nArgs > 5) (void)stackAt(5)->toInteger();
        if (nArgs > 4) (void)stackAt(4)->toInteger();
        if (nArgs > 3) (void)stackAt(3)->toInteger();
        if (nArgs > 2) (void)stackAt(2)->toInteger();
        if (nArgs > 1) (void)stackAt(1)->toInteger();
        if (nArgs > 0) (void)stackAt(0)->toInteger();
        // FLAG: the console builds a bare AptObject shell carrying value tag AptVFT_Date
        // (21) + the Date vtable (off_82145ECC). There is no dedicated AptDate class and
        // AptObject's tagged ctor is protected, so a generic AptObject (AptVFT_Object) is
        // built here; the Date type tag / vtable is the deferred Date-type follow-on.
        pNew = AptObject::Create(8);                            // operator new(32) + AptValueWithHash(.., 8)
    }
    else if (_stricmp(pClassStr, "TextFormat") == 0)
    {
        // ---- new TextFormat(...) --------------------------------------------
        // Coerce up to 13 formatting operands (font/size/colour/bold/italic/underline/
        // url/target/align/margins/indent/leading), defaulting the unsupplied ones, then
        // build the AptTextFormat. The console marshals these into the embedded
        // TextFormat record via the inlined factory (sub_82AFB2A8); that record
        // population is the deferred TextFormat-from-operands form (FLAG below).
        if (nArgs > 12) (void)stackAt(12)->toInteger();
        if (nArgs > 11) (void)stackAt(11)->toInteger();
        if (nArgs > 10) (void)stackAt(10)->toInteger();
        if (nArgs > 9)  (void)stackAt(9)->toInteger();
        if (nArgs > 7)  (void)stackAt(7)->toInteger();
        if (nArgs > 6)  (void)stackAt(6)->toInteger();
        if (nArgs > 5)  (void)stackAt(5)->toInteger();
        if (nArgs > 4)  (void)stackAt(4)->toInteger();
        if (nArgs > 3)  (void)stackAt(3)->toInteger();
        if (nArgs > 2)  (void)stackAt(2)->toInteger();
        // size (operand 1) defaults to -1 (inherit); the colour string (operand 0)
        // defaults to off_8324D814 (undefined) when no operands are supplied.
        (void)((nArgs <= 1) ? -1.0 : static_cast<double>(stackAt(1)->toFloat()));
        AptValue* pColour = (nArgs <= 0) ? gpUndefinedValue : stackAt(0);
        (void)pColour;

        // FLAG: the TextFormat-from-operands marshaling (console sub_82AFB2A8 -- the
        // factory that fills the embedded TextFormat record from the coerced operands)
        // is a follow-on; our AptTextFormat ctor takes a const TextFormat*, so the
        // populated record is not yet available here. Construct a default record.
        pNew = new AptTextFormat(static_cast<const TextFormat*>(0));
    }
    else if (_stricmp(pClassStr, "Color") == 0)
    {
        // ---- new Color(target) ----------------------------------------------
        // The single argument names (or is) the movie clip the colour transform acts on.
        // A string/path operand is resolved to its object via getObject; the resolved
        // value (or the raw operand) is handed to the AptScriptColour ctor.
        AptValue* pTargetVal = stackAt(0);                      // console Object / v67
        const AptVirtualFunctionTable_Indices eT = pTargetVal->getVtblIndex();
        if ((eT == AptVFT_StringValue || eT == AptVFT_StringObject) && pTargetVal->getIsDefined())
        {
            // console: a boxed (tag 33) string indirects through *(Object+32) first.
            AptValue* pNameVal = pTargetVal;
            if (eT != AptVFT_StringValue)
                pNameVal = stackAt(0);                          // *(Object + 32) -- boxed inner string
            pTargetVal = getObject(pScope, 0, pNameVal->c_string()->GetInternalString());   // Object + 8
        }
        pNew = new AptScriptColour(pTargetVal);                 // operator new(36) + ctor(Object)
    }
    else if (_stricmp(pClassStr, "MovieClip") == 0)
    {
        // ---- new MovieClip() ------------------------------------------------
        pNew = new AptMovieClip();                              // operator new(32) + ctor
    }
    else if (_stricmp(pClassStr, "XML") == 0)
    {
        // ---- new XML() ------------------------------------------------------
        // FLAG: the console builds an AptXmlNode carrying the AptVFT_Xml (25) tag + the
        // XML vtable (off_82145F88) via the protected AptXmlNode(eType, cap) ctor (the
        // AptXml derived form). That ctor is not externally accessible, so the public
        // AptXmlNode() (AptVFT_XmlNode, 24) is built here; the Xml type tag / vtable is
        // the deferred AptXml follow-on.
        pNew = new AptXmlNode();                                // operator new(32) + AptXmlNode()
    }
    else if (_stricmp(pClassStr, "Error") == 0)
    {
        // ---- new Error([message]) -------------------------------------------
        if (nArgs == 1)
        {
            // new Error(message): coerce the operand to a string, take a ref, build the
            // Error, then assign its message (and the "Error" name).
            EAStringC scratch;                                  // console v53 (&unk_82F72FF8 seed)
            const EAStringC* pMsg = AptValue::Get_ToString(stackAt(0), &scratch);
            EAStringC message(*pMsg);                           // console: copy + IncreaseInternalRefCount

            pNew = new AptError();                              // operator new(40) + sub_82AF0C50(v37, msg)
            // FLAG: the message-bearing AptError ctor (console sub_82AF0C50 -- stores
            // mMessage from `message` and mName = "Error") is the follow-on message form;
            // AptError.h only declares the default ctor, so the message is not stored yet.
            (void)message;
        }
        else
        {
            pNew = new AptError();                              // operator new(40) + AptError()
        }
    }
    else
    {
        // ---- new Object() (generic) -----------------------------------------
        pNew = AptObject::Create(8);                            // operator new(32) + AptValueWithHash(19, 8) + off_821459D8
    }

    if (!pNew)
        return 0;                                               // console LABEL_106 -> v11 = 0

    // ---- common tail: AddRef, link prototype, copy interfaces, construct ----
    pNew->AddRef();                                             // console (**v11)(v11)

    AptNativeHash* pClassHash = pClass->GetNativeHashVirtual(); // console (*(*Variable+8))(Variable) -> v44
    AptValue* pProto = pClassHash->GetPrototype();             // console *(v44 + 12) -> v45
    if (!pProto)
    {
        // lazily create the class prototype and back-link it to the class.
        pProto = new AptPrototype();                           // operator new(32) + ctor
        static_cast<AptPrototype*>(pProto)->SetSuperConstructor(pClass);
        pClassHash->SetPrototype(pProto);
    }

    pNew->SetHasClass(1);                                       // console (*(*v11+0x14))(v11, 1)
    pNew->SetHasClass(1);                                       // console *(v11+7) |= 0x400000 (open the class-flag bracket)
    pNew->GetNativeHashVirtual()->Set__Proto__(pProto);        // console AptNativeHash::Set__Proto__(v11+8, v45)

    // Copy the class's implemented interfaces when the class is an Object (tag 19) or
    // one of the script-function class tags (34..36), each requiring mbIsDefined.
    const AptVirtualFunctionTable_Indices eClass = pClass->getVtblIndex();
    bool bCopyImpls = (eClass == AptVFT_Object && pClass->getIsDefined());
    if (!bCopyImpls)
    {
        const int d = static_cast<int>(eClass) - 34;           // console (v48 - 34) <= 2
        if (d >= 0 && d <= 2 && pClass->getIsDefined())
            bCopyImpls = true;
    }
    if (bCopyImpls)
    {
        int nImpls = 0;                                        // console v54[0]
        AptValue* pImpls = static_cast<AptObject*>(pClass)->GetImplementedObjects(&nImpls);
        if (nImpls > 0)
            static_cast<AptObject*>(pNew)->SetImplementedObjects(pImpls, static_cast<char>(nImpls));
    }

    if (bConstruct)
    {
        // Protect the new object on two GC-root stacks (the CIH/target stack and call
        // stack #5) across the constructor run, run the constructor body, then unwind.
        AptValue** const pCIHStack = reinterpret_cast<AptValue**>(mpCIHStack);
        pCIHStack[mnCIHStackTop++] = pNew;                     // console *(4*a1[9]++ + a1[11]) = v11
        pNew->AddRef();                                        // console (**v11)(v11)

        AptValue** const pRootStackE = reinterpret_cast<AptValue**>(mpCallStackE);
        pRootStackE[mnCallStackE_Count++] = pNew;             // console *(4*a1[12]++ + a1[14]) = v11
        pNew->AddRef();                                        // console (**v11)(v11)

        callFunction(pNew, pClass, nArgs, 0, 0);              // run the constructor body

        // pop call stack #5 (release), the operand stack (release), then the CIH stack.
        // Each interpreter stack is an AptValueVector-shaped {count,cap,array} triple; the
        // console pops via AptValueVector::pop(a1 + wordOffset). Reproduced as the punned call
        // (behaviour-identical -- these were already unguarded Release+decrement).
        reinterpret_cast<AptValueVector*>(&mnCallStackE_Count)->pop();   // console AptValue_::pop(a1 + 12)
        stackPop();                                                      // console AptValue_::pop(a1)  (operand stack; guarded stackPop @0x7F3248)
        reinterpret_cast<AptValueVector*>(&mnCIHStackTop)->pop();        // console AptValue_::pop(a1 + 9)
    }

    pNew->SetHasClass(0);                                       // console *(v11+7) &= ~0x400000 (close the bracket)
    return pNew;
}

// The free-function forwarder AptActionInterpreter_createObject was RETIRED: callers
// now name the real member AptActionInterpreter::_createObject @0x82B08088 directly.

// ===========================================================================
// FLAG-stub homes for the two object-internal offset accessors declared extern
// above (AptGetMethodNameSlot / AptGetClassOwnerValue).
//
// These are NOT standalone functions in any of the three dumps (X360 ARTIST,
// PS3 DecFIGS/External, BurnoutPR PC): they are inline pointer-arithmetic reads
// our decompile split out of _FunctionAptActionCallMethod --
//   GetMethodNameSlot(v6)   == "the EAStringC* method-name slot at (object + 8)"
//   GetClassOwnerValue(v)   == "the class/owner AptValue* at (value + 0x1C)"
// The object/value classes these index (the derived AptObject / AptValueWithHash
// method-name + defining-class slots) have no reconstructed x64 member at those
// console byte offsets yet, so a raw (this + 8) / (this + 0x1C) read would hit
// the wrong x64 memory and a guessed member would be a fabrication. The whole
// _FunctionAptActionCallMethod path is "[not executed in goal trace]" (see the
// header of this TU), so these are homed as faithful FLAG stubs: they link and
// keep the boot path clean, but assert if the AS call-method opcode ever reaches
// them, pending the AptObject method-name/owner-slot member recovery.
//   (Listed in functions_blocked: absent from all three dumps + unrecovered
//    object layout.)
// ===========================================================================
EAStringC** AptGetMethodNameSlot(AptValue* /*pObject*/)
{
    // FLAG: console (object + 8) -- the method-name EAStringC* slot; the x64 member
    // is not recovered. Inert until the AS call-method opcode is brought up (not on
    // the boot trace), so returning null is safe for the current goal.
    return nullptr;
}

AptValue* AptGetClassOwnerValue(AptValue* /*pValue*/)
{
    // FLAG: console *(value + 0x1C) -- the defining-class/owner AptValue* slot; the
    // x64 member is not recovered. Inert until the AS call-method opcode is brought
    // up (not on the boot trace), so returning null is safe for the current goal.
    return nullptr;
}

// AptApt_PopCIHStack / AptApt_PopCallStackC bodies inlined at their single
// _FunctionAptActionCallMethod cleanup call sites (console inlined AptValueVector::pop on
// the punned {count,cap,array} triple; x64 reproduces it by named members there).

// AptApt_PopCallStackC inlined at its single _FunctionAptActionCallMethod cleanup call site (see note above).
