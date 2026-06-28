// Tiny embed/ODR check for the sprite render-item family.
#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"
#include "SDKs/EATech/include/Apt/AptRenderItemButton.h"
#include "SDKs/EATech/include/Apt/AptRenderItemAnimation.h"

void AptRenderItemSprite_EmbedCheckEntry(AptRenderItemSprite* s, AptRenderItemButton* b,
                                         AptRenderItemAnimation* a, AptRenderingContext* ctx)
{
    s->Render(ctx, static_cast<AptMaskRenderOperation>(0), 0);
    s->PushRenderData(ctx, static_cast<AptMaskRenderOperation>(0), 0);
    s->PopRenderData(ctx, static_cast<AptMaskRenderOperation>(0), 0);
    s->PushRenderDataAbsolute(ctx);
    b->Render(ctx, static_cast<AptMaskRenderOperation>(0), 0);
    a->Render(ctx, static_cast<AptMaskRenderOperation>(0), 0);
}
