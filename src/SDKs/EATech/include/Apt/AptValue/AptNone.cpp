// ===========================================================================
// EATech Apt -- AptNone out-of-line body. Reconstructed from the X360 ARTIST
// (AptNone::AptNone @0x82AE62C8) + the leak shape.
//
// X360 @0x82AE62C8 (store-for-store):
//     sub_82AE3000(this, 3);          // AptValueNoGC base ctor, eType = AptVFT_None
//     *this = off_82145770;           // (automatic) AptNone vtable store
//     AptValue::setIsDefined(this, 0);// mark this value undefined
//     AptValue::setRefCount(this, 0xFFF); // pin MAX_REFCOUNT -- the singleton
//
// The vtable store is implicit C++ codegen; the two trailing calls are the ctor
// body. The same MAX_REFCOUNT pin + explicit setIsDefined pattern as AptBoolean's
// shared singletons -- here setIsDefined(0) is what makes it the AS "undefined".
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptNone.h"

// @ 0x82AE62C8
AptNone::AptNone()
    : AptValueNoGC(AptVFT_None)
{
    setIsDefined(0);              // the AS "undefined" value -- not defined
    setRefCount(MAX_REFCOUNT);   // pinned: the shared singleton is never freed
}
