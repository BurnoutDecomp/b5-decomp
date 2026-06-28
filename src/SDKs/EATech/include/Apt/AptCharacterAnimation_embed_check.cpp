// Tiny embed/ODR check for AptCharacterAnimation.h: verifies the struct + its
// accessor signatures compile against a TU other than AptCharacterAnimation.cpp.
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"

static void AptCharacterAnimation_EmbedCheck(AptCharacterAnimation* p, AptCharacter* c, AptFilePtr f)
{
    (void)p->IsImport(0);
    (void)p->UnmapCharacter(c);
    p->ClearCharacterList();
    p->IncCharacterList(f);
    (void)p->mnCharacterCount;
    (void)p->mpImportTable;
}

void AptCharacterAnimation_EmbedCheckEntry(AptCharacterAnimation* p, AptCharacter* c, AptFilePtr f)
{
    AptCharacterAnimation_EmbedCheck(p, c, f);
}
