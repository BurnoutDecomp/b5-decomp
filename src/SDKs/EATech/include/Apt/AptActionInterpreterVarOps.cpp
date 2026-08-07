// ===========================================================================
// EATech Apt -- the ActionScript variable opcodes GetVariable (0x1C) and
// SetVariable (0x1D), plus AptValue::Get_ToString (their name-coercion helper).
//   DECOMPILED from the X360 ARTIST: GetVariable @0x82B038A0, SetVariable
//   @0x82B03970, Get_ToString @0x82AD8558.
//
// These are the two halves of plain ActionScript variable access -- `name` and
// `name = value`. Each coerces the operand-stack name to an EAStringC
// (Get_ToString) and delegates to the (reconstructed) resolver: getVariable for
// the read, setVariable for the write. The run's scope + target come from the
// execution context: mpCIH (the movie-clip scope) and mpPendingReleaseValue (the
// per-run target slot, normally null during straight-line execution -- runStream
// binds the run's "this" separately into mpScopeVariable). This matches the
// console, where both handlers load ctx+4 and ctx+8 for getVariable/setVariable.
//
// Get_ToString: a string-typed value (meValueType 1 = AptVFT_StringValue, or the
// interned/handle string tag 33) hands back its embedded EAStringC directly (no
// allocation); any other value is rendered into the caller's scratch via toString.
//
// All former deferrals are landed: AptValue::toString (AptValueConvert.cpp
// @0x82AF8FA0) backs the non-string Get_ToString path; the tag-33 boxed string is
// the named AptStringObject::GetBoxedString member; the deferred-release drain is
// the homed AptFlushDeferredReleases over gValuesToRelease (AptGC.cpp,
// off_8324E51C). The stack-empty guard that triggers it is engine-side.
//
// The member opcodes GetMember/SetMember (0x4E/0x4F) are the follow-on: they add
// the array / string / __proto__ fast paths and lean on the value-layer type tags
// 33/37 + AptArray::set + the AptString char accessors.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"       // AptString::GetInternalString
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h" // the tag-33 boxed-string form
#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC
#include "SDKs/EATech/include/Apt/AptCIH.h"               // AptCIH : AptValueGC (mpCIH -> AptValue* upcast)

// The deferred-release drain over off_8324E51C (`if (vector.count)
// vector.ReleaseValues()`) -- homed in AptGC.cpp over the real gValuesToRelease
// vector; SetVariable's stack-empty guard decides when to call it.
extern void AptFlushDeferredReleases();

// ---------------------------------------------------------------------------
// AptValue::Get_ToString @0x82AD8558 -- coerce a value to an EAStringC name.
// A string-typed value returns its own embedded EAStringC (no allocation); any
// other value is rendered into pScratch via toString. Homed here with its first
// callers (the variable opcodes); also used by the member opcodes + others.
// ---------------------------------------------------------------------------
const EAStringC* AptValue::Get_ToString(AptValue* pValue, EAStringC* pScratch)
{
    const AptVirtualFunctionTable_Indices eType = pValue->getVtblIndex();

    // String values expose their EAStringC directly -- no conversion. Type 1 is
    // the AptString itself; tag 33 boxes the AptString behind a pointer at +0x20.
    if ((eType == AptVFT_StringValue || eType == AptVFT_StringObject)
        && pValue->getIsDefined())
    {
        AptString* pString = (eType == AptVFT_StringValue)
            ? reinterpret_cast<AptString*>(pValue)
            : static_cast<AptString*>(
                  static_cast<AptStringObject*>(pValue)->GetBoxedString());   // tag 33: mpValue [c:+0x20]
        return pString->GetInternalString();
    }

    pValue->toString(pScratch);   // homed renderer (AptValueConvert.cpp @0x82AF8FA0)
    return pScratch;
}

// ---------------------------------------------------------------------------
// _FunctionAptActionGetVariable @0x82B038A0 -- opcode 0x1C, AS `name` read.
// The operand-stack top holds the variable name; resolve it through getVariable
// and replace it with the result. An undefined name is a no-op (it stays on the
// stack as the result), matching the console's early return.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGetVariable(AptActionInterpreter* pInterp,
                                                         LocalContextT* pContext)
{
    AptValue* pNameValue = pInterp->mpStack[pInterp->mnStackTop - 1];   // TOS = the name
    if (!pNameValue->getIsDefined())
        return;

    EAStringC name;                                                     // scratch (empty sentinel)
    const EAStringC* pName = AptValue::Get_ToString(pNameValue, &name);
    AptValue* pResult = pInterp->getVariable(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                             pName, 1, 1, 0);

    pInterp->stackPop();                                  // pop the name
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;    // push the result (inlined stackPush)
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionSetVariable @0x82B03970 -- opcode 0x1D, AS `name = value`.
// The stack holds [name, value] (value on top); coerce the name and store the
// value through setVariable, then pop both. When the pop empties the operand
// stack, drain the deferred-release vector (GC housekeeping).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionSetVariable(AptActionInterpreter* pInterp,
                                                         LocalContextT* pContext)
{
    AptValue* pNameValue = pInterp->mpStack[pInterp->mnStackTop - 2];   // under-top = the name
    AptValue* pValue     = pInterp->mpStack[pInterp->mnStackTop - 1];   // top = the value to store

    EAStringC name;
    const EAStringC* pName = AptValue::Get_ToString(pNameValue, &name);
    pInterp->setVariable(pContext->mpCIH, pContext->mpPendingReleaseValue,
                         pName, pValue, 1, 1, 0);

    pInterp->stackPop(2);                                 // pop name + value

    if (pInterp->mnStackTop == 0)
        AptFlushDeferredReleases();   // off_8324E51C drain (homed, AptGC.cpp)
}
