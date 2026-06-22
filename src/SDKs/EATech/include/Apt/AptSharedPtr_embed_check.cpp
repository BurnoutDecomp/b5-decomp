// Tiny embed/ODR + linkage check for the AptSharedPtr<AptFile> home.
// Exercises each owned entry point so the header layout + signatures compile
// against a translation unit other than AptSharedPtr.cpp.
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"

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

void AptSharedPtr_EmbedCheckEntry()
{
    AptSharedPtr_EmbedCheck();
}
