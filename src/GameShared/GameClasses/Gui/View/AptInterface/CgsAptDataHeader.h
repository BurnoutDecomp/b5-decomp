#pragma once

#include "types.hpp"
#include <cstddef>

namespace CgsResource { struct GuiGeometryObject; }
namespace CgsGui
{
    // ARTIST AptDataHeaderType::FixUp 0x828559D8 / FixDown 0x8285D980.
    struct AptDataHeader
    {
        enum EAptDataState
        {
            E_APTDATASTATE_LOADING = 0,
            E_APTDATASTATE_LOADED = 1,
            E_APTDATASTATE_ACTIVE = 2,
        };

        const char* mpacMovieName;
        void* mpAptData;
        void* mpConstData;
        CgsResource::GuiGeometryObject* mpGeomStruct;
        u32 muSizeOfHeader;
        u32 meCurrentState;

        void FixUp(u32 luDelta);
        void FixDown(u32 luDelta, bool lbEndianSwap);
    };
    CGS_STATIC_ASSERT((offsetof(AptDataHeader, mpAptData) == 4), "ARTIST AptDataHeader.data");
    CGS_STATIC_ASSERT((offsetof(AptDataHeader, mpConstData) == 8), "ARTIST AptDataHeader.const");
    CGS_STATIC_ASSERT((offsetof(AptDataHeader, mpGeomStruct) == 12), "ARTIST AptDataHeader.geometry");
}
