// ===========================================================================
// EATech Apt -- AptActionInterpreter::setVariable, the ActionScript variable/
// property WRITE side (getVariable's counterpart).   DECOMPILED from the X360
// ARTIST @0x82B03048.
//
// Resolve the path (getContext, or use the scope directly), then store the value:
//   1. the object's member set (objectMemberSet, vtbl index 7) for a defined object
//      (handles registered class members / AS setters);
//   2. the function scope chain (the frame stack) when scope-chain search is on;
//   3. the context's own native hash (Set + UpdateObjectMethods to maintain the
//      event-handler mask), re-targeting to the scope's hash for a clear-on-absent;
//   4. otherwise the frame-context / frame-stack store.
// bRemoving = the value is null or undefined (clearing the member).
//
// FLAG -- deferred to the value/frame layer (Adriwin's class:Apt* TUs) + AptInit:
//   the scope-chain store (AptFrameStack::SetWhereExistsInScopeChain) and the
//   frame-context fallback (the interpreter frame context + AptFrameStack::
//   CreateFrameStack + the type-12/37 CIH paths) are encapsulated in the
//   AptInterp_Set* helpers; the _global object's __proto__ reset (dword_8324D830 /
//   Set__Proto__) for a function/class value is noted but deferred. The faithful
//   common paths (member set + the context native-hash store) are reconstructed
//   here; getContext is the (reconstructed) path parser.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

// FLAG (AptFrameStack -- Adriwin's class:AptFrameStack TU; wired at AptInit): store
// the value where the name already exists in the function scope chain. Returns true
// if stored. Encapsulates the frame-stack + interpreter frame-context access.
// AptActionInterpreter::SetInScopeChain is now a member (declared in the header).
// FLAG: the frame-context fallback store (the no-scope-chain / type-12-37 / CIH
// paths -- the interpreter frame context + AptFrameStack::CreateFrameStack).
// AptActionInterpreter::SetVariableFallback is now a member (declared in the header).

#if defined(_MSC_VER)
extern "C" void AptSetVariableProbe(const char* pcName, const void* pContext, int nContextType);
#pragma comment(linker, "/alternatename:AptSetVariableProbe=AptSetVariableProbeDefault")
extern "C" void AptSetVariableProbeDefault(const char*, const void*, int) {}
#else
extern "C" void AptSetVariableProbe(const char* pcName, const void* pContext, int nContextType);
#endif

int AptActionInterpreter::setVariable(AptValue* pScope, AptValue* pTarget,
    const EAStringC* pName, AptValue* pValue, int nAllowScopeChain, int nSearchScopeChain, int nDirect)
{
    EAStringC name;            // the resolved leaf name (v43)
    AptValue* pContext = pScope;

    if (nDirect)
    {
        pContext = pScope;
        name     = *pName;
    }
    else
    {
        AptValue* pOutContext = 0;
        getContext(pScope, pTarget, pName, &pOutContext, &name);
        pContext = pOutContext;
    }

    if (!pContext)
        return 0;

    // FLAG (bring-up probe; weak sink pattern): trace the store names + routes so
    // the framework-bootstrap class definitions can be followed.
    AptSetVariableProbe(name.GetBuffer(), pContext,
                        static_cast<int>(pContext->getVtblIndex()) * 1000
                        + (pValue ? static_cast<int>(pValue->getVtblIndex()) : -1));

    const bool bRemoving = (!pValue || !pValue->getIsDefined());

    // 1) the context object's member set (registered members / AS setters).
    if (pContext->getIsDefined() && pContext->objectMemberSet(pContext, &name, pValue))
        return 1;

    if (nAllowScopeChain)
    {
        // 2) store where the name lives in the function scope chain.
        if (nSearchScopeChain && SetInScopeChain(&name, pValue))   // FLAG: AptFrameStack
            return 1;

        // 3) the context's own native hash.
        AptNativeHash* pHash = pContext->GetNativeHashVirtual();
        if (pContext == pTarget && !pValue && pHash && !pHash->Lookup(name))
            pHash = pScope->GetNativeHashVirtual();   // clear-on-absent -> the scope's hash
        if (pHash)
        {
            pHash->Set(name, pValue);
            pHash->UpdateObjectMethods(pContext, &name, bRemoving);
            // FLAG: console also resets the _global object's __proto__ for a
            // function/class value here (dword_8324D830 / Set__Proto__) -- deferred.
            return 1;
        }

        // FLAG: CIH (type 12/37) + frame-context hash fallback (AptFrameStack layer).
        return SetVariableFallback(pContext, &name, pValue, nDirect) ? 1 : 0;
    }

    // No scope-chain search: store in the context's native hash directly. (FLAG: the
    // console routes to the frame stack when an interpreter frame context exists --
    // that branch is the AptFrameStack-layer follow-on.)
    AptNativeHash* pHash = pContext->GetNativeHashVirtual();
    if (pHash)
    {
        pHash->Set(name, pValue);
        pHash->UpdateObjectMethods(pContext, &name, bRemoving);
    }
    return 1;
}
