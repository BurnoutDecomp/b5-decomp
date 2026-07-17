// Tiny embed/ODR check for AptMovie.h.
#include "SDKs/EATech/include/Apt/AptMovie.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
int AptMovie_EmbedCheckEntry(AptMovie* m, const EAStringC* label)
{
    (void)m->mnFrameCount;
    return m->labelToFrame(label);
}
