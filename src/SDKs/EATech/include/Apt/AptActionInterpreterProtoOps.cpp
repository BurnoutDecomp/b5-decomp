// ===========================================================================
// EATech Apt -- the ActionScript class opcodes Extends and ImplementsOp.
//   DECOMPILED from the X360 ARTIST:
//     Extends      @0x82AF41B8 (0x69) -- `class B extends A`: chain B.prototype to A
//     ImplementsOp @0x82AF40A0... (0x2C) -- `implements I0..In`: record the
//                                           implemented interfaces' prototypes
//
// Extends ensures both the sub- and super-class hashes have an AptPrototype, sets
// the subclass prototype's super-constructor, marks both class-bearing, and links
// the subclass prototype's __proto__ to the superclass prototype (the inheritance
// chain). ImplementsOp collects each interface's prototype into an AptArray and
// hands it to AptObject::SetImplementedObjects on the class. Both apply only to
// script-function (type 34..36) / native-function (9) class values.
//
// Fully resolved against the landed value layer -- no FLAGs (AptNativeHash::Get/
// SetPrototype/Set__Proto__, AptPrototype + SetSuperConstructor, AptObject::
// SetImplementedObjects, AptArray are all reconstructed + public).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"      // AptValue + toInteger / Get_ToString
#include "SDKs/EATech/include/Apt/AptNativeHash.h"          // Get/SetPrototype, Set__Proto__, Set
#include "SDKs/EATech/include/Apt/AptPrototype.h"           // AptPrototype + SetSuperConstructor
#include "SDKs/EATech/include/Apt/AptObject.h"              // AptObject::SetImplementedObjects
#include "SDKs/EATech/include/Apt/AptArray.h"               // AptArray (interfaces) + gpUndefinedValue
#include "SDKs/EATech/include/Apt/AptString/EAString.h"     // EAStringC (the coerced names)
#include "SDKs/EATech/include/Apt/AptScriptFunction1.h"     // DefineFunction value + its record
#include "SDKs/EATech/include/Apt/AptScriptFunction2.h"     // DefineFunction2 value + its record
#include "SDKs/EATech/include/Apt/AptCIH.h"                 // ctx->mpCIH -> AptValue* upcast

#include <cstdint>
#include <new>      // placement new (operator new + ctor at the GC-pool slot)

// FLAG (AptObject layer -- Adriwin's class:AptObject TU): record the interface
// prototypes a class implements (console AptObject::SetImplementedObjects). Not
// yet a member of the reconstructed AptObject, so encapsulated as a free helper.
extern void AptObject_SetImplementedObjects(AptObject* pObject, AptArray* pInterfaces, int nCount);

// FLAG (AptActionInterpreter::_createObject @0x82B08088 -- the value-materialiser).
// It is a (private) member of the interpreter in the console, but is NOT declared in
// the reconstructed AptActionInterpreter.h yet (that header is owned by Phase A and
// not editable here), so the New/Init/Define object handlers reach it through a free
// shim -- the same encapsulation this TU already uses for AptObject_SetImplementedObjects.
// Builds the AS object/array/class value for class name pClassName under (pScope,
// pTarget); the trailing int/char are the array-length hint + the "construct" flag.
extern AptValue* AptActionInterpreter_createObject(AptActionInterpreter* pInterp,
                                                   AptValue* pScope, AptValue* pTarget,
                                                   const EAStringC* pClassName,
                                                   int nArrayLenHint, char bConstruct);

// FLAG (engine rodata -- console &dword_8324E650, an entry of the static EAStringC
// class-name table at dword_8324E580): the generic "Object" class name InitObject
// hands to _createObject. Provided by the engine's constant-string table; declared
// here so the handler can name it (the table data itself is the data-segment follow-on).
extern const EAStringC gAptObjectClassName;   // &dword_8324E650

namespace
{
    // A defined class value: a script function (the unnamed type tags 34..36) or a
    // native function (AptVFT_NativeFunction = 9).
    inline bool IsScriptClass(const AptValue* p)
    {
        const int e = static_cast<int>(p->getVtblIndex());
        return e >= 34 && e <= 36 && p->getIsDefined();
    }
}

// ---------------------------------------------------------------------------
// Extends @0x82AF41B8 (0x69) -- link the subclass prototype to the superclass.
// Stack: [subclass, superclass] (superclass on top).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionExtends(AptActionInterpreter* pInterp,
                                                     LocalContextT* /*pContext*/)
{
    AptValue* pSuper = pInterp->mpStack[pInterp->mnStackTop - 1];
    AptValue* pSub   = pInterp->mpStack[pInterp->mnStackTop - 2];

    if (pSuper->ContainsNativeHashVirtual() && IsScriptClass(pSub))
    {
        AptNativeHash* pSuperHash = pSuper->GetNativeHashVirtual();
        AptNativeHash* pSubHash   = pSub->GetNativeHashVirtual();
        AptValue* pSuperProto = pSuperHash->GetPrototype();
        AptValue* pSubProto   = pSubHash->GetPrototype();
        if (!pSuperProto) { pSuperProto = new AptPrototype(); pSuperHash->SetPrototype(pSuperProto); }
        if (!pSubProto)   { pSubProto   = new AptPrototype(); pSubHash->SetPrototype(pSubProto); }

        static_cast<AptPrototype*>(pSubProto)->SetSuperConstructor(pSuper);
        pSuper->SetHasClass(1);
        pSub->SetHasClass(1);
        pSubProto->GetNativeHashVirtual()->Set__Proto__(pSuperProto);   // the inheritance link
    }

    pInterp->stackPop(2);
}

// ---------------------------------------------------------------------------
// ImplementsOp @0x82AF40A0 (0x2C) -- record the interfaces a class implements.
// Stack: [iface_{n-1}..iface_0, count, class] (class on top).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionImplementsOp(AptActionInterpreter* pInterp,
                                                          LocalContextT* /*pContext*/)
{
    AptValue* pClass = pInterp->mpStack[pInterp->mnStackTop - 1];
    const int nCount = pInterp->mpStack[pInterp->mnStackTop - 2]->toInteger();

    if (IsScriptClass(pClass)
        || (pClass->getVtblIndex() == AptVFT_NativeFunction && pClass->getIsDefined()))
    {
        AptArray* pInterfaces = new AptArray();
        for (int i = 0; i < nCount; ++i)
        {
            AptValue* pIface = pInterp->mpStack[pInterp->mnStackTop - i - 3];
            AptNativeHash* pIfaceHash = pIface->GetNativeHashVirtual();
            AptValue* pProto = pIfaceHash->GetPrototype();
            if (!pProto) { pProto = new AptPrototype(); pIfaceHash->SetPrototype(pProto); }
            pInterfaces->set(i, pProto);
        }
        if (pClass->GetNativeHashVirtual())
            AptObject_SetImplementedObjects(static_cast<AptObject*>(pClass), pInterfaces, nCount);  // FLAG
    }

    pInterp->stackPop(nCount + 2);
}

// ---------------------------------------------------------------------------
// NewObject @0x82B08DE0 (0x40) -- AS `new ClassName(args...)`: coerce the class
// name (top), the arg count (under top), pop those two, materialise the object via
// _createObject (with the construct flag), then push it -- or `undefined` when the
// class can't be constructed. Stack: [argCount, className] (className on top); the
// constructor's args have already been collapsed by the caller.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionNewObject(AptActionInterpreter* pInterp,
                                                       LocalContextT* pContext)
{
    AptValue* pNameVal  = pInterp->mpStack[pInterp->mnStackTop - 1];   // class name (top)
    AptValue* pCountVal = pInterp->mpStack[pInterp->mnStackTop - 2];   // arg count (under)

    EAStringC scratch;
    const EAStringC* pClassName = AptValue::Get_ToString(pNameVal, &scratch);
    const int nArgCount = pCountVal->toInteger();

    pInterp->stackPop(2);

    AptValue* pObject = AptActionInterpreter_createObject(   // FLAG: _createObject member
        pInterp, static_cast<AptValue*>(pContext->mpCIH),
        pContext->mpPendingReleaseValue, pClassName, nArgCount, 1);

    if (pObject)
    {
        pInterp->mpStack[pInterp->mnStackTop++] = pObject;   // inlined stackPush
        pObject->AddRef();                                   // the stack takes a ref
        pObject->Release();                                  // drop the construction ref
    }
    else
    {
        pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;
        gpUndefinedValue->AddRef();
    }
}

// ---------------------------------------------------------------------------
// InitObject @0x82B08EE8 (0x43) -- AS object literal `{ k0:v0, k1:v1, ... }`:
// pop the member-pair count, build a generic Object via _createObject, then set
// each (name, value) pair into the object's native hash (top-down), pop the pairs,
// and push the object (or `undefined` on failure -- pairs are SafePop'd). Stack:
// [ ...(value_i, name_i)..., pairCount ] (pairCount on top).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionInitObject(AptActionInterpreter* pInterp,
                                                        LocalContextT* pContext)
{
    const int nPairs = pInterp->mpStack[pInterp->mnStackTop - 1]->toInteger();
    pInterp->stackPop();                                     // pop the pair count

    AptValue* pObject = AptActionInterpreter_createObject(   // FLAG: _createObject member
        pInterp, static_cast<AptValue*>(pContext->mpCIH),
        pContext->mpPendingReleaseValue, &gAptObjectClassName, 0, 1);   // FLAG: class-name rodata

    if (pObject)
    {
        AptNativeHash* pHash = pObject->GetNativeHashVirtual();   // console Object+8

        int nPairLeft = nPairs;
        int nFromTop  = 0;
        while (nPairLeft > 0)
        {
            // top-down: each pair is [value, name] with name above value.
            AptValue* pNameVal = pInterp->mpStack[pInterp->mnStackTop - nFromTop - 2];
            AptValue* pValue   = pInterp->mpStack[pInterp->mnStackTop - nFromTop - 1];

            EAStringC scratch;
            const EAStringC* pName = AptValue::Get_ToString(pNameVal, &scratch);
            pHash->Set(*pName, pValue);

            --nPairLeft;
            nFromTop += 2;
        }

        pInterp->stackPop(2 * nPairs);                       // pop the consumed pairs
        pInterp->mpStack[pInterp->mnStackTop++] = pObject;   // inlined stackPush
        pObject->AddRef();
        pObject->Release();                                  // drop the construction ref
    }
    else
    {
        pInterp->stackSafePop(2 * nPairs);
        pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;
        gpUndefinedValue->AddRef();
    }
}

// ---------------------------------------------------------------------------
// DefineFunction @0x82B05080 (0x9B) -- AS `function name(args) { body }` (v1 form).
// The compiled-function record is read inline from the action bytecode (PC); advance
// the PC past the record + body, patch the interpreter's two constant-pool registers
// into the record, build an AptScriptFunction1 value, then either bind it under its
// name (setVariable) for a named function, or push it for an anonymous one.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDefineFunction(AptActionInterpreter* pInterp,
                                                            LocalContextT* pContext)
{
    // The record begins at the 4-byte-aligned PC; the body follows the 24-byte header.
    AptScriptFunction1ByteCode* pByteCode = reinterpret_cast<AptScriptFunction1ByteCode*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));

    // Advance the PC past the header (24 bytes) + the inline bytecode body.
    pContext->mpProgramCounter =
        reinterpret_cast<const unsigned char*>(pByteCode) + 24 + pByteCode->mnByteCodeSize;

    // Patch the interpreter's constant-pool registers (the movie string dictionary)
    // into the record. FLAG: the console writes the interpreter's +0x40 slot into the
    // record's +0x10 field and +0x44 into +0x14; on x64 the dictionary base is an
    // 8-byte pointer, so it is kept in the pointer-typed mppConstantPool (mpRegisters)
    // and the count in mnConstantPoolCount (field_40) rather than the literal console
    // slot order, which would truncate the pointer.
    pByteCode->mppConstantPool     = reinterpret_cast<const char**>(
                                         reinterpret_cast<uintptr_t>(pInterp->mpRegisters));
    pByteCode->mnConstantPoolCount = static_cast<int32_t>(pInterp->field_40);

    EAStringC name(pByteCode->mpName);   // console InitFromBuffer(&v10, mpName)

    void* pMem = AptScriptFunction1::operator new(52);
    AptScriptFunction1* pFunction = pMem
        ? ::new (pMem) AptScriptFunction1(static_cast<AptValue*>(pInterp->mpCurrentFunction),
                                          pByteCode, static_cast<AptValue*>(pContext->mpCIH))
        : 0;

    if (pByteCode->mpName[0])
    {
        // Named function -> bind it in scope under its name.
        pInterp->setVariable(static_cast<AptValue*>(pContext->mpCIH),
                             pContext->mpPendingReleaseValue, &name,
                             pFunction, 1, 1, 0);
    }
    else
    {
        // Anonymous function -> leave it on the operand stack.
        pInterp->mpStack[pInterp->mnStackTop++] = pFunction;   // inlined stackPush
        pFunction->AddRef();
    }
}

// ---------------------------------------------------------------------------
// DefineFunction2 @0x82B051A0 (0x8E) -- AS `function name(args) { body }` (v2 form,
// with the declared register layout + preload flags). As DefineFunction but the v2
// record carries the extra register-table header (28-byte header, mnBodyLength
// advances the PC) and builds an AptScriptFunction2 value.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDefineFunction2(AptActionInterpreter* pInterp,
                                                             LocalContextT* pContext)
{
    AptScriptFunction2ByteCode* pByteCode = reinterpret_cast<AptScriptFunction2ByteCode*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));

    // Advance the PC past the header (28 bytes) + the inline bytecode body
    // (mnBodyLength == v4[4], the dword at +0x10).
    pContext->mpProgramCounter =
        reinterpret_cast<const unsigned char*>(pByteCode) + 28 + pByteCode->mnBodyLength;

    // Patch the constant-pool registers in (same x64 pointer-width note as v1). FLAG.
    pByteCode->mppConstantPool     = reinterpret_cast<const char**>(
                                         reinterpret_cast<uintptr_t>(pInterp->mpRegisters));
    pByteCode->mnConstantPoolCount = static_cast<int32_t>(pInterp->field_40);

    void* pMem = AptScriptFunction2::operator new(52);
    AptScriptFunction2* pFunction = pMem
        ? ::new (pMem) AptScriptFunction2(static_cast<AptValue*>(pInterp->mpCurrentFunction),
                                          pByteCode, static_cast<AptValue*>(pContext->mpCIH))
        : 0;

    if (pByteCode->mpName[0])
    {
        // Named function -> bind it in scope under its name.
        EAStringC name(pByteCode->mpName);
        pInterp->setVariable(static_cast<AptValue*>(pContext->mpCIH),
                             pContext->mpPendingReleaseValue, &name,
                             pFunction, 1, 1, 0);
    }
    else
    {
        // Anonymous function -> leave it on the operand stack.
        pInterp->mpStack[pInterp->mnStackTop++] = pFunction;   // inlined stackPush
        pFunction->AddRef();
    }
}

// ---------------------------------------------------------------------------
// DefineDictionary @0x82AD9278 (0x88) -- install the movie's ActionScript string
// dictionary (constant pool) for the current run: read the (count, base) pair inline
// from the bytecode (PC, 4-byte aligned), advance the PC past it, and stash the pair
// in the interpreter's two constant-pool registers (field_40 = count, mpRegisters =
// the string-table base) that the Push*StringDict* ops index off.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionDefineDictionary(AptActionInterpreter* pInterp,
                                                              LocalContextT* pContext)
{
    // The (count, base) pair is at the 4-byte-aligned PC; advance past both dwords.
    const uintptr_t* pPair = reinterpret_cast<const uintptr_t*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 3) & ~static_cast<uintptr_t>(3));
    pContext->mpProgramCounter = reinterpret_cast<const unsigned char*>(pPair + 2);

    // field_40 (console +0x40) <- the entry count; mpRegisters (console +0x44) <- the
    // string-table base pointer the dictionary-push ops index. FLAG: on x64 the base
    // is an 8-byte pointer, so it is held in the pointer-typed mpRegisters slot (the
    // console's 32-bit slot widths differ -- access is by member name).
    pInterp->field_40    = static_cast<uint32_t>(pPair[0]);
    pInterp->mpRegisters = reinterpret_cast<AptValue**>(pPair[1]);
}
