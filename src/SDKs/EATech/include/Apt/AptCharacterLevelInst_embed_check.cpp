// Tiny embed/ODR + layout check for AptCharacterLevelInst.h (a header-only AptCharacterInst leaf:
// the only X360 function is the dropped deleting-destructor thunk, so this canary
// forces the header into the build until its display-list consumer TUs land).
#include "SDKs/EATech/include/Apt/AptCharacterLevelInst.h"

// Non-virtual leaf (matches the committed AptCharacterInst manual-vtable base):
// a C++ `virtual` here would inject a second compiler vptr and break parity.
static_assert(sizeof(AptCharacterLevelInst) >= sizeof(AptCharacterInst), "AptCharacterLevelInst layout drift");

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptCharacterLevelInst_EmbedCheck(AptCharacterLevelInst* p)
{
    (void)static_cast<AptCharacterInst*>(p);   // confirms the base relationship
}
