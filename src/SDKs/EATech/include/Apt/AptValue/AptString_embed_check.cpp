// Tiny embed/ODR check for AptString.h (self-contained from a foreign TU).
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
AptNativeString* AptString_EmbedCheckEntry(AptString* s)
{
    return s->GetInternalString();
}
