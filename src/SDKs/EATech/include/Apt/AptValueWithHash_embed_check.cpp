// Tiny embed/ODR check for AptValueWithHash.h.
#include "SDKs/EATech/include/Apt/AptValueWithHash.h"

static void AptValueWithHash_EmbedCheck(AptValueWithHash* p, const EAStringC& k, AptValue* v)
{
    (void)p->GetNativeHashVirtual();
    (void)p->ContainsNativeHashVirtual();
    p->Set(k, v);
    (void)p->Lookup(k);
    (void)p->Get__Proto__();
}

void AptValueWithHash_EmbedCheckEntry(AptValueWithHash* p, const EAStringC& k, AptValue* v)
{
    AptValueWithHash_EmbedCheck(p, k, v);
}
