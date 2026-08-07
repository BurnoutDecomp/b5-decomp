// ===========================================================================
// EATech Apt -- the ActionScript local-variable definition opcodes DefineLocal
// and DefineLocal2.   DECOMPILED from the X360 ARTIST:
//     DefineLocal  @0x82B03EB8 (0x3C) -- `var name = value`
//     DefineLocal2 @0x82B04100 (0x41) -- `var name` (declare, default undefined)
//
// Inside a running function (the interpreter holds the current AptScriptFunctionBase
// at mpCurrentFunction), the name is bound in the function's innermost local frame
// (SetInLocalScope, which lazily creates the frame). At the top level (no current
// function) it falls back to setVariable in the timeline scope. DefineLocal2 only
// declares -- it leaves an existing binding untouched.
//
// Wired to the AptScriptFunctionBase keystone (SetInLocalScope / ExistsInLocalScope,
// public; they delegate to the static frame stack). gpUndefinedValue is the homed
// AptGlobals.cpp singleton (built at AptInit).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"   // mpCurrentFunction->SetInLocalScope/ExistsInLocalScope
#include "SDKs/EATech/include/Apt/AptCIH.h"                  // ctx->mpCIH -> AptValue* upcast

extern AptValue* gpUndefinedValue;   // off_8324D814 (AptGlobals.cpp; wired at AptInit)

// ---------------------------------------------------------------------------
// DefineLocal @0x82B03EB8 (0x3C) -- `var name = value`. Stack: [name, value].
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDefineLocal(AptActionInterpreter* pInterp,
                                                         LocalContextT* pContext)
{
    AptValue* pValue = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pName  = pInterp->mpStack[pInterp->mnStackTop - 2];

    EAStringC scratch;
    const EAStringC* pKey = AptValue::Get_ToString(pName, &scratch);

    if (pInterp->mpCurrentFunction)
        pInterp->mpCurrentFunction->SetInLocalScope(*pKey, pValue);
    else
        pInterp->setVariable(pContext->mpCIH, pContext->mpPendingReleaseValue, pKey, pValue, 0, 1, 0);

    pInterp->stackPop(2);
}

// ---------------------------------------------------------------------------
// DefineLocal2 @0x82B04100 (0x41) -- `var name` (declare only). Stack: [name].
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDefineLocal2(AptActionInterpreter* pInterp,
                                                          LocalContextT* pContext)
{
    AptValue* pName = pInterp->mpStack[pInterp->mnStackTop - 1];

    EAStringC scratch;
    const EAStringC* pKey = AptValue::Get_ToString(pName, &scratch);

    if (pInterp->mpCurrentFunction)
    {
        if (!pInterp->mpCurrentFunction->ExistsInLocalScope(*pKey))
            pInterp->mpCurrentFunction->SetInLocalScope(*pKey, gpUndefinedValue);
    }
    else if (!pInterp->getVariable(pContext->mpCIH, pContext->mpPendingReleaseValue, pKey, 0, 1, 0)->getIsDefined())
    {
        pInterp->setVariable(pContext->mpCIH, pContext->mpPendingReleaseValue, pKey, gpUndefinedValue, 0, 1, 0);
    }

    pInterp->stackPop();
}
