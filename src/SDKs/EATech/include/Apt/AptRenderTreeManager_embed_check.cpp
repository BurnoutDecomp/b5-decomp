// Tiny embed/ODR check for AptRenderTreeManager.h.
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"

void AptRenderTreeManager_EmbedCheckEntry(AptRenderTreeManager* m, AptCharacter* ch,
                                          AptRenderItem* item)
{
    (void)m->Update_CreateItem(ch, 0);
    (void)m->Update_GetTickItemWritable(item, 0);
    m->Update_ItemRemoved(item, 0);
}
