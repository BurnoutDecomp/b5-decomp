// Tiny embed/ODR check for AptRenderTreeManager.h.
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptRenderTreeManager_EmbedCheckEntry(AptRenderTreeManager* m, AptCharacter* ch,
                                          AptRenderItem* item)
{
    (void)m->Update_CreateItem(ch, 0);
    (void)m->Update_GetTickItemWritable(item, 0);
    m->Update_ItemRemoved(item, 0);
}
