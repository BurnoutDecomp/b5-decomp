// Tiny embed/ODR check for AptNativeHash.h: verifies the struct + its public
// method signatures compile against a TU other than AptNativeHash.cpp.
#include "SDKs/EATech/include/Apt/AptNativeHash.h"

struct AptValue;

static void AptNativeHash_EmbedCheck(AptNativeHash* p, const EAStringC& key, AptValue* v)
{
    p->Set(key, v);
    (void)p->Lookup(key);
    (void)p->IsEmpty();
    (void)p->Get__Proto__();
    (void)p->GetPrototype();
    p->Set__Proto__(v);
    p->SetPrototype(v);
}

void AptNativeHash_EmbedCheckEntry(AptNativeHash* p, const EAStringC& key, AptValue* v)
{
    AptNativeHash_EmbedCheck(p, key, v);
}
