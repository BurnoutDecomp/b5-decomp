// Tiny embed/ODR check for AptMovie.h.
#include "SDKs/EATech/include/Apt/AptMovie.h"

int AptMovie_EmbedCheckEntry(AptMovie* m, const EAStringC* label)
{
    (void)m->mnFrameCount;
    return m->labelToFrame(label);
}
