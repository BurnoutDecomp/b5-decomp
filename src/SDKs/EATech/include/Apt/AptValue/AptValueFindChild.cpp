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
//      singletons -- none of which are reconstructed yet -- so they are delegated
//      to AptApt_ResolveSpecialName (FLAG, the follow-on; see below).
//   2. GENERIC -- otherwise, if this value is defined, look the name up in this
//      value's native hash (GetNativeHashVirtual), then walk the __proto__ chain
//      (each proto's native hash), then the global-extension object, then the
//      global object. The console reads the embedded hash at value+8; here that is
//      GetNativeHashVirtual() (returns the same &mHash) -- x64-correct, no offset.
//
// EXTRACTION NOTE: the jump table (word_821453C0) and ObjectIndex's name->id
// wordlist were recovered from the decrypted XEX (the rodata technique). Filling
// ObjectIndex's own data tables (so in_word_set actually recognises the names) is
// the ObjectIndex TU's follow-on -- until then in_word_set returns null and
// findChild takes the generic path (still correct for ordinary property lookups).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/Apt/AptObjectIndex.h"

// FLAG (follow-on / AptInit): the special-name resolver (this/_root/_parent/
// _level<N>/_global/Math/Mouse/Stage/Key/String/extern/super). It navigates the
// interpreter target state + the global-object singletons (not reconstructed); it
// switches on the ObjectIndex object-type id (the map is documented above).
extern AptValue* AptApt_ResolveSpecialName(int nObjectTypeId, AptValue* pScope,
                                           const EAStringC* pName, AptValue* pTarget);

// FLAG (AptInit): the AS _global object and the global-extension object (their
// native hashes back the global-scope variable lookup).
extern AptValue* gpGlobalGlobalObject;
extern AptValue* gpGlobalExtensionObject;

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
        return AptApt_ResolveSpecialName(pEntry->miData, this, pName, pTarget);   // FLAG

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

    // The global-extension object, then the global object (skip self).
    if (AptValue* pFound = LookupInObject(gpGlobalExtensionObject, *pName))
        return pFound;
    if (this != gpGlobalGlobalObject)
    {
        if (AptValue* pFound = LookupInObject(gpGlobalGlobalObject, *pName))
            return pFound;
    }
    return 0;
}
