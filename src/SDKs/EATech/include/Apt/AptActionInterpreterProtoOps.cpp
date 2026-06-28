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
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"          // Get/SetPrototype, Set__Proto__
#include "SDKs/EATech/include/Apt/AptPrototype.h"           // AptPrototype + SetSuperConstructor
#include "SDKs/EATech/include/Apt/AptObject.h"              // AptObject::SetImplementedObjects
#include "SDKs/EATech/include/Apt/AptArray.h"               // AptArray (interfaces)

// FLAG (AptObject layer -- Adriwin's class:AptObject TU): record the interface
// prototypes a class implements (console AptObject::SetImplementedObjects). Not
// yet a member of the reconstructed AptObject, so encapsulated as a free helper.
extern void AptObject_SetImplementedObjects(AptObject* pObject, AptArray* pInterfaces, int nCount);

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
