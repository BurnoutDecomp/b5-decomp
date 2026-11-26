// XDK size optimization: ARTIST compiled RenderWare's rwcore for size (record-form cr0
// tests, as in the rwcore filesys sources).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
#include "rw/rwcore_structs.h"

// FAKEMATCH: retain the descriptor constructor emitted at ARTIST 0x821F05C8.
#pragma inline_depth(0)
template rw::BaseResourceDescriptors<5>::BaseResourceDescriptors();
#pragma inline_depth()
