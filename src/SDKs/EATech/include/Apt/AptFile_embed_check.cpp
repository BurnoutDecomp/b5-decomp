// Tiny embed/ODR check for AptFile.h: verifies the struct + its EAStringC member
// compile against a translation unit other than AptFile.cpp (i.e. the header is
// self-contained).
#include "SDKs/EATech/include/Apt/AptFile.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
static void AptFile_EmbedCheck(AptFile* p)
{
    p->mnRefCount     = 0;
    p->mnState        = 1;
    p->mnField12      = 1;
    p->mpResolveContext = nullptr;
    p->mpData         = nullptr;
    p->mpDataBlock    = nullptr;
    (void)p->mFileName.c_str();
}

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptFile_EmbedCheckEntry(AptFile* p)
{
    AptFile_EmbedCheck(p);
}
