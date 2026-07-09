// ===========================================================================
// EATech Apt -- AptActionInterpreter operand stack.   DECOMPILED from the PS3
// EXTERNAL ELF:
//     stackPush       @0x7F1790    stackPushNoInc  @0x7E990C
//     stackPop()      @0x7F3248    stackPop(int)   @0x7FDB68
//     stackSafePop    @0x7DF7F8    stackGetPop     @0x7DF7B4
//     stackPopNoDec   @0x7DF7DC    stackPopAndPush @0x7FB288
//
// The interpreter's AptValue* evaluation stack: mpStack[0..mnStackTop). Pushes
// take a counted reference (AddRef), pops release it (Release) -- except the
// *NoInc/*NoDec/GetPop variants, which transfer ownership instead of touching the
// refcount. The console's `4 * top + base` byte addressing is reproduced here as
// element indexing (mpStack[top]); on x64 AptValue* is 8 bytes, so the indexing
// stays correct without a pointer-width transcode.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptLookup.h"       // AptLookup::GetIndex (Lookup indirection)
#include "SDKs/EATech/include/Apt/AptValue/AptRegister.h"     // AptRegister::GetIndex (Register indirection)
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"    // GetRegisterValue (Register indirection)
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"       // AptString peek (FLAG bring-up trace)

// ---------------------------------------------------------------------------
// stackPush @0x7F1790 -- store at top, advance, AddRef.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPush(AptValue* pValue)
{
    mpStack[mnStackTop] = pValue;
    ++mnStackTop;
    pValue->AddRef();
}

// ---------------------------------------------------------------------------
// stackPushNoInc @0x7E990C -- store at top, advance, but DO NOT AddRef (the
// caller's reference is handed to the stack).
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPushNoInc(AptValue* pValue)
{
    mpStack[mnStackTop] = pValue;
    ++mnStackTop;
}

// ---------------------------------------------------------------------------
// stackPop @0x7F3248 -- release + pop the top value, returning it. The returned
// pointer has already been Release()'d (the console returns it regardless); null
// on an empty stack.
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::stackPop()
{
    AptValue* pValue = 0;
    if (mnStackTop > 0)
    {
        pValue = mpStack[mnStackTop - 1];
        pValue->Release();
        --mnStackTop;
    }
    return pValue;
}

// ---------------------------------------------------------------------------
// stackPop @0x7FDB68 -- pop nCount values (releasing each), only if the stack
// holds at least that many.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPop(int nCount)
{
    if (mnStackTop >= nCount)
    {
        for (int i = 1; i <= nCount; ++i)
            mpStack[mnStackTop - i]->Release();
        mnStackTop -= nCount;
    }
}

// ---------------------------------------------------------------------------
// stackSafePop @0x7DF7F8 -- stackPop(nCount) with the additional nCount>0 guard.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackSafePop(int nCount)
{
    if (nCount > 0 && mnStackTop >= nCount)
    {
        for (int i = 1; i <= nCount; ++i)
            mpStack[mnStackTop - i]->Release();
        mnStackTop -= nCount;
    }
}

// ---------------------------------------------------------------------------
// stackGetPop @0x7DF7B4 -- pop + return the top value WITHOUT releasing it (the
// caller takes over the reference). No bounds check (matches the console).
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::stackGetPop()
{
    --mnStackTop;
    return mpStack[mnStackTop];
}

// ---------------------------------------------------------------------------
// stackPopNoDec @0x7DF7DC -- lower the top slot without releasing the value (its
// reference is kept alive by another owner).
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPopNoDec()
{
    if (mnStackTop > 0)
        --mnStackTop;
}

// ---------------------------------------------------------------------------
// stackPopAndPush @0x7FB288 -- collapse the top nCount operands into one result:
// AddRef the result, Release the nCount popped, store the result at the collapsed
// slot, set top = top - nCount + 1. No-op unless the stack holds nCount values.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPopAndPush(int nCount, AptValue* pValue)
{
    if (mnStackTop >= nCount)
    {
        pValue->AddRef();
        for (int i = 1; i <= nCount; ++i)
            mpStack[mnStackTop - i]->Release();
        mpStack[mnStackTop - nCount] = pValue;
        mnStackTop = mnStackTop - nCount + 1;
    }
}

// ---------------------------------------------------------------------------
// stackPushIndirect @0x7ECE34 -- push pValue, first resolving an indirection.
// A Lookup value (tag 8, defined) reads the interpreter's string-constant dictionary
// mpConstantPool[index]; a Register value (tag 4, defined) reads the AS function
// register file via AptScriptFunctionBase::GetRegisterValue(index). The push is
// the console's inlined stackPush (store/advance/AddRef), reproduced via stackPush.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPushIndirect(AptValue* pValue)
{
    int nRegIndex = -1;

    if (pValue->isLookup())            // AptVFT_Lookup (tag 8) && defined
        pValue = mpConstantPool[static_cast<AptLookup*>(pValue)->GetIndex()];
    else if (pValue->isRegister())     // AptVFT_Register (tag 4) && defined
    {
        nRegIndex = static_cast<int>(static_cast<AptRegister*>(pValue)->GetIndex());
        pValue = AptScriptFunctionBase::GetRegisterValue(nRegIndex);
    }

    stackPush(pValue);
}

// ===========================================================================
// Non-uniform interpreter helpers (lifecycle / run scaffolding / FSCommand /
// path-name / instanceof). DECOMPILED from the X360 ARTIST.XEX:
//     ~AptActionInterpreter     @0x82AE3918    CleanupAfterExecution @0x82AE9388
//     isFSCommand               @0x82AD9148    doFSCommand           @0x82AD91A0
//     cbCallMethod_ASSetPropFlags @0x82AD8448  getObject             @0x82B07F18
//     getName  @0x82AF75C8       getName2 @0x82AF7540  isObjectOfType @0x82AEA5B8
//     urlDecode @0x82AEE208      _doEnumerate @0x82B036D8
//
// The console addresses each AptValue-stack slot by 32-bit byte math (4*top+base)
// and indexes the interpreter's five {count,capacity,array} stacks by raw offsets;
// reproduced here as typed element indexing (mpStack[mnStackTop-1]) -- identical on
// x64 where AptValue* is 8 bytes -- and named-member access. The deallocation sizes
// switch from the console's `4 * count` to `sizeof(AptValue*) * count` (the x64
// pointer width), matching this struct's reconstruction.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptObject.h"        // DoesImplementObject (the instanceof walk)
#include "SDKs/EATech/include/Apt/AptNativeHash.h"    // mpPrototype / mp__Proto__ (the instanceof walk)
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"     // AptString::Create / GetInternalString
#include "SDKs/EATech/include/Apt/AptString/EAString.h"     // EAStringC
#include "SDKs/EATech/Apt/DogmaAllocator.h"                 // DOGMA_PoolManager::Deallocate
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"  // gpAptOperandStackPool (off_8324D808)
#include <cstring>                                          // strncmp / strlen
#include <cctype>    // isxdigit (the un-escape codec)
#include <cstdlib>   // strtoul (the %XX decode)

// gpUndefinedValue -- the shared `undefined` singleton (console off_8324D814). The
// dispatch handlers and the builtins reuse it; declared extern here too.
extern AptValue* gpUndefinedValue;

// FLAG (homed by the apt VM native-call dispatch): the global native-method arg stack
// (X360 off_8324E768 = gAptActionInterpreter.mpStack, dword_8324E760 = its mnStackTop).
// The i-th AS argument (i=0 = last pushed) is gppAptNativeArgStack[gnAptNativeArgCount-1-i].
extern AptValue** gppAptNativeArgStack;   // off_8324E768
extern int        gnAptNativeArgCount;    // dword_8324E760

// FLAG (host FSCommand sink -- console dword_8324E848, installed by the host): the
// getURL("FSCommand:...") callback. Invoked as hook(command, args). Null until the
// host installs it; the AS path is faithful, the sink is the host boundary.
extern void (*gpAptFSCommandHook)(const char* pCommand, const char* pArgs);

// FLAG: the "FSCommand:" url prefix literal (console off_82F7300C). A plain C string
// constant in the X360 .rodata; reproduced verbatim.
static const char* const kAptFSCommandPrefix = "FSCommand:";

// AptScriptFunctionBase::PopStaticData (console @0x82AE9388's tail call) is the real
// static member (AptScriptFunctionBase.cpp): pop the register-block window back to the
// saved base. The arg is the void* window base (PushStaticData's return) -- the earlier
// SavedExecutionState* mis-typing is resolved (2026-07-01).

// ---------------------------------------------------------------------------
// ~AptActionInterpreter @0x82AE3918 -- free each of the five {count,capacity,array}
// stacks back to the operand-stack pool. The console deallocates `4 * capacity`
// bytes; x64-native that is `sizeof(AptValue*) * capacity`. Order matches the asm
// (call-stack E, CIH stack, call-stack C, call-stack B, operand stack).
// ---------------------------------------------------------------------------
AptActionInterpreter::~AptActionInterpreter()
{
    if (mpCallStackE)
        gpAptOperandStackPool->Deallocate(mpCallStackE, sizeof(AptValue*) * mnCallStackE_Capacity);
    if (mpCIHStack)
        gpAptOperandStackPool->Deallocate(mpCIHStack, sizeof(AptValue*) * mnCIHStackCapacity);
    if (mpCallStackC)
        gpAptOperandStackPool->Deallocate(mpCallStackC, sizeof(AptValue*) * mnCallStackC_Capacity);
    if (mpCallStackB)
        gpAptOperandStackPool->Deallocate(mpCallStackB, sizeof(AptValue*) * mnCallStackB_Capacity);
    if (mpStack)
        gpAptOperandStackPool->Deallocate(mpStack, sizeof(AptValue*) * mnStackCapacity);
}

// ---------------------------------------------------------------------------
// CleanupAfterExecution @0x82AE9388 -- end-of-run cleanup. If a thrown value is
// still latched (mpAbortValue), render its string form (Get_ToString) so the scratch
// is dropped, Release the value, and clear the slot; then restore the script
// function's per-call execution state (PopStaticData on the saved-state record).
// ---------------------------------------------------------------------------
void AptActionInterpreter::CleanupAfterExecution(void* pSavedBase)
{
    if (mpAbortValue)
    {
        EAStringC scratch;
        AptValue::Get_ToString(mpAbortValue, &scratch);  // drops a held string ref via the scratch dtor
        mpAbortValue->Release();
        mpAbortValue = 0;
    }
    // Pop the register-block window back to the base the caller saved before the run
    // (X360: return PopStaticData(a2)).
    AptScriptFunctionBase::PopStaticData(pSavedBase);
}

// ---------------------------------------------------------------------------
// cbCallMethod_ASSetPropFlags @0x82AD8448 -- ASSetPropFlags(obj, props, set, clear):
// the AS member-visibility primitive is a no-op in this build; it just returns
// `undefined`.
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::cbCallMethod_ASSetPropFlags(AptValue* /*pThis*/, int /*nArgCount*/)
{
    return gpUndefinedValue;
}

// ---------------------------------------------------------------------------
// isFSCommand @0x82AD9148 -- does pUrl begin with the "FSCommand:" prefix? (The
// console computes the prefix length by scanning to its NUL, then strncmp; folded
// here to strlen + strncmp.)
// ---------------------------------------------------------------------------
bool AptActionInterpreter::isFSCommand(const char* pUrl)
{
    const size_t nPrefix = std::strlen(kAptFSCommandPrefix);
    return std::strncmp(pUrl, kAptFSCommandPrefix, nPrefix) == 0;
}

// ---------------------------------------------------------------------------
// doFSCommand @0x82AD91A0 -- dispatch a getURL("FSCommand:<cmd>", <args>) to the
// host hook: skip the prefix and hand (command, args) to the sink. Returns 1.
// ---------------------------------------------------------------------------
int AptActionInterpreter::doFSCommand(const char* pUrl, const char* pArgs)
{
    const size_t nPrefix = std::strlen(kAptFSCommandPrefix);
    gpAptFSCommandHook(pUrl + nPrefix, pArgs);   // FLAG: host FSCommand sink (null until installed)
    return 1;
}

// ---------------------------------------------------------------------------
// getName @0x82AF75C8 / getName2 @0x82AF7540 -- build pValue's slash/dot path name
// into pOut. getName seeds pOut empty (path mode 1); getName2 seeds it empty (mode 0)
// and, when the produced path is empty, falls back to "/". FLAG: the actual path
// walk (console sub_82AF7400, the (value, out, mode) builder) is the path-builder
// follow-on; declared extern so this entry layer is faithful and links.
// ---------------------------------------------------------------------------
extern int AptActionInterpreter_BuildPathName(AptActionInterpreter* pInterp,
                                              AptValue* pValue, EAStringC* pOut, int nMode);  // FLAG: sub_82AF7400

void AptActionInterpreter::getName(AptValue* pValue, EAStringC* pOut)
{
    *pOut = "";   // EAStringC::operator= from the empty seed (console InitFromBuffer + assign)
    AptActionInterpreter_BuildPathName(this, pValue, pOut, 1);   // FLAG: path-builder follow-on
}

void AptActionInterpreter::getName2(AptValue* pValue, EAStringC* pOut)
{
    *pOut = "";
    AptActionInterpreter_BuildPathName(this, pValue, pOut, 0);   // FLAG: path-builder follow-on
    if (pOut->GetLength() == 0)
        *pOut = "/";
}

// ---------------------------------------------------------------------------
// getObject @0x82B07F18 -- resolve the path name pName (under the (pScope,pTarget)
// context) to the AptValue object it designates: parse the path context, findChild
// the leaf, and accept it only if it is a real object (the vtbl "is object" probe).
// An empty name resolves to pScope unchanged. FLAG: the path-context parser (console
// sub_82B02F80) is the same path follow-on; declared extern.
// ---------------------------------------------------------------------------
extern void AptActionInterpreter_ParsePathContext(AptActionInterpreter* pInterp,
                                                  AptValue* pTarget, const EAStringC* pName,
                                                  AptValue** ppOutContext, EAStringC* pOutLeaf);  // FLAG: sub_82B02F80

AptValue* AptActionInterpreter::getObject(AptValue* pScope, AptValue* pTarget, const EAStringC* pName)
{
    if (pName->GetLength() == 0)
        return pScope;   // empty path -> the scope value itself (console returns the a1 it was handed)

    EAStringC  leaf;
    AptValue*  pContext = 0;
    AptActionInterpreter_ParsePathContext(this, pTarget, pName, &pContext, &leaf);  // FLAG: path-context parser

    AptValue* pResult = 0;
    if (pContext)
    {
        AptValue* pChild = pContext->findChild(&leaf, pTarget);   // FLAG: findChild body deferred (AptValue.h)
        if (pChild && pChild->getIsDefined() && pChild->getVtblIndex() == AptVFT_Object)
            pResult = pChild;
    }
    return pResult;
}

// ---------------------------------------------------------------------------
// isObjectOfType @0x82AEA5B8 -- AS `instanceof`: is pObject an instance of pClass?
// When both are objects, fetch pClass's prototype and walk pObject's prototype /
// interface chain looking for it; otherwise fall back to a type-tag comparison
// (a defined Object value of the same value-type matches). FLAG: the prototype /
// interface chain walk reads console vtable slots + the AptObject prototype links
// that are not yet reconstructed as named members; the walk is declared extern so
// this entry layer is faithful and links.
// ---------------------------------------------------------------------------
// AptActionInterpreter_InstanceOfChainWalk -- HOMED 2026-07-02 from the X360
// isObjectOfType @0x82AEA5B8's object arm (retiring the return-0 link-stub).
// The needle is pClass's prototype: GetNativeHashVirtual()->mpPrototype (+0xC;
// the asm reads it unguarded -- the caller's is-object gate guarantees a hash).
// Three arms:
//   * a CIH-family pObject (vtbl 12 defined / 37): walk the ENTIRE hash
//     __proto__ (+8) chain, latching result=1 on a needle match WITHOUT early
//     exit (the shipped loop keeps walking);
//   * a defined Object (vtbl 19): AptObject::DoesImplementObject(needle) --
//     the own-prototype + interface-table reachability test;
//   * anything else: an identity walk down the __proto__ chain (step first --
//     the object itself is never compared), bailing when a hop is not a
//     defined object (the in-loop vtbl[+0xC] predicate) or the chain ends.
int AptActionInterpreter::instanceOfChainWalk(AptValue* pObject, AptValue* pClass)
{
    int nResult = 0;

    AptNativeHash* const pClassHash = pClass->GetNativeHashVirtual();   // vtbl[2]
    AptValue* const pNeedle = pClassHash->mpPrototype;                  // +0xC

    const AptVirtualFunctionTable_Indices eType = pObject->getVtblIndex();
    if ((eType == AptVFT_CharacterInstHandle && pObject->getIsDefined())
        || eType == AptVFT_CIHNone)
    {
        // CIH-family chain walk (no early exit on a match).
        AptValue* p = pObject;
        for (;;)
        {
            AptNativeHash* const pHash = p->GetNativeHashVirtual();
            p = pHash ? pHash->mp__Proto__ : nullptr;                   // +8
            if (p == nullptr)
                break;
            if (p == pNeedle)
                nResult = 1;
        }
        return nResult;
    }

    if (eType == AptVFT_Object && pObject->getIsDefined())
        return static_cast<AptObject*>(pObject)->DoesImplementObject(pNeedle) ? 1 : 0;

    // Generic identity walk (step, then compare).
    for (AptValue* p = pObject;;)
    {
        AptNativeHash* const pHash = p->GetNativeHashVirtual();
        p = pHash ? pHash->mp__Proto__ : nullptr;
        if (p == nullptr)
            return 0;
        if (p == pNeedle)
            return 1;
        if (!(p->getIsDefined() && p->isObject()))   // the in-loop object predicate
            return 0;
    }
}

int AptActionInterpreter::isObjectOfType(AptValue* pObject, AptValue* pClass)
{
    const bool bBothObjects = pObject->getIsDefined() && pObject->isObject()
                           && pClass->getIsDefined()  && pClass->isObject();
    if (bBothObjects)
        return instanceOfChainWalk(pObject, pClass);  // the proto/interface chain walk (isObjectOfType's object arm)

    // Non-object fallback (console LABEL_36 path): a script-function-typed pObject
    // never matches; otherwise a defined Object value of the same value-type matches.
    const AptVirtualFunctionTable_Indices eType = pObject->getVtblIndex();
    if (pObject->isScriptFunction())
        return 0;
    if (eType == AptVFT_Object && pObject->getIsDefined())
        return 0;   // an undefined-class object cannot be an instanceof in this branch
    if (eType == pClass->getVtblIndex())
        return 1;
    return 0;
}

// ---------------------------------------------------------------------------
// urlDecode @0x82AEE208 -- parse one "key=value" pair out of pStream (terminated by
// '&' or NUL) into (pOutKey, pOutValue), un-escaping each side. Returns the read
// position past the consumed pair (skipping a trailing '&'), or null when the pair
// has no '=' / pStream is empty. EAStringC::Append's byte-range form and the
// un-escape pass are reused via the EAStringC API; unEscape @0x82AEE110 is homed
// below as the AptActionInterpreter::unEscape static member.
// ---------------------------------------------------------------------------
// The un-escape codec -- HOMED 2026-07-02 (retiring the {} stub). The X360
// _unEscape @0x82AEE110 + _escape2Char @0x82AD90D8: '+' decodes to a space;
// a '%' with at least one following char decodes the two-char hex pair via
// strtoul(16) when the first is a hex digit (else the SECOND char passes
// through verbatim -- the shipped fallback), consuming both; everything else
// copies. The console advances two past the '%' unconditionally, tolerating a
// pair that straddles the terminator (EAStringC reps are NUL-terminated with
// slack); reproduced with the loop re-checking the cursor each iteration.
namespace
{
    char AptUnEscape2Char(char c1, char c2)
    {
        if (!isxdigit(static_cast<unsigned char>(c1)))
            return c2;
        char aBuf[3] = { c1, c2, 0 };
        return static_cast<char>(strtoul(aBuf, nullptr, 16));
    }
}

void AptActionInterpreter::unEscape(EAStringC* pStr)
{
    EAStringC lDecoded("");
    const char* p = pStr->GetBuffer();
    while (*p != 0)
    {
        const char c = *p++;
        char cOut;
        if (c == '+')
        {
            cOut = ' ';
        }
        else if (c == '%' && *p != 0)
        {
            cOut = AptUnEscape2Char(p[0], p[1]);
            p += 2;   // the console consumes both unconditionally
        }
        else
        {
            cOut = c;
        }
        const char aOne[2] = { cOut, 0 };
        lDecoded += aOne;
    }
    *pStr = lDecoded;
}

const char* AptActionInterpreter::urlDecode(const char* pStream, EAStringC* pOutKey, EAStringC* pOutValue)
{
    *pOutKey   = "";
    *pOutValue = "";
    if (!pStream)
        return 0;

    const char* p     = pStream;
    const char* pEq   = 0;
    for (; *p && *p != '&'; ++p)
    {
        if (*p == '=')
            pEq = p;
    }

    if (!pEq)
        return 0;   // no '=' -> no pair

    pOutKey->Append(pStream, static_cast<uint32_t>(pEq - pStream));
    unEscape(pOutKey);                                                        // the un-escape codec
    pOutValue->Append(pEq + 1, static_cast<uint32_t>(p - (pEq + 1)));
    unEscape(pOutValue);                                                      // the un-escape codec
    if (*p == '&')
        ++p;
    return p;
}

// ---------------------------------------------------------------------------
// _doEnumerate @0x82B036D8 -- AS `for..in`: resolve the enumeration target (the
// stack top, or its variable when it is a name), replace the top with the null/
// undefined enumeration marker, then push each enumerable member name held in the
// target's native hash (skipping the two reserved internal keys). FLAG: the native-
// hash key iteration walks AptNativeHash + the reserved-key set; the per-key string
// construction reuses AptString/EAStringC, but the resolve + walk read console
// globals (the reserved-key sentinels) not yet reconstructed -- declared extern.
// ---------------------------------------------------------------------------
// AptActionInterpreter::EnumerateMembers is now a member (declared in the header);
// _doEnumerate() below forwards to it.

void AptActionInterpreter::_doEnumerate(AptValue* pScope, AptValue* pTarget)
{
    // The full body resolves the enumeration target, pushes the marker, and emits one
    // AptString per enumerable member key. The key-walk + reserved-key sentinels read
    // console data globals (dword_8324E580 / dword_8324E698) that are not yet
    // reconstructed as named symbols here; deferred to the enumerate-walk helper so
    // this stays faithful and links.
    EnumerateMembers(pScope, pTarget);  // the native-hash enumerate walk (this member)
}

// ---------------------------------------------------------------------------
// callFunction @0x82AE3C08 -- invoke an AS callable value (function body run /
// native callback / `new` dispatch) with nArgs operands already on the stack.
// FLAG: the faithful body drives AptScriptFunctionBase::SetupBeforeExecution /
// CleanupAfterExecution + runStream over the captured scope, saving/restoring the
// interpreter's current function (mpCurrentFunction) + string-constant-dictionary pair
// (mnConstantPoolCount / mpConstantPool, which GetConstantPool installs per-function) and
// driving the AptScriptFunctionBase register/frame machinery. Deferred to the call-dispatch
// helper (see AptInterp_ExecuteScriptFunction's FLAG for the exact branch + open items) so this entry
// layer is faithful and links; the operand-stack collapse the console performs around
// it (Burnout_X360_Artist_01e3_0 == the pop-N primitive) is part of that helper.
// ---------------------------------------------------------------------------
// AptActionInterpreter::CallFunctionDispatch is now a member (declared in the header);
// callFunction() below forwards to it.

AptValue* AptActionInterpreter::callFunction(AptValue* pScope, AptValue* pFunction, int nArgs,
                                             AptValue* pNewTarget, AptValue* pConstructTarget)
{
        return CallFunctionDispatch(pScope, pFunction, nArgs,
                                pNewTarget, pConstructTarget);  // the dispatch body (this member)
}

// ---------------------------------------------------------------------------
// _parseStream @0x82AF3440 -- relocate/transcode an action bytecode stream's inline
// pointer/constant operands between the on-disk (.apt) and live forms (the resolve /
// unresolve pass; nDirection 0 = resolve). FLAG: the body is a 0x42-entry jump-table
// dispatch (console word_82145280) over the opcode operand shapes, rebasing each
// inline pointer by nRelocBase and swapping the constant-pool / function-record
// fields against the live value tables (StringPool / AptFloat / AptInteger / the
// gpUndefinedValue & boolean singletons). That jump-table + the .apt operand-shape
// records are binary DATA not in the code-only IDA exports; deferred to the resolve
// transcode helper so this entry layer is faithful and links. (Same deferral the
// header documents.)
// ---------------------------------------------------------------------------
// AptActionInterpreter::ResolveTranscode is now a member (declared in the header);
// _parseStream() below forwards to it.

const unsigned char* AptActionInterpreter::_parseStream(const unsigned char* pStream, int nRelocBase,
                                                        AptValue* pResolveCtx, int nDirection)
{
    return ResolveTranscode(pStream, nRelocBase, pResolveCtx, nDirection);  // the opcode-length walk (this member)
}

// ---------------------------------------------------------------------------
// cbCallMethod_setInterval @0x82B019D8 -- setInterval(fn|obj, method, ms, ...args):
// claim a free AptIntervalTimer slot in the animation target's timer table, bind the
// callback (a function value, or an object + method-name pair resolved via findChild),
// store the period, append the trailing args, and return the new integer id (or
// `undefined` when the table is full or the period arg is undefined).
// cbCallMethod_clearInterval @0x82AE3AE0 -- clearInterval(id): tear down the timer
// slot whose id matches the integer argument.
//
// FLAG: both index the AptIntervalTimer slot by its setup-path fields (the in-use
// flag, the callback/method value slots at +0x04/+0x10, the period/elapsed floats and
// the id at +0x20) which the AptIntervalTimer header still leaves as opaque
// placeholders (it documents that THESE TUs name them). Naming those members requires
// editing AptIntervalTimer.h, which is out of this task's edit scope; so the slot
// manipulation is deferred to the interval-table helpers and the entry layer here
// reads the args off the native-arg stack faithfully.
// ---------------------------------------------------------------------------

AptValue* AptActionInterpreter::cbCallMethod_setInterval(AptValue* /*pThis*/, int nArgCount)
{
    // Per the asm (r6 = arg[count-1]): the CALLBACK is at native-arg depth count-1, and
    // the defined-guard tests THAT slot (it is bound as the timer callback). The period
    // (count-2, r28) is read + toFloat'd inside the interval-table setup.
    AptValue* pCallback = gppAptNativeArgStack[gnAptNativeArgCount - 1];
    if (!pCallback->getIsDefined())
        return gpUndefinedValue;
    return SetIntervalImpl(pCallback, nArgCount);   // the interval-table setup body
}

AptValue* AptActionInterpreter::cbCallMethod_clearInterval(AptValue* /*pThis*/, int /*nArgCount*/)
{
    AptValue* pIdArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];
    if (pIdArg->getIsDefined())
        ClearIntervalImpl(pIdArg->toInteger());   // the interval-table teardown body
    return gpUndefinedValue;
}
