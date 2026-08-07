// ===========================================================================
// EATech Apt -- AptActionInterpreter::getVariable, the ActionScript variable /
// property resolver.   DECOMPILED from the X360 ARTIST @0x82B03430 (cross-checked
// vs the PS3 EXTERNAL ELF @0x819814).
//
// Resolve a name against a scope, in order:
//   * scope == the undefined-CIH sentinel            -> undefined;
//   * a "$"-prefixed name                            -> a fresh dynamic AptString
//                                                       naming the variable;
//   * parse the path (getContext) into (context, leaf name); an empty leaf means
//     the path WAS the context -> return the context (or undefined);
//   * the context's direct child (findChild) when the path is a plain name;
//   * the function scope chain (the frame stack) when searching is enabled;
//   * the context object's member lookup (objectMemberLookup), then findChild
//     against self/target;
//   * the _level / global-object fallback (CIH contexts);
//   * otherwise recurse once with the target cleared, else undefined (firing the
//     not-found callback).
//
// The "object" test `(bitfield >> 27) & 1` is mbIsDefined (the console reads the
// big-endian bitfield; here it is the endian-safe getIsDefined()). The member
// virtual at vtbl+0x18 is objectMemberLookup (index 6).
//
// Every deep leaf is landed in its own TU: getContext (the path parser,
// AptActionInterpreterContext.cpp), AptValue::findChild (AptValueFindChild.cpp),
// AptLookupScopeChain (AptFrameStack.cpp) and LookupGlobalFallback
// (AptActionInterpreterInterpHelpers.cpp). The empty-scope sentinel is the pinned
// EmptyCIH placeholder (dword_8324D700, built at AptInit); gpAptVarNotFoundCb is
// defined in AptGlobals.cpp, null until a host installs it.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptCIH.h"   // AptCIH : AptValueGC (gpAptEmptyCIH -> AptValue* upcast)

extern AptValue* gpUndefinedValue;   // (AptValueConvert.cpp)
extern AptCIH*   gpAptEmptyCIH;      // dword_8324D700 -- the pinned "EmptyCIH" placeholder (built at AptInit)

// The function-local scope chain (walks AptScriptFunctionBase::spFrameStack, falling
// back to the running function's captured parent scope) -- homed in AptFrameStack.cpp.
extern AptValue* AptLookupScopeChain(AptActionInterpreter* pInterp, const EAStringC* pName);

// LookupGlobalFallback (the _level / global-frame lookup) is a homed member
// (AptActionInterpreterInterpHelpers.cpp).

// The "variable not found" diagnostic callback (storage in AptGlobals.cpp; null
// until a host installs it -- the console's indirect dword_8324E8B4 slot).
typedef void (*AptVarNotFoundCb)(const char* pName);
extern AptVarNotFoundCb gpAptVarNotFoundCb;

AptValue* AptActionInterpreter::getVariable(AptValue* pScope, AptValue* pTarget,
    const EAStringC* pName, int nAllowSelf, int nSearchScopeChain, int nDirect)
{
    // The scope is the pinned EmptyCIH placeholder -> the variable is undefined.
    // (console getVariable @0x82B03430 head: a2 == dword_8324D700. The old
    // never-assigned gpUndefinedCIH duplicate compared against null here, letting
    // EmptyCIH scopes fall through into the resolver.)
    if (pScope == static_cast<AptValue*>(gpAptEmptyCIH))
        return gpUndefinedValue;

    // "$name": a dynamically-named variable -> an AptString naming it.
    if (pName->GetBuffer()[0] == '$')
    {
        AptString* pDyn = AptString::Create("");          // console seed @0x820046A7 ("" -- overwritten below)
        *pDyn->GetInternalString() = *pName;
        return pDyn;
    }

    AptValue*        pContext = pScope;
    const EAStringC* pLeaf    = pName;
    int              nKind    = 0;
    EAStringC        ctxName;   // getContext's leaf-name out-param (auto-released at return)

    if (nDirect)
    {
        pContext = pScope;
        pLeaf    = pName;
    }
    else
    {
        nKind = getContext(pScope, pTarget, pName, &pContext, &ctxName);   // homed path parser (AptActionInterpreterContext.cpp)
        pLeaf = &ctxName;
    }

    // The path resolved to just a context (no leaf name): return it (or undefined).
    if (pLeaf->IsEmpty())
        return pContext ? pContext : gpUndefinedValue;

    // 1) plain-name child of the resolved context; 2) the function scope chain.
    AptValue* pFound = 0;
    if (nKind == 1 && pContext)
        pFound = pContext->findChild(pLeaf, pTarget);                      // homed (AptValueFindChild.cpp)
    if (!pFound && nSearchScopeChain)
        pFound = AptLookupScopeChain(this, pLeaf);                  // homed (AptFrameStack.cpp)

    if (!pFound)
    {
        if (pContext && pContext->getIsDefined())
        {
            // 3) the context object's member; 4) findChild against self/target.
            pFound = pContext->objectMemberLookup(pContext, pLeaf);
            if (!pFound)
            {
                AptValue* pTgt = (nAllowSelf && nDirect) ? pContext : pTarget;
                pFound = pContext->findChild(pLeaf, pTgt);                 // homed (AptValueFindChild.cpp)
            }
            // 5) the _level / global-object fallback (only when not targeting).
            if (!pFound && !pTarget)
            {
                pFound = LookupGlobalFallback(pContext, pLeaf, nDirect);  // the _level / global-frame fallback
                if (!pFound)
                {
                    if (gpAptVarNotFoundCb)
                        gpAptVarNotFoundCb(pName->GetBuffer());
                    return gpUndefinedValue;
                }
            }
        }
        else if (!pTarget)
        {
            return gpUndefinedValue;
        }

        // 6) recurse once up the scope, with the target cleared.
        if (!pFound)
            pFound = getVariable(pScope, 0, pName, nAllowSelf, 1, 0);
    }

    return pFound;
}
