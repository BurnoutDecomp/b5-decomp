// ===========================================================================
// EATech Apt -- AptObject.   DECOMPILED from the PS3 EXTERNAL ELF.
//   GetHasClass @0x7DF374 / SetHasClass @0x7DF34C / objectMemberLookup @0x7FC7E8.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptObject.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

#include <cstring>   // strcmp

bool AptObject::GetHasClass() const
{
    return ((mClassFlags >> 23) & 1) != 0;
}

void AptObject::SetHasClass(int bHasClass)
{
    // Clear bit 23, then set it iff bHasClass != 0 (the console's rotate+mask
    // idiom expressed on the whole word).
    mClassFlags = (mClassFlags & ~0x00800000u) | (bHasClass ? 0x00800000u : 0u);
}

AptValue* AptObject::objectMemberLookup(AptValue* const /*pThis*/,
                                        const AptNativeString* const pName) const
{
    // The only native member the base object exposes is "registerClass"; all
    // other members are resolved through the property hash by the caller.
    if (strcmp(pName->c_str(), "registerClass") == 0)
        return gpObjRegistrationFunc;
    return 0;
}
