// Tiny embed/translation-unit gate for CgsResource::BaseResourcePtr.
// Forces the header to compile in a standalone TU and exercises every reconstructed
// member of the corrected layout (ctor, dtor, IsEqual, AddToNewList) so the gate fails
// loudly on any offset/name regression.
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

namespace
{
    // ResourcePtr<T> exposes the protected lifecycle methods only to itself, so drive
    // the alias-ring path through a small typed instantiation.
    struct Probe : public CgsResource::ResourcePtr<int>
    {
        static void Exercise()
        {
            Probe a;   // self-circular ring
            Probe b;
            a.AddToNewList(&b);             // splice a before b
            bool lEqual = a.IsEqual(&b);    // 3-dword identity compare
            (void)lEqual;
            // dtors run here: each splices itself back out of the ring.
        }
    };
}

void CgsBaseResourcePtr_embed_check()
{
    Probe::Exercise();
}
