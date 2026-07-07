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
// FLAG -- follow-on TUs / AptInit globals (this TU is the orchestration; its deep
// leaves are declared and stubbed, the work `stubs` pattern):
//   getContext (the path parser, ~115 lines) and AptValue::findChild (the special-
//   name + display-tree resolver, ~190 asm-only insns) -- their own TUs;
//   AptInterp_LookupScopeChain (the AptFrameStack function-local lookup) and
//   AptInterp_LookupGlobalFallback (the _level / global-frame lookup) encapsulate
//   the AptScriptFunctionBase frame state (not reconstructed) -- x64-native helpers
//   so no console frame-context offsets are baked in;
//   gpUndefinedCIH, gpAptVarNotFoundCb -- wired at AptInit;
//   AptString::Create's seed prefix const.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

extern AptValue* gpUndefinedValue;   // (AptValueConvert.cpp)
extern AptValue* gpUndefinedCIH;     // FLAG: the undefined-CIH sentinel (AptInit)

// FLAG: the function-local scope chain (walks AptScriptFunctionBase::spFrameStack
// via the interpreter's current frame context). Returns the found value or null.
extern AptValue* AptInterp_LookupScopeChain(AptActionInterpreter* pInterp, const EAStringC* pName);

// FLAG: the _level / global-object fallback for CIH contexts (the global frame's
// AptNativeHash). Returns the found value or null.
// AptActionInterpreter::LookupGlobalFallback is now a member (declared in the header).

// FLAG: the "variable not found" diagnostic callback (null until AptInit).
typedef void (*AptVarNotFoundCb)(const char* pName);
extern AptVarNotFoundCb gpAptVarNotFoundCb;

AptValue* AptActionInterpreter::getVariable(AptValue* pScope, AptValue* pTarget,
    const EAStringC* pName, int nAllowSelf, int nSearchScopeChain, int nDirect)
{
    // The scope is the undefined-CIH sentinel -> the variable is undefined.
    if (pScope == gpUndefinedCIH)
        return gpUndefinedValue;

    // "$name": a dynamically-named variable -> an AptString naming it.
    if (pName->GetBuffer()[0] == '$')
    {
        AptString* pDyn = AptString::Create("");          // FLAG: console seed-prefix const
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
        nKind = getContext(pScope, pTarget, pName, &pContext, &ctxName);   // FLAG: path parser
        pLeaf = &ctxName;
    }

    // The path resolved to just a context (no leaf name): return it (or undefined).
    if (pLeaf->IsEmpty())
        return pContext ? pContext : gpUndefinedValue;

    // 1) plain-name child of the resolved context; 2) the function scope chain.
    AptValue* pFound = 0;
    if (nKind == 1 && pContext)
        pFound = pContext->findChild(pLeaf, pTarget);                      // FLAG: findChild
    if (!pFound && nSearchScopeChain)
        pFound = AptInterp_LookupScopeChain(this, pLeaf);                  // FLAG: frame stack

    if (!pFound)
    {
        if (pContext && pContext->getIsDefined())
        {
            // 3) the context object's member; 4) findChild against self/target.
            pFound = pContext->objectMemberLookup(pContext, pLeaf);
            if (!pFound)
            {
                AptValue* pTgt = (nAllowSelf && nDirect) ? pContext : pTarget;
                pFound = pContext->findChild(pLeaf, pTgt);                 // FLAG: findChild
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
