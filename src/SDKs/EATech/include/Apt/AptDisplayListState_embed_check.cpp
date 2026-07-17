// Tiny embed/ODR check for AptDisplayListState.h.
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptDisplayListState_EmbedCheckEntry(AptDisplayListState* s, AptCIH* cih)
{
    (void)s->GetFirstItem();
    (void)s->getLength();
    (void)s->getValue(0);
    (void)s->insert(2, cih);
    (void)s->removeItem(cih);
}
