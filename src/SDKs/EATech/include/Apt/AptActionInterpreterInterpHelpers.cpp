// ===========================================================================
// EATech Apt -- AptActionInterpreter interpreter helper free-functions. These are
// the encapsulation shims the sibling call/variable opcode TUs (StackOps,
// SpecialOps, SetVariable, Variable) declare `extern` and call, but whose bodies
// live here. Each is a faithful DECOMPILE of a real X360 ARTIST routine (or a
// 1:1-lifted sub-section of getVariable/setVariable), cross-checked against the
// assembly. x64-native: 8-byte pointers, NAMED members, console offsets only in
// [c:0xNN] comments.
//
//   AptInterp_ResolveTargetContext  <- console sub_82B02F80 (the path-context
//       resolver getVariable/CallFunction/StartDrag/GotoFrame2 all wrap)
//   AptInterp_HasMember             <- console sub_82AE4058 (the bounded own/proto
//       member probe)
//   AptInterp_NameEquals            <- console Burnout_X360_Artist_0040_0
//       (EAStringC::operator==, used as the resolved-method-name compare)
//   AptInterp_HashLookupName        <- the AptNativeHash::Lookup over the resolved
//       method-name slot (CallMethod's Extension-typed name path)
//   AptInterp_LookupGlobalFallback  <- the _level / global-object frame fallback,
//       the no-target tail of getVariable @0x82B03430
//   AptInterp_SetInScopeChain       <- the scope-chain store, the nSearchScopeChain
//       branch of setVariable @0x82B03048
//   AptInterp_SetVariableFallback   <- setVariable's CIH (type 12/37) + frame-context
//       hash fallback tail (the deepest frame-context branch FLAG-encapsulated)
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptFrameStack.h"
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"
#include "SDKs/EATech/include/Apt/AptRenderItem.h"
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"
#include "SDKs/EATech/include/Apt/AptConstFile.h"
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"   // AptGetAnimationAtLevel (script-fn root-anim resolve)

// ---------------------------------------------------------------------------
// AptInterp_GetNodeFrameContextHash (HOMED 2026-07-02, retiring the null
// stub). The X360 setVariable node branch @0x82B03374 resolves a node's
// "frame context" -- the variable scope a timeline write lands in -- as the
// node's DISPLAY PARENT's char-inst property hash (`*(ctx+0x1C) -> +0x20 ->
// +0xC` == GetDisplayListParent()->GetCharacterInst()->mpProperties): an AS
// timeline variable lives on the ENCLOSING clip. Null when the chain breaks
// (the caller treats that as handled-store-nothing; the console's null-inst
// arm falls back to the caller's current dest, which a live parent never
// exercises -- every placed CIH owns an inst).
// ---------------------------------------------------------------------------
AptNativeHash* AptActionInterpreter::GetNodeFrameContextHash(AptValue* pContext)
{
    AptCIH* const pNode = static_cast<AptCIH*>(pContext);
    AptCIH* const pParent = pNode->GetDisplayListParent();
    if (pParent == nullptr)
        return nullptr;
    AptCharacterInst* const pInst = pParent->GetCharacterInst();
    return pInst ? pInst->mpProperties : nullptr;
}

extern AptValue* gpUndefinedValue;   // off_8324D814 (AptValueConvert.cpp)

// ---------------------------------------------------------------------------
// AptInterp_ResolveTargetContext @0x82B02F80 -- resolve pName into (*ppOutContext,
// *pOutLeaf) under (pScope, pTarget). When the name is a plain identifier (no path
// separator: no byte < '0' and no ':') AND there is no explicit target, the scope
// IS the context and the whole name IS the leaf -- copy it through and return. Any
// path syntax (or an explicit target) routes to the full getContext path parser,
// whose leaf-name out-param is copied into pOutLeaf. The console returns the context
// kind (int); the callers ignore it, so the shim is void.
// ---------------------------------------------------------------------------
void AptInterp_ResolveTargetContext(AptValue* pScope, AptValue* pTarget,
                                    const EAStringC* pName,
                                    AptValue** ppOutContext, EAStringC* pOutLeaf)
{
    // console: v8 = *a3 + 8 (the name's char buffer); scan for a path separator.
    const char* pCh = pName->GetBuffer();      // console (*(_DWORD*)a3 + 8)
    char        bPlain = 1;                     // console v9 = 1
    int         nCh    = static_cast<signed char>(*pCh);  // console extsb. of *r11
    if (nCh)                                    // non-empty name
    {
        // console: while ( v11 >= 0x30 && v11 != 0x3A ) advance; a byte below '0'
        // (0x30) or a ':' (0x3A) marks a path -> not a plain name.
        while (nCh >= '0' && nCh != ':')
        {
            nCh = static_cast<signed char>(*++pCh);
            if (!nCh)
                goto done_scan;                 // console goto LABEL_7
        }
        bPlain = 0;                             // console v9 = 0
    }
done_scan:

    if (!bPlain || pTarget)                     // console: !v9 || a2
    {
        // The path (or the targeted case) -> the full context parser. getContext is a
        // pure (scope, target) resolver -- it touches no interpreter member -- so the
        // console reaches it with this == the scope value (asm: r3 = scope on the call,
        // not the interpreter); the cpp member call mirrors that exactly.
        EAStringC ctxLeaf;                      // console v14 (InitFromBuffer scratch)
        // console: AptActionInterpreter::getContext(pScope, pTarget, pName, ppOut, &leaf).
        reinterpret_cast<AptActionInterpreter*>(pScope)->getContext(
            pScope, pTarget, pName, ppOutContext, &ctxLeaf);
        *pOutLeaf = ctxLeaf;                    // console EAStringC::operator= (then the
                                               // scratch's DecreaseInternalRefCount on dtor)
    }
    else
    {
        // A plain identifier with no target: the scope is the context, the name the leaf.
        *pOutLeaf      = *pName;                // console EAStringC::operator=(a5, a3)
        *ppOutContext  = pScope;               // console *a4 = a1
    }
}

// ---------------------------------------------------------------------------
// AptInterp_GetDictEntry -- fetch a string-dictionary slot with a null guard.
// FLAG hardening: a resolved dictionary slot can be NULL on x64 when its const
// record resolved to nothing at parse (e.g. an AptLookup pool miss -- the
// console pre-seeds its pools, so its slots are never null). The dict-byte
// opcodes AddRef the slot unconditionally; a null here AV'd MAIN's first init
// actions (2026-07-05, cdb: StringDictByteGetMember+0x75). Log + `undefined`.
// ---------------------------------------------------------------------------
#if defined(_MSC_VER)
extern "C" void AptDictNullEntryProbe(unsigned int nIndex, const void* pPool);
#pragma comment(linker, "/alternatename:AptDictNullEntryProbe=AptDictNullEntryProbeDefault")
extern "C" void AptDictNullEntryProbeDefault(unsigned int, const void*) {}
extern "C" void AptDictFetchProbe(unsigned int nIndex, const void* pPool, int nType,
                                  const char* pcText);
#pragma comment(linker, "/alternatename:AptDictFetchProbe=AptDictFetchProbeDefault")
extern "C" void AptDictFetchProbeDefault(unsigned int, const void*, int, const char*) {}
#else
extern "C" void AptDictNullEntryProbe(unsigned int nIndex, const void* pPool);
extern "C" void AptDictFetchProbe(unsigned int nIndex, const void* pPool, int nType,
                                  const char* pcText);
#endif

AptValue* AptActionInterpreter::GetDictEntry(unsigned int nIndex)
{
    AptValue* pEntry = mpConstantPool[nIndex];
    if (!pEntry)
    {
        AptDictNullEntryProbe(nIndex, mpConstantPool);
        pEntry = gpUndefinedValue;
    }
    else
    {
        // FLAG bring-up trace: name + type of every dictionary fetch (first N).
        const char* pcText = nullptr;
        if (pEntry->getVtblIndex() == AptVFT_StringValue && pEntry->getIsDefined())
            pcText = static_cast<AptString*>(pEntry)->GetInternalString()->GetBuffer();
        AptDictFetchProbe(nIndex, mpConstantPool,
                          static_cast<int>(pEntry->getVtblIndex()), pcText);
    }
    return pEntry;
}

// ---------------------------------------------------------------------------
// AptInterp_HasMember @0x82AE4058 -- does pValue (or, for object/CIH-typed values,
// its prototype) hold a member keyed by *pNameSlot? A Prototype-typed (tag 20)
// defined value is probed directly; an Object (19) / CharacterInstHandle (12) /
// CIHNone (37) value is stepped one link up its __proto__ chain first; any other
// type has no member. pNameSlot is the (object+8) method-name slot the console reads
// -- an EAStringC* worth of storage, passed straight to AptNativeHash::Lookup.
// ---------------------------------------------------------------------------
bool AptActionInterpreter::HasMember(AptValue* pValue, EAStringC** pNameSlot)
{
    const AptVirtualFunctionTable_Indices eType = pValue->getVtblIndex();  // (a1[1]<<25)>>25
    const bool bDefined = pValue->getIsDefined();                          // (a1[1]>>27)&1

    // console v6: a defined Prototype-typed value is probed directly (LABEL_17).
    bool bDirect = (eType == AptVFT_Prototype && bDefined);
    if (!bDirect)
    {
        // console v7/v8: object / CIH / CIHNone -> step up one __proto__ link.
        const bool bStepProto =
            (eType == AptVFT_CharacterInstHandle && bDefined)   // tag 12
         || (eType == AptVFT_CIHNone)                           // tag 37
         || (eType == AptVFT_Object && bDefined);               // tag 19
        if (!bStepProto)
            return false;                                       // console v4 = 0

        // console LABEL_14: v9 = GetNativeHashVirtual(); pValue = v9 ? *(v9+8) : 0.
        AptNativeHash* const pHash = pValue->GetNativeHashVirtual();
        pValue = pHash ? pHash->mp__Proto__ : nullptr;          // console *(v9 + 8)
    }

    // console LABEL_17: probe the (resolved) value's own hash for the name.
    // (Null-slot guard: the console never passes a null/empty name slot here; a
    // not-yet-complete x64 caller path can -- same hardening as NameEquals. FLAG.)
    if (pValue && pNameSlot && *reinterpret_cast<void* const*>(pNameSlot))
    {
        AptNativeHash* const pHash = pValue->GetNativeHashVirtual();   // (*(*a1+8))(a1)
        if (pHash)
            return pHash->Lookup(*reinterpret_cast<const EAStringC*>(pNameSlot)) != nullptr;
    }
    return false;
}

// ---------------------------------------------------------------------------
// AptInterp_NameEquals @0x82AD85D8 (Burnout_X360_Artist_0040_0) -- the resolved
// method-name slot equals the constant key. This is EAStringC::operator== over the
// slot (treated as an EAStringC) and the constant: same StringDataC pointer (fast
// path) or same length + byte-equal buffer. The console uses it to test the resolved
// method name against the apply/call/empty-method/this sentinels.
// ---------------------------------------------------------------------------
bool AptActionInterpreter::NameEquals(EAStringC** pNameSlot, const EAStringC* pConst)
{
    // The slot address is punned as an EAStringC (its one data member IS the stored
    // pointer). A NULL stored pointer means "no resolved name": the console never
    // holds raw null here (its strings carry the empty-string sentinel data block),
    // but the x64 CallMethod path can leave the slot null before any name resolves --
    // operator== would then deref null reading the length word (the 2026-07-05
    // FADE_IN-window AV, cdb-verified stack: EAStringC::operator== <- NameEquals <-
    // _FunctionAptActionCallMethod <- DictCallMethodPop). Null slot == "not this name".
    if (pNameSlot == nullptr || *reinterpret_cast<void* const*>(pNameSlot) == nullptr)
        return false;
    return *reinterpret_cast<const EAStringC*>(pNameSlot) == *pConst;
}

// ---------------------------------------------------------------------------
// AptInterp_HashLookupName -- AptNativeHash::Lookup over the resolved method-name
// slot. CallMethod's Extension-typed (tag 29) name path resolves the method through
// the object's own native hash by this slot; a null hash yields no value.
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::HashLookupName(AptNativeHash* pHash, EAStringC** pNameSlot)
{
    if (!pHash)
        return nullptr;
    return pHash->Lookup(*reinterpret_cast<const EAStringC*>(pNameSlot));
}

// ---------------------------------------------------------------------------
// AptInterp_LookupGlobalFallback -- the no-target tail of getVariable @0x82B03430:
// when a name resolved against a DEFINED context object found nothing in the object,
// its members, or self/target, the _level / global-object frame is the last resort.
// It applies only when the context is NOT itself a CharacterInstHandle (12)/CIHNone
// (37) node and a script function is currently executing: the global members live in
// that function's ParentAnim frame's native hash. Returns the found value, or null
// (the caller then fires the not-found callback and yields `undefined`).
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::LookupGlobalFallback(AptValue* pContext, const EAStringC* pName,
                                                     int nDirect)
{
    AptActionInterpreter* const pInterp = this;
    // console @0x82B03628: nDirect (a7/r22) gates the global-frame lookup OFF -- a direct
    // member access (obj.member) returns not-found rather than consulting the _global frame.
    if (nDirect)
        return nullptr;

    // console: v25 = tag; a CIH/CIHNone context skips the global frame (v26 = 1).
    const AptVirtualFunctionTable_Indices eType = pContext->getVtblIndex();
    const bool bIsNode =
        (eType == AptVFT_CharacterInstHandle && pContext->getIsDefined())  // tag 12
     || (eType == AptVFT_CIHNone);                                         // tag 37
    if (bIsNode)
        return nullptr;

    // console: v27 = *(a1 + 60) (the running script function); null -> no global frame.
    AptScriptFunctionBase* const pFunction = pInterp->mpCurrentFunction;
    if (!pFunction)
        return nullptr;

    // console: v28 = (*(**(v27+36)+8))(*(v27+36)) -- the ParentAnim frame's native hash.
    AptValue* const pGlobalFrame = pFunction->GetParentAnim();   // *(mpCurrentFunction + 0x24)
    if (!pGlobalFrame)
        return nullptr;
    AptNativeHash* const pHash = pGlobalFrame->GetNativeHashVirtual();
    if (!pHash)
        return nullptr;

    // console: AptNativeHash::Lookup(v28, &leafName).
    return pHash->Lookup(*pName);
}

// ---------------------------------------------------------------------------
// AptInterp_SetInScopeChain -- the scope-chain store of setVariable @0x82B03048
// (the nSearchScopeChain branch): when a script function is executing, store the
// value into whichever enclosing frame already binds the name -- the live frame
// stack (spFrameStack) when one is installed, else the function's captured parent
// scope. Returns true when an existing binding was found and overwritten (no new
// binding is created here).
// ---------------------------------------------------------------------------
bool AptActionInterpreter::SetInScopeChain(const EAStringC* pName, AptValue* pValue)
{
    AptActionInterpreter* const pInterp = this;
    // console: v22 = *(a1 + 60); a null running function has no scope chain.
    AptScriptFunctionBase* const pFunction = pInterp->mpCurrentFunction;
    if (!pFunction)
        return false;

    // console: v23 = off_8324E3DC (spFrameStack); else *(v22 + 40) (mpParentScope).
    AptFrameStack* pFrame = AptScriptFunctionBase::GetActiveFrameStack();
    if (!pFrame)
        pFrame = reinterpret_cast<AptFrameStack*>(pFunction->GetParentScope());
    if (!pFrame)
        return false;

    // console: AptFrameStack::SetWhereExistsInScopeChain(v23, &name, value).
    return pFrame->SetWhereExistsInScopeChain(*pName, pValue);
}

// FLAG (the deepest setVariable fallback branch -- the CIH/CIHNone node's frame-
// context native hash: console *(*(pContext+0x1C) + 0x20) -> +0xC). The character-
// instance frame-context layout the console reaches by these raw offsets is not yet
// a reconstructed named-member path, so resolving the destination hash for a node
// context is encapsulated here, matching the sibling call/special-op TUs. Returns the
// node's frame-context property hash, or null when it has none.

// ---------------------------------------------------------------------------
// AptInterp_SetVariableFallback -- the tail of setVariable @0x82B03048 reached when
// the resolved context exposes no native hash of its own. The destination hash is:
//   * for a CIH/CIHNone context whose boxed object is a movie-clip-class value
//     (the packed 0x10000000 class-bit), the node's frame-context property hash;
//   * otherwise (a plain context with a running script function), the global frame's
//     native hash (the same ParentAnim hash the read-side fallback consults).
// When a hash is found the value is stored under the name. Returns true in every
// faithful path the console returns 1 (it always reports "handled").
// ---------------------------------------------------------------------------
bool AptActionInterpreter::SetVariableFallback(AptValue* pContext, const EAStringC* pName,
                                               AptValue* pValue, int nDirect)
{
    AptActionInterpreter* const pInterp = this;
    const AptVirtualFunctionTable_Indices eType = pContext->getVtblIndex();
    const bool bIsNode =
        (eType == AptVFT_CharacterInstHandle && pContext->getIsDefined())  // tag 12
     || (eType == AptVFT_CIHNone);                                         // tag 37

    AptNativeHash* pDest = nullptr;

    // console: v34 (CIH/CIHNone) || mpCurrentFunction==0 -> the node-context branch
    // (this shim is only entered post-getContext, so nDirect is folded out).
    if (bIsNode || pInterp->mpCurrentFunction == nullptr)
    {
        // console: a non-node context here is unhandled (return 1, store nothing).
        if (!bIsNode)
            return true;
        // FLAG: resolve the node's frame-context hash (the +0x1C boxed object ->
        // +0x20 char-inst -> +0xC hash chain); only a movie-clip-class node has one.
        pDest = GetNodeFrameContextHash(pContext);
        if (!pDest)
            return true;   // console goto LABEL_16 (handled, nothing to store)
    }
    else
    {
        // console @0x82B03308: nDirect (a8/r25) diverts a non-node context to the node
        // branch, which for a non-node falls straight through to "handled, store nothing".
        // So a direct member store (obj.member = v) must NOT write the _global frame hash.
        if (nDirect)
            return true;
        // console: v36 = (*(**(v35+36)+8))(*(v35+36)) -- the global ParentAnim hash.
        AptValue* const pGlobalFrame = pInterp->mpCurrentFunction->GetParentAnim();
        pDest = pGlobalFrame ? pGlobalFrame->GetNativeHashVirtual() : nullptr;
        if (!pDest)
            return true;   // console goto LABEL_16
    }

    // console: AptNativeHash::Set(v36, &name, value).
    pDest->Set(*pName, pValue);
    return true;
}

// ===========================================================================
// The un-homed AptActionInterpreter behavioural entry points the sibling opcode /
// native-method TUs (AptActionInterpreter.cpp, AptActionInterpreterSpecialOps.cpp,
// AptActionInterpreterMemberOps.cpp, AptActionInterpreterBuiltins.cpp,
// AptCIHNativeFunctionHelper.cpp) declare `extern` and call but never define. Each
// is a faithful DECOMPILE of the matching X360 ARTIST routine, cross-checked against
// the PS3 DecFIGS DWARF lift (which carries the canonical demangled signatures).
// x64-native: 8-byte pointers, NAMED members, console offsets only in [c:0xNN].
//
//   AptActionInterpreter_valueToObject       <- valueToObject       X360 @0x82B07FB8 / PS3 0xF563DC
//   AptActionInterpreter_ParsePathContext    <- sub_82B02F80 (alias of ResolveTargetContext above)
//   AptActionInterpreter_EnumerateMembers    <- _doEnumerate        X360 @0x82B036D8
//   AptActionInterpreter_BuildPathName       <- sub_82AF7400 (the getName/getName2 path walk)
//   AptActionInterpreter_loadVariables       <- loadVariables       X360 @0x82B07DF8 / PS3 0xF54764
//   AptActionInterpreter_doCloneSprite       <- _doCloneSprite      X360 @0x82B0DC60 / PS3 0xF608AC
//   AptActionInterpreter_CallFunctionDispatch <- callFunction       X360 @0x82AE3C08 / PS3 0xF57A90
//   AptActionInterpreter_ResolveTranscode    <- _parseStream        X360 @0x82AF3440 / PS3 0xF41688
// ===========================================================================

// The two AS reserved member keys both _doEnumerate and _doCloneSprite skip: the
// __proto__ key (StringPool::saConstant, console dword_8324E580) and the prototype
// key (gAptKeyPrototype, console dword_8324E698). See AptNativeHash.h.

// ---------------------------------------------------------------------------
// AptActionInterpreter::valueToObject @0x82B07FB8 (X360) / 0xF563DC (PS3) -- coerce
// pValue to the AptValue object it designates under (pScope, pTarget), writing the
// result through *ppOut. A clip-handle (tag 12, defined) / CIHNone (tag 37) value IS
// its own object. Otherwise probe the value's object-ness virtual (vtbl +0xC,
// ContainsNativeHashVirtual): on success the value itself is the object. Failing
// that, a defined string value (tag 1 / boxed 33) is treated as a path NAME and
// resolved through getObject; any other value yields no object (*ppOut untouched).
// (The console returns the resolved value in r3; the callers read *ppOut, so this
// shim is void -- matching every extern declaration of it.)
// ---------------------------------------------------------------------------
void AptActionInterpreter_valueToObject(AptValue* pScope, AptValue* pTarget,
                                        AptValue* pValue, AptValue** ppOut)
{
    const AptVirtualFunctionTable_Indices eType = pValue->getVtblIndex();   // (*(v+4)<<25)>>25

    // console: tag 0xC needs the defined bit; tag 0x25 is unconditional. Either way
    // the value boxes its own object.
    if ((eType == AptVFT_CharacterInstHandle && pValue->getIsDefined())
        || eType == AptVFT_CIHNone)
    {
        *ppOut = pValue;            // console *a4 = a3 (the value), then return
        return;
    }

    // console: this = (**(*a3 + 0xC))(a3) -- the object-ness probe (vtbl +0xC). On a
    // non-null result the value itself is the designated object (the console stores
    // r31 = the value, NOT the probe result).
    if (pValue->ContainsNativeHashVirtual())
    {
        *ppOut = pValue;
        return;
    }

    // console LABEL after the probe: a DEFINED string value (tag 1 / boxed 33) names a
    // path -- resolve it via getObject (its EAStringC lives at c_string()+8 = the
    // embedded AptNativeString). Anything else designates no object.
    if ((eType == AptVFT_StringValue || eType == AptVFT_StringObject) && pValue->getIsDefined())
    {
        AptString* const pStr = pValue->c_string();
        // console: getObject(scope, target, c_string(value) + 8). getObject is a method
        // on the interpreter, but it touches no interpreter member (a pure (scope,
        // target) resolver), so the console reaches it with this == the scope value.
        *ppOut = reinterpret_cast<AptActionInterpreter*>(pScope)->getObject(
            pScope, pTarget, pStr->GetInternalString());
    }
}

// ---------------------------------------------------------------------------
// AptActionInterpreter_ParsePathContext -- the same console path-context resolver
// (sub_82B02F80) AptInterp_ResolveTargetContext already homes above; getObject /
// getName declare this second extern alias of it. Forward to the single body.
// ---------------------------------------------------------------------------
void AptActionInterpreter_ParsePathContext(AptActionInterpreter* pInterp,
                                           AptValue* pTarget, const EAStringC* pName,
                                           AptValue** ppOutContext, EAStringC* pOutLeaf)
{
    // console sub_82B02F80(a1, a2=target, a3=name, a4=&ctx, a5=&leaf): a1 is the scope
    // value the parser resolves against (and the value it stores into *ppOutContext in
    // the plain-identifier branch). getObject reaches this with a1 == its own first
    // argument (the X360 collapses scope == the implicit `this`), so the interpreter
    // pointer it forwards IS that scope value -- pass it straight through, exactly as
    // the already-homed ResolveTargetContext (the same routine) does.
    AptInterp_ResolveTargetContext(reinterpret_cast<AptValue*>(pInterp),
                                   pTarget, pName, ppOutContext, pOutLeaf);
}

// FLAG (host URL-variable fetch -- console indirect through dword_8324E84C (named-URL
// fetch) / dword_8324E850 (default no-arg fetch); each returns the AptValue whose
// string form is the downloaded "key=value&..." body). The host installs the fetch at
// AptInit; modelled as a single named host boundary (a null pUrl selects the default
// fetch), matching the AptApt_* FLAG link-stub family so the parse below stays faithful
// without introducing a new unresolved symbol.
extern AptValue* AptApt_LoadVariablesFetch(const char* pUrl);   // dword_8324E84C / dword_8324E850

// ---------------------------------------------------------------------------
// AptActionInterpreter::loadVariables @0x82B07DF8 (X360) / 0xF54764 (PS3) -- AS
// loadVariables: fetch the URL pURL through the host hook, render its body to a
// string, then parse it as a "key=value[&...]" sequence (urlDecode), storing each
// pair as a variable on (pScope, pTarget) via setVariable. An empty/absent URL uses
// the default no-arg fetch hook. Each emitted value is a fresh AptString seeded from
// the decoded value text.
// ---------------------------------------------------------------------------
void AptActionInterpreter::loadVariables(AptValue* pScope, AptValue* pTarget, EAStringC* pURL)
{
    AptActionInterpreter* const pInterp = this;
    // console: v7 = a4 ? dword_8324E84C(a4->m_strText) : dword_8324E850().
    AptValue* const pLoaded = AptApt_LoadVariablesFetch(pURL ? pURL->GetBuffer() : nullptr);   // FLAG: host fetch

    EAStringC strBody;                                  // console v13 = &unk_82F72FF8 (empty)
    if (pLoaded)
        pLoaded->Append_ToString(&strBody);             // console AptValue::Append_ToString(v7, &v13)

    EAStringC strKey;                                   // console v11
    EAStringC strValue;                                 // console v12
    // console: for ( i = urlDecode(this, body+8, &key, &val); i; i = urlDecode(...) )
    for (const char* p = pInterp->urlDecode(strBody.GetBuffer(), &strKey, &strValue);
         p != nullptr;
         p = pInterp->urlDecode(p, &strKey, &strValue))
    {
        // console: a non-empty key (v11 != the empty sentinel) -> store key = value.
        if (strKey.GetInternalSize() != 0)
        {
            AptString* const pStr = AptString::Create("");          // console AptString::Create(&unk_820046A7)
            *pStr->GetInternalString() = strValue;                  // console EAStringC::operator=(v9+8, &v12)
            pInterp->setVariable(pScope, pTarget, &strKey, pStr, 1, 1, 0);
        }
    }
    // strKey/strValue/strBody auto-release on scope exit (console explicit
    // DecreaseInternalRefCount of v12/v11/v13).
}

// ---------------------------------------------------------------------------
// AptActionInterpreter::_doEnumerate @0x82B036D8 -- AS `for..in` body (the un-homed
// EnumerateMembers walk AptActionInterpreter::_doEnumerate delegates to). Resolve the
// stack-top enumeration target (when it is a name, swap it for its variable), replace
// the top with the null/undefined marker, then push one AptString per enumerable
// member name held in the target object's native hash chain -- skipping the two
// reserved internal keys (__proto__ / prototype). pScope/pTarget are the run scope +
// current target the name resolves against.
// ---------------------------------------------------------------------------
void AptActionInterpreter::EnumerateMembers(AptValue* pScope, AptValue* pTarget)
{
    AptActionInterpreter* const pInterp = this;
    // console: Variable = stack top (4 * *a1 + a1[2] - 4 -> mpStack[mnStackTop-1]).
    AptValue* pVariable = pInterp->mpStack[pInterp->mnStackTop - 1];

    // console: a string-typed (tag 1) / boxed-string (tag 33) DEFINED top is a NAME;
    // resolve it to the variable it designates. A tag-1 name passes its own value+8
    // EAStringC; a boxed (tag 33) name passes its boxed object's (+0x20) +8 instead.
    const AptVirtualFunctionTable_Indices eTop = pVariable->getVtblIndex();
    const bool bIsName =
        ((eTop == AptVFT_StringValue) || (eTop == AptVFT_StringObject)) && pVariable->getIsDefined();
    if (bIsName)
    {
        // console: v9 = (tag != 1) ? Variable[8] : the top; name = v9 + 8.
        AptValue* const pNameHost = (eTop != AptVFT_StringValue)
            ? *reinterpret_cast<AptValue**>(reinterpret_cast<char*>(pVariable) + 0x20)   // FLAG: boxed-string object slot
            : pVariable;
        const EAStringC* const pName =
            reinterpret_cast<AptString*>(pNameHost)->GetInternalString();
        pVariable = pInterp->getVariable(pScope, pTarget, pName, 1, 1, 0);
    }

    // console: replace the top with the null/undefined enumeration marker.
    if (pInterp->mnStackTop > 0)
        --pInterp->mnStackTop;
    pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;   // off_8324D814 (no AddRef -- a singleton)

    // console: walk the target's native-hash chain, pushing each enumerable key.
    AptNativeHash* pHash = pVariable->GetNativeHashVirtual();     // (*(*Variable+8))(Variable)
    while (pHash)
    {
        for (AptHashItem* pItem = pHash->GetFirstItem(); pItem != nullptr;
             pItem = pHash->GetNextItem(pItem))
        {
            // console: skip the two reserved keys (dword_8324E580 / dword_8324E698).
            if (pItem->mKey == StringPool::saConstant[0] || pItem->mKey == gAptKeyPrototype)
                continue;

            AptString* const pStr = AptString::Create("");           // console AptString::Create(&unk_820046A7)
            *pStr->GetInternalString() = pItem->mKey;                // console EAStringC::operator=(v12+2, key)
            pInterp->mpStack[pInterp->mnStackTop++] = pStr;          // inlined stackPush slot
            pStr->AddRef();                                          // console (**v12)(v12)
        }
        // console: step to the next hash in the chain (the __proto__ link's hash).
        AptValue* const pProto = pHash->mp__Proto__;                 // *(v10 + 8)
        pHash = pProto ? pProto->GetNativeHashVirtual() : nullptr;   // (*(*v14+8))(v14)
    }

    pVariable->Release();   // console (*(*Variable+4))(Variable) -- drop the resolved ref
}

// The instance/level number the "instance%ld" / "_level%d" path components encode is
// the node's render-item depth (console *(*(node+0x20)+4)+0x14 == mpCharacterInst's
// render item mDepth); the console inlines that field walk at each use below (pNode's
// +8/+0x1C/+0x20 fields ARE the AptCIH named members mInstanceName /
// mpDisplayListParent / mpCharacterInst), so it is read by name inline here too.
// ---------------------------------------------------------------------------
// sub_82AF7400 -- the recursive slash/dot path-name builder (the getName/getName2
// core; nMode != 0 -> dotted, else slashed). Walks the node's display-list-parent
// chain to the root, appending each node's component into pOut: a named node
// contributes "<sep><name>"; an anonymous node contributes a synthesised
// "instance<idx>" name (which is also bound back into the parent's property hash so
// the name sticks); the root contributes "_level<idx>".
// ---------------------------------------------------------------------------
int AptActionInterpreter_BuildPathName(AptActionInterpreter* /*pInterp*/,
                                       AptValue* pValue, EAStringC* pOut, int nMode)
{
    const char* const szSep = nMode ? "." : "/";        // console v5 = a3 ? "." : "/"

    AptCIH* const pNode   = static_cast<AptCIH*>(pValue);
    AptCIH* const pParent = pNode->mpDisplayListParent;            // console v6 = *(result+0x1C)
    if (pParent)
    {
        // Recurse up to the root first (the console threads pOut through as a2).
        AptActionInterpreter_BuildPathName(nullptr, pParent, pOut, nMode);

        EAStringC& strName = pNode->mInstanceName;                 // console *(v3+8)
        if (strName.GetInternalSize() == 0)   // console: *(v3+8) == &unk_82F72FF8 (the empty sentinel)
        {
            // Anonymous node: synthesise "instance<idx>", append it, and write it back
            // into the parent's property hash so the assigned name persists.
            const int nIdx = pNode->GetCharacterInst()->GetRenderItem()->mDepth;  // console *(*(*(v3+0x20)+4)+0x14)
            EAStringC strInstance;                                  // console v11 = &unk_82F72FF8
            strInstance.Format("instance%ld", nIdx);
            *pOut += szSep;                                         // console sub_82AE8520(a2, v5) -- sep first
            *pOut += strInstance;                                   // console EAStringC::operator+=(a2, &v11)
            strName = strInstance;                                 // console EAStringC::operator=((v3+8), &v11)
            // console: AptNativeHash::Set(*(*(v6+0x20)+0xC), &v11, v3) -- bind name ->
            // node in the parent char-inst's property hash (AptCharacterInst::mpProperties).
            AptNativeHash* const pHash = pParent->GetCharacterInst()->mpProperties;
            if (pHash)
                pHash->Set(strInstance, pValue);
        }
        else
        {
            // Named node: append "<sep><name>".
            EAStringC strComponent = szSep + strName;              // console operator+(v5, v3+8)
            *pOut += strComponent;                                 // console EAStringC::operator+=(a2, ...)
        }
        return 0;
    }

    // Root node: emit "_level<idx>" when this is the root (mode 0 with a non-zero
    // instance index, or mode == 1).
    const int nIdx = pNode->GetCharacterInst()->GetRenderItem()->mDepth;  // console *(*(*(result+0x20)+4)+0x14)
    if ((nMode == 0 && nIdx != 0) || nMode == 1)
    {
        EAStringC strLevel;                                        // console v12
        strLevel.Format("_level%d", nIdx);                        // console sub_82C0CB70 + InitFromBuffer
        *pOut = strLevel;                                         // console EAStringC::operator=(a2, &v12)
    }
    return 0;
}

// FLAG (un-homed AptCIH child-insertion -- console AptCIH::InsertChild; the same
// extern the sibling AptCIHNativeFunctionHelper.cpp clone/attach methods drive): place
// a clone of pSource (or a fresh instance of pCharacter) under pNode at nDepth named
// pName, returning the inserted node. The display-tree insertion is not yet a
// reconstructed body; declared here to keep the clone call shape faithful.
// AptCIH::InsertChild is now a member (declared in AptCIH.h, included above).

// FLAG (the process-wide AS VM -- homed by AptActionInterpreter, console
// dword_8324E760 == &gAptActionInterpreter.mnStackTop): the init-object member copy
// stores variables through it (the console hard-codes &dword_8324E760 as the setVariable
// receiver). Committed extern in the sibling AptCIHNativeFunctionHelper.cpp.
extern AptActionInterpreter gAptActionInterpreter;

// ---------------------------------------------------------------------------
// AptActionInterpreter::_doCloneSprite @0x82B0DC60 (X360) / 0xF608AC (PS3) -- the AS
// duplicateMovieClip core. Resolve pParentValue to the clip object it designates
// (valueToObject), coerce pNameValue to the new instance name, then -- when the
// resolved clip has a display-list parent -- InsertChild a clone of it under that
// parent at nDepth named pName, copy the source's render/visual data onto the clone,
// and (when an init object is given and the clone accepts members) copy each of the
// init object's members onto the clone via setVariable (skipping the reserved keys).
// Ticks the new instance live and returns the inserted clone (or `undefined`).
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::_doCloneSprite(AptCIH* pScope, AptValue* pTarget,
                                               AptValue* pParentValue, AptValue* pNameValue,
                                               int nDepth, AptValue* pInitObject)
{
    // console: valueToObject(scope, target, parentValue, &resolved).
    AptValue* pResolved = nullptr;
    AptActionInterpreter_valueToObject(reinterpret_cast<AptValue*>(pScope), pTarget,
                                       pParentValue, &pResolved);

    // console: name = Get_ToString(nameValue, &scratch).
    EAStringC strName;                                          // console v22 = &unk_82F72FF8
    const EAStringC* const pName = AptValue::Get_ToString(pNameValue, &strName);

    AptValue* pInserted = gpUndefinedValue;
    // console: resolved && *(resolved+0x1C) (its display-list parent).
    AptCIH* const pResolvedNode = static_cast<AptCIH*>(pResolved);
    if (pResolvedNode)
    {
        AptCIH* const pDisplayParent = pResolvedNode->mpDisplayListParent;   // console *(v21+0x1C)
        if (pDisplayParent)
        {
            // console: character = GetRenderItemWritable(*(resolved+0x20))->mpCharacter.
            AptCharacterInst* const pSourceInst = pResolvedNode->GetCharacterInst();  // console *(v21+0x20)
            AptCharacter* const pCharacter = pSourceInst->GetRenderItemWritable()->mpCharacter;

            // console: InsertChild(displayParent, source, character, depth, name, init).
                        AptCIH* const pClone = pDisplayParent->InsertChild(
                pResolvedNode,
                pCharacter, nDepth, const_cast<EAStringC*>(pName), pInitObject);

            // console: copy the source's render data onto the clone --
            // GetRenderItemWritable(clone->charInst)->CopyRenderDataFrom(source render item).
            AptCharacterInst* const pCloneInst = pClone->GetCharacterInst();    // console inserted[8]
            pCloneInst->GetRenderItemWritable()->CopyRenderDataFrom(
                pSourceInst->GetRenderItem());                                  // console (*(*v17+20))(v17, *(v16+4))

            // console: copy each init-object member onto the clone (when it accepts them).
            if (pInitObject && pClone->ContainsNativeHashVirtual())   // (*(*inserted+0xC))(inserted)
            {
                AptNativeHash* const pHash = pInitObject->GetNativeHashVirtual();  // (*(*a7+8))(a7)
                if (pHash)
                {
                    for (AptHashItem* pItem = pHash->GetFirstItem(); pItem != nullptr;
                         pItem = pHash->GetNextItem(pItem))
                    {
                        if (pItem->mKey == StringPool::saConstant[0] || pItem->mKey == gAptKeyPrototype)
                            continue;   // skip the two reserved keys
                        gAptActionInterpreter.setVariable(static_cast<AptValue*>(pClone), nullptr,
                                                          &pItem->mKey, pItem->mpValue, 1, 1, 0);
                    }
                }
            }

            AptAnimationTarget::TickNewInsts();   // console AptAnimationTarget::TickNewInsts()
            pInserted = static_cast<AptValue*>(pClone);
        }
    }
    // strName auto-releases (console EAStringC::DecreaseInternalRefCount(v22)).
    return pInserted;
}

// FLAG (the script-function execution branch of callFunction -- tags 34/35/36; console
// the deep block from `*(a1+60)=a3` through `AptValue_::pop(a1+36)`). It drives the
// AptScriptFunctionBase per-call machinery: install the function as mpCurrentFunction,
// SetupBeforeExecution (snapshot + preload the register window), bind up to
// GetNumArguments stack operands as the call arguments (SetArgument), collapse the
// args, run the function body (runStream over GetByteCodeBase/Size against the
// resolved animation context), CleanupAfterExecution, and leave the result on the
// call-context stack -- restoring the saved mpCurrentFunction / mnConstantPoolCount / mpConstantPool.
//
// It reads the AptScriptFunctionBase vtable slots the console reaches by raw offset
// (+0x50/+0x54 setup, +0x44 GetNumArguments, +0x58 SetArgument, +0x48/+0x4C bytecode,
// +0x5C cleanup) and the interpreter's still-half-mapped per-call window members
// (mnConstantPoolCount / mpConstantPool / the call-context stack #5 at +0x90) which are NOT yet
// reconstructed as named members -- so this innermost branch is encapsulated here,
// matching the deferral the AptActionInterpreter.cpp callFunction FLAG documents.
// Returns the call result value (already on the appropriate stack), or `undefined`.
// AptActionInterpreter::ExecuteScriptFunction is a member (declared in the header);
// definition below.

// ---------------------------------------------------------------------------
// AptActionInterpreter::callFunction @0x82AE3C08 (X360) / 0xF57A90 (PS3) -- invoke an
// AS callable value pFunction with nArgs operands already on the stack. A native-
// function value (tag 9, defined) calls its wrapped callback and collapses the args +
// result via stackPopAndPush. A script-function value (tags 34/35/36, defined) runs
// the encapsulated frame-execution branch. Any other value is not callable: the args
// are popped and `undefined` is pushed. After the call, when an abort fired during it,
// the operand stack is collapsed back to the pre-call depth.
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter::CallFunctionDispatch(AptValue* pScope, AptValue* pFunction,
                                                     int nArgs, AptValue* pNewTarget,
                                                     AptValue* pConstructTarget)
{
    AptActionInterpreter* const pInterp = this;
    // console: HIDWORD(v11) = *a1 - a4 -- the operand-stack depth this call must
    // unwind to if it aborts.
    const int nBase = pInterp->mnStackTop - nArgs;

    AptValue* pResult;
    if (pFunction && pFunction->isNativeFunction())   // tag 9, defined
    {
        // console: v15 = *(a1+64) (mnConstantPoolCount, saved across the native call).
        const uint32_t nSavedField40 = pInterp->mnConstantPoolCount;
        // console: v16 = (*(a3+32))(a2, a4) -- call the wrapped AptExtFunctionPtr.
        typedef AptValue* (*AptExtCall)(AptValue* pThis, int nArgCount);
        AptValue* const pRet = reinterpret_cast<AptExtCall>(
            static_cast<AptNativeFunction*>(pFunction)->GetFunction())(pScope, nArgs);
        // console: AptValue_::PopAndPush(a1, a4, v16) -- collapse the args, push result.
        pInterp->stackPopAndPush(nArgs, pRet);
        pInterp->mnConstantPoolCount = nSavedField40;   // console *(a1+64) = v15
        pResult = pRet;
    }
    else if (pFunction && pFunction->isScriptFunction())   // tags 34/35/36, defined
    {
        // FLAG: the AptScriptFunctionBase frame-execution branch (un-named vtable slots
        // + per-call register window) -- encapsulated; see the extern above.
                pResult = pInterp->ExecuteScriptFunction(pScope, pFunction, nArgs,
                                                 pNewTarget, pConstructTarget);
    }
    else
    {
        // console LABEL_41: not callable -- pop the args, push the undefined singleton.
        pInterp->stackSafePop(nArgs);
        pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;   // *(4*(*a1)++ + ...) = off_8324D814
        pResult = gpUndefinedValue;
    }

    // console LABEL_42: when an abort fired during the call (mpAbortValue set) and the
    // stack grew past the pre-call base, collapse it back.
    if (pInterp->mpAbortValue && pInterp->mnStackTop > nBase)
    {
        pInterp->stackSafePop(pInterp->mnStackTop - nBase);
        pResult = nullptr;   // console: result = the pop's return (the collapsed slot)
    }
    return pResult;
}

// ---------------------------------------------------------------------------
// AptInterp_ExecuteScriptFunction -- the AptScriptFunctionBase frame-execution branch
// of callFunction (tags 34/35/36).
//
extern AptCIH* gpAptEmptyCIH;   // dword_8324D700 -- the pinned "EmptyCIH" AptCIHNone placeholder

// AptInterp_ExecuteScriptFunction -- the AptScriptFunctionBase frame-execution branch of
// callFunction (tags 34/35/36). HOMED + LIVE (2026-07-04). The full faithful body is the
// console branch 0x82AE3CB8..0x82AE3FF4, cross-checked against the PS3 External build
// (0x821734); the AptScriptFunction2 vtable (decrypted XEX @0x82145D10) gave the by-name
// method map (X360 slots 0x44 GetNumArguments / 0x48 GetByteCodeBase / 0x4C GetByteCodeSize /
// 0x50 GetConstantPool / 0x54 SetupBeforeExecution / 0x58 SetArgument / 0x5C
// CleanupAfterExecution -- called by name, so the PS3 slot-order difference is irrelevant).
//
// It is ENABLED (g_bAptHomeScriptFnExec = true): the title's component AS handlers now
// EXECUTE, and boot is GREEN (tick frame=100, childNodes=12, 0 asserts). The earlier gate-on
// stall was NOT this body and NOT integration order -- it was TWO MISSING OVERRIDES on the
// AptScriptFunctionByteCodeBlock subclass (tag 36, which the title's functions are):
// GetNumArguments (console `li r3,0; blr` = 0) and GetByteCodeSize (console `lwz r3,0x34(r3)`
// = the +0x34 member). Without them the base/Fn1 record-deref form ran on this subclass's
// inline byte-code and faulted inside GetNumArguments before this body could complete. Both
// overrides are now homed in AptScriptFunctionByteCodeBlock.{h,cpp}; with them, this body
// runs the AS to completion (probe-verified: enter->Setup->GetNumArgs(0)->runStream->return).
//
// Notes: GetConstantPool already returns cleanly-named {mppEntries=base, mnCount=count}, so
// the interpreter-member map by name is correct. The console shared-tail operand-stack
// abort-collapse (0x82AE4028) belongs to the OUTER dispatch (CallFunctionDispatch reproduces
// it), so it is NOT duplicated here. The gate flag stays a `static bool` (not const) so the
// path can be quickly toggled off if a future regression needs bisecting.
// ---------------------------------------------------------------------------
static bool g_bAptHomeScriptFnExec = true;   // LIVE: the AS script-function executor is homed + enabled

AptValue* AptActionInterpreter::ExecuteScriptFunction(AptValue* pScope, AptValue* pFunction,
                                                      int nArgs, AptValue* pNewTarget,
                                                      AptValue* pConstructTarget)
{
    AptActionInterpreter* const pInterp = this;
    if (g_bAptHomeScriptFnExec)
    {
        AptScriptFunctionBase* const pFunc = static_cast<AptScriptFunctionBase*>(pFunction);

        // Install the function + its constant pool as current, saving the prior (0x82AE3CB8):
        AptScriptFunctionBase* const pSavedFunc      = pInterp->mpCurrentFunction;       // r24
        const uint32_t               nSavedPoolCount = pInterp->mnConstantPoolCount;     // r23 (ld 0x40)
        AptValue** const             pSavedPool      = pInterp->mpConstantPool;
        pInterp->mpCurrentFunction = pFunc;                                              // [0x3C]
        const AptConstantPool pool = pFunc->GetConstantPool();                           // vtbl 0x50
        pInterp->mnConstantPoolCount = static_cast<uint32_t>(pool.mnCount);              // [0x40] = count
        pInterp->mpConstantPool =
            reinterpret_cast<AptValue**>(const_cast<char**>(pool.mppEntries));           // [0x44] = base

        // Scope guard (0x82AE3CF4): a function bound to an undefined -- or a dead/zombie CIH --
        // reduces to `undefined` rather than running.
        bool bReduced = false;
        AptValue* const pBoundCIH = pFunc->GetCIH();                                     // pFunction->mpCIH
        if (!pBoundCIH->getIsDefined())
        {
            bReduced = true;
        }
        else
        {
            const AptVirtualFunctionTable_Indices eCIHType = pBoundCIH->getVtblIndex();
            if (eCIHType == AptVFT_CharacterInstHandle || eCIHType == AptVFT_CIHNone)
            {
                AptCIH* const  pCIHNode = static_cast<AptCIH*>(pBoundCIH);
                const uint32_t nState   = pCIHNode->GetCIHState();                       // (mFlagsA>>29)&3
                if (nState == 3)
                    bReduced = true;
                else if (pCIHNode->GetCharacterInst()->GetTypeTag() == 0xFu &&
                         (nState == 0 || nState == 1))
                    bReduced = true;
            }
        }

        AptValue* pResult;
        if (!bReduced)
        {
            // Push the scope onto the CIH/target stack + AddRef it; hold the function too (0x82AE3D74).
            pInterp->mpCIHStack[pInterp->mnCIHStackTop] = static_cast<AptCIH*>(pScope);
            ++pInterp->mnCIHStackTop;
            pScope->AddRef();                                                            // vtbl[0]
            pFunc->AddRef();                                                             // vtbl[0]

            // Snapshot the frame stack + register window into this call's saved state (0x82AE3DBC).
            AptScriptFunctionBase::SavedExecutionState savedState;
            pFunc->SetupBeforeExecution(&savedState, pScope, pNewTarget, pConstructTarget); // vtbl 0x54

            // Bind provided args (from the operand-stack top), pad the rest with `undefined` (0x82AE3DE0).
            const int nDeclared = pFunc->GetNumArguments();                              // vtbl 0x44
            int nBind = (nDeclared < nArgs) ? nDeclared : nArgs;                         // min(declared, provided)
            if (nBind > pInterp->mnStackTop)                                             // clamp to available
            {
                nBind = pInterp->mnStackTop;
                nArgs = nBind;                                                           // r25 = r28
            }
            int i = 0;
            for (; i < nBind; ++i)
                pFunc->SetArgument(i, pInterp->mpStack[pInterp->mnStackTop - 1 - i]);    // vtbl 0x58
            for (; i < nDeclared; ++i)
                pFunc->SetArgument(i, gpUndefinedValue);
            pInterp->stackSafePop(nArgs);                                               // drop the consumed args

            // Resolve the animation context for the run (0x82AE3EA8): a CIHNone parent resolves to
            // the level-0 root; else walk up the display-list parents to the enclosing anim(9)/
            // custom-control(15) node.
            AptValue* const pAnim = pFunc->GetParentAnim();                             // mpParentAnim (+0x24)
            AptCIH* pNode;
            if (pAnim->getVtblIndex() == AptVFT_CIHNone)
            {
                pNode = AptGetAnimationAtLevel(0);
            }
            else
            {
                pNode = static_cast<AptCIH*>(pAnim);
                for (;;)
                {
                    const uint32_t nT = pNode->GetCharacterInst()->GetTypeTag();
                    if (nT == 9u || nT == 0xFu)
                        break;
                    pNode = pNode->GetDisplayListParent();                             // walk up (+0x1C)
                }
            }
            AptCharacterInst* const pRunInst = pNode ? pNode->GetCharacterInst() : nullptr;

            // Run the compiled body against the bound CIH + resolved inst (0x82AE3F04).
            const int nSize = pFunc->GetByteCodeSize();                                // vtbl 0x4C
            pInterp->runStream(static_cast<const unsigned char*>(pFunc->GetByteCodeBase()), // vtbl 0x48
                               static_cast<AptCIH*>(pFunc->GetCIH()), nSize, pRunInst);

            // Restore the frame/register window, release the function, pop the scope (0x82AE3F50).
            pFunc->CleanupAfterExecution(&savedState);                                 // vtbl 0x5C
            pFunc->Release();                                                          // vtbl[1] (the AddRef above)
            --pInterp->mnCIHStackTop;                                                  // console AptValue::_pop over +0x24
            pInterp->mpCIHStack[pInterp->mnCIHStackTop]->Release();                    // Release the pushed scope

            pResult = (pInterp->mnStackTop > 0) ? pInterp->mpStack[pInterp->mnStackTop - 1]
                                                : gpUndefinedValue;
        }
        else
        {
            // Reduced path (0x82AE3F88): drop the args, push `undefined`, drop the function's dead
            // CIH binding (Release + reset to the pinned EmptyCIH).
            pInterp->stackSafePop(nArgs);
            pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;
            if (AptValue* const pFuncCIH = pFunc->GetCIH())
                pFuncCIH->Release();                                                   // vtbl[1]
            pFunc->SetCIH(static_cast<AptValue*>(gpAptEmptyCIH));                      // mpCIH = EmptyCIH
            pResult = gpUndefinedValue;
        }

        // Restore the interpreter's current function + constant-pool pair (0x82AE3FEC).
        pInterp->mpCurrentFunction   = pSavedFunc;
        pInterp->mnConstantPoolCount = nSavedPoolCount;
        pInterp->mpConstantPool      = pSavedPool;
        return pResult;
    }

    // --- LIVE (held-off) path: the console's own LABEL_37 reduction (pop the args, push
    // `undefined`) -- a faithful subset for an unrun script function. ---
    (void)pScope; (void)pNewTarget; (void)pConstructTarget;
    pInterp->stackSafePop(nArgs);
    pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;
    return gpUndefinedValue;
}

// FLAG (the AptGC deferred-release flush -- console AptValueVector::ReleaseValues over
// off_8324E51C / gpValuesToRelease; the same boundary the sibling SpecialOps TUs reach
// through AptApt_FlushDeferredReleases): _parseStream drains it once per processed
// opcode (resolve direction) and once up front (unresolve direction). Declared here so
// the transcode walk stays faithful; the host vector type/global are not reconstructed.
extern void AptApt_FlushDeferredReleases();

// FLAG (the per-opcode inline-operand POINTER REBASE -- the only part of _parseStream
// that touches the .apt constant-pool / function-record / dictionary operand SHAPES.
// The console rebases each inline pointer/constant operand of the action just walked by
// nRelocBase (nDirection 0 = resolve = subtract; else = unresolve = add), swapping the
// constant-pool entries against the live value tables (StringPool / AptInteger / AptFloat
// / AptLookup / AptRegister / the boolean & undefined singletons). Those operand-shape
// records live in the AptConstFile pool whose entry layout (console *(a3+0x1C) array of
// {type, ptr} pairs) is NOT yet reconstructed as named members, so the rebase itself is
// the documented .apt-resolve follow-on -- encapsulated, NOT fabricated. The PC-advance
// walk below (which determines this routine's return value, the only thing _parseStream
// consumes) is reconstructed exactly from the PS3 opcode-length switch.):
static void AptInterp_RebaseActionOperands(const unsigned char* /*pAction*/, int /*nOpcode*/,
                                           int /*nRelocBase*/, AptConstFile* /*pResolveCtx*/,
                                           int /*nDirection*/)
{
    // FLAG: the .apt operand pointer/constant rebase is the deferred .apt-resolve
    // follow-on (see the note above). On PC the simplified bundle path hands the action
    // streams already in their live form, so no rebase is applied here; the PC-advance
    // walk in AptActionInterpreter_ResolveTranscode is the faithful, complete part.
}

// ---------------------------------------------------------------------------
// AptActionInterpreter::_parseStream @0x82AF3440 (X360) / 0xF41688 (PS3) -- relocate /
// transcode an action-bytecode stream's inline operands between the on-disk (.apt) and
// live forms (nDirection 0 = resolve, else = unresolve). Walk the stream opcode by
// opcode: for each, advance past its inline operands (the exact per-opcode operand
// length from the .apt action format) and rebase those operands by nRelocBase, draining
// the deferred-release vector between opcodes. Returns the read position just past the
// stream-terminating 0 opcode.
//
// The X360 body is an __asm{bctr} over a gperf jump table (binary DATA absent from the
// code-only X360 export); the PS3 DecFIGS DWARF carries the full lifted switch this is
// reconstructed from. The per-opcode pointer rebase is FLAG-encapsulated (see above).
// ---------------------------------------------------------------------------
const unsigned char* AptActionInterpreter::ResolveTranscode(const unsigned char* pStream,
                                                           int nRelocBase, AptValue* pResolveCtx,
                                                           int nDirection)
{
    AptConstFile* const pCtx = reinterpret_cast<AptConstFile*>(pResolveCtx);
    const bool bUnresolve = (nDirection != 0);   // console v8 = a3 != 0

    // console: the unresolve direction drains the deferred-release vector up front.
    if (bUnresolve)
        AptApt_FlushDeferredReleases();          // console: ReleaseValues(off_8324E51C)

    const unsigned char* p = pStream;
    while (true)
    {
        const unsigned char* const pAction = p;   // console `result` -- the opcode position
        const int nOpcode = *p++;                 // console v9 = *a1; a1 = a1 + 1
        if (nOpcode == 0)
            return pAction;                       // stream terminator -> position of the 0 byte

        // Advance the PC past this opcode's inline operands (the exact .apt action
        // operand length), then rebase those operands.
        const uintptr_t aligned4 =
            (reinterpret_cast<uintptr_t>(p) + 3) & ~static_cast<uintptr_t>(3);
        switch (nOpcode)
        {
            // 4 inline bytes immediately after the opcode (no alignment).
            case 119: case 180: case 183:
                p = pAction + 5;
                break;

            // a single 4-byte-aligned dword operand.
            case 129: case 135: case 153: case 157: case 159: case 184:
                p = reinterpret_cast<const unsigned char*>(aligned4 + 4);
                break;

            // GetUrl-style: two 4-byte-aligned pointer operands.
            case 131:
                p = reinterpret_cast<const unsigned char*>(aligned4 + 8);
                AptInterp_RebaseActionOperands(pAction, nOpcode, nRelocBase, pCtx, nDirection);
                break;

            // DefineDictionary / constant-pool: {count, array-ptr}.
            case 136: case 150:
                p = reinterpret_cast<const unsigned char*>(aligned4 + 8);
                AptInterp_RebaseActionOperands(pAction, nOpcode, nRelocBase, pCtx, nDirection);
                break;

            // a single 4-byte-aligned pointer operand (push-string / get-member forms).
            case 139: case 140: case 161: case 164: case 165: case 166: case 167:
                p = reinterpret_cast<const unsigned char*>(aligned4 + 4);
                AptInterp_RebaseActionOperands(pAction, nOpcode, nRelocBase, pCtx, nDirection);
                break;

            // DefineFunction record (0x1C bytes of name/arg/body pointers).
            case 142:
                p = reinterpret_cast<const unsigned char*>(aligned4 + 28);
                AptInterp_RebaseActionOperands(pAction, nOpcode, nRelocBase, pCtx, nDirection);
                break;

            // Try record (0x14 bytes); the 0x4 flag bit means no rebase.
            case 143:
                p = reinterpret_cast<const unsigned char*>(aligned4 + 20);
                if ((*reinterpret_cast<const int*>(aligned4 + 0xC) & 4) == 0)   // serialized .apt Try record: flag @+0xC
                    AptInterp_RebaseActionOperands(pAction, nOpcode, nRelocBase, pCtx, nDirection);
                break;

            // BranchAlways/If: a single 4-byte-aligned signed PC offset (rebased by the
            // post-operand position, not nRelocBase -- the console adds/subtracts a1).
            case 148:
                p = reinterpret_cast<const unsigned char*>(aligned4 + 4);
                AptInterp_RebaseActionOperands(pAction, nOpcode, nRelocBase, pCtx, nDirection);
                break;

            // DefineFunction2 record (0x18 bytes + register-preload list).
            case 155:
                p = reinterpret_cast<const unsigned char*>(aligned4 + 24);
                AptInterp_RebaseActionOperands(pAction, nOpcode, nRelocBase, pCtx, nDirection);
                break;

            // one trailing operand byte.
            case 162: case 174: case 175: case 176: case 177: case 178: case 179: case 181:
                p = pAction + 2;
                break;

            // two trailing operand bytes.
            case 163: case 182:
                p = pAction + 3;
                break;

            // every other opcode has no inline operand.
            default:
                break;
        }

        // console (@0x82AF3AB0): the per-opcode ReleaseValues drain runs on the UNRESOLVE
        // path only (r25==0 i.e. a3!=0); the resolve direction never drains between opcodes.
        if (bUnresolve)
            AptApt_FlushDeferredReleases();
    }
}
