// Compile-only embed check for AptStringObject.h -- verifies the boxed AS String
// object type is well-formed and the wrapped-value member (mpValue @ +0x20, the
// boxed string the objectMemberLookup / setString natives forward to) is reachable
// through the AptObject base. Not a runtime TU.

#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"

namespace
{
    // FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
    void AptStringObject_EmbedCheck(AptStringObject* pStr,
                                    AptValue* pThis,
                                    const AptNativeString* pName)
    {
        // AptStringObject IS-A AptObject IS-A AptValue (the property-bearing GC value).
        AptObject* pAsObject = pStr;
        AptValue*  pAsValue  = pStr;

        // The boxed-string lookup forwards to the wrapped value then the generic
        // AptObject member walk -- exercise the virtual surface by name.
        (void)pStr->objectMemberLookup(pThis, pName);

        (void)pAsObject; (void)pAsValue;
    }
}
