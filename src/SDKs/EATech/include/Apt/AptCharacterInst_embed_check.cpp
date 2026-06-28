// Tiny embed/ODR check for AptCharacterInst.h.
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"

static void AptCharacterInst_EmbedCheck(AptCharacterInst* p)
{
    (void)p->GetRenderItem();
    (void)p->GetDepth();
    (void)p->GetIsVisible();
    (void)p->GetCharacterConst();
    (void)p->GetPositionMatrixConst();
    p->SetDepth(0);
}

void AptCharacterInst_EmbedCheckEntry(AptCharacterInst* p)
{
    AptCharacterInst_EmbedCheck(p);
}
