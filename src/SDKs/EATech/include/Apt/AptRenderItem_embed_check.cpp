// Tiny embed/ODR check for AptRenderItem.h: verifies the base struct + its
// accessor signatures compile against a TU other than AptRenderItem.cpp.
// (AptRenderItem is abstract, so exercise via a pointer.)
#include "SDKs/EATech/include/Apt/AptRenderItem.h"

static void AptRenderItem_EmbedCheck(AptRenderItem* p)
{
    (void)p->GetPositionMatrixConst();
    (void)p->GetColorMatrixConst();
    (void)p->GetDepth();
    p->SetDepth(0);
    (void)p->GetIsVisible();
    (void)p->GetMask();
    (void)p->GetCharacterConst();
    p->AddReference();
    p->ReleaseReference();
    (void)p->Manager_GetFirstChild();
}

void AptRenderItem_EmbedCheckEntry(AptRenderItem* p)
{
    AptRenderItem_EmbedCheck(p);
}
