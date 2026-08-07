// ===========================================================================
// EATech Apt -- AptActionInterpreter ActionScript global builtins + a value opcode.
//
// Reconstructed from the X360 ARTIST.XEX pseudocode/asm via the decompile->verify
// workflow. The cbCallMethod_* are AS global functions (escape/unescape/isNaN/...)
// the CallMethod dispatch invokes as f(thisValue, argCount), reading their args off
// the global native-arg stack; _FunctionAptActionAsciiToChar is the chr() bytecode
// opcode (operates on the interpreter operand stack).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"

#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"   // AptBoolean::Create (shared true/false)
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"        // the isNaN string arm
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"  // the boxed string form
#include <cctype>    // isdigit (the isNaN scan)
#include <cstdlib>   // strtol (the isNaN hex arm)
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"    // AptString::Create / c_string / GetInternalString
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC (the URL/target scratch + operands)
#include "SDKs/EATech/include/Apt/AptCIH.h"                // AptCIH : AptValueGC (mpCIH -> AptValue* upcast)
#include "SDKs/EATech/include/Apt/AptGlobal.h"             // gpAptGlobalFallback (off_8324E380 sentinel)
#include "SDKs/EATech/include/Apt/AptObject.h"             // AptValueWithHash (gpAptGlobalFallback upcast)
#include "SDKs/EATech/include/Apt/AptTarget.h"             // gpAptTarget->mpLinker (off_8324E574+0x20)
#include "SDKs/EATech/include/Apt/AptLinker.h"             // AptLinker::Load (the getURL .swf arm)

#include <cstdint>   // uintptr_t

// The global native-method arg stack (X360 off_8324E768 = gAptActionInterpreter.
// mpStack, dword_8324E760 = its mnStackTop). Storage is defined in AptGlobals.cpp;
// the native-call dispatch (AptActionInterpreterInterpHelpers.cpp) publishes the
// operand-stack window into the pair around each callback. The i-th AS argument
// (i=0 = last pushed) is gppAptNativeArgStack[gnAptNativeArgCount - 1 - i].
extern AptValue** gppAptNativeArgStack;   // off_8324E768
extern int        gnAptNativeArgCount;    // dword_8324E760

// The shared "undefined" value (off_8324D814) -- defined in AptGlobals.cpp, built
// at AptInit.
extern AptValue* gpUndefinedValue;

// The escape/unescape codecs are the homed members AptActionInterpreter::escape
// @0x82AEE008 / ::unEscape @0x82AEE110 (percent-encode / decode an EAStringC in
// place; AptActionInterpreter.cpp).

// The running .swf version (only v7 makes a non-empty string coerce to boolean
// true) -- homed in AptLinker.cpp (the dword_8324E530 SWF-version cache parsed from
// the .apt header at first link).
extern unsigned int AptGetSwfVersion();

// AptActionInterpreter::loadVariables @0x82B07DF8 (the AS loadVariables core) is a
// homed member (AptActionInterpreterInterpHelpers.cpp); the GetUrl2 loadVariables
// branch calls it with the console (node, mpPendingReleaseValue, &url) shape.

// ---------------------------------------------------------------------------
// isNaN (the value test) @0x82AF9768 -- HOMED 2026-07-02, retiring the
// return-false stub. The full ECMA-ish Not-a-Number classification:
//   * a DEFINED Integer (7) or Float (6) is a number -> false;
//   * a DEFINED string (1/33): empty -> NaN; "0x..." parses via strtol(16)
//     (NaN when the parse leaves a tail); otherwise the LAST char must be a
//     digit or one of -+e. and the FIRST a digit or .-+ (else NaN), then the
//     body scan from index 1: a second '.' falls through the 'e' compare into
//     isdigit('.') == NaN (the shipped trick); an 'e' at index 1, or at index
//     2 behind a leading sign, is NaN; a sign is only consumed immediately
//     after 'e'; any other non-digit is NaN; a clean scan -> a number;
//   * any other DEFINED non-Boolean value -> NaN;
//   * undefined / Boolean -> NaN exactly when the movie is SWF7
//     (AptGetSwfVersion() == 7 -- the SWF7 undefined-coercion change).
// ---------------------------------------------------------------------------
bool isNaN(AptValue* pValue)
{
    const AptVirtualFunctionTable_Indices eType = pValue->getVtblIndex();
    const bool bDefined = pValue->getIsDefined();

    if ((eType == AptVFT_Integer || eType == AptVFT_Float) && bDefined)
        return false;

    if ((eType == AptVFT_StringValue || eType == AptVFT_StringObject) && bDefined)
    {
        AptString* const pStr =
            (eType == AptVFT_StringValue)
                ? static_cast<AptString*>(pValue)
                : static_cast<AptString*>(
                      static_cast<AptStringObject*>(pValue)->GetBoxedString());
        const EAStringC* const pS = pStr->GetInternalString();
        const char* const p = pS->GetBuffer();
        const int n = pS->GetLength();

        if (n == 0)
            return true;

        if (n > 2 && p[0] == '0' && p[1] == 'x')
        {
            char* pEnd = nullptr;
            (void)strtol(p, &pEnd, 16);
            return *pEnd != 0;
        }

        const unsigned char cLast = static_cast<unsigned char>(p[n - 1]);
        if (cLast != '-' && cLast != '+' && cLast != 'e' && cLast != '.'
            && !isdigit(cLast))
            return true;
        const unsigned char cFirst = static_cast<unsigned char>(p[0]);
        if (cFirst != '.' && cFirst != '-' && cFirst != '+' && !isdigit(cFirst))
            return true;

        bool bDotSeen = false;
        for (int i = 1; i < n; ++i)
        {
            const char c = p[i];
            if (c == '.' && !bDotSeen)
            {
                bDotSeen = true;   // the first dot is fine; a second falls through
                continue;          // (the shipped code re-enters the 'e' compare,
            }                      // where isdigit('.') == 0 classifies it NaN)
            if (c == 'e' && i != 1)
            {
                if (i == 2 && (p[0] == '+' || p[0] == '-'))
                    return true;   // "e" right behind a bare sign
                const int nNext = i + 1;
                if (nNext < n)
                {
                    const char c2 = p[nNext];
                    if (c2 == '-' || c2 == '+')
                    {
                        ++i;       // consume the exponent sign
                        continue;
                    }
                    if (!isdigit(static_cast<unsigned char>(c2)))
                        return true;
                }
                continue;
            }
            if (!isdigit(static_cast<unsigned char>(c)))
                return true;
        }
        return false;
    }

    if (bDefined && eType != AptVFT_Boolean)
        return true;

    return AptGetSwfVersion() == 7;   // dword_8324E530 == 7 (the SWF7 arm)
}

// ---------------------------------------------------------------------------
// cbCallMethod_isNaN @0x82AF99E8 -- AS isNaN(x): true with no argument, or when the
// single argument is Not-a-Number. Returns the shared true/false singleton.
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::cbCallMethod_isNaN(AptValue* /*pThis*/, int nArgCount)
{
    if (nArgCount == 0)
        return AptBoolean::Create(true);

    AptValue* pArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];
    return AptBoolean::Create(isNaN(pArg));
}

// ---------------------------------------------------------------------------
// cbCallMethod_boolean @0x82AF9BD8 -- AS Boolean(x): coerce the single argument to a
// boolean (returns the shared true/false singleton). No argument -> undefined.
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::cbCallMethod_boolean(AptValue* /*pThis*/, int nArgCount)
{
    if (nArgCount == 0)
        return gpUndefinedValue;

    AptValue* pArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];
    const AptVirtualFunctionTable_Indices eType = pArg->getVtblIndex();

    // A defined movie-clip handle (or a CIHNone), or a defined Object, is always true.
    if ((eType == AptVFT_CharacterInstHandle && pArg->getIsDefined())
        || eType == AptVFT_CIHNone
        || (eType == AptVFT_Object && pArg->getIsDefined()))
        return AptBoolean::Create(true);

    // The shared `undefined` value is false.
    if (pArg == gpUndefinedValue)
        return AptBoolean::Create(false);

    // A defined number -- or any other value whose numeric coercion is well-defined
    // (not NaN) -- is true iff that number is non-zero.
    const bool bNumber = (eType == AptVFT_Float || eType == AptVFT_Integer) && pArg->getIsDefined();
    if (bNumber || !isNaN(pArg))
        return AptBoolean::Create(pArg->toFloat() != 0.0f);

    // NaN bucket: a defined Boolean returns its own value.
    if (eType == AptVFT_Boolean && pArg->getIsDefined())
        return AptBoolean::Create(pArg->c_boolean()->GetBool());

    // Under SWF v7 a defined, non-empty string is true; anything else is false.
    const bool bString = (eType == AptVFT_StringValue || eType == AptVFT_StringObject)
                      && pArg->getIsDefined();
    if (!bString || AptGetSwfVersion() != 7)
        return AptBoolean::Create(false);

    // c_string() unwraps a boxed StringObject to its primitive String (identity for a
    // raw StringValue); a non-empty embedded EAStringC coerces to true.
    const bool bEmpty = pArg->c_string()->GetInternalString()->IsEmpty();
    return AptBoolean::Create(!bEmpty);
}

// ---------------------------------------------------------------------------
// cbCallMethod_escape @0x82AF9B08 -- AS escape(s): URL percent-encode the single
// string argument into a fresh AptString (no/non-string argument -> empty string).
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::cbCallMethod_escape(AptValue* /*pThis*/, int nArgCount)
{
    AptString* pResult = AptString::Create("");
    if (nArgCount != 0)
    {
        AptValue* pArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];
        const AptVirtualFunctionTable_Indices eType = pArg->getVtblIndex();
        const bool bString = (eType == AptVFT_StringValue || eType == AptVFT_StringObject)
                          && pArg->getIsDefined();
        if (bString)
        {
            EAStringC strArg;
            pArg->toString(&strArg);   // render the argument to text
            AptActionInterpreter::escape(&strArg);   // percent-encode in place (@0x82AEE008)
            *pResult->GetInternalString() += strArg;
        }
    }
    return pResult;
}

// ---------------------------------------------------------------------------
// cbCallMethod_unescape @0x82AF9A50 -- AS unescape(s): URL percent-decode the single
// string argument into a fresh AptString. (The X360 reads the argument
// unconditionally -- no arg-count guard -- matched here.)
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::cbCallMethod_unescape(AptValue* /*pThis*/, int /*nArgCount*/)
{
    AptString* pResult = AptString::Create("");

    AptValue* pArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];
    const AptVirtualFunctionTable_Indices eType = pArg->getVtblIndex();
    const bool bString = (eType == AptVFT_StringValue || eType == AptVFT_StringObject)
                      && pArg->getIsDefined();
    if (bString)
    {
        EAStringC strArg;
        pArg->toString(&strArg);   // render the argument to text
        AptActionInterpreter::unEscape(&strArg);   // percent-decode in place (@0x82AEE110)
        *pResult->GetInternalString() = strArg;
    }
    return pResult;
}

// ---------------------------------------------------------------------------
// _FunctionAptActionAsciiToChar @0x82AF3C18 -- AS chr(code): replace the top operand
// with a one-character string whose single byte is the operand's integer character
// code. An undefined operand yields the shared `undefined` value instead.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionAsciiToChar(AptActionInterpreter* pInterp,
                                                         LocalContextT* /*pContext*/)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];

    AptValue* pResult;
    if (pTop->getIsDefined())
    {
        AptString* pStr = AptString::Create("");
        const int nCharCode = pTop->toInteger();
        EAStringC strChar(static_cast<uint32_t>(nCharCode), 1u);   // one byte = the char code
        *pStr->GetInternalString() = strChar;
        pResult = pStr;
    }
    else
    {
        pResult = gpUndefinedValue;
    }

    pInterp->stackPop();                                  // pop the operand (Release)
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;    // push the result
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// The getURL/getURL2 ".swf" arm loads through the SAME homed member the loadMovie
// native uses: AptLinker::Load(EAStringC* pName, EAStringC* pTarget) @0x82B06660
// (AptLinker.h -- the signature was long since corrected to two string pointers;
// the interim AptLinkerGetUrlLoad forwarder and its {} stub, which dropped every
// getURL movie load, were retired 2026-07-10).

// ---------------------------------------------------------------------------
// _FunctionAptActionGetUrl @0x82B09300 -- AS getURL(url, target): the SWF1 form
// with the url + target stored as two inline (4-byte-aligned) string operands in
// the action bytecode. "FSCommand:"-prefixed urls are routed to the host hook;
// otherwise, when the url ends in ".swf" (suffix stripped in place) or is empty,
// the linker loads it into the target level.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGetUrl(AptActionInterpreter* pInterp,
                                                    LocalContextT* pContext)
{
    // Two inline string-pointer operands at the next aligned position; the PC then
    // advances past both. Console operands are 4-byte; the x64 stream is the
    // _parseStream transcode of the serialized .apt action stream and stores native
    // 8-byte pointers (phase-0 native-8 regime) -- the same x64-native PC model as
    // the branch ops.
    const char* const* pOperands = reinterpret_cast<const char* const*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));   // 8-aligned (GUIAPT64)
    pContext->mpProgramCounter = reinterpret_cast<const unsigned char*>(pOperands + 2);

    const char* pUrl    = pOperands[0];
    const char* pTarget = pOperands[1];

    if (pInterp->isFSCommand(pUrl))
    {
        pInterp->doFSCommand(pUrl, pTarget);
        return;
    }

    // Build a scoped EAStringC from the url buffer (the X360 InitFromBuffer; the dtor
    // is the trailing DecreaseInternalRefCount). A url that ends in ".swf" (suffix
    // removed) or that is empty triggers the linker load into the target level.
    EAStringC strUrl(pUrl);
    const bool bSwf = strUrl.EndWithRemoveIgnoreCase(".swf");
    if (bSwf || strUrl.IsEmpty())
    {
        EAStringC strTarget(pTarget);
        gpAptTarget->mpLinker->Load(&strUrl, &strTarget);   // AptLinker::Load (the loadMovie member)
    }
}

// ---------------------------------------------------------------------------
// _FunctionAptActionGetUrl2 @0x82B09588 -- AS getURL2: the operands come off the
// operand stack ([..., url, target] with target on top). It supports the
// "FSCommand:" host hook, a loadVariables form (when the url is a variable
// reference that does not resolve to a ".swf"), and the plain linker load. The two
// operands are popped at the end (stackPop(2)) and the scratch strings released.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGetUrl2(AptActionInterpreter* pInterp,
                                                     LocalContextT* pContext)
{
    AptValue* pUrlValue    = pInterp->mpStack[pInterp->mnStackTop - 2];   // under-top = the url
    AptValue* pTargetValue = pInterp->mpStack[pInterp->mnStackTop - 1];   // top = the target

    EAStringC strTarget;   // v28 -- rendered lazily on the non-FSCommand paths
    EAStringC strUrl;      // v29 -- the url string form
    pUrlValue->toString(&strUrl);

    // "FSCommand:"-prefixed url -> dispatch (url, target) to the host hook, pop, done.
    if (pInterp->isFSCommand(strUrl.GetBuffer()))
    {
        pTargetValue->toString(&strTarget);
        pInterp->doFSCommand(strUrl.GetBuffer(), strTarget.GetBuffer());
        pInterp->stackPop(2);
        return;
    }

    // A non-empty url that does NOT end in ".swf" (suffix stripped in place) is the
    // loadVariables form: the target value names the destination variable scope.
    if (!strUrl.IsEmpty() && !strUrl.EndWithRemoveIgnoreCase(".swf"))
    {
        const AptVirtualFunctionTable_Indices eTargetType = pTargetValue->getVtblIndex();
        const bool bStringTarget =
            (eTargetType == AptVFT_StringValue || eTargetType == AptVFT_StringObject)
            && pTargetValue->getIsDefined();

        AptValue* pNode;
        if (bStringTarget)
        {
            // A string target is a variable path: resolve it to the destination node.
            // c_string() unboxes the StringObject (type 33) form; GetInternalString()
            // is the embedded EAStringC the console reads at the value's +8.
            pNode = pInterp->getVariable(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                         pTargetValue->c_string()->GetInternalString(),
                                         1, 1, 0);
        }
        else
        {
            pNode = pTargetValue;
        }

        // Drive the AS loadVariables core (@0x82B07DF8, the homed member in
        // AptActionInterpreterInterpHelpers.cpp) -- the same core the loadVariables
        // native method (AptCIHNativeFunctionHelper) drives.
        pInterp->loadVariables(pNode, pContext->mpPendingReleaseValue, &strUrl);
        pInterp->stackPop(2);
        return;
    }

    // Plain ".swf"/empty load: resolve the target value to a name, then linker-load
    // the url into it.
    pTargetValue->toString(&strTarget);
    AptValue* pResolved = pInterp->getVariable(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                               &strTarget, 1, 1, 0);

    // A defined CIH-handle (type 12) or a CIH-none (type 37) target gets its full
    // slash/dot path name written back into strTarget before the load.
    const AptVirtualFunctionTable_Indices eResolvedType = pResolved->getVtblIndex();
    if ((eResolvedType == AptVFT_CharacterInstHandle && pResolved->getIsDefined())
        || eResolvedType == AptVFT_CIHNone)
    {
        pInterp->getName(pResolved, &strTarget);
    }

    pInterp->stackPop(2);
    gpAptTarget->mpLinker->Load(&strUrl, &strTarget);   // AptLinker::Load (the loadMovie member)
}

// ---------------------------------------------------------------------------
// _FunctionAptActionTraceStart @0x82ADE688 -- the EA "trace target" opcode: pop the
// target operand (resolving the _global fallback indirection), and when it is a
// defined integer, pop that many further operands off the stack (the trace argument
// list). The PC then advances past the inline count operand -- by the count itself
// when trace-bytecode skipping is enabled (mbSkipTraceBytecodes), else by one word.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionTraceStart(AptActionInterpreter* pInterp,
                                                        LocalContextT* pContext)
{
    // The inline count operand the PC points at (read as a raw word, like the console).
    const int32_t nInlineCount = *reinterpret_cast<const int32_t*>(pContext->mpProgramCounter);

    // Take the current top operand as the trace target, popping it if present.
    AptValue* pTarget = pInterp->mpStack[pInterp->mnStackTop - 1];
    if (pInterp->mnStackTop >= 1)
        pInterp->stackPop();

    // A `_global` target resolves through to the next operand below it.
    if (pTarget == static_cast<AptValue*>(gpAptGlobalFallback))
    {
        pTarget = pInterp->mpStack[pInterp->mnStackTop - 1];
        if (pInterp->mnStackTop >= 1)
            pInterp->stackPop();
    }

    // A defined integer target -> pop that many further operands (its value at +8 is
    // the integer datum the console reads).
    if (pTarget->getVtblIndex() == AptVFT_Integer && pTarget->getIsDefined())
        pInterp->stackPop(pTarget->toInteger());

    // Advance the PC: by the inline count when trace-bytecode skipping is on, else by
    // a single (word-aligned) operand slot.
    if (pInterp->mbSkipTraceBytecodes)
        pContext->mpProgramCounter += nInlineCount;
    else
        pContext->mpProgramCounter += 4;
}
