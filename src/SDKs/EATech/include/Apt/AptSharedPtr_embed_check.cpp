// Tiny embed/ODR + linkage check for the AptSharedPtr<AptFile> home.
// Exercises each owned entry point so the header layout + signatures compile
// against a translation unit other than AptSharedPtr.cpp.
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
static void AptSharedPtr_EmbedCheck()
{
    AptFilePtr a;
    a.pData = nullptr;

    AptFilePtr b;
    b.pData = nullptr;

    a = b;                                  // operator=
    AptSharedPtr<AptFile>::Dispose(a.pData); // static Dispose
    (void)a.VectorDeletingDestructor(0);     // scalar form
}

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptSharedPtr_EmbedCheckEntry()
{
    AptSharedPtr_EmbedCheck();
}
