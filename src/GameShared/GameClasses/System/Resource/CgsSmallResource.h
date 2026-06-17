#pragma once

#include "types.hpp"

// CgsResource small-resource vocabulary. CgsSmallResource.h is the cross-platform
// umbrella: it defines EMemCategory (the resource memory categories) and forward-
// declares the resource/descriptor types, then pulls in the console implementation
// (CgsSmallResourcePS3.h) that actually defines them.
//
// SOURCE: the DecFIGS *PS3* DWARF (CgsSmallResource.h / CgsSmallResourcePS3.h). The X360
// ARTIST target has no DWARF -- only the IDA dump -- but PS3 and X360 share this EA
// codebase, and the X360 IDA confirms the shape used here (the Pool's per-entry
// descriptor occupies 24 bytes == rw::BaseResourceDescriptors<3>).

namespace CgsResource
{
    // CgsSmallResource.h:25 - the memory categories a resource's bytes can live in.
    enum EMemCategory
    {
        E_MEMCAT_MAIN        = 0,
        E_MEMCAT_VIDEO       = 1,
        E_MEMCAT_VIDEOMAPPED = 2,
        E_MEMCAT_INVALID     = 3,
        E_MEMCAT_TOTAL       = 4,
        E_MEMCAT_NUMCATS     = 5,
    };

    struct SmallResourceDescriptor;
    struct SmallResource;
}

// The platform header defines SmallResourceDescriptor / SmallResource (it re-includes
// this umbrella for EMemCategory; the #pragma once guards make the cycle a no-op).
#include "GameShared/GameClasses/System/Resource/CgsSmallResourcePS3.h"
