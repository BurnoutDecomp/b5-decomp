// Tiny embed/ODR check for AptLoader.h: verifies the loader struct + its
// request-layer method signatures compile against a translation unit other than
// AptLoader.cpp (i.e. the header is self-contained).
#include "SDKs/EATech/include/Apt/AptLoader.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
static void AptLoader_EmbedCheck(AptLoader* pLoader, const EAStringC& name, AptFile* pFile)
{
    AptFilePtr a = pLoader->Load(name);
    AptFilePtr b = pLoader->findFile(name);
    AptFilePtr c = pLoader->IsLoaded(name);
    pLoader->Invalidate(pFile);
    (void)a; (void)b; (void)c;
    (void)pLoader->mpHead;
}

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptLoader_EmbedCheckEntry(AptLoader* pLoader, const EAStringC& name, AptFile* pFile)
{
    AptLoader_EmbedCheck(pLoader, name, pFile);
}
