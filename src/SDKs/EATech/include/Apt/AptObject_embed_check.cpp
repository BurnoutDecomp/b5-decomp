// Tiny embed/ODR check for AptObject.h.
#include "SDKs/EATech/include/Apt/AptObject.h"

static void AptObject_EmbedCheck(AptObject* p, const AptNativeString* name)
{
    (void)p->GetHasClass();
    p->SetHasClass(1);
    (void)p->objectMemberLookup(0, name);
}

void AptObject_EmbedCheckEntry(AptObject* p, const AptNativeString* name)
{
    AptObject_EmbedCheck(p, name);
}
