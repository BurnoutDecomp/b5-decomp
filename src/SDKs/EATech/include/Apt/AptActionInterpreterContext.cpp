// ===========================================================================
// EATech Apt -- AptActionInterpreter::getContext, the ActionScript variable-PATH
// parser.   DECOMPILED from the X360 ARTIST @0x82B01830.
//
// Parse a variable path into (context object, leaf name) and return the path kind
// (1 = absolute, 0 = relative). The path syntax:
//   "/..."        absolute -- the context starts at the movie root;
//   "a.b.c"       dot path  -- descend through each segment via findChild;
//   ".."          parent    -- accumulated into the segment (findChild resolves it
//                             as the parent, like _parent);
//   "ctx:leaf"    colon     -- findChild the part before ':' as the context, the
//                             rest is the leaf name;
//   "name"        plain     -- the whole thing is the leaf name in the context.
// Each resolved segment consumes the optional target (a2) once.
//
// x64 NOTE: the console accumulates each segment into a fixed stack char buffer and
// writes the leaf name into a raw char-buffer out-param. Here the segment buffer is
// kept (a local char[]), but the leaf-name out-param is an EAStringC (matching the
// getVariable call site + the rest of the value layer) -- assigned from the segment
// buffer rather than raw-copied -- a deliberate x64 interface adaptation (the
// console's raw char out-buffer only ever feeds string construction at the call
// sites), not a layout change.
//
// AptValue::findChild (the per-segment resolver) is homed in AptValueFindChild.cpp
// (X360 @0x82B01298); the console root global (off_8324E574 -> the current target's
// root) is resolved inline in getContext as AptGetAnimationAtLevel(0), matching the
// console.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

// The movie root context for absolute ("/") paths: the console reads the target
// singleton (off_8324E574) and resolves its level-0 root animation, which is exactly
// AptGetAnimationAtLevel(0) (the resolver the zombie preconditions and RunActions
// already use). Inlined at the single getContext use below, matching the console.
class AptCIH;
extern AptCIH* AptGetAnimationAtLevel(int nLevel);

// The console's fixed segment-buffer size (stack char[336] in the original).
namespace { enum { KI_SEGMENT_MAX = 336 }; }

int AptActionInterpreter::getContext(AptValue* pScope, AptValue* pTarget,
    const EAStringC* pPath, AptValue** ppOutContext, EAStringC* pOutName)
{
    const char* p        = pPath->GetBuffer();
    AptValue*   pContext = pScope;
    int         nKind    = 0;
    *pOutName = EAStringC();   // empty leaf until resolved

    if (*p == '/')             // absolute path: start from the root
    {
        ++p;
        nKind = 1;
        // absolute-path root (console off_8324E574's target -> its level-0 root)
        pContext = reinterpret_cast<AptValue*>(AptGetAnimationAtLevel(0));
        *ppOutContext = pContext;
    }
    else
    {
        *ppOutContext = pScope;
    }

    char seg[KI_SEGMENT_MAX];

    for (;;)   // one iteration per path segment
    {
        char* w = seg;
        bool  segmentResolved = false;

        for (;;)   // accumulate one segment
        {
            char c = *p;

            if (c == 0)        // end of path: the accumulated segment is the leaf
            {
                *w = 0;
                if (pTarget)
                    pContext = pTarget;
                *ppOutContext = pContext;
                *pOutName = EAStringC(seg);
                return nKind;
            }

            if (c == '.')
            {
                char c2 = *(p + 1);
                if (c2 != '.')             // single '.' -> a dot separator
                {
                    *w = 0;
                    ++p;                   // consume the '.'
                    EAStringC name(seg);
                    pContext = pContext->findChild(&name, pTarget);   // findChild @0x82B01298 (AptValueFindChild.cpp)
                    pTarget = 0;
                    if (!pContext)
                    {
                        *ppOutContext = 0;
                        return nKind;
                    }
                    segmentResolved = true;
                    break;                 // -> next segment
                }
                // ".." -> keep both dots in the segment (findChild resolves it)
                *w++ = '.';
                *w++ = '.';
                p += 2;
                continue;
            }

            if (c == ':')                  // "context:leaf"
            {
                *w = 0;
                EAStringC name(seg);
                AptValue* pCtx = pContext->findChild(&name, pTarget);   // findChild @0x82B01298 (AptValueFindChild.cpp)
                pTarget = 0;
                if (!pCtx)
                {
                    ++p;                   // not a context here; retry from after ':'
                    segmentResolved = true;
                    break;
                }
                *ppOutContext = pCtx;
                *pOutName = EAStringC(p + 1);
                return nKind;
            }

            *w++ = c;                      // ordinary character
            ++p;
        }

        (void)segmentResolved;             // loop continues with the next segment
    }
}
