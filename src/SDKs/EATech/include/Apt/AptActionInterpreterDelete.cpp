// ===========================================================================
// EATech Apt -- the ActionScript Delete2 opcode (`delete name`).
//   DECOMPILED from the X360 ARTIST: _FunctionAptActionDelete2 @0x82B03DF0 (0x3B).
//
// Coerce the stack top to a variable name, then remove it by storing a null value
// through setVariable (null/undefined => the "clear" path setVariable already
// handles -- bRemoving), and push AptInteger(1) (delete's truthy result). The run
// scope/target come from the execution context (mpCIH + the target slot).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC
#include "SDKs/EATech/include/Apt/AptCIH.h"                // mpCIH -> AptValue* upcast

// ---------------------------------------------------------------------------
// Delete2 @0x82B03DF0 (0x3B) -- AS `delete name`: clear the variable, push true.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDelete2(AptActionInterpreter* pInterp,
                                                     LocalContextT* pContext)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];   // the variable name
    EAStringC scratch;
    const EAStringC* pName = AptValue::Get_ToString(pTop, &scratch);

    // value = null -> setVariable's clear-on-absent path removes the binding.
    pInterp->setVariable(pContext->mpCIH, pContext->mpPendingReleaseValue, pName, 0, 1, 1, 0);

    pInterp->stackPop();                                          // pop the name
    AptInteger* pResult = AptInteger::Create(1);                  // delete -> true
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;            // inlined stackPush
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// Delete @PS3 0x81DF90 (0x3A) -- AS `delete object.name`. Stack: [object, name]
// (name on top). When the target OWNS a native hash (vtbl slot 3,
// ContainsNativeHashVirtual), render the name and clear the binding on the target
// (setVariable with the target as the scope and a null value); then pop both
// operands (releasing each, the console's inline 2-pop) and push `true`.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDelete(AptActionInterpreter* pInterp,
                                                    LocalContextT* pContext)
{
    AptValue* pName   = pInterp->mpStack[pInterp->mnStackTop - 1];   // v5 (top)
    AptValue* pObject = pInterp->mpStack[pInterp->mnStackTop - 2];   // v6 (under)

    if (pObject->ContainsNativeHashVirtual())                        // console vtbl+0xC
    {
        EAStringC scratch;
        const EAStringC* pStr = AptValue::Get_ToString(pName, &scratch);
        pInterp->setVariable(pObject, pContext->mpPendingReleaseValue, pStr, 0, 1, 1, 0);
    }

    pInterp->stackPop(2);                                            // release name + object
    AptInteger* pResult = AptInteger::Create(1);                     // delete -> true
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;               // inlined stackPush
    pResult->AddRef();
}
