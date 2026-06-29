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
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"    // AptString::Create

// FLAG (homed by the apt VM native-call dispatch): the global native-method arg
// stack (X360 off_8324E768 = gAptActionInterpreter.mpStack, dword_8324E760 = its
// mnStackTop). The i-th AS argument (i=0 = last pushed) is
// gppAptNativeArgStack[gnAptNativeArgCount - 1 - i].
extern AptValue** gppAptNativeArgStack;   // off_8324E768
extern int        gnAptNativeArgCount;    // dword_8324E760

// FLAG (homed by the AS-globals layer): the shared "undefined" value (off_8324D814).
extern AptValue* gpUndefinedValue;

// FLAG (value-layer follow-ons; bodies own their TUs). isNaN already has a committed
// extern in AptActionInterpreterStringOps2.cpp; escape/unescape are the X360 _escape
// @0x82AEE008 / _unEscape @0x82AEE110 (percent-encode / decode an EAStringC in place).
extern bool isNaN(AptValue* pValue);
extern void escape(EAStringC* pString);
extern void unescape(EAStringC* pString);

// FLAG (wired at AptInit; committed externs in the sibling AptActionInterpreter*Ops TUs):
// the running .swf version (only v7 makes a non-empty string coerce to boolean true).
extern unsigned int AptGetSwfVersion();

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
            escape(&strArg);           // percent-encode in place
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
        unescape(&strArg);         // percent-decode in place
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
