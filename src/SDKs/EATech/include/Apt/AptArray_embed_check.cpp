// Tiny embed/ODR check for AptArray.h.
#include "SDKs/EATech/include/Apt/AptArray.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
static void AptArray_EmbedCheck(AptArray* p, AptValue* v)
{
    (void)p->length();
    (void)p->GetAt(0);
    (void)p->get(0);
    p->SetAt(0, v);
    p->set(3, v);
}

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptArray_EmbedCheckEntry(AptArray* p, AptValue* v)
{
    AptArray_EmbedCheck(p, v);
}
