// Tiny embed/ODR check for AptNativeHash.h: verifies the struct + its public
// method signatures compile against a TU other than AptNativeHash.cpp.
#include "SDKs/EATech/include/Apt/AptNativeHash.h"

class AptValue;

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
static void AptNativeHash_EmbedCheck(AptNativeHash* p, const EAStringC& key, AptValue* v)
{
    p->Set(key, v);
    p->Unset(key);
    (void)p->Lookup(key);
    (void)p->IsEmpty();
    (void)p->Get__Proto__();
    (void)p->GetPrototype();
    p->Set__Proto__(v);
    p->SetPrototype(v);

    AptHashItem* it = p->GetFirstItem();
    if (it)
        (void)p->GetNextItem(it);

    p->SetEventHandler(1);
    (void)p->HasEventHandler(1);
    p->RemoveEventHandler(1);

    p->ClearData();
    p->ClearDataNoDelete();
    p->DestroyGCPointers();
}

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptNativeHash_EmbedCheckEntry(AptNativeHash* p, const EAStringC& key, AptValue* v)
{
    AptNativeHash_EmbedCheck(p, key, v);
}
