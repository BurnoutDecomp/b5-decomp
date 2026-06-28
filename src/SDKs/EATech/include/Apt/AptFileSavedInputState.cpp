// ===========================================================================
// EATech Apt -- AptFileSavedInputState::~AptFileSavedInputState.
//
// X360 ARTIST `vector deleting destructor' @0x82AEC728 (the only function the
// build emits for this class). It is the compiler-synthesised deleting-destructor
// thunk for AptFileSavedInputState, called by
// StringAsVectorPolicy<AptFileSavedInputState>::ReAlloc when the backing array is
// grown or freed.
//
// Asm shape (r3 = array/this, r4 = flag byte):
//   - flag & 2 (array form): the element count is stored one word BEFORE the
//     array (`lwz r11, -4(r31)`); walk the elements back-to-front
//     (`addi r31,r31,-8` per step = 8-byte stride), calling the per-element
//     destructor -- which is just EAStringC::DecreaseInternalRefCount on the
//     element's first word (`lwz r3,0(r31); bl EAStringC__DecreaseInternalRefCount`)
//     -- then, if flag & 1, `operator delete[]` on the allocation base (the count
//     prefix at this-1). Returns that base.
//   - flag & 0 (scalar form / array bit clear): destruct the single object
//     (the same EAStringC release on *this), then if flag & 1 `operator delete`.
//
// All of that machinery -- the count-prefixed array walk, the back-to-front
// iteration, and the operator delete[] / operator delete selection -- is exactly
// what the host C++ compiler synthesises from a class with a non-trivial
// destructor. So the faithful, semantic-parity reconstruction is to body the
// ONE logical destructor whose only side effect is releasing the embedded
// EAStringC member; ~EAStringC() emits the DecreaseInternalRefCount call the asm
// performs per element, and the compiler re-derives the deleting thunk and the
// array walk. There is no separate hand-written array-delete loop to recover --
// it has no logical source.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptFileSavedInputState.h"

// The destructor's sole work is releasing mInputName, which the compiler runs
// after this (empty) body via the embedded EAStringC member's own destructor --
// the faithful equivalent of the asm's per-element
// EAStringC::DecreaseInternalRefCount(*element).
AptFileSavedInputState::~AptFileSavedInputState()
{
}
