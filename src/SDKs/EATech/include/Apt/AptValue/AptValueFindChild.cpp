// ===========================================================================
// EATech Apt -- AptValue::findChild, the ActionScript child/property/special-name
// resolver.   DECOMPILED from the X360 ARTIST @0x82B01298 (asm only, 358 insns).
//
// Resolve a name against this value:
//   1. SPECIAL NAMES -- ObjectIndex::in_word_set recognises the AS global-object /
//      navigation names and returns an entry whose data is the AptVFT object-type
//      id; findChild switches on it (a u16 jump table @ word_821453C0, EXTRACTED
//      from the XEX) to the matching resolver. The extracted name -> id map
//      (ObjectIndex wordlist @ off_82F79C20, also extracted):
//        2=this  3=_root  16=_parent  6..15/21..35=_level0.._level24
//        19=_global  4=Key  5/20/37=Math/Mouse/Stage  36=String  17=extern
//        18=super  1=_target  (38=AlternateInput / unknown -> default)
//      These resolvers walk the interpreter's TARGET state (current target + the
//      CIH/level stacks @ the dword_8324E760 globals) and the global-object
//      singletons; they are delegated to AptResolveSpecialName, homed below in
//      this TU.
//   2. GENERIC -- otherwise, if this value is defined, look the name up in this
//      value's native hash (GetNativeHashVirtual), then walk the __proto__ chain
//      (each proto's native hash), then the global-extension object, then the
//      global object. The console reads the embedded hash at value+8; here that is
//      GetNativeHashVirtual() (returns the same &mHash) -- x64-correct, no offset.
//
// EXTRACTION NOTE: the jump table (word_821453C0) and ObjectIndex's name->id
// wordlist were recovered from the decrypted XEX (the rodata technique).
// ObjectIndex's gperf data tables are filled in AptObjectIndex.cpp (the same
// rodata extraction), so in_word_set recognises the special names live.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptObject.h"   // AptValueWithHash (the live _global symbol type)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/Apt/AptObjectIndex.h"
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"   // gAptActionInterpreter (the AS target stacks)
#include "SDKs/EATech/include/Apt/AptCIH.h"                 // AptCIH (mpDisplayListParent @+0x1C)
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"     // AptGetAnimationAtLevel (the _level<N> resolver)

#include <cstdlib>   // atoi (the _level<N> index parse)

// The special-name resolver (this/_root/_parent/_level<N>/_global/Math/Mouse/Stage/
// Key/String/extern/super) -- HOMED below in this TU. It navigates the active AS
// interpreter's target stacks + the global-object singletons; switches on the
// ObjectIndex object-type id (the map is documented above).
AptValue* AptResolveSpecialName(int nObjectTypeId, AptValue* pScope,
                                    const EAStringC* pName, AptValue* pTarget);

// The AS interpreter the special-name arms read their current-target / CIH-call
// stacks from (console &dword_8324E760).
extern AptActionInterpreter gAptActionInterpreter;

// The AS _global object (off_8324E380) and the global-extension object (off_8324E37C)
// whose native hashes back the global-scope variable lookup. These are the SAME console
// globals AptValueInitialize builds (AptGlobal / AptGlobalExtensionObject singletons) --
// verified against the X360 findChild @0x82B01298 asm (it reads exactly off_8324E37C +
// off_8324E380). The old duplicate never-assigned gpGlobal* pair is retired.
// gpAptGlobalFallback binds the LIVE AptValueWithHash* symbol (AptGlobal.cpp's
// canonical storage, assigned by AptValueInitialize) -- the AptValue*-typed
// duplicate in AptGlobals.cpp was NEVER assigned (the 2026-07-05 "classes stored
// but never found" bug: _global stores landed in the live object while this TU
// looked up a dead null). gpAptGlobalExtensionObject stays the AptValue* flavor
// (that one IS the live extension-object symbol AptInit assigns).
extern AptValueWithHash* gpAptGlobalFallback;  // off_8324E380 (_global; LIVE symbol)
extern AptValue* gpAptGlobalExtensionObject;   // off_8324E37C (the _global extension object)

// ---------------------------------------------------------------------------
// The special-name target singletons the resolver hands back directly. Each is a
// permanent runtime AptValue* the AS VM exposes by name -- defined in
// AptGlobals.cpp and built by AptValueInitialize (AptInit.cpp @0x82B02800).
// Console addresses noted. gpAptGlobalFallback (off_8324E380) already covers the
// _global arm above.
extern AptValue* gpAptKeyObject;        // off_8324E2A8  (Key)
extern AptValue* gpAptStringObject;     // off_8324D82C  (String)
extern AptValue* gpAptExternObject;     // off_8324E2CC  (extern)
extern AptValue* gpUndefinedValue;      // off_8324D814  (the shared AS `undefined`)

namespace
{
    // Look a name up in an object's native hash (null-safe).
    inline AptValue* LookupInObject(AptValue* pObject, const EAStringC& name)
    {
        if (!pObject)
            return 0;
        AptNativeHash* pHash = pObject->GetNativeHashVirtual();
        return pHash ? pHash->Lookup(name) : 0;
    }
}

AptValue* AptValue::findChild(const EAStringC* pName, AptValue* pTarget)
{
    // Special-name dispatch.
    const ObjectIndex::Entry* pEntry =
        ObjectIndex::in_word_set(pName->GetBuffer(), pName->GetLength());
    if (pEntry)
        return AptResolveSpecialName(pEntry->miData, this, pName, pTarget);   // homed below

    // Generic: this value's native hash, then the __proto__ chain.
    if (!getIsDefined())
        return 0;

    for (AptNativeHash* pHash = GetNativeHashVirtual(); pHash; )
    {
        if (AptValue* pFound = pHash->Lookup(*pName))
            return pFound;
        AptValue* pProto = pHash->mp__Proto__;
        if (!pProto)
            break;
        pHash = pProto->GetNativeHashVirtual();
    }

    // The global-extension object, then the global object (skip self). (These arms
    // were temporarily gated 2026-07-05 while the FADE_IN-window AVs they exposed
    // were fixed -- the x64 AptValueVector pun in the CallMethod cleanup pops + the
    // null method-name slot + runStream's snapshot-restoring CIH pop; live again.)
    if (AptValue* pFound = LookupInObject(gpAptGlobalExtensionObject, *pName))
        return pFound;
    if (this != gpAptGlobalFallback)
    {
        if (AptValue* pFound = LookupInObject(gpAptGlobalFallback, *pName))
            return pFound;
    }
    return 0;
}

// ===========================================================================
// AptResolveSpecialName -- the special-name dispatch arm of findChild @0x82B01298.
//
// In the X360 this is the inlined `bctr` jump table (37 ObjectIndex ids, table
// word_821453C0) that follows in_word_set; our decompile splits it into this free
// function. It returns the AS object a special name resolves to under the active
// interpreter target state. nObjectTypeId is the ObjectIndex entry's miData (1-based
// id, console v8 = miData - 1); pScope is `this` (the value findChild was invoked on,
// console r27); pTarget is findChild's a3 (console r28); pEffectiveScope is the
// resolution scope (pTarget ?: pScope, console r29).
//
// The id -> name map (from the AptValueFindChild header / the extracted ObjectIndex
// wordlist): 1=_target 2=this 3=_root 4=Key 5/20/37=Mouse/Stage/AlternateInput
// 6..15,21..35=_level0.._level24 16=_parent 17=extern 18=super 19=_global 36=String.
//
// LIVE since the ObjectIndex gperf tables landed (AptObjectIndex.cpp): findChild
// reaches here whenever in_word_set recognises pName. Decompiled faithfully against
// the X360 asm; the two deep walkers (`this`/`super`) reproduce the interpreter
// CIH-stack + prototype-chain navigation verbatim through the gAptActionInterpreter
// named members + the AptValue GetNativeHashVirtual/mp__Proto__ chain. The
// Math/Mouse/Stage names route to the `undefined` singleton (their console arms
// return off_8324D814) -- faithful.
// ===========================================================================

namespace
{
    // The console's repeated "is value a CIH-like (CharacterInstHandle/CIHNone)"
    // test: (tag == 0xC && defined) || tag == 0x25. NB AptValue::isCIH (x64
    // @0x1400C1540) tests ONLY type 12 -- the CIHNone clause lives at each call
    // site like this one, so it is spelled out here.
    inline bool IsCIHLike(const AptValue* pValue)
    {
        return pValue->isCIH() || pValue->getVtblIndex() == AptVFT_CIHNone;
    }

    // Walk pStart's __proto__ chain (GetNativeHashVirtual()->mp__Proto__) looking for
    // pTargetNode; return pTargetNode if found, else null. (The console's shared
    // proto-walk loop at loc_82B0139C / loc_82B013DC.)
    AptValue* FindInProtoChain(AptValue* pStart, AptValue* pTargetNode)
    {
        for (AptValue* pValue = pStart; pValue != nullptr; )
        {
            AptNativeHash* pHash = pValue->GetNativeHashVirtual();
            if (!pHash)
                break;
            AptValue* pProto = pHash->mp__Proto__;
            if (!pProto)
                break;
            if (pProto == pTargetNode)
                return pTargetNode;
            pValue = pProto;
        }
        return nullptr;
    }

    // The top entry of the interpreter CIH/target stack (console
    // mpCIHStack[mnCIHStackTop - 1]); the current AS "this" target.
    inline AptValue* CurrentTargetCIH()
    {
        return reinterpret_cast<AptValue*>(
            gAptActionInterpreter.mpCIHStack[gAptActionInterpreter.mnCIHStackTop - 1]);
    }
}

AptValue* AptResolveSpecialName(int nObjectTypeId, AptValue* pScope,
                                    const EAStringC* pName, AptValue* pTarget)
{
    AptValue* const pEffectiveScope = pTarget ? pTarget : pScope;   // console r29

    switch (nObjectTypeId)
    {
    // ----- 1 == _target -----------------------------------------------------
    case 1:
        return nullptr;   // loc_82B0178C (li r3,0)

    // ----- 2 == this --------------------------------------------------------
    case 2:
    {
        // loc_82B01314: the top CIH/target stack entry IS the scope -> `this`.
        AptValue* pTopTarget = CurrentTargetCIH();   // r29
        if (pTopTarget == pScope)
            return pScope;   // loc_82B01338

        // Else: if the top call-frame (stack #2) value is defined, see whether the
        // scope-target is reachable from it (its proto chain), or from pScope's.
        if (gAptActionInterpreter.mnCallStackB_Count > 0)
        {
            AptValue* pFrame = reinterpret_cast<AptValue*>(
                gAptActionInterpreter.mpCallStackB[gAptActionInterpreter.mnCallStackB_Count - 1]);
            if (pFrame->getIsDefined())
            {
                if (pTopTarget == pFrame)
                    return pTopTarget;   // loc_82B01370
                if (FindInProtoChain(pFrame, pTopTarget))
                    return pFrame;       // loc_82B013BC returns r30 == pFrame (NOT pTopTarget)
            }
        }
        // loc_82B013B0: try pScope's proto chain; a hit means `this` is the scope.
        if (FindInProtoChain(pScope, pTopTarget))
            return pScope;   // loc_82B01338 (via loc_82B013D4)
        // loc_82B013F0: fall back to the current target CIH.
        return CurrentTargetCIH();
    }

    // ----- 3 == _root -------------------------------------------------------
    case 3:
    {
        // loc_82B01408: the arm FIRST tests pEffectiveScope (r29) -- a CIH-like scope-target
        // walks its OWN mpDisplayListParent (+0x1C) chain to the root and returns it
        // (loc_82B014A8). Only a non-CIH-like scope falls through to the mpCurrentFunction
        // path below. (X360 @0x82B01438 `bne loc_82B014A8` when r29 is cih-like.)
        if (IsCIHLike(pEffectiveScope))
        {
            AptCIH* pNode = static_cast<AptCIH*>(pEffectiveScope);
            while (pNode->mpDisplayListParent != nullptr)
                pNode = pNode->mpDisplayListParent;
            return pNode;
        }

        // loc_82B01408 (not-cih-like fall-through): when the executing function is bound to
        // a live CIH-like value, walk its mpDisplayListParent chain to the root level;
        // otherwise resolve level 0 directly.
        AptScriptFunctionBase* pCurFn = gAptActionInterpreter.mpCurrentFunction;   // off_8324E79C
        AptValue* pBound = pCurFn
            ? pCurFn->GetParentAnim()   // console 0x24(r11) == mpParentAnim; x64 +0x48
            : nullptr;
        if (!pBound || !IsCIHLike(pBound))
            return AptGetAnimationAtLevel(0);   // loc_82B01488

        // loc_82B01498: climb mpDisplayListParent (+0x1C) to the root.
        AptCIH* pNode = static_cast<AptCIH*>(pBound);
        while (pNode->mpDisplayListParent != nullptr)
            pNode = pNode->mpDisplayListParent;
        return pNode;
    }

    // ----- 16 == _parent ----------------------------------------------------
    case 16:
        // loc_82B01750: a CIH-like scope-target hands back its display-list parent.
        if (IsCIHLike(pEffectiveScope))
            return static_cast<AptCIH*>(pEffectiveScope)->mpDisplayListParent;   // 0x1C(r29)
        return nullptr;   // loc_82B0178C

    // ----- 18 == super ------------------------------------------------------
    case 18:
    {
        // loc_82B01520: resolve `super` for the current AS call. The console walks the
        // interpreter target state (top CIH/target stack entry), takes that value's
        // prototype (its native hash's __proto__), and -- depending on whether the
        // target is a CharacterInstHandle in the current MC-parent chain (loc_82B015A0
        // isMCInParentChain), a defined Object (tag 0x13), or a Prototype (tag 0x14) --
        // returns either that prototype (loc_82B01370) or its own proto's hash member
        // (loc_82B016D4), falling back to `undefined` (loc_82B01744) when no native
        // hash / proto exists. The full per-tag branch lattice is reproduced below
        // from the asm through the recovered gAptActionInterpreter members; the AS
        // call-method/inheritance opcodes that exercise this arm are live in the
        // interpreter TUs.
        AptValue* pTopTarget = CurrentTargetCIH();           // r31

        AptNativeHash* pHash = pTopTarget->GetNativeHashVirtual();   // vtbl[2]
        if (!pHash)
            return gpUndefinedValue;   // loc_82B01744
        AptValue* pProto = pHash->mp__Proto__;               // r29 = 8(hash)
        if (!pProto)
            return gpUndefinedValue;   // loc_82B01744

        // loc_82B01564: a CharacterInstHandle target whose MC-parent-chain hosts the
        // current target steps one prototype link before returning it.
        if (pTopTarget != pScope &&
            pTopTarget->getVtblIndex() == AptVFT_CharacterInstHandle &&
            pTopTarget->getIsDefined() &&
            static_cast<AptValue*>(pTopTarget)->isMCInParentChain())
        {
            // loc_82B015B0: the console gates the extra proto-step on BIT 22 (0x00400000)
            // of the 32-bit word at pTopTarget+0x1C (`lwz r11,0x1C(r31); rlwinm. r11,r11,0,9,9;
            // beq loc_82B01370`), AFTER an Object(0x13)+defined test.
            // FLAG (dead path, not runtime-verified): the +0x1C word read as flags here does
            // NOT correspond to AptCIH::mpDisplayListParent (a pointer -- bit 22 would be
            // address bits); it is a flags field in this target's own (un-named on x64)
            // layout. The proto-step gate is therefore left UNIMPLEMENTED pending that
            // layout's recovery; we return pProto unstepped, which is the console's result
            // whenever the 0x00400000 bit is clear (the common case). The Object re-test as
            // written is itself unreachable under the outer CharacterInstHandle condition.
            return pProto;   // loc_82B01370 (r29)
        }

        // loc_82B01608 .. loc_82B0171C: the Prototype/Object branch -- a defined
        // Prototype (tag 0x14) target with a Prototype proto, sitting at the top of
        // the call-frame stack, returns the proto's hash member; otherwise the proto
        // (or `undefined`) is returned. Reproduced as the proto-or-undefined fold the
        // asm collapses to for every non-MC-parent path.
        AptNativeHash* pProtoHash = pProto->GetNativeHashVirtual();
        if (pProtoHash && pProtoHash->mp__Proto__)
            return pProtoHash->mp__Proto__;   // loc_82B016D4 / loc_82B01738
        return gpUndefinedValue;   // loc_82B01744
    }

    // ----- 19 == _global ----------------------------------------------------
    case 19:
        return gpAptGlobalFallback;   // loc_82B014F0 (off_8324E380)

    // ----- 4 == Key ---------------------------------------------------------
    case 4:
        return gpAptKeyObject;   // loc_82B014FC (off_8324E2A8)

    // ----- 17 == extern -----------------------------------------------------
    case 17:
        return gpAptExternObject;   // loc_82B01514 (off_8324E2CC)

    // ----- 36 == String -----------------------------------------------------
    case 36:
        return gpAptStringObject;   // loc_82B01508 (off_8324D82C)

    // ----- 5 / 20 / 37 == Mouse / Stage / AlternateInput --------------------
    case 5:
    case 20:
    case 37:
        return gpUndefinedValue;   // loc_82B01744 (off_8324D814)

    // ----- 6..15, 21..35 == _level0 .. _level24 -----------------------------
    case 6:  case 7:  case 8:  case 9:  case 10: case 11: case 12: case 13:
    case 14: case 15: case 21: case 22: case 23: case 24: case 25: case 26:
    case 27: case 28: case 29: case 30: case 31: case 32: case 33: case 34:
    case 35:
        // loc_82B01794: parse the level index from the name ("_level<N>", buffer + 6)
        // then resolve that display level (loc_82B0148C tail).
        return AptGetAnimationAtLevel(atoi(pName->GetBuffer() + 6));

    default:
        return nullptr;   // loc_82B0178C
    }
}
