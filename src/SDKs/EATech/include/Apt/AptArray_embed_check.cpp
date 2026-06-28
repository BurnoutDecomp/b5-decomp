// Tiny embed/ODR check for AptArray.h.
#include "SDKs/EATech/include/Apt/AptArray.h"

static void AptArray_EmbedCheck(AptArray* p, AptValue* v)
{
    (void)p->length();
    (void)p->GetAt(0);
    (void)p->get(0);
    p->SetAt(0, v);
    p->set(3, v);
}

void AptArray_EmbedCheckEntry(AptArray* p, AptValue* v)
{
    AptArray_EmbedCheck(p, v);
}
