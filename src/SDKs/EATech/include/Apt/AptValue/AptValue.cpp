// ===========================================================================
// EATech Apt -- AptValue out-of-line virtual bodies.
//
// SHAPE from the Feb-2007 leak (AptValue.h:509 / :535); BODIES confirmed
// against the X360 ARTIST.XEX pseudocode:
//     AptValue::DeleteThis    @ 0x824ED4B8
//     AptValue::ForceDelete   @ 0x824ED4D8
//
// X360 0x824ED4B8 (DeleteThis): a null guard then a virtual call into the
//   scalar-deleting-destructor slot (vtbl +0x38) with the delete flag set --
//   exactly the codegen of `delete this`. The leak body is `delete this`.
// X360 0x824ED4D8 (ForceDelete): vtbl +0x24 (PreDestroy), then vtbl +0x28
//   (DestroyGCPointers), then vtbl +0x38 (`delete this`). Matches the leak's
//   PreDestroy(); DestroyGCPointers(); delete this;.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"

// DeleteThis @ 0x824ED4B8
void AptValue::DeleteThis()
{
    // X360: `if (this) <scalar deleting destructor>(this, 1)`. `delete this`
    // on a non-null receiver lowers to exactly that virtual dispatch.
    delete this;
}

// ForceDelete @ 0x824ED4D8
void AptValue::ForceDelete()
{
    PreDestroy();
    DestroyGCPointers();
    delete this;
}
