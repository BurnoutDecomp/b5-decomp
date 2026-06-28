// Tiny embed/ODR check for AptString.h (self-contained from a foreign TU).
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"

AptNativeString* AptString_EmbedCheckEntry(AptString* s)
{
    return s->GetInternalString();
}
