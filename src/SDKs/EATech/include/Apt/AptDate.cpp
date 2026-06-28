// ===========================================================================
// EATech Apt -- AptDate out-of-line body.   Reconstructed from the X360 ARTIST.XEX.
//
//   AptDate::~AptDate                      @ 0x82AF2AA0
//   AptDate::`vector deleting destructor'  @ 0x82AF2AB0  (compiler-generated thunk
//                                                         -- intentionally dropped)
//
// X360 0x82AF2AA0 (~AptDate): store the AptDate vtable pointer (off_82145ECC) at
//   *this, then tail-call AptObject::~AptObject. That is exactly the codegen of an
//   empty virtual destructor for an AptObject subclass with no members of its own:
//   the compiler resets the vtable to AptDate's at entry, there are no own members
//   to destroy, and the AptObject sub-object destructor (which tears down the
//   AptValueWithHash property hash) runs as the base chain. The body is therefore
//   empty.
//
// The `vector deleting destructor' (0x82AF2AB0) is a compiler-emitted deleting
// destructor thunk (the array-delete / scalar-delete dispatch that calls
// AptObject::operator delete(this, 32)); per project policy these thunks are not
// reconstructed.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptDate.h"

// @ 0x82AF2AA0
AptDate::~AptDate()
{
    // No own members: the vtable reset + the AptObject base-destructor chain
    // (which releases the property hash) are compiler-emitted.
}
