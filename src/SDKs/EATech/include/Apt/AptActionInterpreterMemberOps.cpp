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
//   * EXTERN (type 11) -> the host extern-object interface (PC-platform leaf, see below).
//   * otherwise / undefined operands -> GetMember pushes `undefined`.
//
// The member name is coerced from the key via AptValue::Get_ToString (string keys
// hand back their own EAStringC; others render via toString). Extern path: for an
// EXTERN (type 11) object the console inlines a raw string-ONLY key extraction
// (assumes the key is already a string: a tag-1 key IS the AptString, any other key
// is read as a boxed AptStringObject) rather than Get_ToString -- reproduced exactly
// below on both the GetMember and SetMember extern arms (the SetMember VALUE renders
// via Get_ToString, matching the console). vtable slots used:
// ContainsNativeHashVirtual() = vtbl[3], SetHasClass() = vtbl[5] (both declared on
// AptValue). GetMember ignores the execution context; SetMember uses its target
// slot (mpPendingReleaseValue, normally null) as the setVariable target, matching
// the console (ctx+8).
//
// The host extern-object interface (AptVFT_Extern type 11): the console reads/
// writes extern members through the host fn-ptr slots dword_8324E858 (get) /
// dword_8324E854 (set), encapsulated as AptExtern_GetMember/SetMember -- PC-platform
// leaves at their AptRenderLinkStubs.cpp definitions (no extern objects register on
// the PC title path, so the un-installed slots answer null/no-op, matching the
// console null fn-ptrs). The deferred-release drain on stack-empty is the homed
// AptFlushDeferredReleases (AptGC.cpp).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"       // AptString::Create / GetInternalString (TypeOf)
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h" // the tag-33 boxed-string form (extern-arm keys)
#include "SDKs/EATech/include/Apt/AptArray.h"              // AptArray::get/set + gpUndefinedValue
#include "SDKs/EATech/include/Apt/AptNativeHash.h"         // StringPool::saConstant (the __proto__ key)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC
#include "SDKs/EATech/include/Apt/AptCIH.h"                // AptCIH::GetCharacterInst (TypeOf CIH tag)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"      // AptCharacterInst::GetTypeTag

#include <cstdint>

// The host extern-object member hooks (console fn-ptr slots dword_8324E858 get /
// dword_8324E854 set) -- PC-platform leaves, defined in AptRenderLinkStubs.cpp.
extern AptValue* AptExtern_GetMember(const char* szName);                       // dword_8324E858
extern void      AptExtern_SetMember(const char* szName, const char* szValue);  // dword_8324E854

// Drain the deferred-release value vector (off_8324E51C) once the operand stack
// empties -- homed in AptGC.cpp over the real gValuesToRelease vector.
extern void AptFlushDeferredReleases();

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

        // Extern object -> the host extern interface. The console inlines a string-
        // ONLY key read (a tag-1 key IS the AptString, any other key is read as a
        // boxed AptStringObject) -- no Get_ToString materialisation on this arm.
        if (eObj == AptVFT_Extern)
        {
            AptString* pKeyStr = (eKey == AptVFT_StringValue)
                ? reinterpret_cast<AptString*>(pKey)
                : static_cast<AptString*>(static_cast<AptStringObject*>(pKey)->GetBoxedString());
            AptValue* pElem = AptExtern_GetMember(
                pKeyStr->GetInternalString()->GetBuffer());   // dword_8324E858 (host slot; leaf in AptRenderLinkStubs.cpp)
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
            if (*pName == StringPool::saConstant[0])
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
            // 3) Extern object -> the host extern interface. The VALUE renders via
            // Get_ToString; the KEY uses the console's string-only read (a tag-1 key
            // IS the AptString, any other key is read as a boxed AptStringObject).
            EAStringC valScratch;
            const EAStringC* pValStr = AptValue::Get_ToString(pValue, &valScratch);
            AptString* pKeyStr = (pKey->getVtblIndex() == AptVFT_StringValue)
                ? reinterpret_cast<AptString*>(pKey)
                : static_cast<AptString*>(static_cast<AptStringObject*>(pKey)->GetBoxedString());
            AptExtern_SetMember(pKeyStr->GetInternalString()->GetBuffer(),
                                pValStr->GetBuffer());   // dword_8324E854 (host slot; leaf in AptRenderLinkStubs.cpp)
        }
        // else: not a settable target -> nothing stored (matches the console).
    }

    pInterp->stackPop(3);                                 // pop object + key + value
    if (pInterp->mnStackTop == 0)
        AptFlushDeferredReleases();   // off_8324E51C drain (homed, AptGC.cpp)
}

// ===========================================================================
// The property / enumerate / cast / typeof opcodes.
//   DECOMPILED from the X360 ARTIST:
//     GetProperty @0x82B08B58 (0x22) -- AS `obj.prop` builtin-property read
//     SetProperty @0x82B08C70 (0x23) -- AS `obj.prop = value` builtin-property write
//     Enumerate   @0x82B05070 (0x46) -- AS `for..in` over a named target
//     CastOp      @0x82AEA818 (0x2B) -- AS `(Class) obj` / dynamic downcast
//     TypeOf      @0x82AF3DD0 (0x44) -- AS `typeof obj` -> the type-name string
//
// GetProperty/SetProperty resolve the object operand to its designated object
// (valueToObject) and then route the access through getVariable / setVariable
// under a builtin-property NAME -- the property index (the second/under operand,
// coerced via toInteger) selects the name from the static property-name table
// (console &dword_8324E580[dword_82F73010[index]]: a remap of the property id
// into the engine's class/property name EAStringC table). Enumerate is a thin
// tail-call into the reconstructed _doEnumerate. CastOp leans on isObjectOfType
// (`instanceof`): a matching object stays on the stack, a non-match collapses to
// `undefined`. TypeOf renders the operand's value type into a fresh AptString.
//
// Dependencies, all landed:
//   * AptActionInterpreter::valueToObject @0x82B07FB8 is a homed member
//     (AptActionInterpreterInterpHelpers.cpp).
//   * gAptPropertyNameTable / gAptPropertyIndexRemap (console dword_8324E580 /
//     dword_82F73010) are defined in AptGlobals.cpp, which carries the remaining
//     table-contents FLAG for the un-recovered rodata.
//   * gAptTypeName_* are defined in AptGlobals.cpp with the literal AS typeof()
//     type-name strings ("undefined"/"number"/"boolean"/"string"/"object"/
//     "movieclip"/"null"/"function").
// ===========================================================================

// AptActionInterpreter::valueToObject @0x82B07FB8 -- a homed member (declared in
// AptActionInterpreter.h, body in AptActionInterpreterInterpHelpers.cpp).

// The static AS builtin-property name table + its id remap (console
// dword_8324E580[dword_82F73010[index]]): gAptPropertyIndexRemap maps a property id
// to its slot in gAptPropertyNameTable. Storage is defined in AptGlobals.cpp, which
// carries the remaining table-contents FLAG for the un-recovered rodata.
extern const EAStringC gAptPropertyNameTable[];   // dword_8324E580
extern const int       gAptPropertyIndexRemap[];  // dword_82F73010

// The AS typeof() type-name literals (entries of the same dword_8324E580 EAStringC
// table) -- defined with their literal contents in AptGlobals.cpp.
extern const EAStringC gAptTypeName_Undefined;   // dword_8324E6C8 "undefined"
extern const EAStringC gAptTypeName_Number;      // dword_8324E64C "number"
extern const EAStringC gAptTypeName_Boolean;     // dword_8324E608 "boolean"
extern const EAStringC gAptTypeName_String;      // dword_8324E6B4 "string"
extern const EAStringC gAptTypeName_Object;      // dword_8324E650 "object"
extern const EAStringC gAptTypeName_MovieClip;   // dword_8324E640 "movieclip"
extern const EAStringC gAptTypeName_Null;        // dword_8324E648 "null"
extern const EAStringC gAptTypeName_Function;    // dword_8324E624 "function"

// ---------------------------------------------------------------------------
// _FunctionAptActionGetProperty @0x82B08B58 (0x22) -- AS `obj.prop` read.
// Stack: [object, propertyIndex] (the index on top). Coerce the object to its
// designated object (valueToObject); on success resolve the builtin-property name
// (the index -> the name table) through getVariable and replace both operands with
// the result, else replace them with `undefined`.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGetProperty(AptActionInterpreter* pInterp,
                                                         LocalContextT* pContext)
{
    AptValue* pIndexValue = pInterp->mpStack[pInterp->mnStackTop - 1];   // TOS = property index
    AptValue* pObjectOper = pInterp->mpStack[pInterp->mnStackTop - 2];   // under-top = the object

    AptValue* pObject = nullptr;
    pInterp->valueToObject(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                       pObjectOper, &pObject);   // homed member (InterpHelpers.cpp)

    if (pObject)
    {
        const int nProperty = pIndexValue->toInteger();
        const EAStringC* pName = &gAptPropertyNameTable[gAptPropertyIndexRemap[nProperty]];   // AptGlobals.cpp tables
        AptValue* pResult = pInterp->getVariable(pObject, pContext->mpPendingReleaseValue,
                                                 pName, 1, 1, 0);
        pInterp->stackPop(2);
        pInterp->mpStack[pInterp->mnStackTop++] = pResult;   // inlined stackPush
        pResult->AddRef();
    }
    else
    {
        pInterp->stackPop(2);
        pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;
        gpUndefinedValue->AddRef();
    }
}

// ---------------------------------------------------------------------------
// _FunctionAptActionSetProperty @0x82B08C70 (0x23) -- AS `obj.prop = value`.
// Stack: [object, propertyIndex, value] (value on top). Coerce the object operand
// to its designated object; on success store the value through setVariable under
// the index-selected builtin-property name, then pop all three operands.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionSetProperty(AptActionInterpreter* pInterp,
                                                         LocalContextT* pContext)
{
    AptValue* pValue      = pInterp->mpStack[pInterp->mnStackTop - 1];   // TOS = the value to store
    AptValue* pIndexValue = pInterp->mpStack[pInterp->mnStackTop - 2];   // under-top = property index
    AptValue* pObjectOper = pInterp->mpStack[pInterp->mnStackTop - 3];   // the object

    AptValue* pObject = nullptr;
    pInterp->valueToObject(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                       pObjectOper, &pObject);   // homed member (InterpHelpers.cpp)

    const int nProperty = pIndexValue->toInteger();
    if (pObject)
    {
        const EAStringC* pName = &gAptPropertyNameTable[gAptPropertyIndexRemap[nProperty]];   // AptGlobals.cpp tables
        pInterp->setVariable(pObject, pContext->mpPendingReleaseValue,
                             pName, pValue, 1, 1, 0);
    }

    pInterp->stackPop(3);   // pop object + index + value
}

// ---------------------------------------------------------------------------
// _FunctionAptActionEnumerate @0x82B05070 (0x46) -- AS `for..in` over a named
// target. A thin tail-call into _doEnumerate with the run scope (ctx.mpCIH) and
// the target slot (ctx.mpPendingReleaseValue).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionEnumerate(AptActionInterpreter* pInterp,
                                                       LocalContextT* pContext)
{
    pInterp->_doEnumerate(pContext->mpCIH, pContext->mpPendingReleaseValue);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionEnumerate2 (0x55; PS3 @0x81C488) -- the ECMA `for..in` form.
// The console body is identical to Enumerate's (both tail-call _doEnumerate with
// ctx.mpCIH + ctx.mpPendingReleaseValue); kept as its own handler, faithful to
// the two distinct console functions (ICF-folded on X360).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionEnumerate2(AptActionInterpreter* pInterp,
                                                        LocalContextT* pContext)
{
    pInterp->_doEnumerate(pContext->mpCIH, pContext->mpPendingReleaseValue);
}

// ---------------------------------------------------------------------------
// _FunctionAptActionCastOp @0x82AEA818 (0x2B) -- AS `(Class) obj`. Stack:
// [object, class] (class on top). When `object instanceof class`, collapse both
// operands to the object (an in-place downcast); otherwise (or when fewer than two
// operands are present) collapse to `undefined`.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionCastOp(AptActionInterpreter* pInterp,
                                                    LocalContextT* /*pContext*/)
{
    if (pInterp->mnStackTop < 2)
    {
        pInterp->stackPopAndPush(pInterp->mnStackTop, gpUndefinedValue);
        return;
    }

    AptValue* pObject = pInterp->mpStack[pInterp->mnStackTop - 1];   // TOS = the object to cast
    AptValue* pClass  = pInterp->mpStack[pInterp->mnStackTop - 2];   // under-top = the class

    if (pInterp->isObjectOfType(pObject, pClass))
    {
        pInterp->stackPopAndPush(2, pObject);   // the cast succeeds -> keep the object
        return;
    }

    pInterp->stackPop(2);
    // The no-match push stores the `undefined` singleton WITHOUT an AddRef
    // (faithful to the console: no refcount bump on this collapse path).
    pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;   // inlined stackPush
}

// ---------------------------------------------------------------------------
// _FunctionAptActionTypeOf @0x82AF3DD0 (0x44) -- AS `typeof obj`. Replace the top
// operand with a fresh AptString holding its ECMA type name. An undefined value is
// "undefined"; otherwise the value type selects the name (number/boolean/string/
// object/null/function), with a CIH resolving through its character instance's type
// tag (a clip/sprite -> "movieclip", a destroyed placeholder -> "undefined", else
// "object"). Unrecognised types leave the freshly-created empty string.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionTypeOf(AptActionInterpreter* pInterp,
                                                    LocalContextT* /*pContext*/)
{
    AptValue* pTop = pInterp->mpStack[pInterp->mnStackTop - 1];   // TOS = the operand

    AptString* pResult = AptString::Create("");   // console seed @0x820046A7 ("" -- overwritten below)
    const EAStringC* pTypeName = nullptr;

    if (!pTop->getIsDefined())
    {
        pTypeName = &gAptTypeName_Undefined;
    }
    else
    {
        const int eType = static_cast<int>(pTop->getVtblIndex());
        switch (eType)
        {
        case AptVFT_Integer:               // 7
        case AptVFT_Float:                 // 6
            pTypeName = &gAptTypeName_Number;
            break;
        case AptVFT_Boolean:               // 5
            pTypeName = &gAptTypeName_Boolean;
            break;
        case AptVFT_StringValue:           // 1
        case AptVFT_StringObject:          // 33
            pTypeName = &gAptTypeName_String;
            break;
        case AptVFT_Object:                // 19
        case AptVFT_Array:                 // 14
            pTypeName = &gAptTypeName_Object;
            break;
        case AptVFT_CharacterInstHandle:   // 12
        case AptVFT_CIHNone:               // 37
        {
            const AptCharacterInst* pInst =
                static_cast<const AptCIH*>(pTop)->GetCharacterInst();
            const uint32_t nTag = pInst->GetTypeTag();   // mTypeFlags >> 26
            if (nTag == 5 || nTag == 16 || nTag == 9)
                pTypeName = &gAptTypeName_MovieClip;
            else if (nTag == 15)
                pTypeName = &gAptTypeName_Undefined;
            else
                pTypeName = &gAptTypeName_Object;
            break;
        }
        case AptVFT_None:                  // 3
            pTypeName = &gAptTypeName_Null;
            break;
        default:
            // Script functions (34/35/36) and native functions (9) -> "function";
            // anything else leaves the empty seed string. (Console: unsigned
            // `(v6 - 34) <= 2` -- the range test must be unsigned.)
            if (static_cast<unsigned int>(eType - 34) <= 2u || eType == AptVFT_NativeFunction)
                pTypeName = &gAptTypeName_Function;
            break;
        }
    }

    if (pTypeName)
        *pResult->GetInternalString() = *pTypeName;   // copy the pooled type-name literal

    pInterp->stackPop();                                 // pop the operand
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;   // inlined stackPush
    pResult->AddRef();
}
