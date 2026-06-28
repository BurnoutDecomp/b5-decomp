// Tiny embed/ODR check for AptRenderItemShape.h.
#include "SDKs/EATech/include/Apt/AptRenderItemShape.h"

void AptRenderItemShape_EmbedCheckEntry(AptRenderItemShape* p, AptRenderingContext* ctx)
{
    p->Render(ctx, static_cast<AptMaskRenderOperation>(0), 0);
    (void)p->GetDepth();
}
