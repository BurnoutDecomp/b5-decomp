// ===========================================================================
// EATech Apt -- AptActionInterpreter operand stack.   DECOMPILED from the PS3
// EXTERNAL ELF:
//     stackPush       @0x7F1790    stackPushNoInc  @0x7E990C
//     stackPop()      @0x7F3248    stackPop(int)   @0x7FDB68
//     stackSafePop    @0x7DF7F8    stackGetPop     @0x7DF7B4
//     stackPopNoDec   @0x7DF7DC    stackPopAndPush @0x7FB288
//
// The interpreter's AptValue* evaluation stack: mOperandStack.mppItems[0..mOperandStack.mnTop). Pushes
// take a counted reference (AddRef), pops release it (Release) -- except the
// *NoInc/*NoDec/GetPop variants, which transfer ownership instead of touching the
// refcount. The console's `4 * top + base` byte addressing is reproduced here as
// element indexing (mOperandStack.mppItems[top]); on x64 AptValue* is 8 bytes, so the indexing
// stays correct without a pointer-width transcode.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

// XDK size optimization matches ARTIST at 0x82AE3918 and 0x82AD4A98.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/Apt.h"
#include "SDKs/EATech/include/Apt/AptValue/AptLookup.h"       // AptLookup::GetIndex (Lookup indirection)
#include "SDKs/EATech/include/Apt/AptValue/AptRegister.h"     // AptRegister::GetIndex (Register indirection)
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"    // GetRegisterValue (Register indirection)

// ---------------------------------------------------------------------------
// stackPop @0x7F3248 -- release + pop the top value. VOID per the x64 mangling
// (?stackPop@AptActionInterpreter@@QEAAXXZ @0x14085D380 -- the body releases and
// decrements without returning; the old AptValue* return modelled a PPC r3
// leftover and exposed an already-Release()'d pointer the real API never did).
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPop()
{
    if (mOperandStack.mnTop > 0)
    {
        mOperandStack.mppItems[mOperandStack.mnTop - 1]->Release();
        --mOperandStack.mnTop;
    }
}

// ---------------------------------------------------------------------------
// stackSafePop @0x7DF7F8 -- stackPop(nCount) with the additional nCount>0 guard.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackSafePop(int nCount)
{
    if (nCount > 0 && mOperandStack.mnTop >= nCount)
    {
        for (int i = 1; i <= nCount; ++i)
            mOperandStack.mppItems[mOperandStack.mnTop - i]->Release();
        mOperandStack.mnTop -= nCount;
    }
}

// ---------------------------------------------------------------------------
// stackGetPop @0x7DF7B4 -- pop + return the top value WITHOUT releasing it (the
// caller takes over the reference). No bounds check (matches the console).
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::stackGetPop()
{
    --mOperandStack.mnTop;
    return mOperandStack.mppItems[mOperandStack.mnTop];
}

// ---------------------------------------------------------------------------
// stackPopAndPush @0x7FB288 -- collapse the top nCount operands into one result:
// AddRef the result, Release the nCount popped, store the result at the collapsed
// slot, set top = top - nCount + 1. No-op unless the stack holds nCount values.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPopAndPush(int nCount, AptValue* pValue)
{
    mOperandStack.PopAndPush(nCount, pValue);
}

// ---------------------------------------------------------------------------
// stackPushIndirect @0x7ECE34 -- push pValue, first resolving an indirection.
// A Lookup value (tag 8, defined) reads the interpreter's string-constant dictionary
// mConstantPool.mppEntries[index]; a Register value (tag 4, defined) reads the AS function
// register file via AptScriptFunctionBase::GetRegisterValue(index). The push is
// the console's inlined stackPush (store/advance/AddRef), reproduced via stackPush.
// ---------------------------------------------------------------------------
void AptActionInterpreter::stackPushIndirect(AptValue* pValue)
{
    int nRegIndex = -1;

    if (pValue->isLookup())            // AptVFT_Lookup (tag 8) && defined
        pValue = mConstantPool.mppEntries[static_cast<AptLookup*>(pValue)->GetIndex()];
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
// reproduced here as typed element indexing (mOperandStack.mppItems[mOperandStack.mnTop-1]) -- identical on
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
#include <cctype>    // isxdigit (the un-escape codec) / isalnum (the escape codec)
#include <cstdlib>   // strtoul (the %XX decode)
#include <cstdio>    // sprintf (the %XX encode)

// gpUndefinedValue -- the shared `undefined` singleton (console off_8324D814). The
// dispatch handlers and the builtins reuse it; declared extern here too.
extern AptValue* gpUndefinedValue;

// The global native-method arg stack (X360 off_8324E768 = gAptActionInterpreter.mOperandStack.mppItems,
// dword_8324E760 = its mOperandStack.mnTop; defined in AptGlobals.cpp, published per native call
// by CallFunctionDispatch). The i-th AS argument (i=0 = last pushed) is
// gppAptNativeArgStack[gnAptNativeArgCount-1-i].
extern AptValue** gppAptNativeArgStack;   // off_8324E768
extern int        gnAptNativeArgCount;    // dword_8324E760

// ARTIST loads the prefix pointer at 0x82F7300C and scans the string at 0x82144A40.
const char* FSCOMMAND = "FSCommand:";

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
    if (mOperandStack.mppItems)
        gpAptOperandStackPool->Deallocate(mOperandStack.mppItems, sizeof(AptValue*) * mOperandStack.mnCapacity);
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
// isFSCommand @0x82AD9148 -- compare the URL with the configured command prefix.
// ---------------------------------------------------------------------------
bool AptActionInterpreter::isFSCommand(const char* pUrl)
{
    return std::strncmp(pUrl, FSCOMMAND, std::strlen(FSCOMMAND)) == 0;
}

// ---------------------------------------------------------------------------
// doFSCommand @0x82AD91A0 -- dispatch a getURL("FSCommand:<cmd>", <args>) to the
// host hook: skip the prefix and hand (command, args) to the sink. Returns 1.
// ---------------------------------------------------------------------------
int AptActionInterpreter::doFSCommand(const char* pUrl, const char* pArgs)
{
    pUrl += std::strlen(FSCOMMAND);
    gAptFuncs.pfnCommand(pUrl, pArgs);
    return 1;
}

// ---------------------------------------------------------------------------
// getName @0x82AF75C8 / getName2 @0x82AF7540 -- build pValue's slash/dot path name
// into pOut. getName seeds pOut empty (path mode 1); getName2 seeds it empty (mode 0)
// and, when the produced path is empty, falls back to "/". The path walk (console
// sub_82AF7400, the (value, out, mode) builder) is AptBuildPathName, homed in
// AptActionInterpreterInterpHelpers.cpp.
// ---------------------------------------------------------------------------
extern int AptBuildPathName(AptActionInterpreter* pInterp,
                                              AptValue* pValue, EAStringC* pOut, int nMode);  // sub_82AF7400 (InterpHelpers.cpp)

void AptActionInterpreter::getName(AptValue* pValue, EAStringC* pOut)
{
    *pOut = "";   // EAStringC::operator= from the empty seed (console InitFromBuffer + assign)
    AptBuildPathName(this, pValue, pOut, 1);   // the sub_82AF7400 path walk
}

void AptActionInterpreter::getName2(AptValue* pValue, EAStringC* pOut)
{
    *pOut = "";
    AptBuildPathName(this, pValue, pOut, 0);   // the sub_82AF7400 path walk
    if (pOut->GetLength() == 0)
        *pOut = "/";
}

// ---------------------------------------------------------------------------
// getObject @0x82B07F18 -- resolve the path name pName (under the (pScope,pTarget)
// context) to the AptValue object it designates: parse the path context, findChild
// the leaf, and accept it only if it is a real object (the vtbl "is object" probe).
// An empty name resolves to pScope unchanged. The path-context parser is the single
// sub_82B02F80 recon (AptResolveTargetContext, AptActionInterpreterInterpHelpers.cpp).
// ---------------------------------------------------------------------------
   // sub_82B02F80

AptValue* AptActionInterpreter::getObject(AptValue* pScope, AptValue* pTarget, const EAStringC* pName)
{
    if (pName->GetLength() == 0)
        return pScope;   // empty path -> the scope value itself (console returns the a1 it was handed)

    EAStringC  leaf;
    AptValue*  pContext = 0;
    // console sub_82B02F80(a1=scope, a2=target, a3=name, a4=&ctx, a5=&leaf): the parser
    // is a pure (scope, target) resolver -- the X360 reaches it with r3 == the SCOPE
    // value (it collapses scope == the implicit `this`), so pass pScope, not `this`.
    getContext(pScope, pTarget, pName, &pContext, leaf);

    AptValue* pResult = 0;
    if (pContext)
    {
        AptValue* pChild = pContext->findChild(&leaf, pTarget);   // findChild @0x82B01298 (AptValueFindChild.cpp)
        if (pChild && pChild->getIsDefined() && pChild->getVtblIndex() == AptVFT_Object)
            pResult = pChild;
    }
    return pResult;
}

// ---------------------------------------------------------------------------
// ARTIST 82AEA5B8. Values carrying native hashes use the class's prototype.
// The other values use a raw type comparison, with two defined-type exclusions.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

bool AptActionInterpreter::isObjectOfType(AptValue* pObject, AptValue* pClass)
{
    bool bIsOfType = false;
    if (pObject->ContainsNativeHashVirtual() && pClass->ContainsNativeHashVirtual())
    {
        AptValue* pPrototype = pClass->GetNativeHashVirtual()->mpPrototype;
        if (pObject->isCIH())
        {
            AptValue* pLink = pObject->GetNativeHashVirtual()->mp__Proto__;
            while (pLink)
            {
                if (pLink == pPrototype)
                    bIsOfType = true;
                pLink = pLink->GetNativeHashVirtual()->mp__Proto__;
            }
        }
        else if (pObject->isObject())
        {
            if (static_cast<AptObject*>(pObject)->DoesImplementObject(pPrototype))
                bIsOfType = true;
        }
        else
        {
            AptValue* pLink = pObject->GetNativeHashVirtual()->mp__Proto__;
            while (pLink)
            {
                if (pLink == pPrototype)
                {
                    bIsOfType = true;
                    return bIsOfType;
                }
                if (pLink->ContainsNativeHashVirtual())
                    pLink = pLink->GetNativeHashVirtual()->mp__Proto__;
                else
                    break;
            }
            bIsOfType = false;
        }
    }
    else if (!pObject->isScriptFunction() && !pObject->isObject()
          && pObject->getVtblIndex() == pClass->getVtblIndex())
    {
        bIsOfType = true;
    }
    return bIsOfType;
}

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// ---------------------------------------------------------------------------
// urlDecode @0x82AEE208 -- parse one "key=value" pair out of pStream (terminated by
// '&' or NUL) into (pOutKey, pOutValue), un-escaping each side. Returns the read
// position past the consumed pair (skipping a trailing '&'), or null when the pair
// has no '=' / pStream is empty. EAStringC::Append's byte-range form and the
// un-escape pass are reused via the EAStringC API; unEscape @0x82AEE110 is homed
// below as the native _unEscape codec.
// ---------------------------------------------------------------------------
// The un-escape codec -- HOMED 2026-07-02 (retiring the {} stub). The X360
// _unEscape @0x82AEE110 + _escape2Char @0x82AD90D8: '+' decodes to a space;
// a '%' with at least one following char decodes the two-char hex pair via
// strtoul(16) when the first is a hex digit (else the SECOND char passes
// through verbatim -- the shipped fallback), consuming both; everything else
// copies. The console advances two past the '%' unconditionally, tolerating a
// pair that straddles the terminator (EAStringC reps are NUL-terminated with
// slack); reproduced with the loop re-checking the cursor each iteration.
#pragma optimize("s", on)
// ARTIST calls this helper from the decoder at 0x82AEE1AC.
static __declspec(noinline) char _escape2Char(char cFirst, char cSecond)
{
    if (!isxdigit(cFirst))
        return cSecond;

    char aBuffer[3];
    aBuffer[0] = cFirst;
    aBuffer[1] = cSecond;
    aBuffer[2] = 0;
    return static_cast<char>(strtoul(aBuffer, 0, 16));
}

void _escape(AptNativeString& rString)
{
    char aEscaped[6] = "";
    const uint32_t uReserve = 3 * rString.GetLength();
    char aCharacter[2] = "";
    AptNativeString encoded;
    encoded.ReserveSize(uReserve);

    const char* pBuffer = rString.GetBuffer();
    aCharacter[1] = 0;
    for (;;)
    {
        const unsigned char c = static_cast<unsigned char>(*pBuffer++);
        if (!c)
            break;
        if (c >= 0x80 || !isalnum(c))
        {
            sprintf(aEscaped, "%%%X", c);
            encoded += aEscaped;
        }
        else
        {
            aCharacter[0] = static_cast<char>(c);
            encoded += aCharacter;
        }
    }
    rString = encoded;
}

void _unEscape(AptNativeString& rString)
{
    const uint32_t uReserve = rString.GetLength();
    char aCharacter[2] = "*";
    AptNativeString decoded;
    decoded.ReserveSize(uReserve);

    const char* pBuffer = rString.GetBuffer();
    for (;;)
    {
        const unsigned char c = static_cast<unsigned char>(*pBuffer++);
        if (!c)
            break;
        if (c == '+')
        {
            aCharacter[0] = ' ';
        }
        else if (c == '%' && *pBuffer)
        {
            aCharacter[0] = _escape2Char(pBuffer[0], pBuffer[1]);
            pBuffer += 2;
        }
        else
        {
            aCharacter[0] = static_cast<char>(c);
        }
        decoded += aCharacter;
    }
    rString = decoded;
}

#pragma optimize("", on)

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
    _unEscape(*pOutKey);
    pOutValue->Append(pEq + 1, static_cast<uint32_t>(p - (pEq + 1)));
    _unEscape(*pOutValue);
    if (*p == '&')
        ++p;
    return p;
}

// ---------------------------------------------------------------------------
// _doEnumerate @0x82B036D8 -- AS `for..in`: resolve the enumeration target (the
// stack top, or its variable when it is a name), replace the top with the null/
// undefined enumeration marker, then push each enumerable member name held in the
// target's native hash (skipping the two reserved internal keys). The key walk is
// EnumerateMembers (AptActionInterpreterInterpHelpers.cpp); the reserved-key
// sentinels (console dword_8324E580 / dword_8324E698) are the named
// StringPool::saConstant[0] / gAptKeyPrototype globals.
// ---------------------------------------------------------------------------
// AptActionInterpreter::EnumerateMembers is now a member (declared in the header);
// _doEnumerate() below forwards to it.

void AptActionInterpreter::_doEnumerate(AptValue* pScope, AptValue* pTarget)
{
    // The full body resolves the enumeration target, pushes the marker, and emits one
    // AptString per enumerable member key; the reserved-key sentinels (console
    // dword_8324E580 / dword_8324E698) are the named StringPool::saConstant[0] /
    // gAptKeyPrototype globals the walk reads.
    EnumerateMembers(pScope, pTarget);  // the native-hash enumerate walk (this member)
}

// ---------------------------------------------------------------------------
// callFunction @0x82AE3C08 -- invoke an AS callable value (function body run /
// native callback / `new` dispatch) with nArgs operands already on the stack.
// The faithful body drives AptScriptFunctionBase::SetupBeforeExecution /
// CleanupAfterExecution + runStream over the captured scope, saving/restoring the
// interpreter's current function (mpCurrentFunction) + string-constant-dictionary pair
// (mnConstantPoolCount / mConstantPool.mppEntries, which GetConstantPool installs per-function):
// CallFunctionDispatch / ExecuteScriptFunction, both homed + LIVE in
// AptActionInterpreterInterpHelpers.cpp (including the post-call abort collapse --
// Burnout_X360_Artist_01e3_0, the pop-N primitive).
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
// cbCallMethod_setInterval @0x82B019D8 -- setInterval(fn|obj, method, ms, ...args):
// claim a free AptIntervalTimer slot in the animation target's timer table, bind the
// callback (a function value, or an object + method-name pair resolved via findChild),
// store the period, append the trailing args, and return the new integer id (or
// `undefined` when the table is full or the period arg is undefined).
// cbCallMethod_clearInterval @0x82AE3AE0 -- clearInterval(id): tear down the timer
// slot whose id matches the integer argument.
//
// Both index the AptIntervalTimer slot by its setup-path fields -- now the NAMED
// AptIntervalTimer.h members mpCBFunction (+0x04) / mpContext (+0x10) / miId (+0x20).
// The slot manipulation lives with the timer table (SetIntervalImpl /
// ClearIntervalImpl, AptIntervalTimer.cpp); the entry layer here reads the args off
// the native-arg stack faithfully.
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
