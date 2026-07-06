// Tiny embed/ODR check for AptCharacterInst.h.
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
static void AptCharacterInst_EmbedCheck(AptCharacterInst* p)
{
    (void)p->GetRenderItem();
    (void)p->GetDepth();
    (void)p->GetIsVisible();
    (void)p->GetCharacterConst();
    (void)p->GetPositionMatrixConst();
    p->SetDepth(0);
}

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptCharacterInst_EmbedCheckEntry(AptCharacterInst* p)
{
    AptCharacterInst_EmbedCheck(p);
}
