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
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC (dictionary-string temp)
#include "SDKs/EATech/include/Apt/AptCIH.h"                // AptCIH : AptValueGC (mpCIH -> AptValue* upcast)

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
