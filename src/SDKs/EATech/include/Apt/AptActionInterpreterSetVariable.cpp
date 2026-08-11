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
// All layers are landed: the scope-chain store (SetInScopeChain -> AptFrameStack::
// SetWhereExistsInScopeChain) and the node/frame-context fallback
// (SetVariableFallback) are homed members in AptActionInterpreterInterpHelpers.cpp;
// getContext is the homed path parser (AptActionInterpreterContext.cpp). The
// root-target-MC prototype sever (dword_8324D830), the shape-instance store bail
// and the no-scope-chain frame-stack store are reconstructed below from @0x82B03048.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptCIH.h"            // AptCIH::GetCharacterInst (the shape-inst bail)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"  // GetTypeTag (shape == 1)
#include "SDKs/EATech/include/Apt/AptFrameStack.h"     // the active local-variable frame (no-scope-chain store)

extern AptValue* gpAptRootTargetMC;   // dword_8324D830 (AptGlobals.cpp; the root target MC)

// SetInScopeChain (the scope-chain store) and SetVariableFallback (the node/frame-
// context fallback) are homed members in AptActionInterpreterInterpHelpers.cpp.

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

    // A CIH (12, defined) / CIHNone (37) context whose character instance is a
    // SHAPE (type tag 1) refuses the store outright -- console @0x82B03048 bails
    // with 0 on (inst->mTypeFlags tag) == 1 before the member-set probe.
    const AptVirtualFunctionTable_Indices eCtx = pContext->getVtblIndex();
    const bool bNodeContext =
        (eCtx == AptVFT_CharacterInstHandle && pContext->getIsDefined()) || eCtx == AptVFT_CIHNone;
    if (bNodeContext)
    {
        const AptCharacterInst* const pInst = static_cast<AptCIH*>(pContext)->GetCharacterInst();
        if (pInst && pInst->GetTypeTag() == 1)   // shape inst -> not a variable target
            return 0;
    }

    const bool bRemoving = (!pValue || !pValue->getIsDefined());

    // 1) the context object's member set (registered members / AS setters).
    if (pContext->getIsDefined() && pContext->objectMemberSet(pContext, &name, pValue))
        return 1;

    if (nAllowScopeChain)
    {
        // 2) store where the name lives in the function scope chain.
        if (nSearchScopeChain && SetInScopeChain(&name, pValue))   // homed member (InterpHelpers.cpp)
            return 1;

        // 3) the context's own native hash.
        AptNativeHash* pHash = pContext->GetNativeHashVirtual();
        if (pContext == pTarget && !pValue && pHash && !pHash->Lookup(name))
            pHash = pScope->GetNativeHashVirtual();   // clear-on-absent -> the scope's hash
        if (pHash)
        {
            pHash->Set(name, pValue);
            pHash->UpdateObjectMethods(pContext, &name, bRemoving);
            // Storing a script function (tag 34..36, defined) onto the root target
            // MC (console dword_8324D830) severs the new class's prototype from the
            // inheritance chain: value hash -> prototype -> its hash -> __proto__ = 0
            // (console @0x82B03048's root-MC arm: Set__Proto__(protoHash, 0)).
            if (pContext == gpAptRootTargetMC && pValue)
            {
                const unsigned int eVal = static_cast<unsigned int>(pValue->getVtblIndex());
                if (eVal - 34u <= 2u && pValue->getIsDefined())
                {
                    AptValue* const pProto = pValue->GetNativeHashVirtual()->GetPrototype();
                    if (pProto)   // console derefs unconditionally; null-guarded for x64 bring-up
                        pProto->GetNativeHashVirtual()->Set__Proto__(0);
                }
            }
            return 1;
        }

        // CIH (type 12/37) + frame-context hash fallback -- homed member (InterpHelpers.cpp).
        return SetVariableFallback(pContext, &name, pValue, nDirect) ? 1 : 0;
    }

    // No scope-chain search: with a script function executing the store lands in the
    // current activation's frame-stack locals (console off_8324E3DC + 8, lazily built
    // via CreateFrameStack @0x82AF1260 -- the same pattern as the Try catch-name
    // bind); otherwise in the context's native hash directly.
    if (mpCurrentFunction)
    {
        AptFrameStack* pFrame = AptScriptFunctionBase::GetActiveFrameStack();   // off_8324E3DC
        if (!pFrame)
        {
            mpCurrentFunction->CreateFrameStack();
            pFrame = AptScriptFunctionBase::GetActiveFrameStack();
        }
        pFrame->GetNativeHashVirtual()->Set(name, pValue);   // console AptNativeHash::Set(frame + 8, ...)
        return 1;
    }
    AptNativeHash* pHash = pContext->GetNativeHashVirtual();
    if (pHash)
    {
        pHash->Set(name, pValue);
        pHash->UpdateObjectMethods(pContext, &name, bRemoving);
    }
    return 1;
}
