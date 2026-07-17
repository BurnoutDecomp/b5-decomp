// Tiny embed/ODR check for AptCharacter.h: verifies the base struct + method
// signatures compile against a translation unit other than AptCharacter.cpp.
#include "SDKs/EATech/include/Apt/AptCharacter.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
static void AptCharacter_EmbedCheck(AptCharacter* p)
{
    (void)p->GetRefCount();
    p->AddCharacterReference();
    p->ReleaseCharacterReference();
    p->ReleaseAnimationFile();
    p->SetupCharacter();
    p->mnType = 0;
    p->mpFixupLink = nullptr;
    p->mpAnimationFile = nullptr;
}

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptCharacter_EmbedCheckEntry(AptCharacter* p)
{
    AptCharacter_EmbedCheck(p);
}
