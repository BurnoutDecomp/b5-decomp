// Tiny embed/ODR + layout check for AptCharacterSpriteInst.h (a header-only
// AptCharacterSpriteInstBase leaf: the only X360 function is the dropped scalar
// deleting-destructor thunk @0x82B004F0, so this canary forces the header into
// the build until its CreateCharacterInst / display-list consumer TUs land).
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInst.h"

// Non-virtual leaf (matches the committed AptCharacterInst manual-vtable family):
// a C++ `virtual` here would inject a second compiler vptr and break parity.
// The scalar deleting destructor frees the object at size 36 / 0x24, which equals
// sizeof(AptCharacterSpriteInstBase) -- the leaf adds no members of its own.
static_assert(sizeof(AptCharacterSpriteInst) == sizeof(AptCharacterSpriteInstBase),
              "AptCharacterSpriteInst layout drift (expected == sizeof(AptCharacterSpriteInstBase) == 36)");

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptCharacterSpriteInst_EmbedCheck(AptCharacterSpriteInst* p)
{
    (void)static_cast<AptCharacterSpriteInstBase*>(p);   // confirms the base relationship
    (void)static_cast<AptCharacterInst*>(p);             // ... and the full family chain
}
