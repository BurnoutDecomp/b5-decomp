#include "SDKs/Xam/CCriticalSection.h"

// ===========================================================================
// CCriticalSection -- body reconstructed from BURNOUT_X360_ARTIST.XEX.
// See CCriticalSection.h for the layout and asm notes.
//
//   `scalar deleting destructor' @ 0x8297D9E0: store vtable off_8210BC20 into
//   *this; if (flag & 1) ::operator delete(this). The host compiler emits that
//   thunk from the virtual dtor + the default global operator delete, so only
//   the dtor is written here.
// ===========================================================================

// Empty teardown -- the asm dtor only resets the vtable and conditionally frees
// (via the global operator delete); there is no member release in this TU.
CCriticalSection::~CCriticalSection()
{
}
