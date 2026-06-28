// ===========================================================================
// EATech Apt -- the ActionScript member opcodes GetMember (0x4E) and SetMember
// (0x4F).   DECOMPILED from the X360 ARTIST: _FunctionAptActionGetMember
// @0x82B04200, _FunctionAptActionSetMember @0x82B043C8.
//
// Object/array property access -- `object[key]` and `object[key] = value`. Both
// dispatch on the object's value type (meValueType):
//   * ARRAY (type 14) with a NUMERIC key (Integer 7 / Float 6) -> direct element
//     access (AptArray::get -> undefined out-of-range / SetMember AptArray::set).
//     An array with a non-numeric key falls through to the object path (arrays
//     carry a native hash, so ContainsNativeHashVirtual() is true -> named member).
//   * OBJECT (ContainsNativeHashVirtual(), or a CharacterInstHandle type 12 / the
//     unnamed scriptable tag 37) -> the named-member path: getVariable / setVariable
//     resolved DIRECTLY against the object (nDirect=1, no scope-chain search).
//     SetMember additionally marks the object as class-bearing (SetHasClass) when
//     the assigned member is the prototype key (__proto__ == StringPool::saConstant)
//     and the object is _global (19) / a CIH (12) / tag 37.
//   * EXTERN (type 11) -> the host extern-object interface (FLAG'd, see below).
//   * otherwise / undefined operands -> GetMember pushes `undefined`.
//
// The member name is coerced from the key via AptValue::Get_ToString (string keys
// hand back their own EAStringC; others render via toString). vtable slots used:
// ContainsNativeHashVirtual() = vtbl[3], SetHasClass() = vtbl[5] (both declared on
// AptValue). GetMember ignores the execution context; SetMember uses its target
// slot (mpPendingReleaseValue, normally null) as the setVariable target, matching
// the console (ctx+8).
//
// FLAG -- the host extern-object interface (AptVFT_Extern type 11): the console
// reads/writes extern members through fn-ptrs dword_8324E858 (get) / dword_8324E854
// (set). The extern subsystem is host-provided and not reconstructed, so the two
// hooks are encapsulated as AptExtern_GetMember/SetMember. Also FLAG'd: the
// deferred-release vector drain on stack-empty (AptApt_FlushDeferredReleases, the
// AptGC layer; the stack-empty guard is faithful + engine-side).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptArray.h"              // AptArray::get/set + gpUndefinedValue
#include "SDKs/EATech/include/Apt/AptNativeHash.h"         // StringPool::saConstant (the __proto__ key)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC

// FLAG (host extern-object interface -- AptVFT_Extern type 11; console fn-ptrs
// dword_8324E858 get / dword_8324E854 set): the extern subsystem is host-provided
// and not reconstructed; the member get/set route through these hooks.
extern AptValue* AptExtern_GetMember(const char* szName);                       // dword_8324E858
extern void      AptExtern_SetMember(const char* szName, const char* szValue);  // dword_8324E854

// FLAG (AptGC layer -- AptValueVector): drain the deferred-release value vector
// (off_8324E51C) once the operand stack empties. See AptActionInterpreterVarOps.cpp.
extern void AptApt_FlushDeferredReleases();

namespace
{
    // The unnamed 7-bit meValueType tag 37 (0x25) -- a CIH/target-like scriptable
    // value the member dispatch treats alongside the CharacterInstHandle (12).
    const AptVirtualFunctionTable_Indices AptVFT_Tag37 =
        static_cast<AptVirtualFunctionTable_Indices>(37);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionGetMember @0x82B04200 -- opcode 0x4E, AS `object[key]` read.
// Stack: [object, key] (key on top); replace both with the looked-up member.
// (The execution context is unused -- the console handler takes only `this`.)
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGetMember(AptActionInterpreter* pInterp,
                                                       LocalContextT* /*pContext*/)
{
    AptValue* pObject = pInterp->mpStack[pInterp->mnStackTop - 2];   // the object
    AptValue* pKey    = pInterp->mpStack[pInterp->mnStackTop - 1];   // the member key

    if (pObject->getIsDefined() && pKey->getIsDefined())
    {
        const AptVirtualFunctionTable_Indices eObj = pObject->getVtblIndex();
        const AptVirtualFunctionTable_Indices eKey = pKey->getVtblIndex();

        // Array + numeric index -> direct element (bounds/null -> undefined).
        if (eObj == AptVFT_Array && (eKey == AptVFT_Integer || eKey == AptVFT_Float))
        {
            AptValue* pElem = static_cast<AptArray*>(pObject)->get(pKey->toInteger());
            pInterp->stackPopAndPush(2, pElem);
            return;
        }

        // Extern object -> the host extern interface.
        if (eObj == AptVFT_Extern)
        {
            EAStringC scratch;
            const EAStringC* pName = AptValue::Get_ToString(pKey, &scratch);
            AptValue* pElem = AptExtern_GetMember(pName->GetBuffer());   // FLAG: dword_8324E858
            pInterp->stackPopAndPush(2, pElem);
            return;
        }

        // Generic object -> resolve the named member directly on the object.
        EAStringC scratch;
        const EAStringC* pName = AptValue::Get_ToString(pKey, &scratch);
        AptValue* pResult = pInterp->getVariable(pObject, 0, pName, 1, 0, 1);
        pInterp->stackPopAndPush(2, pResult);
        return;
    }

    // Undefined object or key -> push `undefined`.
    pInterp->stackPop(2);
    pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;   // inlined stackPush
    gpUndefinedValue->AddRef();
}

// ---------------------------------------------------------------------------
// _FunctionAptActionSetMember @0x82B043C8 -- opcode 0x4F, AS `object[key] = value`.
// Stack: [object, key, value] (value on top); store, then pop all three.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionSetMember(AptActionInterpreter* pInterp,
                                                       LocalContextT* pContext)
{
    AptValue* pObject = pInterp->mpStack[pInterp->mnStackTop - 3];   // the object
    AptValue* pKey    = pInterp->mpStack[pInterp->mnStackTop - 2];   // the member key
    AptValue* pValue  = pInterp->mpStack[pInterp->mnStackTop - 1];   // the value to store

    bool bStored = false;

    // 1) Array + numeric index -> direct element store.
    if (pObject->getVtblIndex() == AptVFT_Array && pObject->getIsDefined())
    {
        const AptVirtualFunctionTable_Indices eKey = pKey->getVtblIndex();
        if ((eKey == AptVFT_Integer || eKey == AptVFT_Float) && pKey->getIsDefined())
        {
            static_cast<AptArray*>(pObject)->set(pKey->toInteger(), pValue);
            bStored = true;
        }
    }

    if (!bStored)
    {
        const AptVirtualFunctionTable_Indices eObj = pObject->getVtblIndex();
        const bool bObjectPath = pObject->ContainsNativeHashVirtual()
            || (eObj == AptVFT_CharacterInstHandle && pObject->getIsDefined())
            || eObj == AptVFT_Tag37;

        if (bObjectPath)
        {
            // 2) Named member -> store directly on the object.
            EAStringC scratch;
            const EAStringC* pName = AptValue::Get_ToString(pKey, &scratch);
            pInterp->setVariable(pObject, pContext->mpPendingReleaseValue, pName, pValue, 1, 0, 1);

            // Assigning __proto__ to _global / a CIH / tag-37 marks it class-bearing.
            if (*pName == StringPool::saConstant)
            {
                const AptVirtualFunctionTable_Indices t = pObject->getVtblIndex();
                if ((t == AptVFT_Object && pObject->getIsDefined())
                    || (t == AptVFT_CharacterInstHandle && pObject->getIsDefined())
                    || t == AptVFT_Tag37)
                    pObject->SetHasClass(1);
            }
        }
        else if (pObject->getVtblIndex() == AptVFT_Extern && pObject->getIsDefined())
        {
            // 3) Extern object -> the host extern interface.
            EAStringC keyScratch, valScratch;
            const EAStringC* pKeyStr = AptValue::Get_ToString(pKey, &keyScratch);
            const EAStringC* pValStr = AptValue::Get_ToString(pValue, &valScratch);
            AptExtern_SetMember(pKeyStr->GetBuffer(), pValStr->GetBuffer());   // FLAG: dword_8324E854
        }
        // else: not a settable target -> nothing stored (matches the console).
    }

    pInterp->stackPop(3);                                 // pop object + key + value
    if (pInterp->mnStackTop == 0)
        AptApt_FlushDeferredReleases();   // FLAG: off_8324E51C / AptValueVector::ReleaseValues
}
