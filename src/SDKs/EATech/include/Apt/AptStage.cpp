// ===========================================================================
// EATech Apt -- AptStage out-of-line body. Reconstructed from the X360 ARTIST.XEX
//   AptStage::~AptStage @ 0x82AF2B40.
//
// AptStage is a plain AptObject (the AS `Stage` object) with no data members of
// its own, so the destructor is empty: the X360 body just stores the AptStage
// vtable pointer and tail-calls AptObject::~AptObject -- exactly the codegen MSVC
// emits for an empty user destructor whose base owns all the teardown (the
// AptValueWithHash member destructor releases the property hash).
//
// The X360 `vector deleting destructor' @0x82AF2B50 is the compiler-synthesized
// array-delete thunk (it loops AptObject::~AptObject over the elements, else
// AptObject::operator delete(this, 32)); per the project rule deleting-destructor
// compiler thunks are dropped, not written.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptStage.h"

// @ 0x82AF2B40
AptStage::~AptStage()
{
}
